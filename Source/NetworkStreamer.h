#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "PacketFormat.h"

// Handles the actual UDP send/receive engine for the plugin.
//
// Send mode: processSendBlock() runs on the audio thread and must never
// block or make syscalls (a blocking network write can easily blow a small
// host buffer's real-time deadline, e.g. a 128-sample buffer is only ~2.7ms
// at 48kHz). So it only packetizes audio and pushes the raw bytes into a
// lock-free single-producer/single-consumer queue; a dedicated
// SendWorkerThread drains that queue and does the actual socket writes (and
// also listens for incoming status packets from the receiver on the same
// socket - JUCE's DatagramSocket supports one thread reading while another
// writes, but here it's simpler still: only the worker thread ever touches
// the socket at all).
//
// Receive mode: a dedicated background thread owns the listening socket
// exclusively and pushes decoded samples into a lock-free single-producer/
// single-consumer ring buffer; processReceiveBlock() drains it from the
// audio thread and never touches the socket. That same thread also learns
// the sender's address from incoming audio packets and periodically echoes
// a status packet back to it.
//
// In both modes, getLocalPeakLevel() reflects the audio this instance is
// directly touching (outgoing when sending, incoming when receiving), while
// getRemotePeakLevel()/isPeerAlive() reflect what the *other* end last
// reported about itself - so either side can show "is my peer okay?"
// without needing to look at the peer's own window.
class NetworkStreamer
{
public:
    enum class Mode { off, send, receive };

    NetworkStreamer();
    ~NetworkStreamer();

    void prepare (double sampleRate, int maxBlockSize);

    void setMode (Mode newMode);
    void setRemote (const juce::String& host, int port);
    void setLocalPort (int port);

    // Call once after changing mode/remote/localPort to (re)apply them.
    void applySettings();

    void processSendBlock (const juce::AudioBuffer<float>& buffer, double sampleRate);
    void processReceiveBlock (juce::AudioBuffer<float>& buffer);

    // Stats, safe to poll from the message thread (e.g. a UI timer).
    uint32_t getPacketsSent() const noexcept       { return packetsSent.load(); }
    uint32_t getPacketsReceived() const noexcept   { return packetsReceived.load(); }
    uint32_t getPacketsDropped() const noexcept    { return packetsDropped.load(); }
    uint32_t getUnderruns() const noexcept         { return underruns.load(); }
    uint32_t getOverruns() const noexcept          { return overruns.load(); }
    uint32_t getSendQueueOverflows() const noexcept { return sendQueueOverflows.load(); }
    uint32_t getLastReceivedSampleRate() const noexcept { return lastReceivedSampleRate.load(); }
    uint32_t getLastReceivedChannels() const noexcept   { return lastReceivedChannels.load(); }
    bool isReceiverBound() const noexcept          { return receiverBound.load(); }

    // Diagnostics for the "stream stops mid-session" failure. These exist to
    // tell apart the causes that look identical from the receiving end (the
    // packet counter simply stops climbing):
    //   - the host stopped calling processBlock  -> audioBlocksProcessed stalls
    //   - the socket started refusing writes     -> socketWriteFailures climbs
    //   - the queue backed up                    -> sendQueueOverflows climbs
    //   - the worker thread died                 -> isSendWorkerRunning() false
    // Without these all four are indistinguishable after the fact.
    uint32_t getSocketWriteFailures() const noexcept { return socketWriteFailures.load(); }
    uint32_t getAudioBlocksProcessed() const noexcept { return audioBlocksProcessed.load(); }
    bool isSendWorkerRunning() const noexcept;

    // Metering, safe to poll from the message thread. Linear peak amplitude,
    // roughly 0..1+; convert to dB for display.
    float getLocalPeakLevel() const noexcept  { return localPeakLevel.load(); }
    float getRemotePeakLevel() const noexcept { return remotePeakLevel.load(); }

    // True if a status packet from the peer has arrived within the last second.
    bool isPeerAlive() const noexcept
    {
        return lastPeerStatusMs.load() != 0
               && (juce::Time::currentTimeMillis() - lastPeerStatusMs.load()) < 1000;
    }

private:
    class ReceiverThread : public juce::Thread
    {
    public:
        ReceiverThread (NetworkStreamer& ownerToUse, int portToUse)
            : juce::Thread ("Tstream Receiver"), owner (ownerToUse), port (portToUse) {}

        void run() override;

    private:
        NetworkStreamer& owner;
        int port;
    };

    // Drains the lock-free outbound packet queue that processSendBlock()
    // (audio thread) fills, doing the actual (potentially slow) socket
    // writes off the real-time thread. Also opportunistically listens for
    // incoming status packets from the receiver on the same socket.
    class SendWorkerThread : public juce::Thread
    {
    public:
        explicit SendWorkerThread (NetworkStreamer& ownerToUse)
            : juce::Thread ("Tstream Sender"), owner (ownerToUse) {}

        void run() override;

    private:
        NetworkStreamer& owner;
    };

    void stop();
    void pushSamplesToFifo (const float* interleaved, int numSamples, int numChannels);
    void updateLocalPeak (float blockPeak) noexcept;
    void handleStatusPacket (const void* data, int numBytes);
    void enqueueOutbound (const void* data, int size);

    // Guards every field below against a settings change (applySettings(),
    // triggered from the message thread by mode/IP/port edits) or a host
    // buffer/sample-rate change (prepare(), which resizes fifo/fifoBuffer)
    // racing the audio thread's processSendBlock()/processReceiveBlock().
    // Both of those take a blocking ScopedLock (they're message-thread-only,
    // never real-time); the audio thread only ever takes a non-blocking
    // tryLock and simply skips that block if reconfiguration is in progress
    // - so the audio thread can never stall waiting on the message thread,
    // and reconfiguration always has exclusive access to fully tear down and
    // rebuild without a torn read/write of e.g. a QueuedPacket or fifoBuffer.
    juce::CriticalSection reconfigureLock;

    std::atomic<Mode> mode { Mode::off };
    juce::String pendingHost = "127.0.0.1";
    std::atomic<int> pendingRemotePort { 9200 };
    std::atomic<int> pendingLocalPort { 9200 };

    // --- send mode state ---
    // sendSocket is only ever touched by SendWorkerThread (both writing
    // audio/status packets and reading incoming acks) and by the message
    // thread during applySettings()/stop() - and the worker thread is always
    // fully stopped and joined before the message thread resets the socket,
    // so no lock is needed for it.
    std::unique_ptr<juce::DatagramSocket> sendSocket;
    std::unique_ptr<SendWorkerThread> sendWorkerThread;
    juce::String remoteHostForSend;
    int remotePortForSend = 0;
    std::atomic<uint32_t> sendSequence { 0 };
    int samplesSinceStatusSend = 0; // audio-thread-only

    // Lock-free outbound packet queue: processSendBlock() (producer) only
    // ever memcpy's a pre-sized packet into the next slot; SendWorkerThread
    // (consumer) is the only thing that ever calls the network write().
    struct QueuedPacket { char data[NetStream::kMaxPacketSize]; int size = 0; };
    static constexpr int kSendQueueCapacity = 256; // ~680ms of headroom at 128 samples/48kHz
    juce::AbstractFifo sendFifo { kSendQueueCapacity };
    std::vector<QueuedPacket> sendQueueStorage { (size_t) kSendQueueCapacity };
    std::atomic<uint32_t> sendQueueOverflows { 0 };

    // --- receive mode state ---
    std::unique_ptr<ReceiverThread> receiverThread;
    juce::AbstractFifo fifo { 8192 };
    juce::AudioBuffer<float> fifoBuffer;
    std::atomic<bool> receiverBound { false };

    // Jitter buffer: real network paths (unlike loopback/LAN) never deliver
    // packets at perfectly even spacing, so draining the fifo the instant
    // there's *any* data causes constant partial underruns (pops), even with
    // zero real packet loss. Instead we wait for a small cushion of audio to
    // accumulate before starting playback, and re-buffer (rather than
    // limping along starved) any time the cushion runs out.
    static constexpr int kJitterBufferTargetMs = 100;
    std::atomic<bool> primed { false };
    int primeThresholdSamples = 0; // computed in prepare()

    // Declicking: every silence transition (underrun shortfall, or the
    // silence->audio moment playback resumes after re-buffering) used to be
    // an instant jump to/from exactly 0.0f - a textbook click generator,
    // independent of how often the transition fires. A short linear ramp
    // removes the discontinuity. lastOutputSample carries the fade-out's
    // starting amplitude across calls, for when the fifo is already empty
    // entering a call (so there's no in-block sample left to taper from).
    static constexpr int kDeclickFadeMs = 5;
    int declickFadeSamples = 0; // computed in prepare()
    std::array<float, NetStream::kMaxChannels> lastOutputSample {};

    // Appends a periodic stats line to a log file so a failure that happens
    // while the user is looking at OBS (not at this plugin's window) still
    // leaves a record. Deliberately called only from the send worker thread
    // and the message thread - never from the audio thread, since it does
    // real file I/O.
    void writeDiagnosticLine();

    std::unique_ptr<juce::FileLogger> diagnosticLog;
    juce::int64 lastDiagnosticLogMs = 0;

    // --- shared stats ---
    std::atomic<uint32_t> packetsSent { 0 };
    std::atomic<uint32_t> socketWriteFailures { 0 };
    std::atomic<uint32_t> audioBlocksProcessed { 0 };
    std::atomic<uint32_t> packetsReceived { 0 };
    std::atomic<uint32_t> packetsDropped { 0 };
    std::atomic<uint32_t> underruns { 0 };
    std::atomic<uint32_t> overruns { 0 };
    std::atomic<uint32_t> lastReceivedSampleRate { 0 };
    std::atomic<uint32_t> lastReceivedChannels { 0 };

    // --- metering / peer status ---
    std::atomic<float> localPeakLevel { 0.0f };
    std::atomic<float> remotePeakLevel { 0.0f };
    std::atomic<juce::int64> lastPeerStatusMs { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NetworkStreamer)
};
