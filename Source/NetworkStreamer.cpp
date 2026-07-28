// Must come before NetworkStreamer.h pulls in JuceHeader.h (which transitively
// includes <windows.h>): including winsock2.h first defines _WINSOCKAPI_, which
// makes windows.h skip the legacy winsock.h it would otherwise drag in, avoiding
// a winsock.h/winsock2.h redefinition conflict.
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <winsock2.h>

// SIO_UDP_CONNRESET's home header (mstcpip.h) doesn't reliably resolve given
// this file's JUCE-driven include order, so define its well-known value
// directly - it's derived purely from constants winsock2.h already provides.
#ifndef SIO_UDP_CONNRESET
 #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

#include "NetworkStreamer.h"

namespace
{
    // Windows-specific UDP quirk: if a write() to a peer whose port has become
    // unreachable triggers an ICMP "port unreachable" back to this socket, the
    // *next* read on it can return an error immediately instead of waiting out
    // its normal timeout - and a loop that treats every read error as "nothing
    // yet, try again" turns that into a tight CPU-spinning busy-loop until the
    // condition clears. Disabling this behaviour makes reads just keep timing
    // out normally instead.
    void disableConnResetNotifications (juce::DatagramSocket& socket)
    {
        const SOCKET sock = (SOCKET) socket.getRawSocketHandle();
        if (sock == INVALID_SOCKET)
            return;

        BOOL reportPortUnreachable = FALSE;
        DWORD bytesReturned = 0;
        WSAIoctl (sock, SIO_UDP_CONNRESET, &reportPortUnreachable, sizeof (reportPortUnreachable),
                  nullptr, 0, &bytesReturned, nullptr, nullptr);
    }
}

NetworkStreamer::NetworkStreamer()
{
    fifoBuffer.setSize (NetStream::kMaxChannels, fifo.getTotalSize());
}

NetworkStreamer::~NetworkStreamer()
{
    stop();
}

void NetworkStreamer::prepare (double sampleRate, int /*maxBlockSize*/)
{
    // Some hosts call prepareToPlay() again mid-session (sample-rate or
    // buffer-size change, freeze/unfreeze, etc.) while a receiverThread may
    // already be live and writing into fifo/fifoBuffer. Take the same lock
    // applySettings() uses and fully stop first, so resizing fifoBuffer
    // (which reallocates its backing storage) can never race a concurrent
    // write into the old memory.
    const juce::ScopedLock sl (reconfigureLock);
    stop();

    const int capacity = juce::jmax (8192, (int) (sampleRate * 2.0));
    fifo.setTotalSize (capacity);
    fifoBuffer.setSize (NetStream::kMaxChannels, capacity, false, true, true);
    fifo.reset();

    primeThresholdSamples = (int) (sampleRate * (kJitterBufferTargetMs / 1000.0));
    primed.store (false);

    declickFadeSamples = juce::jmax (1, (int) (sampleRate * (kDeclickFadeMs / 1000.0)));
    lastOutputSample.fill (0.0f);
}

void NetworkStreamer::setMode (Mode newMode)
{
    mode.store (newMode);
}

void NetworkStreamer::setRemote (const juce::String& host, int port)
{
    pendingHost = host;
    pendingRemotePort.store (port);
}

void NetworkStreamer::setLocalPort (int port)
{
    pendingLocalPort.store (port);
}

void NetworkStreamer::applySettings()
{
    // Always called from the message thread. Fully tear down then rebuild,
    // so there is never a window where a half-configured socket is live.
    // The lock ensures the audio thread (which only ever tries a non-
    // blocking lock in processSendBlock()/processReceiveBlock()) can never
    // observe fifo/sendFifo mid-teardown.
    const juce::ScopedLock sl (reconfigureLock);
    stop();

    packetsSent.store (0);
    packetsReceived.store (0);
    packetsDropped.store (0);
    underruns.store (0);
    overruns.store (0);
    lastReceivedSampleRate.store (0);
    lastReceivedChannels.store (0);
    localPeakLevel.store (0.0f);
    remotePeakLevel.store (0.0f);
    lastPeerStatusMs.store (0);
    samplesSinceStatusSend = 0;
    fifo.reset();
    primed.store (false);
    lastOutputSample.fill (0.0f);

    switch (mode.load())
    {
        case Mode::send:
        {
            remoteHostForSend = pendingHost;
            remotePortForSend = pendingRemotePort.load();
            sendSocket = std::make_unique<juce::DatagramSocket>();
            sendSocket->bindToPort (0);
            disableConnResetNotifications (*sendSocket);
            sendFifo.reset();

            sendWorkerThread = std::make_unique<SendWorkerThread> (*this);
            sendWorkerThread->startThread();
            break;
        }

        case Mode::receive:
        {
            receiverBound.store (false);
            receiverThread = std::make_unique<ReceiverThread> (*this, pendingLocalPort.load());
            receiverThread->startThread();
            break;
        }

        case Mode::off:
        default:
            break;
    }
}

void NetworkStreamer::stop()
{
    // Stop the worker thread BEFORE tearing the socket down - it's the only
    // thing that ever touches sendSocket, so once it's joined there's no one
    // left who could race with resetting it.
    if (sendWorkerThread != nullptr)
    {
        sendWorkerThread->signalThreadShouldExit();
        sendWorkerThread->stopThread (2000);
        sendWorkerThread.reset();
    }

    sendSocket.reset();

    if (receiverThread != nullptr)
    {
        receiverThread->signalThreadShouldExit();
        receiverThread->stopThread (2000);
        receiverThread.reset();
    }

    receiverBound.store (false);
}

void NetworkStreamer::updateLocalPeak (float blockPeak) noexcept
{
    const float decayed = localPeakLevel.load (std::memory_order_relaxed) * 0.85f;
    localPeakLevel.store (juce::jmax (blockPeak, decayed), std::memory_order_relaxed);
}

void NetworkStreamer::handleStatusPacket (const void* data, int numBytes)
{
    if (numBytes < (int) NetStream::kStatusPacketSize)
        return;

    NetStream::StatusPacket status;
    std::memcpy (&status, data, sizeof (status));

    if (status.magic != NetStream::kStatusMagic)
        return;

    remotePeakLevel.store (status.peakLevel);
    lastPeerStatusMs.store (juce::Time::currentTimeMillis());
}

void NetworkStreamer::enqueueOutbound (const void* data, int size)
{
    int start1, sz1, start2, sz2;
    sendFifo.prepareToWrite (1, start1, sz1, start2, sz2);
    const int granted = sz1 + sz2;
    if (granted < 1)
    {
        sendQueueOverflows.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    const int slot = (sz1 > 0) ? start1 : start2;
    std::memcpy (sendQueueStorage[(size_t) slot].data, data, (size_t) size);
    sendQueueStorage[(size_t) slot].size = size;
    sendFifo.finishedWrite (1);
}

// Audio-thread only: must never block or make a syscall (a small host
// buffer, e.g. 128 samples, leaves only ~2.7ms of budget at 48kHz). This
// only computes a peak and memcpy's pre-sized packets into a lock-free
// queue; SendWorkerThread does the actual (potentially slow) network I/O.
void NetworkStreamer::processSendBlock (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    // Non-blocking: if a settings/prepare reconfiguration is in progress on
    // the message thread right now, just skip this block entirely rather
    // than risk touching sendFifo/sendQueueStorage mid-teardown. This is a
    // rare, brief, user-triggered event (changing mode/IP/port), never part
    // of steady-state streaming.
    const juce::GenericScopedTryLock<juce::CriticalSection> reconfigureGuard (reconfigureLock);
    if (! reconfigureGuard.isLocked())
        return;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    const int numCh = juce::jlimit (1, NetStream::kMaxChannels, buffer.getNumChannels());

    float blockPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            blockPeak = juce::jmax (blockPeak, std::abs (data[i]));
    }
    updateLocalPeak (blockPeak);

    char packet[NetStream::kMaxPacketSize];
    int offset = 0;

    while (offset < numSamples)
    {
        const int chunk = juce::jmin (NetStream::kMaxSamplesPerPacket, numSamples - offset);

        NetStream::PacketHeader header;
        header.magic = NetStream::kMagic;
        header.sequence = sendSequence.fetch_add (1);
        header.sampleRate = (uint32_t) sampleRate;
        header.numChannels = (uint32_t) numCh;
        header.numSamples = (uint32_t) chunk;

        std::memcpy (packet, &header, sizeof (header));
        auto* dst = reinterpret_cast<float*> (packet + NetStream::kHeaderSize);

        for (int i = 0; i < chunk; ++i)
            for (int ch = 0; ch < numCh; ++ch)
                dst[i * numCh + ch] = buffer.getReadPointer (ch)[offset + i];

        const int packetSize = (int) (NetStream::kHeaderSize + (size_t) numCh * (size_t) chunk * sizeof (float));
        enqueueOutbound (packet, packetSize);

        offset += chunk;
    }

    // Throttled status packet, roughly every 150ms regardless of host block
    // size - lets the receiver show what this end is sending without
    // flooding the network. Shares the same outbound queue as audio.
    samplesSinceStatusSend += numSamples;
    if (samplesSinceStatusSend >= (int) (sampleRate * 0.15))
    {
        samplesSinceStatusSend = 0;

        NetStream::StatusPacket status;
        status.magic = NetStream::kStatusMagic;
        status.peakLevel = localPeakLevel.load (std::memory_order_relaxed);
        status.packetsSeen = packetsSent.load();
        enqueueOutbound (&status, (int) NetStream::kStatusPacketSize);
    }
}

void NetworkStreamer::processReceiveBlock (juce::AudioBuffer<float>& buffer)
{
    // Same rationale as processSendBlock(): never block waiting on a
    // reconfiguration; just output silence for this one block if so.
    const juce::GenericScopedTryLock<juce::CriticalSection> reconfigureGuard (reconfigureLock);
    if (! reconfigureGuard.isLocked())
    {
        buffer.clear();
        return;
    }

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    bool justPrimed = false;
    if (! primed.load (std::memory_order_relaxed))
    {
        // Stay silent until enough audio has queued up to absorb network
        // jitter, rather than starting playback the instant any data shows up.
        if (fifo.getNumReady() >= primeThresholdSamples)
        {
            primed.store (true, std::memory_order_relaxed);
            justPrimed = true;
        }
        else
        {
            buffer.clear();
            return;
        }
    }

    // Clock-drift correction: the sender and receiver each have their own
    // independent audio clock, which never match exactly. Left uncorrected,
    // a faster sender slowly grows this backlog forever, adding more and
    // more latency over the course of a session (not just the intended
    // jitter-buffer cushion). If the backlog has grown well past target,
    // silently discard the excess to pull latency back down.
    const int readyNow = fifo.getNumReady();
    const int driftCeiling = primeThresholdSamples * 4;
    if (readyNow > driftCeiling)
    {
        const int excess = readyNow - primeThresholdSamples;
        int dt1, ds1, dt2, ds2;
        fifo.prepareToRead (excess, dt1, ds1, dt2, ds2);
        fifo.finishedRead (ds1 + ds2);
    }

    int start1, size1, start2, size2;
    fifo.prepareToRead (numSamples, start1, size1, start2, size2);
    const int available = size1 + size2;

    const int numCh = buffer.getNumChannels();

    auto copyRange = [&] (int destOffset, int fifoStart, int count)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            const int srcCh = juce::jmin (ch, fifoBuffer.getNumChannels() - 1);
            buffer.copyFrom (ch, destOffset, fifoBuffer, srcCh, fifoStart, count);
        }
    };

    if (size1 > 0) copyRange (0, start1, size1);
    if (size2 > 0) copyRange (size1, start2, size2);

    fifo.finishedRead (available);

    // Fade in the first block after re-buffering, instead of jumping
    // straight from silence to whatever amplitude the first real sample
    // happens to be.
    if (justPrimed && available > 0)
    {
        const int fadeLen = juce::jmin (declickFadeSamples, available);
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < fadeLen; ++i)
                data[i] *= (float) i / (float) fadeLen;
        }
    }

    if (available < numSamples)
    {
        underruns.fetch_add (1);
        buffer.clear (available, numSamples - available);

        // Fade the tail down to silence instead of an instant jump to
        // exactly 0.0f - that jump is a textbook click generator, and it
        // fires on every underrun regardless of how the priming logic is
        // tuned, so it needs fixing at the waveform level, not by changing
        // when underruns happen.
        const int shortfall = numSamples - available;
        const int fadeLen = juce::jmin (declickFadeSamples, shortfall);
        for (int ch = 0; ch < numCh; ++ch)
        {
            const int srcCh = juce::jmin (ch, (int) lastOutputSample.size() - 1);
            const float startLevel = (available > 0) ? buffer.getSample (ch, available - 1)
                                                       : lastOutputSample[(size_t) srcCh];
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < fadeLen; ++i)
            {
                const float t = (float) (i + 1) / (float) (fadeLen + 1);
                data[available + i] = startLevel * (1.0f - t);
            }
        }

        primed.store (false, std::memory_order_relaxed); // re-buffer rather than limp along starved
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        const int srcCh = juce::jmin (ch, (int) lastOutputSample.size() - 1);
        lastOutputSample[(size_t) srcCh] = buffer.getSample (ch, numSamples - 1);
    }
}

void NetworkStreamer::pushSamplesToFifo (const float* interleaved, int numSamples, int numChannels)
{
    float blockPeak = 0.0f;
    const int total = numSamples * numChannels;
    for (int i = 0; i < total; ++i)
        blockPeak = juce::jmax (blockPeak, std::abs (interleaved[i]));
    updateLocalPeak (blockPeak);

    if (fifo.getFreeSpace() < numSamples)
    {
        overruns.fetch_add (1);
        return;
    }

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

    auto deinterleaveInto = [&] (int fifoStart, int fifoOffset, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const int srcIndex = fifoOffset + i;
            const float left = interleaved[(size_t) srcIndex * numChannels + 0];
            const float right = numChannels > 1 ? interleaved[(size_t) srcIndex * numChannels + 1] : left;

            fifoBuffer.setSample (0, fifoStart + i, left);
            fifoBuffer.setSample (1, fifoStart + i, right);
        }
    };

    if (size1 > 0) deinterleaveInto (start1, 0, size1);
    if (size2 > 0) deinterleaveInto (start2, size1, size2);

    fifo.finishedWrite (size1 + size2);
}

void NetworkStreamer::ReceiverThread::run()
{
    juce::DatagramSocket socket;

    if (! socket.bindToPort (port))
        return;

    disableConnResetNotifications (socket);
    owner.receiverBound.store (true);

    juce::HeapBlock<char> packetBuf ((size_t) NetStream::kMaxPacketSize);
    uint32_t expectedSeq = 0;
    bool haveSeq = false;

    juce::String peerAddress;
    int peerPort = 0;
    bool havePeer = false;
    int packetsSinceStatus = 0;

    while (! threadShouldExit())
    {
        const int ready = socket.waitUntilReady (true, 200);
        if (ready <= 0)
            continue;

        juce::String senderIP;
        int senderPort = 0;
        const int bytesRead = socket.read (packetBuf.getData(), (int) NetStream::kMaxPacketSize, false, senderIP, senderPort);

        if (bytesRead < (int) sizeof (uint32_t))
            continue;

        uint32_t magic = 0;
        std::memcpy (&magic, packetBuf.getData(), sizeof (magic));

        if (magic == NetStream::kStatusMagic)
        {
            owner.handleStatusPacket (packetBuf.getData(), bytesRead);
            continue;
        }

        if (magic != NetStream::kMagic || bytesRead < (int) NetStream::kHeaderSize)
            continue;

        NetStream::PacketHeader header;
        std::memcpy (&header, packetBuf.getData(), sizeof (header));

        const size_t expectedBytes = NetStream::kHeaderSize
                                        + (size_t) header.numChannels * (size_t) header.numSamples * sizeof (float);
        if ((size_t) bytesRead < expectedBytes
            || header.numChannels == 0 || header.numChannels > (uint32_t) NetStream::kMaxChannels
            || header.numSamples == 0 || header.numSamples > (uint32_t) NetStream::kMaxSamplesPerPacket)
            continue;

        peerAddress = senderIP;
        peerPort = senderPort;
        havePeer = true;

        owner.lastReceivedSampleRate.store (header.sampleRate);
        owner.lastReceivedChannels.store (header.numChannels);
        owner.packetsReceived.fetch_add (1);

        if (haveSeq)
        {
            // A reordered/duplicate packet, or the sender restarting its
            // sequence counter, delivers header.sequence < expectedSeq.
            // Unsigned subtraction would then underflow to a huge garbage
            // value; only count it as drops when it's genuinely a forward gap.
            if (header.sequence >= expectedSeq)
            {
                const uint32_t gap = header.sequence - expectedSeq;
                if (gap != 0)
                    owner.packetsDropped.fetch_add (gap);
            }
        }
        expectedSeq = header.sequence + 1;
        haveSeq = true;

        const auto* samples = reinterpret_cast<const float*> (packetBuf.getData() + NetStream::kHeaderSize);
        owner.pushSamplesToFifo (samples, (int) header.numSamples, (int) header.numChannels);

        if (havePeer && ++packetsSinceStatus >= 40)
        {
            packetsSinceStatus = 0;

            NetStream::StatusPacket status;
            status.magic = NetStream::kStatusMagic;
            status.peakLevel = owner.getLocalPeakLevel();
            status.packetsSeen = owner.packetsReceived.load();
            socket.write (peerAddress, peerPort, &status, (int) NetStream::kStatusPacketSize);
        }
    }
}

void NetworkStreamer::SendWorkerThread::run()
{
    char ackBuf[64];

    while (! threadShouldExit())
    {
        bool sentAny = false;

        // Drain everything currently queued - this is where the real
        // (potentially slow) network write happens, entirely off the audio
        // thread's real-time deadline.
        for (;;)
        {
            int start1, sz1, start2, sz2;
            owner.sendFifo.prepareToRead (1, start1, sz1, start2, sz2);
            const int granted = sz1 + sz2;
            if (granted < 1)
                break;

            const int slot = (sz1 > 0) ? start1 : start2;
            const auto& pkt = owner.sendQueueStorage[(size_t) slot];

            if (owner.sendSocket != nullptr)
            {
                const int written = owner.sendSocket->write (owner.remoteHostForSend, owner.remotePortForSend,
                                                               pkt.data, pkt.size);
                if (written > 0 && pkt.size >= (int) sizeof (uint32_t))
                {
                    uint32_t magic = 0;
                    std::memcpy (&magic, pkt.data, sizeof (magic));
                    if (magic == NetStream::kMagic)
                        owner.packetsSent.fetch_add (1);
                }
            }

            owner.sendFifo.finishedRead (1);
            sentAny = true;
        }

        // Opportunistically check for incoming status packets from the
        // receiver. Also paces the loop when idle (bounded wake latency).
        if (owner.sendSocket != nullptr)
        {
            const int ready = owner.sendSocket->waitUntilReady (true, sentAny ? 0 : 5);
            if (ready > 0)
            {
                const int bytesRead = owner.sendSocket->read (ackBuf, (int) sizeof (ackBuf), false);
                if (bytesRead >= (int) sizeof (uint32_t))
                {
                    uint32_t magic = 0;
                    std::memcpy (&magic, ackBuf, sizeof (magic));
                    if (magic == NetStream::kStatusMagic)
                        owner.handleStatusPacket (ackBuf, bytesRead);
                }
            }
        }
    }
}
