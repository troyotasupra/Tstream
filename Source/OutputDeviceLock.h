#pragma once

#include <JuceHeader.h>

// Pins the standalone's output to one named device and keeps it there.
//
// The problem this solves: an HDMI audio endpoint - a TV - is not a permanent
// fixture. When the display sleeps, Windows removes the endpoint entirely.
// JUCE's AudioDeviceManager reacts the way a general-purpose audio app should,
// by falling back to another device, and then persists that fallback as the
// chosen one. The result is that leaving the machine alone quietly moves audio
// off the TV and onto the audio interface, permanently.
//
// A fallback is the wrong behaviour here. The user picked a device on purpose,
// and a display going to sleep is not a decision to play out of something else.
// So this holds the choice: while the locked device is missing, no device is
// opened at all, and the moment it comes back it is reclaimed.
//
// Silence is deliberate. Playing out of the wrong hardware is worse than not
// playing - especially when the wrong hardware is an interface sitting next to
// a DAW - and isWaitingForDevice() exists so the UI can say why it is quiet
// rather than leaving it a mystery.
//
// Everything here runs on the message thread.
class OutputDeviceLock : private juce::Timer
{
public:
    OutputDeviceLock();
    ~OutputDeviceLock() override;

    OutputDeviceLock (const OutputDeviceLock&) = delete;
    OutputDeviceLock& operator= (const OutputDeviceLock&) = delete;

    // Reads any previously locked device out of the settings file and starts
    // watching. Safe to call when not running standalone - it simply idles.
    void begin();

    // The device to hold on to. Empty clears the lock and restores JUCE's own
    // fall-back-to-anything behaviour.
    void setLockedDevice (const juce::String& deviceName);
    juce::String getLockedDevice() const { return lockedName; }

    bool isLocked() const noexcept          { return lockedName.isNotEmpty(); }

    // True when the locked device is not currently present, so nothing is
    // playing and we are holding out for its return.
    bool isWaitingForDevice() const noexcept { return waiting; }

    // Empty when all is well; otherwise a line fit to show the user.
    juce::String getStatus() const;

private:
    void timerCallback() override;

    // The holder exposes its settings as a PropertySet, not a PropertiesFile.
    static juce::PropertySet* settings();
    void loadFromSettings();
    void saveToSettings() const;

    // Attempts to make the locked device the live one. Returns true if it is.
    bool claimLockedDevice();

    juce::String lockedName;
    bool waiting = false;

    /*  The settings file belongs to the standalone holder, which may not exist yet
        when this is constructed. Loading is therefore deferred to the first tick
        that finds a holder, rather than being done once and silently coming back
        empty.
    */
    bool loadedFromSettings = false;

    // Every couple of seconds: fast enough that waking a display restores sound
    // without the user wondering, slow enough that rescanning the device list
    // is not a constant background cost.
    static constexpr int kPollIntervalMs = 2000;

    static constexpr const char* kLockedDeviceKey = "lockedOutputDevice";
};
