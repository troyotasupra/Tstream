#include "PluginProcessor.h"
#include "PluginEditor.h"

TstreamAudioProcessor::TstreamAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

TstreamAudioProcessor::~TstreamAudioProcessor() = default;

void TstreamAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate.store (sampleRate);
    streamer.prepare (sampleRate, samplesPerBlock);
    applyStreamerSettings();
}

void TstreamAudioProcessor::releaseResources()
{
}

bool TstreamAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto mono = juce::AudioChannelSet::mono();

    const bool inOk  = layouts.getMainInputChannelSet()  == stereo || layouts.getMainInputChannelSet()  == mono;
    const bool outOk = layouts.getMainOutputChannelSet() == stereo || layouts.getMainOutputChannelSet() == mono;

    return inOk && outOk && layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void TstreamAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

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
