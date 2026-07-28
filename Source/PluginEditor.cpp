#include "PluginProcessor.h"
#include "PluginEditor.h"

TstreamAudioProcessorEditor::TstreamAudioProcessorEditor (TstreamAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    titleLabel.setText ("TSTREAM", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setColour (juce::Label::textColourId, TstreamColours::text);
    addAndMakeVisible (titleLabel);

    auto setupModeButton = [this] (juce::TextButton& button)
    {
        button.setClickingTogglesState (true);
        button.setRadioGroupId (1);
        button.setColour (juce::TextButton::buttonColourId, TstreamColours::background);
        button.setColour (juce::TextButton::buttonOnColourId, TstreamColours::highlight);
        button.setColour (juce::TextButton::textColourOffId, TstreamColours::text);
        button.setColour (juce::TextButton::textColourOnId, TstreamColours::background);
        addAndMakeVisible (button);
    };
    setupModeButton (offButton);
    setupModeButton (sendButton);
    setupModeButton (receiveButton);
    offButton.onClick    = [this] { setMode (NetworkStreamer::Mode::off); };
    sendButton.onClick   = [this] { setMode (NetworkStreamer::Mode::send); };
    receiveButton.onClick = [this] { setMode (NetworkStreamer::Mode::receive); };

    hostLabel.setText ("Remote IP (Send)", juce::dontSendNotification);
    hostLabel.setColour (juce::Label::textColourId, TstreamColours::text);
    addAndMakeVisible (hostLabel);
    hostEditor.setText (processor.getRemoteHost(), juce::dontSendNotification);
    hostEditor.onFocusLost = [this] { applyHostAndPorts(); };
    hostEditor.onReturnKey = [this] { applyHostAndPorts(); };
    styleTextEditor (hostEditor);
    addAndMakeVisible (hostEditor);

    remotePortLabel.setText ("Remote Port (Send)", juce::dontSendNotification);
    remotePortLabel.setColour (juce::Label::textColourId, TstreamColours::text);
    addAndMakeVisible (remotePortLabel);
    remotePortEditor.setText (juce::String (processor.getRemotePort()), juce::dontSendNotification);
    remotePortEditor.setInputRestrictions (5, "0123456789");
    remotePortEditor.onFocusLost = [this] { applyHostAndPorts(); };
    remotePortEditor.onReturnKey = [this] { applyHostAndPorts(); };
    styleTextEditor (remotePortEditor);
    addAndMakeVisible (remotePortEditor);

    localPortLabel.setText ("Local Port (Receive)", juce::dontSendNotification);
    localPortLabel.setColour (juce::Label::textColourId, TstreamColours::text);
    addAndMakeVisible (localPortLabel);
    localPortEditor.setText (juce::String (processor.getLocalPort()), juce::dontSendNotification);
    localPortEditor.setInputRestrictions (5, "0123456789");
    localPortEditor.onFocusLost = [this] { applyHostAndPorts(); };
    localPortEditor.onReturnKey = [this] { applyHostAndPorts(); };
    styleTextEditor (localPortEditor);
    addAndMakeVisible (localPortEditor);

    localMeter.setCaption ("LOCAL");
    addAndMakeVisible (localMeter);

    remoteMeter.setCaption ("REMOTE");
    remoteMeter.setShowAliveDot (true);
    addAndMakeVisible (remoteMeter);

    statusLabel.setJustificationType (juce::Justification::topLeft);
    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    statusLabel.setColour (juce::Label::textColourId, TstreamColours::text.withAlpha (0.75f));
    addAndMakeVisible (statusLabel);

    const auto currentMode = processor.getMode();
    offButton.setToggleState (currentMode == NetworkStreamer::Mode::off, juce::dontSendNotification);
    sendButton.setToggleState (currentMode == NetworkStreamer::Mode::send, juce::dontSendNotification);
    receiveButton.setToggleState (currentMode == NetworkStreamer::Mode::receive, juce::dontSendNotification);
    updateFieldEnablement();

    // Tall enough for the full status block. At the previous 340px the
    // fixed-height rows above left the status label only ~9px, so it was
    // clipped to nothing - the diagnostics are useless if they don't fit.
    setSize (360, 420);
    startTimerHz (30);
}

TstreamAudioProcessorEditor::~TstreamAudioProcessorEditor()
{
    stopTimer();
}

void TstreamAudioProcessorEditor::styleTextEditor (juce::TextEditor& editor)
{
    editor.setColour (juce::TextEditor::backgroundColourId, TstreamColours::background.brighter (0.08f));
    editor.setColour (juce::TextEditor::textColourId, TstreamColours::text);
    editor.setColour (juce::TextEditor::outlineColourId, TstreamColours::dim);
    editor.setColour (juce::TextEditor::focusedOutlineColourId, TstreamColours::highlight);
}

void TstreamAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (TstreamColours::background);

    g.setColour (TstreamColours::highlight);
    g.fillRect (getLocalBounds().removeFromTop (3));
}

void TstreamAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (3); // leave room for the top accent stripe

    titleLabel.setBounds (area.removeFromTop (30));
    area.removeFromTop (8);

    auto modeRow = area.removeFromTop (28);
    const int buttonWidth = modeRow.getWidth() / 3;
    offButton.setBounds (modeRow.removeFromLeft (buttonWidth).reduced (2));
    sendButton.setBounds (modeRow.removeFromLeft (buttonWidth).reduced (2));
    receiveButton.setBounds (modeRow.reduced (2));
    area.removeFromTop (10);

    auto row = area.removeFromTop (24);
    hostLabel.setBounds (row.removeFromLeft (140));
    hostEditor.setBounds (row);
    area.removeFromTop (6);

    row = area.removeFromTop (24);
    remotePortLabel.setBounds (row.removeFromLeft (140));
    remotePortEditor.setBounds (row);
    area.removeFromTop (6);

    row = area.removeFromTop (24);
    localPortLabel.setBounds (row.removeFromLeft (140));
    localPortEditor.setBounds (row);
    area.removeFromTop (14);

    auto meterRow = area.removeFromTop (100);
    const int meterWidth = meterRow.getWidth() / 2;
    localMeter.setBounds (meterRow.removeFromLeft (meterWidth).reduced (6, 0));
    remoteMeter.setBounds (meterRow.reduced (6, 0));
    area.removeFromTop (10);

    statusLabel.setBounds (area);
}

void TstreamAudioProcessorEditor::setMode (NetworkStreamer::Mode newMode)
{
    processor.setMode (newMode);
    updateFieldEnablement();
}

namespace
{
    // Deliberately IP-literal-only, no hostnames: JUCE's DatagramSocket::write()
    // does a blocking DNS lookup for anything that isn't already a dotted-quad
    // IPv4 address, which could stall the send worker thread (and therefore
    // NetworkStreamer::stop()'s join of it) for an unbounded time under a
    // slow/unreachable DNS server.
    bool isValidIPv4 (const juce::String& text)
    {
        auto parts = juce::StringArray::fromTokens (text, ".", "");
        if (parts.size() != 4)
            return false;

        for (auto& part : parts)
        {
            if (part.isEmpty() || part.length() > 3 || ! part.containsOnly ("0123456789"))
                return false;

            if (part.length() > 1 && part[0] == '0')
                return false; // reject leading zeros (ambiguous/legacy octal interpretation)

            if (part.getIntValue() > 255)
                return false;
        }

        return true;
    }
}

void TstreamAudioProcessorEditor::applyHostAndPorts()
{
    auto hostText = hostEditor.getText();
    if (! isValidIPv4 (hostText))
    {
        // Revert to the last known-good value rather than pass an
        // unvalidated/hostname string down into the network layer.
        hostText = processor.getRemoteHost();
        hostEditor.setText (hostText, juce::dontSendNotification);
    }

    processor.setNetworkSettings (hostText,
                                   remotePortEditor.getText().getIntValue(),
                                   localPortEditor.getText().getIntValue());
}

void TstreamAudioProcessorEditor::updateFieldEnablement()
{
    const auto mode = processor.getMode();
    hostEditor.setEnabled (mode == NetworkStreamer::Mode::send);
    remotePortEditor.setEnabled (mode == NetworkStreamer::Mode::send);
    localPortEditor.setEnabled (mode == NetworkStreamer::Mode::receive);
}

void TstreamAudioProcessorEditor::timerCallback()
{
    auto& streamer = processor.getStreamer();

    localMeter.setLevel (streamer.getLocalPeakLevel());
    remoteMeter.setLevel (streamer.getRemotePeakLevel());
    remoteMeter.setAlive (streamer.isPeerAlive());

    juce::String text;

    switch (processor.getMode())
    {
        case NetworkStreamer::Mode::send:
            // Host-block count sits first deliberately: if the stream stops,
            // the first thing to check is whether the DAW is still calling
            // the plugin at all, which looks identical to a network failure
            // from the receiving end.
            text << "Host blocks: " << (int) streamer.getAudioBlocksProcessed() << "\n"
                 << "Packets sent: " << (int) streamer.getPacketsSent() << "\n"
                 << "Socket write failures: " << (int) streamer.getSocketWriteFailures() << "\n"
                 << "Send queue overflows: " << (int) streamer.getSendQueueOverflows() << "\n"
                 << "Sender thread: " << (streamer.isSendWorkerRunning() ? "running" : "STOPPED") << "\n"
                 << (streamer.isPeerAlive() ? "Peer confirmed receiving" : "No confirmation from peer yet");
            break;

        case NetworkStreamer::Mode::receive:
            text << "Bound: " << (streamer.isReceiverBound() ? "yes" : "no") << "\n"
                 << "Packets received: " << (int) streamer.getPacketsReceived() << "\n"
                 << "Packets dropped: " << (int) streamer.getPacketsDropped() << "\n"
                 << "Buffer underruns: " << (int) streamer.getUnderruns() << "\n"
                 << "Overruns: " << (int) streamer.getOverruns();

            if (streamer.getLastReceivedSampleRate() > 0)
                text << "\nIncoming format: " << (int) streamer.getLastReceivedSampleRate()
                     << " Hz, " << (int) streamer.getLastReceivedChannels() << " ch";
            break;

        case NetworkStreamer::Mode::off:
        default:
            text = "Passthrough (off)";
            break;
    }

    statusLabel.setText (text, juce::dontSendNotification);
}
