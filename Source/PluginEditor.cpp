#include "PluginEditor.h"
using namespace yl;
static const char* kN[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static bool isBlack (int n) { return n==1||n==3||n==6||n==8||n==10; }

YoloLook::YoloLook()
{
    setColour (juce::ComboBox::backgroundColourId, cream); setColour (juce::ComboBox::textColourId, cocoa);
    setColour (juce::ComboBox::outlineColourId, pink);  setColour (juce::ComboBox::arrowColourId, deep);
    setColour (juce::PopupMenu::backgroundColourId, cream); setColour (juce::PopupMenu::textColourId, cocoa);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, pink); setColour (juce::PopupMenu::highlightedTextColourId, ink);
    setColour (juce::Label::textColourId, cocoa);
    setColour (juce::Slider::textBoxTextColourId, cocoa); setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void YoloLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f; auto c = b.getCentre();
    g.setColour (blush); g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    juce::Path track; track.addCentredArc (c.x, c.y, r - 3, r - 3, 0, a0, a1, true);
    g.setColour (pink.withAlpha (.35f)); g.strokePath (track, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float a = a0 + pos * (a1 - a0);
    juce::Path val; val.addCentredArc (c.x, c.y, r - 3, r - 3, 0, a0, a, true);
    g.setColour (deep); g.strokePath (val, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cocoa); g.drawEllipse (c.x - r + 9, c.y - r + 9, 2 * r - 18, 2 * r - 18, 1.5f);
    juce::Path n; n.addRoundedRectangle (-1.5f, -(r - 9), 3.f, r * 0.45f, 1.5f);
    g.fillPath (n, juce::AffineTransform::rotation (a).translated (c));
}

void YoloLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cx = x + w * 0.5f;
    g.setColour (blush); g.fillRoundedRectangle (cx - 3, (float) y, 6.f, (float) h, 3.f);
    g.setColour (deep);  g.fillRoundedRectangle (cx - 3, pos, 6.f, y + h - pos, 3.f);
    g.setColour (cream); g.fillEllipse (cx - 9, pos - 9, 18, 18);
    g.setColour (cocoa); g.drawEllipse (cx - 9, pos - 9, 18, 18, 2.f);
}

void YoloLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1);
    const bool on = b.getToggleState();
    g.setColour (on ? deep : cream); g.fillRoundedRectangle (r, 8.f);
    g.setColour (on ? deep : pink);  g.drawRoundedRectangle (r, 8.f, 1.5f);
    g.setColour (on ? cream : cocoa); g.setFont (juce::FontOptions (12.f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

// ---------- piano ----------
static void currentMask (YoloProcessor& p, bool out[12])
{
    bool cu[12]; for (int i = 0; i < 12; ++i) cu[i] = p.apvts.getRawParameterValue ("n" + juce::String (i))->load() > .5f;
    YoloProcessor::scaleMask ((int) p.apvts.getRawParameterValue ("key")->load(), (int) p.apvts.getRawParameterValue ("scale")->load(), cu, out);
}

static juce::Rectangle<float> keyRect (juce::Rectangle<float> b, int n)
{
    static const int whiteIdx[12] = {0,-1,1,-1,2,3,-1,4,-1,5,-1,6};
    const float ww = b.getWidth() / 7.f;
    if (! isBlack (n)) return { b.getX() + whiteIdx[n] * ww, b.getY(), ww, b.getHeight() };
    const int left = whiteIdx[n - 1];
    return { b.getX() + (left + 1) * ww - ww * 0.3f, b.getY(), ww * 0.6f, b.getHeight() * 0.62f };
}

int Piano::keyAt (juce::Point<float> pt) const
{
    auto b = getLocalBounds().toFloat();
    for (int n = 0; n < 12; ++n) if (isBlack (n) && keyRect (b, n).contains (pt)) return n;
    for (int n = 0; n < 12; ++n) if (! isBlack (n) && keyRect (b, n).contains (pt)) return n;
    return -1;
}

void Piano::paint (juce::Graphics& g)
{
    bool al[12]; currentMask (proc, al);
    const float outM = proc.engine.outM.load(); const int act = ((int) std::lround (outM) % 12 + 12) % 12;
    const bool vc = proc.engine.voiced.load();
    auto b = getLocalBounds().toFloat();
    for (int pass = 0; pass < 2; ++pass)
        for (int n = 0; n < 12; ++n)
        {
            if (isBlack (n) != (pass == 1)) continue;
            auto r = keyRect (b, n).reduced (1);
            juce::Colour c = isBlack (n) ? cocoa : juce::Colours::white;
            if (al[n]) c = isBlack (n) ? deep : pink.brighter (.3f);
            if (vc && n == act) c = isBlack (n) ? pink : deep;
            g.setColour (c); g.fillRoundedRectangle (r, 4.f);
            g.setColour (cocoa.withAlpha (.5f)); g.drawRoundedRectangle (r, 4.f, 1.f);
            if (! isBlack (n)) { g.setColour (cocoa.withAlpha (.7f)); g.setFont (juce::FontOptions (11.f)); g.drawText (kN[n], r.withTrimmedBottom (4), juce::Justification::centredBottom); }
        }
}

void Piano::mouseDown (const juce::MouseEvent& e)
{
    const int n = keyAt (e.position); if (n < 0) return;
    bool al[12]; currentMask (proc, al);
    // switch to Custom, seeded with what was audible, then toggle the clicked note
    for (int i = 0; i < 12; ++i)
        if (auto* p = proc.apvts.getParameter ("n" + juce::String (i))) p->setValueNotifyingHost ((i == n ? ! al[i] : al[i]) ? 1.f : 0.f);
    if (auto* s = proc.apvts.getParameter ("scale")) s->setValueNotifyingHost (s->convertTo0to1 (3.f));
}

// ---------- yin yang ----------
void YinYang::timerCallback()
{
    const float pk = proc.engine.peakIn.load();
    level += (juce::jlimit (0.f, 1.f, pk * 3.f) - level) * 0.25f;
    angle += 0.01f + level * 0.12f;
    repaint();
}

void YinYang::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (8);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * .5f; auto c = b.getCentre();
    g.setColour (pink.withAlpha (.25f + .35f * level)); g.fillEllipse (c.x - r - 6, c.y - r - 6, 2 * r + 12, 2 * r + 12);
    juce::Graphics::ScopedSaveState ss (g);
    g.addTransform (juce::AffineTransform::rotation (angle, c.x, c.y));
    g.setColour (juce::Colours::white); g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    g.setColour (ink);
    juce::Path half; half.addPieSegment (c.x - r, c.y - r, 2 * r, 2 * r, 0.f, juce::MathConstants<float>::pi, 0.f); // right half (0..pi clockwise from top)
    g.fillPath (half);
    g.setColour (juce::Colours::white); g.fillEllipse (c.x - r / 2, c.y - r, r, r);   // top lobe white (left side swirl)
    g.setColour (ink);                  g.fillEllipse (c.x - r / 2, c.y, r, r);       // bottom lobe dark
    g.setColour (ink);                  g.fillEllipse (c.x - r * .12f, c.y - r * .62f, r * .24f, r * .24f);
    g.setColour (juce::Colours::white); g.fillEllipse (c.x - r * .12f, c.y + r * .38f, r * .24f, r * .24f);
    g.setColour (ink); g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 3.f);
}

// ---------- editor ----------
static void setupDial (juce::Component& parent, juce::Slider& s, juce::Label& l, const juce::String& name, bool isSpeed)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    if (isSpeed) s.setTextValueSuffix (" ms");
    l.setText (name, juce::dontSendNotification); l.setJustificationType (juce::Justification::centred);
    parent.addAndMakeVisible (s); parent.addAndMakeVisible (l);
}
static void setupBar (juce::Component& parent, juce::Slider& s, juce::Label& l, const juce::String& name, bool vertical)
{
    s.setSliderStyle (vertical ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    l.setText (name, juce::dontSendNotification); l.setJustificationType (juce::Justification::centred);
    parent.addAndMakeVisible (s); parent.addAndMakeVisible (l);
}

YoloEditor::YoloEditor (YoloProcessor& p) : AudioProcessorEditor (&p), proc (p), yin (p), piano (p)
{
    setLookAndFeel (&look);
    setSize (900, 580);
    addAndMakeVisible (yin); addAndMakeVisible (piano);
    setupDial (*this, speed, lSpeed, "SPEED", true);
    setupDial (*this, amount, lAmount, "AMOUNT", false);
    setupDial (*this, sustain, lSustain, "SUSTAIN", false);
    setupDial (*this, flex, lFlex, "FLEX-TUNE", false);
    setupDial (*this, hum, lHum, "HUMANIZE", false);
    setupDial (*this, dbl, lDbl, "DOUBLER", false);
    setupBar (*this, inG, lIn, "IN", true);
    setupBar (*this, outG, lOut, "OUT", true);
    key.addItemList (juce::StringArray (kN, 12), 1);
    scale.addItemList ({"Chromatic", "Major", "Minor", "Custom"}, 1);
    stab.addItemList ({"Stabilizer: None", "Stabilizer: Short", "Stabilizer: Mid", "Stabilizer: Long"}, 1);
    for (int i = 0; i < proc.getNumPrograms(); ++i) preset.addItem (proc.getProgramName (i), i + 1);
    preset.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    preset.onChange = [this] { proc.setCurrentProgram (preset.getSelectedId() - 1); };
    for (auto* c : {&key, &scale, &stab, &preset}) addAndMakeVisible (c);
    inst.setButtonText ("Instrument"); lowlat.setButtonText ("Low Latency"); bypass.setButtonText ("Bypass");
    for (auto* b : {&inst, &lowlat, &bypass}) addAndMakeVisible (b);
    auto& a = proc.apvts;
    aSp = std::make_unique<SA> (a, "speed", speed);   aAm = std::make_unique<SA> (a, "amount", amount);
    aSu = std::make_unique<SA> (a, "sustain", sustain); aFl = std::make_unique<SA> (a, "flex", flex);
    aHu = std::make_unique<SA> (a, "humanize", hum);  aDb = std::make_unique<SA> (a, "doubler", dbl);
    aIn = std::make_unique<SA> (a, "inGain", inG);    aOut = std::make_unique<SA> (a, "outGain", outG);
    aKey = std::make_unique<CA> (a, "key", key); aScale = std::make_unique<CA> (a, "scale", scale); aStab = std::make_unique<CA> (a, "stab", stab);
    aInst = std::make_unique<BA> (a, "inst", inst); aLow = std::make_unique<BA> (a, "lowlat", lowlat); aByp = std::make_unique<BA> (a, "bypass", bypass);
    startTimerHz (15);
}
YoloEditor::~YoloEditor() { setLookAndFeel (nullptr); }

void YoloEditor::timerCallback()
{
    static const char* names[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    juce::String s;
    if (proc.engine.voiced.load())
    {
        const int d = (int) std::lround (proc.engine.detM.load()), o = (int) std::lround (proc.engine.outM.load());
        s = juce::String (names[(d % 12 + 12) % 12]) + " -> " + names[(o % 12 + 12) % 12];
    }
    else s = "listening";
    if (s != status) { status = s; repaint (0, getHeight() - 28, getWidth(), 28); }
}

void YoloEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (cream, 0, 0, blush, 0, (float) getHeight(), false)); g.fillAll();
    g.setColour (cocoa); g.setFont (juce::FontOptions ("Snell Roundhand", 46.f, juce::Font::italic));
    g.drawText ("Yolo", 24, 10, 200, 56, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.f)); g.setColour (cocoa.withAlpha (.6f));
    g.drawText ("PITCH CORRECTION", 28, 58, 200, 14, juce::Justification::centredLeft);
    g.setColour (pink.withAlpha (.5f)); g.fillRect (0, getHeight() - 28, getWidth(), 28);
    g.setColour (cocoa); g.setFont (juce::FontOptions (13.f));
    g.drawText (status, 16, getHeight() - 28, 300, 28, juce::Justification::centredLeft);
    g.drawText ("latency " + juce::String (proc.getLatencySamples()) + " smp", getWidth() - 216, getHeight() - 28, 200, 28, juce::Justification::centredRight);
}

void YoloEditor::resized()
{
    auto r = getLocalBounds().withTrimmedBottom (28);
    auto top = r.removeFromTop (76).reduced (16, 12);
    preset.setBounds (top.removeFromRight (160).withHeight (26));
    bypass.setBounds (top.removeFromRight (90).withHeight (26).translated (-10, 0));
    auto bars = r.removeFromRight (96).reduced (6, 6);
    inG.setBounds (bars.removeFromLeft (44).withTrimmedBottom (20)); lIn.setBounds (inG.getX() - 4, inG.getBottom(), 52, 18);
    outG.setBounds (bars.withTrimmedBottom (20)); lOut.setBounds (outG.getX() - 4, outG.getBottom(), 52, 18);
    auto pn = r.removeFromBottom (150).reduced (20, 10);
    piano.setBounds (pn);
    auto left = r.removeFromLeft (260).reduced (10);
    yin.setBounds (left.removeFromTop (200).reduced (10));
    auto sel = left.reduced (6, 0);
    key.setBounds (sel.removeFromTop (26)); sel.removeFromTop (6);
    scale.setBounds (sel.removeFromTop (26)); sel.removeFromTop (6);
    stab.setBounds (sel.removeFromTop (26));
    auto dials = r.reduced (10, 6);
    const int dw = dials.getWidth() / 3, dh = dials.getHeight() / 2;
    auto place = [&] (juce::Slider& s, juce::Label& l, int col, int row)
    {
        juce::Rectangle<int> c (dials.getX() + col * dw, dials.getY() + row * dh, dw, dh);
        l.setBounds (c.removeFromTop (18)); s.setBounds (c.reduced (4));
    };
    place (speed, lSpeed, 0, 0); place (amount, lAmount, 1, 0); place (sustain, lSustain, 2, 0);
    place (flex, lFlex, 0, 1); place (hum, lHum, 1, 1); place (dbl, lDbl, 2, 1);
    // switches sit in the left column under the dropdowns
    inst.setBounds (left.getX() + 6, stab.getBottom() + 10, 118, 26);
    lowlat.setBounds (left.getX() + 6 + 124, stab.getBottom() + 10, 118, 26);
}
