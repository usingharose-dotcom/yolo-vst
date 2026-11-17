#pragma once
#include "PluginProcessor.h"

class YoloEditor : public juce::AudioProcessorEditor
{
public:
    explicit YoloEditor (YoloProcessor&);
    ~YoloEditor() override = default;
    void resized() override;

private:
    YoloProcessor& proc;
    juce::WebBrowserComponent browser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YoloEditor)
};
