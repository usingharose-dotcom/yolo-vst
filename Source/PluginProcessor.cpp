#include "PluginProcessor.h"
#include "PluginEditor.h"

static const char* kNotes[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
struct Preset { const char* name; float speed, sustain, amount; };   // speed = dial position
static const Preset kPresets[] = {
    {"Init", 0.489f, 0.f, 0.5f}, {"Transparent", .6f, .4f, .9f}, {"Natural Lead", .57f, .2f, .7f},
    {"Pop Tight", .73f, .4f, .9f}, {"Hard Tune", 1.f, 0.f, 1.f}, {"Slow Natural", .23f, .5f, .6f}};

using APF = juce::AudioParameterFloat; using APC = juce::AudioParameterChoice; using APB = juce::AudioParameterBool;

juce::AudioProcessorValueTreeState::ParameterLayout YoloProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::NormalisableRange<float> speedRange (-3.f, 200.f,
        [] (float, float, float v) { return speedToMs (v); },
        [] (float, float, float ms) { return msToSpeed (ms); });
    l.add (std::make_unique<APF> (juce::ParameterID {"speed", 1}, "Speed", speedRange, 50.f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));
    auto pct = juce::NormalisableRange<float> (0.f, 100.f, 0.1f);
    l.add (std::make_unique<APF> (juce::ParameterID {"amount", 1},  "Amount",  pct, 50.f));
    l.add (std::make_unique<APF> (juce::ParameterID {"sustain", 1}, "Sustain", pct, 0.f));
    l.add (std::make_unique<APF> (juce::ParameterID {"flex", 1},    "Flex-Tune", pct, 0.f));
    l.add (std::make_unique<APF> (juce::ParameterID {"humanize", 1},"Humanize", pct, 0.f));
    l.add (std::make_unique<APF> (juce::ParameterID {"doubler", 1}, "Doubler",  pct, 0.f));
    l.add (std::make_unique<APF> (juce::ParameterID {"inGain", 1},  "Input",    pct, 100.f));
    l.add (std::make_unique<APF> (juce::ParameterID {"outGain", 1}, "Output",   pct, 100.f));
    l.add (std::make_unique<APF> (juce::ParameterID {"ref", 1}, "Reference (cents)", juce::NormalisableRange<float> (-100.f, 100.f, 1.f), 0.f));
    l.add (std::make_unique<APC> (juce::ParameterID {"key", 1}, "Key", juce::StringArray (kNotes, 12), 0));
    l.add (std::make_unique<APC> (juce::ParameterID {"scale", 1}, "Scale", juce::StringArray {"Chromatic", "Major", "Minor", "Custom"}, 0));
    l.add (std::make_unique<APC> (juce::ParameterID {"stab", 1}, "Note Stabilizer", juce::StringArray {"None", "Short", "Mid", "Long"}, 0));
    for (int i = 0; i < 12; ++i)
        l.add (std::make_unique<APB> (juce::ParameterID {juce::String ("n") + juce::String (i), 1}, juce::String ("Note ") + kNotes[i], true));
    l.add (std::make_unique<APB> (juce::ParameterID {"inst", 1}, "Instrument", false));
    l.add (std::make_unique<APB> (juce::ParameterID {"lowlat", 1}, "Low Latency", true));
    l.add (std::make_unique<APB> (juce::ParameterID {"bypass", 1}, "Bypass", false));
    return l;
}

void YoloProcessor::scaleMask (int key, int scale, const bool custom[12], bool out[12])
{
    static const bool maj[12] = {1,0,1,0,1,1,0,1,0,1,0,1}, mnr[12] = {1,0,1,1,0,1,0,1,1,0,1,0};
    for (int i = 0; i < 12; ++i)
    {
        const int d = ((i - key) % 12 + 12) % 12;
        out[i] = scale == 0 ? true : scale == 1 ? maj[d] : scale == 2 ? mnr[d] : custom[i];
    }
}

YoloProcessor::YoloProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "YOLO", layout())
{
    bypassParam = apvts.getParameter ("bypass");
}

bool YoloProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet(), i = l.getMainInputChannelSet();
    return (o == juce::AudioChannelSet::stereo()) && (i == juce::AudioChannelSet::stereo() || i == juce::AudioChannelSet::mono());
}

void YoloProcessor::prepareToPlay (double sr, int)
{
    engine.prepare (sr);
    setLatencySamples (engine.latencySamples());
}

void YoloProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    yolo::Params p;
    p.speed = pf ("speed")->load();
    p.amount = pf ("amount")->load() / 100.f;   p.sustain = pf ("sustain")->load() / 100.f;
    p.flex = pf ("flex")->load() / 100.f;       p.hum = pf ("humanize")->load() / 100.f;
    p.dbl = pf ("doubler")->load() / 100.f;
    p.inG = pf ("inGain")->load() / 50.f;       p.outG = pf ("outGain")->load() / 50.f;
    p.refCents = pf ("ref")->load();
    p.stab = (int) pf ("stab")->load();
    p.inst = pf ("inst")->load() > 0.5f;        p.lowlat = pf ("lowlat")->load() > 0.5f;
    p.bypass = pf ("bypass")->load() > 0.5f;
    bool cu[12]; for (int i = 0; i < 12; ++i) cu[i] = pf (("n" + juce::String (i)).toRawUTF8())->load() > 0.5f;
    bool al[12]; scaleMask ((int) pf ("key")->load(), (int) pf ("scale")->load(), cu, al);
    bool any = false; for (int i = 0; i < 12; ++i) { p.allowed[i] = al[i]; any |= al[i]; }
    if (! any) for (auto& a : p.allowed) a = true;
    const int before = engine.latencySamples();
    engine.setParams (p);
    if (engine.latencySamples() != before) setLatencySamples (engine.latencySamples());

    const int n = b.getNumSamples(), nIn = getTotalNumInputChannels();
    if (b.getNumChannels() < 2) return;
    float* L = b.getWritePointer (0);
    float* R = b.getWritePointer (1);
    if (nIn < 2) std::copy (L, L + n, R);          // mono in -> duplicate so the sum is the signal itself
    engine.process (L, R, L, R, n);
}

juce::AudioProcessorEditor* YoloProcessor::createEditor() { return new YoloEditor (*this); }

int YoloProcessor::getNumPrograms() { return (int) (sizeof (kPresets) / sizeof (kPresets[0])); }
const juce::String YoloProcessor::getProgramName (int i) { return kPresets[juce::jlimit (0, getNumPrograms() - 1, i)].name; }
void YoloProcessor::setCurrentProgram (int i)
{
    i = juce::jlimit (0, getNumPrograms() - 1, i); curProg = i;
    auto set = [this] (const char* id, float real) { if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (real)); };
    set ("speed", speedToMs (kPresets[i].speed));
    set ("sustain", kPresets[i].sustain * 100.f);
    set ("amount", kPresets[i].amount * 100.f);
}

void YoloProcessor::getStateInformation (juce::MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}
void YoloProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new YoloProcessor(); }
