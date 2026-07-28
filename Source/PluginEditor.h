#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace TstreamColours
{
    const juce::Colour background { 0xff1D1D1C };
    const juce::Colour highlight  { 0xffDFFF00 };
    const juce::Colour text       { 0xff01F7F7 };
    const juce::Colour dim        { 0xff4A4A48 };
}

// Small vertical peak meter: a dB-scaled bar in the highlight colour, a
// label above ("LOCAL"/"REMOTE"), a numeric dB readout below, and an
// optional "alive" dot (used only by the remote meter, to show whether a
// status packet from the peer has arrived recently).
class LevelMeter : public juce::Component
{
public:
    void setCaption (const juce::String& newCaption) { caption = newCaption; repaint(); }
    void setShowAliveDot (bool shouldShow) { showAliveDot = shouldShow; repaint(); }

    void setLevel (float linearPeak) noexcept
    {
        const float db = 20.0f * std::log10 (juce::jmax (linearPeak, 1.0e-5f));
        currentDb = juce::jlimit (-60.0f, 6.0f, db);
        repaint();
    }

    void setAlive (bool isAlive) noexcept
    {
        alive = isAlive;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();

        auto captionArea = area.removeFromTop (16.0f);
        g.setColour (TstreamColours::text);
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText (caption, captionArea, juce::Justification::centred);

        if (showAliveDot)
        {
            auto dotArea = captionArea.removeFromRight (14.0f).reduced (3.0f);
            g.setColour (alive ? TstreamColours::highlight : TstreamColours::dim);
            g.fillEllipse (dotArea);
        }

        auto dbTextArea = area.removeFromBottom (16.0f);
        area.reduce (area.getWidth() * 0.28f, 4.0f);

        g.setColour (TstreamColours::dim);
        g.fillRect (area);

        const float normalised = (currentDb + 60.0f) / 66.0f; // maps [-60,6] -> [0,1]
        auto fillArea = area.removeFromBottom (area.getHeight() * juce::jlimit (0.0f, 1.0f, normalised));
        g.setColour (TstreamColours::highlight);
        g.fillRect (fillArea);

        g.setColour (TstreamColours::text.withAlpha (0.8f));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        const juce::String dbText = currentDb <= -59.9f ? juce::String ("-inf")
                                                          : juce::String (currentDb, 1);
        g.drawText (dbText, dbTextArea, juce::Justification::centred);
    }

private:
    juce::String caption;
    float currentDb = -60.0f;
    bool alive = false;
    bool showAliveDot = false;
};

class TstreamAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit TstreamAudioProcessorEditor (TstreamAudioProcessor&);
    ~TstreamAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setMode (NetworkStreamer::Mode newMode);
    void applyHostAndPorts();
    void updateFieldEnablement();
    void styleTextEditor (juce::TextEditor&);

    TstreamAudioProcessor& processor;

    juce::Label titleLabel;

    juce::TextButton offButton { "OFF" };
    juce::TextButton sendButton { "SEND" };
    juce::TextButton receiveButton { "RECEIVE" };

    juce::Label hostLabel;
    juce::TextEditor hostEditor;

    juce::Label remotePortLabel;
    juce::TextEditor remotePortEditor;

    juce::Label localPortLabel;
    juce::TextEditor localPortEditor;

    LevelMeter localMeter;
    LevelMeter remoteMeter;

    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TstreamAudioProcessorEditor)
};
