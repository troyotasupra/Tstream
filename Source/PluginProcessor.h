#pragma once

#include <JuceHeader.h>
#include "NetworkStreamer.h"
#include "MonitorOutput.h"

class TstreamAudioProcessor : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    TstreamAudioProcessor();
    ~TstreamAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    using Mode = NetworkStreamer::Mode;

    void setMode (Mode newMode);
    Mode getMode() const { return currentMode; }

    void setRemoteHost (const juce::String& host);
    juce::String getRemoteHost() const { return remoteHost; }

    void setRemotePort (int port);
    int getRemotePort() const { return remotePort; }

    void setLocalPort (int port);
    int getLocalPort() const { return localPort; }

    // Applies all three network settings in one go (one reconfigure/rebuild
    // cycle instead of three back-to-back ones from three separate setter
    // calls). host must already be a validated dotted-quad IPv4 address -
    // see PluginEditor's isValidIPv4(); this deliberately does not accept
    // hostnames, since JUCE's DatagramSocket::write() does a blocking DNS
    // lookup for non-IP-literal hosts, which could otherwise stall the
    // network thread (and therefore this call, on the message thread) for
    // an unbounded time under a slow/unreachable DNS server.
    void setNetworkSettings (const juce::String& host, int newRemotePort, int newLocalPort);

    NetworkStreamer& getStreamer() { return streamer; }

    // Output stage, applied after the receive path has filled the buffer.
    // This is the standalone's replacement for the OBS mixer fader the
    // stream used to sit behind: with OBS gone there is otherwise no way to
    // ride the level going out to Discord without touching the DAW's master,
    // which would also change what everyone in the room hears.
    void setOutputGainDb (float db) { outputGainDb.store (db, std::memory_order_relaxed); }
    float getOutputGainDb() const { return outputGainDb.load (std::memory_order_relaxed); }

    void setMuted (bool shouldMute) { muted.store (shouldMute, std::memory_order_relaxed); }
    bool isMuted() const { return muted.load (std::memory_order_relaxed); }

    // Counts every processBlock call, in every mode. Receive mode previously
    // had no way to distinguish "the audio device never started calling us"
    // from "the stream never arrived" - both look like zero packets and
    // silence, but only one of them is a network problem.
    uint32_t getRenderBlockCount() const { return renderBlocks.load (std::memory_order_relaxed); }

    // Optional second output so the user can hear what listeners are getting.
    // Fed post-gain, so it reflects the fader and mute rather than the raw
    // received stream.
    MonitorOutput& getMonitorOutput() { return monitorOutput; }

    // Auto-launch: when this instance is a plugin in send mode and no receiver
    // is running, start the standalone. Saves the "why can't anyone hear me"
    // round trip that happens when the DAW session is opened but the receiver
    // was never started.
    void setAutoLaunchEnabled (bool shouldLaunch);
    bool isAutoLaunchEnabled() const { return autoLaunch.load (std::memory_order_relaxed); }

    // Empty when a receiver has been seen; otherwise why the last launch
    // attempt could not proceed. Display only.
    juce::String getAutoLaunchStatus() const;

    // True while a standalone instance holds the cross-process lock.
    static bool isReceiverRunning();

    // Where the standalone recorded its own executable path, so a plugin
    // running in a DAW can find it without anything being hardcoded.
    static juce::File getReceiverPathFile();

    // Bottom of the fader travel. Matches the meter's -60dB floor, and is
    // treated as true silence rather than -60dB of signal so the slider's
    // minimum is a real "off" position.
    static constexpr float kMinGainDb = -60.0f;

private:
    // Static member rather than a free function: BusesProperties is protected
    // inside AudioProcessor, so only a member of this class can construct one.
    static BusesProperties makeBusesProperties();

    void applyStreamerSettings();
    void applyOutputGain (juce::AudioBuffer<float>& buffer);

    void timerCallback() override;
    void tryLaunchReceiver();

    float currentTargetGain() const
    {
        return muted.load (std::memory_order_relaxed)
                 ? 0.0f
                 : juce::Decibels::decibelsToGain (outputGainDb.load (std::memory_order_relaxed), kMinGainDb);
    }

    NetworkStreamer streamer;
    MonitorOutput monitorOutput;

    // Held for the process's whole lifetime by the standalone, and used by
    // plugin instances purely as a "is a receiver already up?" probe. A lock
    // rather than a process-name scan, so it can't be fooled by an unrelated
    // Tstream.exe or by the receiver being mid-shutdown.
    std::unique_ptr<juce::InterProcessLock> receiverLock;

    // Non-null only if THIS instance started the receiver. That is exactly the
    // ownership rule we want on shutdown: a receiver the user opened by hand,
    // or one another plugin instance started, must outlive this one - so the
    // handle's existence is the permission to close it, and no extra flag or
    // pid bookkeeping is needed.
    std::unique_ptr<juce::ChildProcess> launchedReceiver;

    std::atomic<bool> autoLaunch { true };
    juce::String autoLaunchStatus;
    juce::uint32 lastLaunchAttemptMs = 0;

    // Long enough that a failing launch can't turn into a process-spawn loop,
    // short enough to recover if the user closes the receiver mid-session.
    static constexpr juce::uint32 kLaunchRetryMs = 15000;

    // Smoothed rather than applied raw: mute is a jump straight to zero, which
    // is the same click generator NetworkStreamer's declick ramp already exists
    // to avoid. The ramp length is set in prepareToPlay once the rate is known.
    static constexpr double kGainRampSeconds = 0.02;
    std::atomic<float> outputGainDb { 0.0f };
    std::atomic<bool> muted { false };
    std::atomic<uint32_t> renderBlocks { 0 };
    juce::SmoothedValue<float> gainSmoothed { 1.0f };
    std::atomic<Mode> currentMode { Mode::off };
    juce::String remoteHost = "127.0.0.1";
    int remotePort = 9200;
    int localPort = 9200;
    std::atomic<double> currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TstreamAudioProcessor)
};
