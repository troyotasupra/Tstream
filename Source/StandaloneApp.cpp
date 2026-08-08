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
#include <BinaryData.h>

namespace
{
    const char* const kAlwaysOnTopKey = "alwaysOnTop";
}

// Tray presence, because this app's normal state is "running and not being
// looked at" - it exists to feed a screen-share, and once configured there is
// nothing to see. Minimising sends it here rather than to the taskbar.
//
// The close button still quits outright. Apps that silently keep running after
// you close them are a common annoyance, and the tray is already the answer for
// "get it out of the way" - so there is no need to overload close as well.
class TstreamTrayIcon final : public SystemTrayIconComponent
{
public:
    std::function<void()> onShowRequested;
    std::function<void()> onQuitRequested;

    TstreamTrayIcon()
    {
        const auto icon = ImageCache::getFromMemory (BinaryData::icon_png, BinaryData::icon_pngSize);
        setIconImage (icon, icon);
        setIconTooltip ("Tstream");
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            PopupMenu m;
            m.addItem (1, "Show Tstream");
            m.addSeparator();
            m.addItem (2, "Quit");

            m.showMenuAsync (PopupMenu::Options(), [this] (int result)
            {
                if (result == 1 && onShowRequested != nullptr) onShowRequested();
                if (result == 2 && onQuitRequested != nullptr) onQuitRequested();
            });

            return;
        }

        if (onShowRequested != nullptr)
            onShowRequested();
    }
};

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

        trayIcon.onShowRequested = [this] { restoreFromTray(); };
        trayIcon.onQuitRequested = [] { JUCEApplication::getInstance()->systemRequestedQuit(); };
    }

    // Hide entirely rather than minimise. setVisible(false) also drops the
    // taskbar button, which is the point - the tray icon becomes the single
    // place the app lives while it is doing its job in the background.
    void minimiseButtonPressed() override
    {
        setVisible (false);
    }

    void restoreFromTray()
    {
        setVisible (true);
        setMinimised (false);
        toFront (true);
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
    TstreamTrayIcon trayIcon;
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
    bool moreThanOneInstanceAllowed() override            { return false; }

    // Only one receiver may run - a second would fight the first for UDP 9200
    // and the receiver lock. But refusing silently is a trap once the app can
    // be hidden in the tray: launching the shortcut would appear to do nothing
    // at all. Treat a second launch as "show me the one that already exists".
    //
    // Unless it carries --tray, which means the VST3's auto-start raced an
    // instance that was already up. That must stay hidden, since the user never
    // asked to see it.
    void anotherInstanceStarted (const String& commandLine) override
    {
        if (mainWindow != nullptr && ! commandLine.containsIgnoreCase ("--tray"))
            mainWindow->restoreFromTray();
    }

    void initialise (const String& commandLine) override
    {
        // Started by the VST3's auto-launch rather than by the user. The audio
        // path and the tray icon come up exactly as normal; only the window
        // stays hidden, so nothing jumps in front of the DAW.
        const bool startHidden = commandLine.containsIgnoreCase ("--tray");

        mainWindow = std::make_unique<TstreamStandaloneWindow> (
            LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
            std::make_unique<StandalonePluginHolder> (appProperties.getUserSettings(),
                                                      false,
                                                      String{},
                                                      nullptr,
                                                      Array<StandalonePluginHolder::PluginInOuts>{},
                                                      false),
            appProperties.getUserSettings());

        mainWindow->setVisible (! startHidden);
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
