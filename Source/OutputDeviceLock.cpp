#include "OutputDeviceLock.h"

#if JucePlugin_Build_Standalone
 // The only supported way to reach the standalone wrapper's AudioDeviceManager,
 // same as PluginEditor.cpp. Guarded because this header is meaningless - and
 // will not compile - in the VST3 target.
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

OutputDeviceLock::OutputDeviceLock() = default;

OutputDeviceLock::~OutputDeviceLock()
{
    stopTimer();
}

#if ! JucePlugin_Build_Standalone

//==============================================================================
// Inside a DAW the host owns the output device, so there is nothing to hold on
// to and every entry point is a no-op.

void OutputDeviceLock::begin() {}
void OutputDeviceLock::setLockedDevice (const juce::String&) {}
void OutputDeviceLock::timerCallback() {}
void OutputDeviceLock::loadFromSettings() {}
void OutputDeviceLock::saveToSettings() const {}

juce::String OutputDeviceLock::getStatus() const   { return {}; }
juce::PropertySet* OutputDeviceLock::settings()    { return nullptr; }
bool OutputDeviceLock::claimLockedDevice()         { return false; }

#else

//==============================================================================
juce::PropertySet* OutputDeviceLock::settings()
{
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        return holder->settings.get();

    return nullptr;
}

void OutputDeviceLock::begin()
{
    // Runs even with no lock set, so that picking a device later starts being
    // honoured immediately rather than only after a restart. The stored device is
    // read on the first tick that finds a standalone holder, since one may not
    // exist yet at construction time.
    startTimer (kPollIntervalMs);
}

void OutputDeviceLock::loadFromSettings()
{
    if (auto* properties = settings())
    {
        lockedName = properties->getValue (kLockedDeviceKey, {});
        loadedFromSettings = true;
    }
}

void OutputDeviceLock::saveToSettings() const
{
    if (auto* properties = settings())
    {
        properties->setValue (kLockedDeviceKey, lockedName);

        // Flush now rather than at PropertiesFile's own leisure: the whole point
        // is to survive the machine being left alone, which may end in a hard
        // power-off rather than a clean exit.
        if (auto* file = dynamic_cast<juce::PropertiesFile*> (properties))
            file->saveIfNeeded();
    }
}

void OutputDeviceLock::setLockedDevice (const juce::String& deviceName)
{
    if (lockedName == deviceName)
        return;

    lockedName = deviceName;
    waiting = false;
    loadedFromSettings = true;   // an explicit choice outranks whatever was stored
    saveToSettings();

    if (isLocked())
        waiting = ! claimLockedDevice();
}

juce::String OutputDeviceLock::getStatus() const
{
    if (! isLocked() || ! waiting)
        return {};

    return lockedName + " is not available - holding output for it rather than falling back "
                        "to another device (a sleeping display removes an HDMI audio endpoint)";
}

bool OutputDeviceLock::claimLockedDevice()
{
    auto* holder = juce::StandalonePluginHolder::getInstance();

    if (holder == nullptr || ! isLocked())
        return false;

    auto& manager = holder->deviceManager;

    if (auto* current = manager.getCurrentAudioDevice())
        if (current->getName() == lockedName)
            return true;

    auto* type = manager.getCurrentDeviceTypeObject();

    if (type == nullptr)
        return false;

    // Rescan first. Without this the list is whatever it was at startup, and a
    // device that has just come back would stay invisible indefinitely.
    type->scanForDevices();

    if (! type->getDeviceNames (false).contains (lockedName))
    {
        /*  Gone. Close whatever JUCE fell back to rather than letting it play.

            This is the whole point of the lock: the fallback is not a device the
            user chose, and on this machine it is the audio interface sitting
            beside a DAW. Closing also releases it for whatever does want it.
        */
        if (manager.getCurrentAudioDevice() != nullptr)
            manager.closeAudioDevice();

        return false;
    }

    auto setup = manager.getAudioDeviceSetup();
    setup.outputDeviceName = lockedName;
    setup.useDefaultOutputChannels = true;

    // Keep the input side shut, matching the picker: this app only ever plays
    // back audio it received over the network, and opening a capture device
    // would hold the interface's input for nothing.
    setup.inputDeviceName.clear();
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();

    manager.setAudioDeviceSetup (setup, true);

    /*  Read back rather than trusting the return value. setAudioDeviceSetup can
        report no error while quietly leaving the previous device live - the same
        trap the device picker already guards against.
    */
    auto* now = manager.getCurrentAudioDevice();

    return now != nullptr && now->getName() == lockedName;
}

void OutputDeviceLock::timerCallback()
{
    auto* holder = juce::StandalonePluginHolder::getInstance();

    if (holder == nullptr)
        return;

    if (! loadedFromSettings)
    {
        loadFromSettings();

        if (isLocked())
            waiting = ! claimLockedDevice();

        return;
    }

    if (! isLocked())
        return;

    // Fast path: already on the right device, nothing to do and no rescan.
    if (auto* current = holder->deviceManager.getCurrentAudioDevice())
    {
        if (current->getName() == lockedName)
        {
            waiting = false;
            return;
        }
    }

    waiting = ! claimLockedDevice();
}

#endif
