// Custom standalone application class, replacing JUCE's default
// StandaloneFilterApp (enabled by JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP in
// CMakeLists.txt).
//
// It exists for two things the default window cannot do:
//   1. An always-on-top pin in the title bar, next to Options. This window
//      lives alongside a DAW, and JUCE's window has no such control.
//   2. A blank window title. The DocumentWindow's own "Tstream" caption sat
//      directly above the editor's TSTREAM label - the same word twice.
//
// EVERYTHING ELSE IS DELIBERATELY IDENTICAL to JUCE's version. In particular
// the PropertiesFile options below are copied verbatim from
// juce_audio_plugin_client_Standalone.cpp: applicationName, filenameSuffix and
// folderName together decide where settings live, and changing any of them
// moves the file, which presents as every saved setting silently resetting.

#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{
    const char* const kAlwaysOnTopKey = "alwaysOnTop";
}

// Adds the pin without touching anything else the base class does. Notably
// closeButtonPressed() is NOT overridden - it calls savePluginState(), and
// that is the path that writes settings to disk.
class TstreamStandaloneWindow final : public StandaloneFilterWindow
{
public:
    TstreamStandaloneWindow (Colour backgroundColour,
                             std::unique_ptr<StandalonePluginHolder> holder,
                             PropertySet* settingsToUse)
        // Empty title: the editor draws its own, and two captions reading
        // "Tstream" stacked on top of each other looked like a bug.
        : StandaloneFilterWindow ({}, backgroundColour, std::move (holder)),
          settings (settingsToUse)
    {
        pinButton.setClickingTogglesState (true);
        pinButton.setTooltip ("Keep this window above other windows");
        pinButton.setColour (TextButton::buttonColourId, Colours::transparentBlack);
        pinButton.setColour (TextButton::buttonOnColourId, Colour (0xffDFFF00));
        pinButton.setColour (TextButton::textColourOffId, Colour (0xff01F7F7));
        pinButton.setColour (TextButton::textColourOnId, Colour (0xff1D1D1C));

        const bool pinned = settings != nullptr && settings->getBoolValue (kAlwaysOnTopKey, false);
        pinButton.setToggleState (pinned, dontSendNotification);
        setAlwaysOnTop (pinned);

        pinButton.onClick = [this]
        {
            const bool on = pinButton.getToggleState();
            setAlwaysOnTop (on);

            if (settings != nullptr)
                settings->setValue (kAlwaysOnTopKey, on);
        };

        Component::addAndMakeVisible (pinButton);
    }

    void resized() override
    {
        // Base class positions optionsButton at (8, 6, 60, titleBarHeight - 8);
        // sit immediately to its right rather than hardcoding a second layout.
        StandaloneFilterWindow::resized();

        const int h = getTitleBarHeight() - 8;
        pinButton.setBounds (8 + 60 + 6, 6, 34, h);
    }

private:
    TextButton pinButton { "PIN" };
    PropertySet* settings = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TstreamStandaloneWindow)
};

class TstreamStandaloneApp final : public JUCEApplication
{
public:
    TstreamStandaloneApp()
    {
        PropertiesFile::Options options;

        // Copied verbatim from JUCE's StandaloneFilterApp - see the note at the
        // top of this file. Do not "tidy" these.
        options.applicationName     = CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #else
        options.folderName          = "";
       #endif

        appProperties.setStorageParameters (options);
    }

    const String getApplicationName() override           { return CharPointer_UTF8 (JucePlugin_Name); }
    const String getApplicationVersion() override        { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override           { return false; }
    void anotherInstanceStarted (const String&) override {}

    void initialise (const String&) override
    {
        mainWindow = std::make_unique<TstreamStandaloneWindow> (
            LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
            std::make_unique<StandalonePluginHolder> (appProperties.getUserSettings(),
                                                      false,
                                                      String{},
                                                      nullptr,
                                                      Array<StandalonePluginHolder::PluginInOuts>{},
                                                      false),
            appProperties.getUserSettings());

        mainWindow->setVisible (true);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        // Mirrors JUCE's own handling: persist before the window goes away,
        // since savePluginState() is what writes the plugin's state out.
        if (mainWindow != nullptr)
            if (auto* holder = mainWindow->getPluginHolder())
                holder->savePluginState();

        appProperties.saveIfNeeded();
        quit();
    }

private:
    ApplicationProperties appProperties;
    std::unique_ptr<TstreamStandaloneWindow> mainWindow;
};

juce::JUCEApplicationBase* juce_CreateApplication()
{
    return new TstreamStandaloneApp();
}
