#pragma once
#include "PluginProcessor.h"

namespace yl { // palette
    const juce::Colour cream (0xfffff4f1), pink (0xffff8fb1), deep (0xffe0507f), cocoa (0xff3a2a2a), ink (0xff1c1416), blush (0xffffd9e3);
}

class YoloLook : public juce::LookAndFeel_V4
{
public:
    YoloLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    juce::Font getLabelFont (juce::Label&) override { return juce::FontOptions (13.f); }
};

class Piano : public juce::Component, private juce::Timer
{
public:
    explicit Piano (YoloProcessor& p) : proc (p) { startTimerHz (20); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    int keyAt (juce::Point<float>) const;
    void timerCallback() override { repaint(); }
    YoloProcessor& proc;
};

class YinYang : public juce::Component, private juce::Timer
{
public:
    explicit YinYang (YoloProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    YoloProcessor& proc; float angle = 0, level = 0;
};

class YoloEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit YoloEditor (YoloProcessor&);
    ~YoloEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    YoloProcessor& proc; YoloLook look;
    YinYang yin; Piano piano;
    juce::Slider speed, amount, sustain, flex, hum, dbl, inG, outG;
    juce::Label lSpeed, lAmount, lSustain, lFlex, lHum, lDbl, lIn, lOut;
    juce::ComboBox key, scale, stab, preset;
    juce::ToggleButton inst, lowlat, bypass;
    std::unique_ptr<SA> aSp, aAm, aSu, aFl, aHu, aDb, aIn, aOut;
    std::unique_ptr<CA> aKey, aScale, aStab;
    std::unique_ptr<BA> aInst, aLow, aByp;
    juce::String status;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YoloEditor)
};
