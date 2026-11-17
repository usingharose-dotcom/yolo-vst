#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "YoloTune.h"

class YoloProcessor : public juce::AudioProcessor
{
public:
    YoloProcessor();
    void prepareToPlay (double sr, int bs) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) override { processBlock (b, m); }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Yolo"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return curProg; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    static float speedToMs (float v) { return -3.f + (1.f - v) * (1.f - v) * 203.f; }
    static float msToSpeed (float ms) { return 1.f - std::sqrt (juce::jlimit (0.f, 1.f, (ms + 3.f) / 203.f)); }
    static void scaleMask (int key, int scale, const bool custom[12], bool out[12]);

    juce::AudioProcessorValueTreeState apvts;
    yolo::Engine engine;

private:
    juce::AudioProcessorParameter* bypassParam = nullptr;
    std::atomic<float>* pf (const char* id) const { return apvts.getRawParameterValue (id); }
    int curProg = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YoloProcessor)
};
