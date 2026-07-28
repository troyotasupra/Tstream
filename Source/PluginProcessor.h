#pragma once

#include <JuceHeader.h>
#include "NetworkStreamer.h"

class TstreamAudioProcessor : public juce::AudioProcessor
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

private:
    void applyStreamerSettings();

    NetworkStreamer streamer;
    std::atomic<Mode> currentMode { Mode::off };
    juce::String remoteHost = "127.0.0.1";
    int remotePort = 9200;
    int localPort = 9200;
    std::atomic<double> currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TstreamAudioProcessor)
};
