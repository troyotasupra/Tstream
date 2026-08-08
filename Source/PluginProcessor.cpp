#include "PluginProcessor.h"
#include "PluginEditor.h"

// Both formats get an input bus. The standalone needs one to be able to SEND
// at all - without it there is nothing to capture and the app can only receive.
//
// The reason it was briefly removed (the app holding the UR12's capture side
// open next to the DAW) is solved instead by keeping the *device* closed unless
// the user is actually in send mode - see the editor's applyInputDevice(). A
// bus is a capability; an open capture device is the thing that costs something.
TstreamAudioProcessor::BusesProperties TstreamAudioProcessor::makeBusesProperties()
{
    const auto stereo = juce::AudioChannelSet::stereo();

    return BusesProperties()
             .withInput  ("Input",  stereo, true)
             .withOutput ("Output", stereo, true);
}

namespace
{
    // One name for the whole machine. The standalone holds it; plugins probe it.
    const char* const kReceiverLockName = "TstreamReceiverRunning";
}

juce::File TstreamAudioProcessor::getReceiverPathFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Tstream")
             .getChildFile ("receiver.path");
}

bool TstreamAudioProcessor::isReceiverRunning()
{
    // Probe by trying to take the lock: if it can be taken, nobody is holding
    // it, so no receiver is up. Released immediately either way - this only
    // ever asks a question, it never keeps the lock away from a real receiver.
    juce::InterProcessLock probe (kReceiverLockName);

    if (probe.enter (0))
    {
        probe.exit();
        return false;
    }

    return true;
}

TstreamAudioProcessor::TstreamAudioProcessor()
    : AudioProcessor (makeBusesProperties())
{
    if (wrapperType == wrapperType_Standalone)
    {
        // Claim the lock for as long as this process lives, and record where
        // this executable is so a plugin in a DAW can start it later. Writing
        // the path on every launch means a rebuild or a move is picked up
        // automatically rather than leaving a stale path behind.
        receiverLock = std::make_unique<juce::InterProcessLock> (kReceiverLockName);
        receiverLock->enter (0);

        auto pathFile = getReceiverPathFile();
        pathFile.getParentDirectory().createDirectory();
        pathFile.replaceWithText (juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName());
    }
    else
    {
        startTimer (2000);
    }
}

TstreamAudioProcessor::~TstreamAudioProcessor()
{
    stopTimer();

    // Close the receiver, but only the one this instance started. A receiver
    // the user opened themselves is theirs and is left alone.
    //
    // kill() rather than a graceful request because there is no cheap
    // cross-process "please quit" channel here, and little is lost: the
    // standalone writes its settings on every change rather than only on exit,
    // so at worst a change made in the last few seconds misses PropertiesFile's
    // batching timer.
    if (launchedReceiver != nullptr && launchedReceiver->isRunning())
        launchedReceiver->kill();

    if (receiverLock != nullptr)
        receiverLock->exit();
}

void TstreamAudioProcessor::setAutoLaunchEnabled (bool shouldLaunch)
{
    autoLaunch.store (shouldLaunch, std::memory_order_relaxed);

    if (! shouldLaunch)
        autoLaunchStatus.clear();
}

juce::String TstreamAudioProcessor::getAutoLaunchStatus() const
{
    return autoLaunchStatus;
}

void TstreamAudioProcessor::timerCallback()
{
    if (! autoLaunch.load (std::memory_order_relaxed))
        return;

    // Only in send mode: that is the only configuration where a missing
    // receiver is actually a problem worth solving on the user's behalf.
    if (currentMode.load() != Mode::send)
        return;

    if (isReceiverRunning())
    {
        autoLaunchStatus.clear();
        return;
    }

    tryLaunchReceiver();
}

void TstreamAudioProcessor::tryLaunchReceiver()
{
    const auto now = juce::Time::getMillisecondCounter();

    if (lastLaunchAttemptMs != 0 && now - lastLaunchAttemptMs < kLaunchRetryMs)
        return;

    lastLaunchAttemptMs = now;

    auto pathFile = getReceiverPathFile();

    if (! pathFile.existsAsFile())
    {
        autoLaunchStatus = "Receiver location unknown - run the standalone once";
        return;
    }

    const juce::File exe (pathFile.loadFileAsString().trim());

    if (! exe.existsAsFile())
    {
        autoLaunchStatus = "Receiver missing at " + exe.getFullPathName();
        return;
    }

    // Launched as a tracked child rather than fire-and-forget, so this instance
    // can close the one it started when the DAW shuts down.
    //
    // --tray: an auto-started receiver should not steal focus or throw a window
    // in front of whatever the user is doing in the DAW. It has nothing to show
    // - it is being started precisely because it is meant to run unattended.
    // Launching it by hand from the shortcut still opens normally.
    auto child = std::make_unique<juce::ChildProcess>();

    if (child->start (exe.getFullPathName().quoted() + " --tray"))
    {
        launchedReceiver = std::move (child);
        autoLaunchStatus = "Launching receiver...";
    }
    else
    {
        autoLaunchStatus = "Could not launch " + exe.getFileName();
    }
}

void TstreamAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate.store (sampleRate);
    streamer.prepare (sampleRate, samplesPerBlock);
    applyStreamerSettings();

    gainSmoothed.reset (sampleRate, kGainRampSeconds);
    gainSmoothed.setCurrentAndTargetValue (currentTargetGain());

    monitorOutput.prepare (sampleRate);
}

void TstreamAudioProcessor::releaseResources()
{
}

bool TstreamAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto mono = juce::AudioChannelSet::mono();

    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != stereo && out != mono)
        return false;

    // An absent input bus is explicitly supported, and this is load-bearing for
    // the standalone: it opens no capture device at all (it only ever plays
    // back audio arriving over UDP), which presents the processor with a
    // disabled input bus. Rejecting that layout doesn't produce an error
    // anywhere visible - the wrapper simply fails to configure the processor
    // and the app goes silent, which looks identical to a dead network stream.
    if (in.isDisabled())
        return true;

    return in == out;
}

void TstreamAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    renderBlocks.fetch_add (1, std::memory_order_relaxed);

    switch (currentMode.load())
    {
        case Mode::send:
            streamer.processSendBlock (buffer, currentSampleRate.load());
            break; // pass audio through unchanged, mirroring ReaStream's insert behaviour

        case Mode::receive:
            streamer.processReceiveBlock (buffer);
            break;

        case Mode::off:
        default:
            break; // straight passthrough
    }

    applyOutputGain (buffer);

    // After the gain stage on purpose: the monitor exists to answer "what do
    // listeners hear", so it has to reflect the fader and the mute rather than
    // the raw received stream.
    monitorOutput.pushAudio (buffer);
}

// Applied to every mode, not just receive. In send mode the plugin is an
// insert on someone's master chain, so a fader here would silently alter the
// DAW's own output - which is why it defaults to 0dB/unmuted and why the
// editor only exposes it in the standalone, where this processor IS the
// output stage and nothing downstream can undo it.
void TstreamAudioProcessor::applyOutputGain (juce::AudioBuffer<float>& buffer)
{
    gainSmoothed.setTargetValue (currentTargetGain());

    if (! gainSmoothed.isSmoothing())
    {
        const float g = gainSmoothed.getTargetValue();
        if (g != 1.0f)
            buffer.applyGain (g);

        return;
    }

    // Mid-ramp: the multiplier has to advance once per frame and be shared
    // across channels, otherwise the channels ramp at different rates and
    // a stereo image smears during the fade.
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = gainSmoothed.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }
}

juce::AudioProcessorEditor* TstreamAudioProcessor::createEditor()
{
    return new TstreamAudioProcessorEditor (*this);
}

void TstreamAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("TSTREAM_STATE");
    state.setProperty ("mode", (int) currentMode.load(), nullptr);
    state.setProperty ("remoteHost", remoteHost, nullptr);
    state.setProperty ("remotePort", remotePort, nullptr);
    state.setProperty ("localPort", localPort, nullptr);
    state.setProperty ("outputGainDb", outputGainDb.load(), nullptr);
    state.setProperty ("muted", muted.load(), nullptr);
    state.setProperty ("autoLaunch", autoLaunch.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TstreamAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            currentMode.store ((Mode) (int) state.getProperty ("mode", (int) Mode::off));
            remoteHost = state.getProperty ("remoteHost", remoteHost).toString();
            remotePort = (int) state.getProperty ("remotePort", remotePort);
            localPort  = (int) state.getProperty ("localPort", localPort);
            outputGainDb.store ((float) (double) state.getProperty ("outputGainDb", 0.0));

            // Mute is deliberately NOT restored. This app's entire job is to be
            // audible to listeners, and a session that happened to be saved
            // muted would come back silent while every meter and counter still
            // reads healthy - the single most expensive failure mode here,
            // because it is only discovered by someone else not hearing you.
            muted.store (false);
            autoLaunch.store ((bool) state.getProperty ("autoLaunch", true));

            // Jump straight to the restored gain rather than ramping up to it
            // from whatever the previous session left behind.
            gainSmoothed.setCurrentAndTargetValue (currentTargetGain());

            streamer.setMode (currentMode.load());
            streamer.setRemote (remoteHost, remotePort);
            streamer.setLocalPort (localPort);
            applyStreamerSettings();
        }
    }
}

void TstreamAudioProcessor::setMode (Mode newMode)
{
    currentMode.store (newMode);
    streamer.setMode (newMode);
    applyStreamerSettings();
}

void TstreamAudioProcessor::setRemoteHost (const juce::String& host)
{
    remoteHost = host;
    streamer.setRemote (remoteHost, remotePort);
    applyStreamerSettings();
}

void TstreamAudioProcessor::setRemotePort (int port)
{
    remotePort = port;
    streamer.setRemote (remoteHost, remotePort);
    applyStreamerSettings();
}

void TstreamAudioProcessor::setLocalPort (int port)
{
    localPort = port;
    streamer.setLocalPort (localPort);
    applyStreamerSettings();
}

void TstreamAudioProcessor::setNetworkSettings (const juce::String& host, int newRemotePort, int newLocalPort)
{
    remoteHost = host;
    remotePort = newRemotePort;
    localPort = newLocalPort;
    streamer.setRemote (remoteHost, remotePort);
    streamer.setLocalPort (localPort);
    applyStreamerSettings();
}

void TstreamAudioProcessor::applyStreamerSettings()
{
    streamer.applySettings();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TstreamAudioProcessor();
}
