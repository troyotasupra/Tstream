#include "PluginProcessor.h"
#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
 // Pulled in for StandalonePluginHolder::getInstance(), which is the only
 // supported way to reach the standalone wrapper's AudioDeviceManager from
 // inside the plugin's own editor. Guarded because this header is meaningless
 // - and won't compile - in the VST3 target.
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

TstreamAudioProcessorEditor::TstreamAudioProcessorEditor (TstreamAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      isStandalone (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
{
    titleLabel.setText ("TSTREAM", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    titleLabel.setJustificationType (juce::Justification::centred);
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

    if (isStandalone)
    {
    outputDeviceLabel.setText ("Output Device", juce::dontSendNotification);
    outputDeviceLabel.setColour (juce::Label::textColourId, TstreamColours::text);
    addAndMakeVisible (outputDeviceLabel);

    outputDeviceBox.setColour (juce::ComboBox::backgroundColourId, TstreamColours::background.brighter (0.08f));
    outputDeviceBox.setColour (juce::ComboBox::textColourId, TstreamColours::text);
    outputDeviceBox.setColour (juce::ComboBox::outlineColourId, TstreamColours::dim);
    outputDeviceBox.setColour (juce::ComboBox::arrowColourId, TstreamColours::highlight);
    outputDeviceBox.onChange = [this] { applyOutputDevice(); };
    addAndMakeVisible (outputDeviceBox);

    refreshOutputDevices();

    inputDeviceLabel.setText ("Input Device", juce::dontSendNotification);
    inputDeviceLabel.setColour (juce::Label::textColourId, TstreamColours::text);
    addAndMakeVisible (inputDeviceLabel);

    inputDeviceBox.setColour (juce::ComboBox::backgroundColourId, TstreamColours::background.brighter (0.08f));
    inputDeviceBox.setColour (juce::ComboBox::textColourId, TstreamColours::text);
    inputDeviceBox.setColour (juce::ComboBox::outlineColourId, TstreamColours::dim);
    inputDeviceBox.setColour (juce::ComboBox::arrowColourId, TstreamColours::highlight);
    inputDeviceBox.onChange = [this] { applyInputDevice(); };
    addAndMakeVisible (inputDeviceBox);

    refreshInputDevices();
    unmuteWrapperInput();

    gainLabel.setText ("Output Level", juce::dontSendNotification);
    gainLabel.setColour (juce::Label::textColourId, TstreamColours::text);
    addAndMakeVisible (gainLabel);

    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    gainSlider.setRange (TstreamAudioProcessor::kMinGainDb, 6.0, 0.1);
    gainSlider.setTextValueSuffix (" dB");
    gainSlider.setValue (processor.getOutputGainDb(), juce::dontSendNotification);
    gainSlider.setColour (juce::Slider::backgroundColourId, TstreamColours::dim);
    gainSlider.setColour (juce::Slider::trackColourId, TstreamColours::highlight);
    gainSlider.setColour (juce::Slider::thumbColourId, TstreamColours::highlight);
    gainSlider.setColour (juce::Slider::textBoxTextColourId, TstreamColours::text);
    gainSlider.setColour (juce::Slider::textBoxBackgroundColourId, TstreamColours::background.brighter (0.08f));
    gainSlider.setColour (juce::Slider::textBoxOutlineColourId, TstreamColours::dim);
    gainSlider.onValueChange = [this]
    {
        processor.setOutputGainDb ((float) gainSlider.getValue());
        persistSettings();
    };
    addAndMakeVisible (gainSlider);

    muteButton.setClickingTogglesState (true);
    muteButton.setToggleState (processor.isMuted(), juce::dontSendNotification);
    muteButton.setColour (juce::TextButton::buttonColourId, TstreamColours::background);
    // Red rather than the usual highlight yellow: mute is the one state where
    // "the stream is running fine but nobody can hear it" is possible, so it
    // needs to read as a warning at a glance, not as another armed control.
    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffE03B3B));
    muteButton.setColour (juce::TextButton::textColourOffId, TstreamColours::text);
    muteButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    muteButton.onClick = [this]
    {
        processor.setMuted (muteButton.getToggleState());
        persistSettings();
    };
    addAndMakeVisible (muteButton);

    monitorLabel.setText ("Listen On", juce::dontSendNotification);
    monitorLabel.setColour (juce::Label::textColourId, TstreamColours::text);
    addAndMakeVisible (monitorLabel);

    monitorDeviceBox.setColour (juce::ComboBox::backgroundColourId, TstreamColours::background.brighter (0.08f));
    monitorDeviceBox.setColour (juce::ComboBox::textColourId, TstreamColours::text);
    monitorDeviceBox.setColour (juce::ComboBox::outlineColourId, TstreamColours::dim);
    monitorDeviceBox.setColour (juce::ComboBox::arrowColourId, TstreamColours::highlight);
    monitorDeviceBox.onChange = [this]
    {
        if (processor.getMonitorOutput().isRunning())
            applyMonitorDevice(); // already listening, so follow the new choice
    };
    addAndMakeVisible (monitorDeviceBox);

    monitorButton.setClickingTogglesState (true);
    monitorButton.setColour (juce::TextButton::buttonColourId, TstreamColours::background);
    monitorButton.setColour (juce::TextButton::buttonOnColourId, TstreamColours::highlight);
    monitorButton.setColour (juce::TextButton::textColourOffId, TstreamColours::text);
    monitorButton.setColour (juce::TextButton::textColourOnId, TstreamColours::background);
    monitorButton.onClick = [this] { applyMonitorDevice(); };
    addAndMakeVisible (monitorButton);

    refreshMonitorDevices();
    }
    else
    {
        autoLaunchButton.setColour (juce::ToggleButton::textColourId, TstreamColours::text);
        autoLaunchButton.setColour (juce::ToggleButton::tickColourId, TstreamColours::highlight);
        autoLaunchButton.setColour (juce::ToggleButton::tickDisabledColourId, TstreamColours::dim);
        autoLaunchButton.setToggleState (processor.isAutoLaunchEnabled(), juce::dontSendNotification);
        autoLaunchButton.onClick = [this] { processor.setAutoLaunchEnabled (autoLaunchButton.getToggleState()); };
        addAndMakeVisible (autoLaunchButton);
    }

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
    // The output-device and level rows added 60px on top of that; the device
    // row only exists in the standalone, hence the two heights.
   // Receive mode's status block is now up to 11 lines (device readout, render
   // blocks, network counters, plus the mute/error warnings), so the standalone
   // needs the extra height - see the note above about the status label being
   // silently clipped to nothing when the fixed rows above it grow.
    setSize (400, isStandalone ? 605 : 460);
    startTimerHz (30);
}

TstreamAudioProcessorEditor::~TstreamAudioProcessorEditor()
{
    stopTimer();
}

// JUCE only writes settings on a clean window close, so a crash, a Task
// Manager kill, or a Windows shutdown silently discards everything the user
// changed. Pushing state out on every change instead makes the app come back
// the way it was left regardless of how it went down. PropertiesFile batches
// the actual disk write behind its own timer, so this is cheap to call often.
// JUCE's standalone wrapper mutes the input by default (shouldMuteInput
// defaults to true) as a feedback guard for plugins that have both inputs and
// outputs. It zeroes the input buffer before processBlock ever sees it, so with
// it left on, SEND mode transmits pure silence while the packet counters, the
// sender thread and the peer-alive indicator all report perfect health. There
// is no feedback risk here: in send mode this app never routes input to its own
// output, it only packetizes and transmits.
void TstreamAudioProcessorEditor::unmuteWrapperInput()
{
    if (! isStandalone)
        return;

    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        holder->shouldMuteInput = false;
}

void TstreamAudioProcessorEditor::persistSettings()
{
    if (! isStandalone)
        return;

    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        holder->savePluginState();
        holder->saveAudioDeviceState();
    }
}

void TstreamAudioProcessorEditor::refreshInputDevices()
{
    inputDeviceBox.clear (juce::dontSendNotification);

    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return;

    auto* deviceType = holder->deviceManager.getCurrentDeviceTypeObject();
    if (deviceType == nullptr)
        return;

    const auto names = deviceType->getDeviceNames (true); // true = inputs

    for (int i = 0; i < names.size(); ++i)
        inputDeviceBox.addItem (names[i], i + 1);

    const auto currentName = holder->deviceManager.getAudioDeviceSetup().inputDeviceName;
    const int index = names.indexOf (currentName);

    if (index >= 0)
        inputDeviceBox.setSelectedId (index + 1, juce::dontSendNotification);
    else
        inputDeviceBox.setTextWhenNothingSelected ("(none)");
}

void TstreamAudioProcessorEditor::applyInputDevice()
{
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return;

    // Only hold capture open while sending. Anything else and the interface
    // stays free for the DAW.
    const bool wantInput = processor.getMode() == NetworkStreamer::Mode::send;
    const auto chosen = wantInput ? inputDeviceBox.getText() : juce::String();

    auto setup = holder->deviceManager.getAudioDeviceSetup();

    if (setup.inputDeviceName == chosen)
        return;

    setup.inputDeviceName = chosen;
    setup.useDefaultInputChannels = chosen.isNotEmpty();

    if (chosen.isEmpty())
        setup.inputChannels.clear();

    const auto error = holder->deviceManager.setAudioDeviceSetup (setup, true);

    // Same read-back check as the output picker: setAudioDeviceSetup reports
    // success while quietly opening something else, and an interface a DAW
    // holds via ASIO is the common case for that here.
    const auto actual = holder->deviceManager.getAudioDeviceSetup().inputDeviceName;

    if (error.isNotEmpty())
        deviceError = error;
    else if (chosen.isNotEmpty() && actual != chosen)
        deviceError = chosen + " would not open for capture - another app may hold it exclusively (a DAW using ASIO will do this)";
    else
        deviceError.clear();

    unmuteWrapperInput(); // setAudioDeviceSetup can reset the wrapper's mute flag
    refreshInputDevices();
    persistSettings();
}

void TstreamAudioProcessorEditor::refreshMonitorDevices()
{
    monitorDeviceBox.clear (juce::dontSendNotification);

    const auto names = processor.getMonitorOutput().getAvailableDevices();

    for (int i = 0; i < names.size(); ++i)
        monitorDeviceBox.addItem (names[i], i + 1);

    monitorDeviceBox.setTextWhenNothingSelected ("(select a device)");
}

void TstreamAudioProcessorEditor::applyMonitorDevice()
{
    auto& monitor = processor.getMonitorOutput();

    if (! monitorButton.getToggleState())
    {
        monitor.stop();
        monitorError.clear();
        return;
    }

    monitorError = monitor.start (monitorDeviceBox.getText());

    // A refused device must not leave the button lit, or it reads as "you are
    // hearing this" while nothing is open. The usual cause is the DAW holding
    // the interface via ASIO, which the error text says explicitly.
    if (monitorError.isNotEmpty())
        monitorButton.setToggleState (false, juce::dontSendNotification);
}

void TstreamAudioProcessorEditor::refreshOutputDevices()
{
    outputDeviceBox.clear (juce::dontSendNotification);

    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return;

    auto* deviceType = holder->deviceManager.getCurrentDeviceTypeObject();
    if (deviceType == nullptr)
        return;

    const auto names = deviceType->getDeviceNames (false); // false = outputs

    for (int i = 0; i < names.size(); ++i)
        outputDeviceBox.addItem (names[i], i + 1);

    const auto currentName = holder->deviceManager.getAudioDeviceSetup().outputDeviceName;
    const int index = names.indexOf (currentName);

    if (index >= 0)
        outputDeviceBox.setSelectedId (index + 1, juce::dontSendNotification);
    else
        outputDeviceBox.setTextWhenNothingSelected ("(none)");
}

void TstreamAudioProcessorEditor::applyOutputDevice()
{
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return;

    const auto chosen = outputDeviceBox.getText();
    if (chosen.isEmpty())
        return;

    auto setup = holder->deviceManager.getAudioDeviceSetup();
    if (setup.outputDeviceName == chosen)
        return;

    setup.outputDeviceName = chosen;
    setup.useDefaultOutputChannels = true;

    // Close the input side outright. This app only ever plays back audio it
    // received over UDP, and the wrapper otherwise opens a capture device by
    // default - which on this rig means holding the audio interface's input
    // open for nothing, right next to a DAW that wants it.
    setup.inputDeviceName.clear();
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();

    const auto error = holder->deviceManager.setAudioDeviceSetup (setup, true);

    // setAudioDeviceSetup returning no error is NOT proof the requested device
    // opened - it can quietly leave the previous one live. The only reliable
    // check is to read the device back, which is exactly the case that bit
    // here: a UR12 held exclusively by REAPER's ASIO driver cannot be opened
    // by WASAPI, and without this the picker just appears to do nothing.
    auto* device = holder->deviceManager.getCurrentAudioDevice();
    const bool opened = device != nullptr && device->getName() == chosen;

    if (error.isNotEmpty())
        deviceError = error;
    else if (! opened)
        deviceError = chosen + " would not open - another app may hold it exclusively (a DAW using it via ASIO will do this)";
    else
        deviceError.clear();

    if (! opened)
        refreshOutputDevices(); // snap the box back to whatever is actually live
    else
        persistSettings();
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
    area.removeFromTop (6);

    if (isStandalone)
    {
        row = area.removeFromTop (24);
        inputDeviceLabel.setBounds (row.removeFromLeft (140));
        inputDeviceBox.setBounds (row);
        area.removeFromTop (6);

        row = area.removeFromTop (24);
        outputDeviceLabel.setBounds (row.removeFromLeft (140));
        outputDeviceBox.setBounds (row);
        area.removeFromTop (6);

        row = area.removeFromTop (24);
        gainLabel.setBounds (row.removeFromLeft (140));
        muteButton.setBounds (row.removeFromRight (56));
        row.removeFromRight (6);
        gainSlider.setBounds (row);
        area.removeFromTop (6);

        row = area.removeFromTop (24);
        monitorLabel.setBounds (row.removeFromLeft (140));
        monitorButton.setBounds (row.removeFromRight (64));
        row.removeFromRight (6);
        monitorDeviceBox.setBounds (row);
    }
    else
    {
        autoLaunchButton.setBounds (area.removeFromTop (24));
    }

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

    // Open or release the capture device to match the new mode.
    if (isStandalone)
        applyInputDevice();

    persistSettings();
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
    persistSettings();
}

void TstreamAudioProcessorEditor::updateFieldEnablement()
{
    const auto mode = processor.getMode();
    hostEditor.setEnabled (mode == NetworkStreamer::Mode::send);
    remotePortEditor.setEnabled (mode == NetworkStreamer::Mode::send);
    localPortEditor.setEnabled (mode == NetworkStreamer::Mode::receive);

    // The input picker is only meaningful while sending - that is the only
    // mode where this app captures anything.
    inputDeviceBox.setEnabled (mode == NetworkStreamer::Mode::send);
    inputDeviceLabel.setEnabled (mode == NetworkStreamer::Mode::send);
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

            if (isStandalone)
            {
                // Which device is actually being captured, read back from the
                // wrapper. An empty name here is the whole explanation for
                // "sending, but the meter never moves".
                if (auto* holder = juce::StandalonePluginHolder::getInstance())
                {
                    const auto capture = holder->deviceManager.getAudioDeviceSetup().inputDeviceName;
                    text << "\nCapturing: " << (capture.isNotEmpty() ? capture : juce::String ("NO INPUT DEVICE"));
                }
            }
            else
            {
                text << "\nReceiver app: "
                     << (TstreamAudioProcessor::isReceiverRunning() ? "running" : "not running");

                const auto launchStatus = processor.getAutoLaunchStatus();

                if (launchStatus.isNotEmpty())
                    text << "\n" << launchStatus;
            }
            break;

        case NetworkStreamer::Mode::receive:
            // Render blocks first, for the same reason send mode leads with the
            // host-block count: if the output device isn't actually running,
            // every network number below is meaningless and the app is silent
            // no matter how healthy the stream is.
            text << "Render blocks: " << (int) processor.getRenderBlockCount() << "\n"
                 << "Bound: " << (streamer.isReceiverBound() ? "yes" : "no") << "\n"
                 << "Packets received: " << (int) streamer.getPacketsReceived() << "\n"
                 << "Packets dropped: " << (int) streamer.getPacketsDropped() << "\n"
                 << "Buffer underruns: " << (int) streamer.getUnderruns() << "\n"
                 << "Overruns: " << (int) streamer.getOverruns();

            if (streamer.getLastReceivedSampleRate() > 0)
            {
                const int incoming = (int) streamer.getLastReceivedSampleRate();
                const int deviceRate = (int) processor.getSampleRate();

                text << "\nIncoming format: " << incoming
                     << " Hz, " << (int) streamer.getLastReceivedChannels() << " ch";

                // Worth stating outright rather than leaving the user to spot
                // that two numbers elsewhere in this block differ - resampling
                // is the difference between correct pitch and a semitone flat.
                if (deviceRate > 0 && incoming != deviceRate)
                    text << "\nResampling " << incoming << " -> " << deviceRate << " Hz";
            }
            break;

        case NetworkStreamer::Mode::off:
        default:
            text = "Passthrough (off)";
            break;
    }

    if (isStandalone)
    {
    // What the app is actually rendering to, read back from the device rather
    // than from the combo box - the two disagree exactly when it matters, i.e.
    // when the requested device failed to open and something else is live.
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        if (auto* device = holder->deviceManager.getCurrentAudioDevice())
        {
            // Keep the picker honest. If a requested device failed to open, or
            // the wrapper switched devices behind our back, the box must follow
            // reality rather than keep displaying a choice that never happened.
            const auto liveName = device->getName();

            if (outputDeviceBox.getText() != liveName)
            {
                for (int i = 0; i < outputDeviceBox.getNumItems(); ++i)
                {
                    if (outputDeviceBox.getItemText (i) == liveName)
                    {
                        outputDeviceBox.setSelectedItemIndex (i, juce::dontSendNotification);
                        break;
                    }
                }
            }

            text << "\nRendering to: " << liveName
                 << "\n" << (int) device->getCurrentSampleRate() << " Hz, "
                 << device->getActiveOutputChannels().countNumberOfSetBits() << " out ch";

            // Worth showing because the whole point of this app is being
            // capturable by Discord, and exclusive mode silently breaks that:
            // it bypasses the Windows mixer, so no other process can ever see
            // the audio. Everything else would still look perfectly healthy.
            if (auto* type = holder->deviceManager.getCurrentDeviceTypeObject())
            {
                const auto typeName = type->getTypeName();
                text << "\nvia " << typeName;

                if (typeName.containsIgnoreCase ("Exclusive"))
                    text << "  <-- Discord CANNOT capture exclusive mode";
            }
        }
        else
            text << "\nNO OUTPUT DEVICE OPEN";
    }

    if (deviceError.isNotEmpty())
        text << "\nOUTPUT DEVICE ERROR: " << deviceError;

    auto& monitor = processor.getMonitorOutput();

    if (monitor.isRunning())
        text << "\nListening on: " << monitor.getCurrentDeviceName()
             << " (" << (int) monitor.getCurrentSampleRate() << " Hz, "
             << (int) monitor.getUnderruns() << " underruns)";

    if (monitorError.isNotEmpty())
        text << "\nLISTEN: " << monitorError;

    // The LOCAL meter shows the level arriving off the network, deliberately
    // pre-fader, so it stays a live "is the stream still there" indicator even
    // at zero. That makes muted the one state the meter cannot show, and it is
    // exactly the state worth warning about - everything looks healthy while
    // the far end hears silence.
    if (processor.isMuted())
        text << "\nMUTED - listeners hear nothing";
    }

    statusLabel.setText (text, juce::dontSendNotification);
}
