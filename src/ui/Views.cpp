#include "Views.h"
#include <cmath>

namespace vellum { namespace ui {

using namespace colours;

// ---------------------------------------------------------------------------
// Knob
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& lbl, bool isBig)
    : label (lbl), paramId (id), big (isBig)
{
    param = apvts.getParameter (id);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setDoubleClickReturnValue (true, param ? param->convertFrom0to1 (param->getDefaultValue()) : 0.0);
    slider.setMouseDragSensitivity (big ? 320 : 220);
    slider.onDragStart = [this] { dragging = true; repaint(); };
    slider.onDragEnd = [this] { dragging = false; repaint(); };
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int labelH = big ? 34 : 18;
    auto k = r.withTrimmedBottom (labelH);
    const int d = std::min (k.getWidth(), k.getHeight());
    slider.setBounds (k.withSizeKeepingCentre (d, d));
}

juce::String Knob::valueText() const
{
    const double v = slider.getValue();
    if (paramId == "level" || paramId == "velBoost") return juce::String (v, 1) + " dB";
    if (paramId == "velCurve") return "^" + juce::String (v, 2);
    if (paramId == "decayScale") return juce::String::charToString (juce::juce_wchar (0xd7)) + juce::String (v, 2);
    if (paramId == "seqSwing") return juce::String (50.0 + v * 25.0, 0) + "%";
    if (slider.getMinimum() < 0.0) return (v >= 0 ? "+" : "") + juce::String (v * 100.0, 0) + "%";
    return juce::String (v * 100.0, 0) + "%";
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const bool showValue = dragging || slider.isMouseOverOrDragging();
    auto lab = r.removeFromBottom (big ? 34.0f : 18.0f);
    if (big)
    {
        // small accent tick above the label like the reference
        g.setColour (accentDim);
        g.fillRect (lab.getCentreX() - 12.0f, lab.getY() + 2.0f, 24.0f, 1.0f);
        g.setFont (font (12.5f));
        g.setColour (showValue ? accent : text.withAlpha (0.85f));
        drawSpacedText (g, showValue ? valueText() : label.toUpperCase(), lab.withTrimmedTop (8.0f), 3.2f);
    }
    else
    {
        g.setFont (font (9.5f));
        g.setColour (showValue ? accent : textDim);
        drawSpacedText (g, showValue ? valueText() : label.toUpperCase(), lab, 0.9f);
    }
}

// ---------------------------------------------------------------------------
// IconButton
void IconButton::paintButton (juce::Graphics& g, bool hover, bool down)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    juce::Colour c = on ? accent : (hover ? colours::text : textDim);
    if (down) c = c.brighter (0.2f);
    const float cx = r.getCentreX(), cy = r.getCentreY();
    switch (kind)
    {
        case Power:
        {
            const float R = std::min (r.getWidth(), r.getHeight()) * 0.28f;
            juce::Path p;
            p.addCentredArc (cx, cy, R, R, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.drawLine (cx, cy - R - 1.5f, cx, cy - R * 0.25f, 1.8f);
            if (on) { g.setColour (accent.withAlpha (0.18f)); g.fillEllipse (cx - R * 1.8f, cy - R * 1.8f, R * 3.6f, R * 3.6f); }
            break;
        }
        case Play:
        {
            const float s = std::min (r.getWidth(), r.getHeight()) * 0.26f;
            juce::Path p;
            if (on) { p.addRectangle (cx - s, cy - s, s * 0.7f, 2 * s); p.addRectangle (cx + s * 0.3f, cy - s, s * 0.7f, 2 * s); }
            else p.addTriangle (cx - s * 0.8f, cy - s, cx - s * 0.8f, cy + s, cx + s, cy);
            g.setColour (c); g.fillPath (p);
            break;
        }
        case Gear:
        {
            const float R = std::min (r.getWidth(), r.getHeight()) * 0.3f;
            juce::Path p;
            for (int i = 0; i < 8; ++i)
            {
                const float a = (float) i * juce::MathConstants<float>::twoPi / 8.0f;
                juce::Path tooth; tooth.addRoundedRectangle (-R * 0.22f, -R * 1.25f, R * 0.44f, R * 0.5f, R * 0.1f);
                p.addPath (tooth, juce::AffineTransform::rotation (a).translated (cx, cy));
            }
            p.addEllipse (cx - R, cy - R, 2 * R, 2 * R);
            p.setUsingNonZeroWinding (false);
            p.addEllipse (cx - R * 0.42f, cy - R * 0.42f, R * 0.84f, R * 0.84f);
            g.setColour (c); g.fillPath (p);
            break;
        }
        case ArrowLeft: case ArrowRight:
        {
            const float s = 4.5f;
            juce::Path p;
            if (kind == ArrowLeft) p.addTriangle (cx + s * 0.6f, cy - s, cx + s * 0.6f, cy + s, cx - s * 0.8f, cy);
            else p.addTriangle (cx - s * 0.6f, cy - s, cx - s * 0.6f, cy + s, cx + s * 0.8f, cy);
            g.setColour (c); g.fillPath (p);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// HeaderBar
HeaderBar::HeaderBar()
{
    save.setComponentID ("pill");
    addAndMakeVisible (prev); addAndMakeVisible (next); addAndMakeVisible (save); addAndMakeVisible (gear);
    gear.setClickingTogglesState (true);
}

void HeaderBar::resized()
{
    const int cx = getWidth() / 2;
    save.setBounds (cx - 60, 27, 120, 17);
    prev.setBounds (cx - 92, 25, 22, 22);
    next.setBounds (cx + 70, 25, 22, 22);
    gear.setBounds (getWidth() - 44, 12, 28, 28);
}

void HeaderBar::paint (juce::Graphics& g)
{
    g.fillAll (header);
    g.setColour (line); g.fillRect (0, getHeight() - 1, getWidth(), 1);
    // logo mark
    juce::Path logo;
    logo.startNewSubPath (22.0f, 14.0f); logo.lineTo (30.0f, 34.0f); logo.lineTo (38.0f, 14.0f);
    g.setColour (text);
    g.strokePath (logo, juce::PathStrokeType (2.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    g.fillRect (42.0f, 14.0f, 6.0f, 2.6f);
    // kit name
    g.setFont (font (13.5f));
    g.setColour (text);
    g.drawText (kitName, getWidth() / 2 - 150, 6, 300, 18, juce::Justification::centred, true);
}

// ---------------------------------------------------------------------------
// PadCell
PadCell::PadCell (VellumProcessor& p, int i) : processor (p), index (i) { refresh(); }

juce::Rectangle<int> PadCell::waveArea() const { return getLocalBounds().withTrimmedTop (40).withTrimmedBottom (46).reduced (5, 0); }
juce::Rectangle<int> PadCell::menuArea() const { return juce::Rectangle<int> (getWidth() / 2 - 14, getHeight() - 42, 28, 20); }

void PadCell::refresh()
{
    const auto& p = processor.pad (index);
    model = p.model; name = p.name; analyzing = p.analyzing;
    repaint();
}

void PadCell::hit (const HitEvent& e, double now)
{
    hitTime = now; hitVel = e.velocity; hitX = e.posX; hitY = e.posY;
    repaint();
}

bool PadCell::tick (double now)
{
    const double t = now - hitTime;
    if (t < 1.3) { repaint(); return true; }
    return false;
}

void PadCell::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    const bool hover = isMouseOver (true);
    juce::ColourGradient bg (padTop.brighter (selected ? 0.12f : 0.0f), 0, 0, padBottom, 0, (float) getHeight(), false);
    g.setGradientFill (bg); g.fillRect (r);
    if (hover) { g.setColour (juce::Colours::white.withAlpha (0.025f)); g.fillRect (r); }
    if (dropHighlight) { g.setColour (accent.withAlpha (0.15f)); g.fillRect (r); g.setColour (accent); g.drawRect (r, 1); }
    g.setColour (line); g.fillRect (r.getRight() - 1, 0, 1, getHeight());
    if (selected) { g.setColour (accent.withAlpha (0.8f)); g.fillRect (0, 0, getWidth() - 1, 2); }

    g.setFont (font (10.0f)); g.setColour (textDim);
    g.drawText (juce::String (index + 1), r.withHeight (18).withY (4), juce::Justification::centred, false);
    g.setFont (font (11.5f)); g.setColour (text.withAlpha (model ? 0.95f : 0.55f));
    g.drawText (name, r.withHeight (16).withY (19).reduced (4, 0), juce::Justification::centred, true);

    auto wa = waveArea();
    const double t = juce::Time::getMillisecondCounterHiRes() * 0.001 - hitTime;
    const float glow = t < 0.5 ? std::pow ((float) (1.0 - t / 0.5), 1.6f) * (0.35f + 0.65f * hitVel) : 0.0f;

    if (analyzing)
    {
        const float a = 0.5f + 0.4f * std::sin ((float) juce::Time::getMillisecondCounterHiRes() * 0.006f);
        g.setFont (font (10.5f)); g.setColour (accent.withAlpha (a));
        drawSpacedText (g, "ANALYSING", wa.toFloat(), 1.2f);
    }
    else if (model && model->isValid())
    {
        const auto& mn = model->previewMin; const auto& mx = model->previewMax;
        const int n = (int) mn.size();
        float pk = 1e-4f; for (int i = 0; i < n; ++i) pk = std::max ({ pk, std::fabs (mn[(size_t) i]), std::fabs (mx[(size_t) i]) });
        const float cy = (float) wa.getCentreY(), hh = (float) wa.getHeight() * 0.48f / pk;
        juce::Path wavePath;
        for (int i = 0; i < n; ++i)
        {
            const float x = (float) wa.getX() + (float) wa.getWidth() * (float) i / (float) (n - 1);
            const float y = cy - mx[(size_t) i] * hh;
            if (i == 0) wavePath.startNewSubPath (x, y); else wavePath.lineTo (x, y);
        }
        for (int i = n - 1; i >= 0; --i)
            wavePath.lineTo ((float) wa.getX() + (float) wa.getWidth() * (float) i / (float) (n - 1), cy - mn[(size_t) i] * hh);
        wavePath.closeSubPath();
        if (glow > 0.0f)
        {
            juce::ColourGradient rad (accent.withAlpha (glow * 0.35f), (float) wa.getCentreX() + hitX * wa.getWidth() * 0.3f, cy + hitY * wa.getHeight() * 0.25f,
                                      accent.withAlpha (0.0f), (float) wa.getX(), (float) wa.getY(), true);
            g.setGradientFill (rad); g.fillRect (wa);
        }
        g.setColour (colours::wave.interpolatedWith (accent, glow));
        g.fillPath (wavePath);
    }
    else
    {
        g.setColour (textDim.withAlpha (0.5f));
        g.setFont (font (18.0f)); g.drawText ("+", wa.withTrimmedBottom (20), juce::Justification::centred, false);
        g.setFont (font (9.5f)); drawSpacedText (g, "DROP", wa.withTrimmedTop (wa.getHeight() / 2 + 6).toFloat(), 1.2f);
    }

    // strike ripple + velocity bar
    if (t < 1.3 && hitTime > 0.0)
    {
        const float cx = (float) wa.getCentreX() + hitX * wa.getWidth() * 0.3f;
        const float cy = (float) wa.getCentreY() + hitY * wa.getHeight() * 0.25f;
        for (int k = 0; k < 2; ++k)
        {
            const double tt = t - k * 0.09;
            if (tt < 0.0 || tt > 0.55) continue;
            const float rad = 4.0f + (float) tt * 120.0f * (0.6f + 0.6f * hitVel);
            g.setColour (accent.withAlpha ((float) (1.0 - tt / 0.55) * 0.75f));
            g.drawEllipse (cx - rad, cy - rad, 2 * rad, 2 * rad, 1.4f);
        }
        g.setColour (accent.withAlpha (0.9f)); g.fillEllipse (cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
        const float ba = (float) juce::jlimit (0.0, 1.0, 1.0 - t / 1.3);
        g.setColour (juce::Colour (0xff2a2a2f)); g.fillRect (wa.getX(), wa.getBottom() + 3, wa.getWidth(), 2);
        g.setColour (accent.withAlpha (ba)); g.fillRect ((float) wa.getX(), (float) wa.getBottom() + 3.0f, (float) wa.getWidth() * hitVel, 2.0f);
    }

    // menu glyph
    auto m = menuArea();
    const bool mh = hover && m.contains (getMouseXYRelative());
    g.setColour (mh ? text : textDim.withAlpha (0.8f));
    for (int i = 0; i < 3; ++i) g.fillRect (m.getCentreX() - 7, m.getY() + 5 + i * 4, 14, 1);
}

void PadCell::mouseDown (const juce::MouseEvent& e)
{
    if (menuArea().contains (e.getPosition()) || e.mods.isPopupMenu()) { showMenu(); return; }
    if (onSelect) onSelect (index);
    if (! model)
    {
        if (analyzing) return;
        chooser = std::make_unique<juce::FileChooser> ("Load sample(s)", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a;*.caf");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc) { auto files = fc.getResults(); if (! files.isEmpty()) processor.loadFiles (files, index); });
        return;
    }
    auto wa = waveArea();
    float vel = 0.8f;
    if (wa.contains (e.getPosition())) vel = juce::jlimit (0.15f, 1.0f, 1.0f - (float) (e.y - wa.getY()) / (float) wa.getHeight());
    processor.triggerPad (index, vel);
}

void PadCell::showMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Load sample(s)...");
    menu.addItem (2, "Rename...", model != nullptr);
    menu.addItem (3, "Clear", model != nullptr);
    menu.addSeparator();
    juce::PopupMenu gain;
    const float cur = processor.pad (index).userGainDb;
    for (int i = 0; i <= 8; ++i)
    {
        const float db = -12.0f + 3.0f * i;
        gain.addItem (100 + i, (db > 0 ? "+" : "") + juce::String (db, 0) + " dB", true, std::fabs (cur - db) < 0.01f);
    }
    menu.addSubMenu ("Pad gain", gain);
    juce::PopupMenu cat;
    const int curCat = processor.pad (index).categoryOverride;
    cat.addItem (200, "Auto (from name)", true, curCat < 0);
    for (int c = 0; c < (int) DrumCategory::Count; ++c) cat.addItem (201 + c, categoryName ((DrumCategory) c), true, curCat == c);
    menu.addSubMenu ("Level category", cat);
    if (model && model->isValid())
    {
        menu.addSeparator();
        menu.addItem (900, juce::String (categoryName (model->category)) + "  auto-level " + (model->autoGainDb >= 0 ? "+" : "") + juce::String (model->autoGainDb, 1) + " dB", false);
        menu.addItem (901, juce::String (model->modes.size()) + " modes  " + dot() + "  body " + juce::String ((int) std::round (model->modalEnergyRatio * 100)) + "%  " + dot() + "  " + juce::String (model->durationSec, 2) + " s", false);
        if (model->truncated) menu.addItem (902, "sliced from loop " + dot() + " tail resynthesised", false);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (180), [this] (int r)
    {
        if (r == 1)
        {
            chooser = std::make_unique<juce::FileChooser> ("Load sample(s)", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a;*.caf");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                                  [this] (const juce::FileChooser& fc) { auto files = fc.getResults(); if (! files.isEmpty()) processor.loadFiles (files, index); });
        }
        else if (r == 2)
        {
            auto* w = new juce::AlertWindow ("Rename pad", "", juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("name", name);
            w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int res)
            {
                if (res == 1) processor.renamePad (index, w->getTextEditorContents ("name"));
            }), true);
        }
        else if (r == 3) processor.clearPad (index);
        else if (r >= 100 && r <= 108) processor.setPadGain (index, -12.0f + 3.0f * (r - 100));
        else if (r == 200) processor.setPadCategory (index, -1);
        else if (r > 200 && r <= 200 + (int) DrumCategory::Count) processor.setPadCategory (index, r - 201);
    });
}

// ---------------------------------------------------------------------------
PadStrip::PadStrip (VellumProcessor& p) : processor (p)
{
    for (int i = 0; i < kNumPads; ++i)
    {
        auto c = std::make_unique<PadCell> (p, i);
        c->onSelect = [this] (int idx) { select (idx); };
        addAndMakeVisible (*c);
        cells.push_back (std::move (c));
    }
    select (0);
}

void PadStrip::resized()
{
    const float w = (float) getWidth() / kNumPads;
    for (int i = 0; i < kNumPads; ++i)
        cells[(size_t) i]->setBounds ((int) std::round (i * w), 0, (int) std::round ((i + 1) * w) - (int) std::round (i * w), getHeight());
}

void PadStrip::paint (juce::Graphics& g) { g.fillAll (padBottom); }
int PadStrip::padAt (int x) const { return juce::jlimit (0, kNumPads - 1, (int) (x * kNumPads / std::max (1, getWidth()))); }
void PadStrip::refresh() { for (auto& c : cells) c->refresh(); }
void PadStrip::select (int pad)
{
    selected = juce::jlimit (0, kNumPads - 1, pad);
    for (int i = 0; i < kNumPads; ++i) cells[(size_t) i]->setSelected (i == selected);
    if (onSelect) onSelect (selected);
}

// ---------------------------------------------------------------------------
// TabBar
TabBar::TabBar (VellumProcessor& p)
{
    for (auto* b : { &samples, &kits, &slicer, &groove })
    {
        b->setComponentID ("tab");
        b->setClickingTogglesState (true);
        b->setRadioGroupId (77);
        addAndMakeVisible (*b);
    }
    samples.onClick = [this] { setTab (0); };
    kits.onClick = [this] { setTab (1); };
    slicer.onClick = [this] { setTab (2); };
    groove.onClick = [this] { setTab (3); };
    samples.setToggleState (true, juce::dontSendNotification);
    power.setClickingTogglesState (true);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "seqOn", power);
    play.setClickingTogglesState (true);
    play.onClick = [&p, this] { p.internalPlay.store (play.getToggleState()); juce::ignoreUnused (this); };
    addAndMakeVisible (power); addAndMakeVisible (play);
}

void TabBar::resized()
{
    const int h = getHeight() - 8, y = 4;
    int x = getWidth() - 12;
    play.setBounds (x - 46, y, 46, h); x -= 46;
    power.setBounds (x - 46, y, 46, h); x -= 46;
    groove.setBounds (x - 110, y, 110, h); x -= 110 + 12;
    slicer.setBounds (x - 110, y, 110, h); x -= 110;
    kits.setBounds (x - 110, y, 110, h); x -= 110;
    samples.setBounds (x - 130, y, 130, h);
}

void TabBar::paint (juce::Graphics& g)
{
    g.fillAll (header);
    g.setColour (line); g.fillRect (0, 0, getWidth(), 1); g.fillRect (0, getHeight() - 1, getWidth(), 1);
    // frame around groove + transport
    auto r = power.getBounds().getUnion (play.getBounds()).getUnion (groove.getBounds()).toFloat().expanded (0.5f);
    g.setColour (line); g.drawRect (r, 1.0f);
}

void TabBar::setTab (int t, bool notify)
{
    tab = t;
    juce::TextButton* bs[] = { &samples, &kits, &slicer, &groove };
    bs[t]->setToggleState (true, juce::dontSendNotification);
    if (notify && onTab) onTab (t);
}

// ---------------------------------------------------------------------------
// ModalViz
void ModalViz::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (line); g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);
    if (! model || ! model->isValid() || model->modes.empty())
    {
        g.setFont (font (10.0f)); g.setColour (textDim.withAlpha (0.6f));
        drawSpacedText (g, model ? "NO TONAL MODES " + dot() + " RESIDUAL ONLY" : "DROP A SAMPLE", r, 1.2f);
        return;
    }
    const float fLo = std::log2 (30.0f), fHi = std::log2 (16000.0f);
    float maxAmp = 1e-6f; for (const auto& m : model->modes) maxAmp = std::max (maxAmp, m.amp);
    const double t = hasHit ? std::max (0.0, now - hitTime) : 1e9;
    const float base = r.getBottom() - 2.0f, H = r.getHeight() - 6.0f;
    auto heightFor = [&] (float amp) { const float db = 20.0f * std::log10 (std::max (amp / maxAmp, 1e-4f)); return juce::jlimit (0.0f, 1.0f, (db + 54.0f) / 54.0f) * H; };
    for (size_t k = 0; k < model->modes.size(); ++k)
    {
        const auto& m = model->modes[k];
        const float x = r.getX() + (std::log2 (std::max (m.freq, 30.0f)) - fLo) / (fHi - fLo) * r.getWidth();
        const float gh = heightFor (m.amp);
        g.setColour (textDim.withAlpha (0.35f));
        g.fillRect (x - 1.0f, base - gh, 2.0f, gh);
        if (hasHit && k < (size_t) kMaxModes)
        {
            const float amp = m.amp * hit.modeAmpMul[k] * std::exp ((float) -t / std::max (0.002f, m.tau * hit.modeDecayMul[k]));
            const float lh = heightFor (amp);
            if (lh > 0.5f)
            {
                g.setColour (accent.withAlpha (0.35f + 0.65f * lh / H));
                g.fillRect (x - 1.5f, base - lh, 3.0f, lh);
                g.setColour (accent.withAlpha (0.18f)); g.fillRect (x - 4.0f, base - lh, 8.0f, lh);
            }
        }
    }
    g.setFont (font (8.5f)); g.setColour (textDim.withAlpha (0.5f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = r.getX() + (std::log2 (hz) - fLo) / (fHi - fLo) * r.getWidth();
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String (hz, 0), (int) x - 14, (int) r.getBottom() + 1, 28, 10, juce::Justification::centred, false);
    }
}

void MeterPair::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float h = 5.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto row = juce::Rectangle<float> (r.getX() + 14.0f, r.getY() + ch * 14.0f, r.getWidth() - 14.0f, h);
        g.setFont (font (8.0f)); g.setColour (textDim);
        g.drawText (ch == 0 ? "L" : "R", (int) r.getX(), (int) row.getY() - 3, 10, 10, juce::Justification::left, false);
        g.setColour (juce::Colour (0xff2a2a2f)); g.fillRoundedRectangle (row, 2.0f);
        const float v = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (ch == 0 ? L : R) + 48.0f) / 48.0f);
        juce::ColourGradient grad (accentDim, row.getX(), 0, accent, row.getRight(), 0, false);
        g.setGradientFill (grad); g.fillRoundedRectangle (row.withWidth (row.getWidth() * v), 2.0f);
        const float m3 = (45.0f / 48.0f) * row.getWidth();
        g.setColour (textDim.withAlpha (0.6f)); g.fillRect (row.getX() + m3, row.getY() - 2.0f, 1.0f, h + 4.0f);
    }
}

// ---------------------------------------------------------------------------
// SamplesView
SamplesView::SamplesView (VellumProcessor& p)
    : processor (p),
      sensitivity (p.apvts, "sensitivity", "Sensitivity", true), instability (p.apvts, "instability", "Instability", true),
      space (p.apvts, "space", "Space", true), level (p.apvts, "level", "Level", false), punch (p.apvts, "punch", "Punch", false),
      ceiling (p.apvts, "ceiling", "Ceiling", false), lift (p.apvts, "lift", "Lift", false), drive (p.apvts, "drive", "Drive", false)
{
    for (auto* k : { &sensitivity, &instability, &space, &level, &punch, &ceiling, &lift, &drive }) addAndMakeVisible (*k);
    addAndMakeVisible (viz); addAndMakeVisible (meters);
}

void SamplesView::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (620);
    const int colW = left.getWidth() / 3;
    {
        Knob* big[3] = { &sensitivity, &instability, &space };
        for (int i = 0; i < 3; ++i)
        {
            auto col = left.withX (left.getX() + i * colW).withWidth (colW);
            big[i]->setBounds (col.withSizeKeepingCentre (190, 215).translated (0, -6));
        }
    }
    auto right = r.reduced (24, 0);
    auto bottom = right.removeFromBottom (120);
    viz.setBounds (right.getX(), right.getY() + 120, right.getWidth(), 72);
    const int kw = bottom.getWidth() / 5;
    {
        Knob* small[5] = { &level, &punch, &ceiling, &lift, &drive };
        for (int i = 0; i < 5; ++i) small[i]->setBounds (bottom.getX() + i * kw, bottom.getY() - 4, kw, 74);
    }
    meters.setBounds (bottom.getX() + 4, bottom.getY() + 78, bottom.getWidth() - 8, 30);
}

void SamplesView::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (line);
    g.fillRect (206, 40, 1, 190); g.fillRect (413, 40, 1, 190); g.fillRect (620, 30, 1, 300);
    // brand block
    g.setFont (font (44.0f, true)); g.setColour (accent);
    drawSpacedText (g, "VELLUM", juce::Rectangle<float> (644.0f, 30.0f, 332.0f, 56.0f), 13.0f);
    g.setFont (font (11.0f)); g.setColour (textDim);
    drawSpacedText (g, "DRUM RESYNTHESIS", juce::Rectangle<float> (644.0f, 86.0f, 332.0f, 16.0f), 5.0f);
    g.setFont (font (9.5f)); g.setColour (textDim.withAlpha (0.9f));
    g.drawText (readout, 644, 208, 332, 14, juce::Justification::centred, true);
}

void SamplesView::tick (double now)
{
    viz.tick (now);
    meters.setLevels (processor.meterLeft(), processor.meterRight());
}

void SamplesView::showPad (int pad)
{
    shownPad = pad;
    viz.setModel (processor.modelForPad (pad));
    auto m = processor.modelForPad (pad);
    readout = m ? (processor.pad (pad).name.toUpperCase() + "  " + dot() + "  " + juce::String (categoryName (m->category)).toUpperCase()
                   + "  " + dot() + "  " + juce::String (m->modes.size()) + " MODES  " + dot() + "  AUTO " + (m->autoGainDb >= 0 ? "+" : "") + juce::String (m->autoGainDb, 1) + " dB")
                : juce::String ("DROP ONE-SHOTS OR A LOOP ONTO THE PADS  " + dot() + "  C1 = PAD 1");
    repaint();
}

void SamplesView::onHit (const HitEvent& e, double now)
{
    if (e.pad != shownPad) { shownPad = e.pad; viz.setModel (processor.modelForPad (e.pad)); }
    viz.setHit (processor.lastRealization (e.pad), now);
    auto sgn = [] (float v, int dp) { return (v >= 0 ? "+" : "") + juce::String (v, dp); };
    readout = processor.pad (e.pad).name.toUpperCase() + "  " + dot() + "  VEL " + juce::String (e.velocity, 2)
            + "  " + dot() + "  POS " + sgn (e.posX, 2) + " " + sgn (e.posY, 2) + "  " + dot() + "  " + sgn (e.tuneCents, 1) + " c"
            + "  " + dot() + "  DECAY " + juce::String::charToString (juce::juce_wchar (0xd7)) + juce::String (e.decayMul, 2)
            + "  " + dot() + "  " + sgn (e.timingMs, 1) + " ms";
    repaint();
}

// ---------------------------------------------------------------------------
// KitsView
KitsView::KitsView (VellumProcessor& p) : processor (p)
{
    list.setModel (this); list.setRowHeight (24);
    addAndMakeVisible (list);
    nameEditor.setTextToShowWhenEmpty ("kit name", textDim);
    nameEditor.setFont (font (12.0f));
    addAndMakeVisible (nameEditor);
    for (auto* b : { &saveBtn, &loadBtn, &folderBtn, &deleteBtn }) addAndMakeVisible (*b);
    saveBtn.onClick = [this] { auto n = nameEditor.getText().trim(); if (n.isEmpty()) n = processor.getKitName(); processor.saveKit (n); refresh(); };
    loadBtn.onClick = [this] { const int r = list.getSelectedRow(); if (r >= 0 && r < kits.size()) processor.loadKit (kits[r]); };
    folderBtn.onClick = [this] { VellumProcessor::kitsFolder().revealToUser(); };
    deleteBtn.onClick = [this]
    {
        const int r = list.getSelectedRow();
        if (r < 0 || r >= kits.size()) return;
        juce::NativeMessageBox::showAsync (juce::MessageBoxOptions().withTitle ("Delete kit").withMessage ("Move \"" + kits[r].getFileNameWithoutExtension() + "\" to the Trash?")
                                           .withButton ("Delete").withButton ("Cancel"), [this, r] (int res) { if (res == 0 && r < kits.size()) { kits[r].moveToTrash(); refresh(); } });
    };
}

void KitsView::resized()
{
    auto r = getLocalBounds().reduced (40, 24);
    auto side = r.removeFromRight (260);
    list.setBounds (r.withTrimmedRight (30));
    nameEditor.setBounds (side.removeFromTop (28));
    side.removeFromTop (10);
    saveBtn.setBounds (side.removeFromTop (28)); side.removeFromTop (8);
    loadBtn.setBounds (side.removeFromTop (28)); side.removeFromTop (8);
    deleteBtn.setBounds (side.removeFromTop (28)); side.removeFromTop (8);
    folderBtn.setBounds (side.removeFromTop (28));
}

void KitsView::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setFont (font (10.0f)); g.setColour (textDim);
    drawSpacedText (g, "KITS  " + dot() + "  " + VellumProcessor::kitsFolder().getFullPathName().toUpperCase(), juce::Rectangle<float> (40.0f, 6.0f, (float) getWidth() - 80.0f, 14.0f), 1.2f, juce::Justification::centredLeft);
    if (kits.isEmpty())
    {
        g.setFont (font (11.0f)); g.setColour (textDim.withAlpha (0.7f));
        g.drawText ("No kits saved yet. Name it on the right and press SAVE - a kit stores the pads (with audio), pattern and all humanisation settings.",
                    list.getBounds(), juce::Justification::centred, true);
    }
}

void KitsView::refresh()
{
    kits = processor.listKits();
    list.updateContent(); list.repaint();
    for (int i = 0; i < kits.size(); ++i) if (kits[i].getFileNameWithoutExtension() == processor.getKitName()) list.selectRow (i);
    repaint();
}

void KitsView::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= kits.size()) return;
    if (selected) { g.setColour (accent.withAlpha (0.12f)); g.fillRect (0, 0, w, h); }
    g.setColour (line); g.fillRect (0, h - 1, w, 1);
    const bool current = kits[row].getFileNameWithoutExtension() == processor.getKitName();
    g.setFont (font (12.5f)); g.setColour (current ? accent : text);
    g.drawText (kits[row].getFileNameWithoutExtension(), 10, 0, w - 20, h, juce::Justification::centredLeft, true);
    g.setFont (font (10.0f)); g.setColour (textDim);
    g.drawText (kits[row].getLastModificationTime().formatted ("%d %b %Y"), 0, 0, w - 10, h, juce::Justification::centredRight, false);
}

void KitsView::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < kits.size()) processor.loadKit (kits[row]);
}

// ---------------------------------------------------------------------------
// SlicerView
SlicerView::SlicerView (VellumProcessor& p) : processor (p)
{
    processor.slicerChanged.addChangeListener (this);
    for (auto* s : { &sensitivity, &maxPads })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (*s);
    }
    sensitivity.setRange (0.0, 1.0, 0.01); sensitivity.setValue (0.5);
    maxPads.setRange (1, kNumPads, 1); maxPads.setValue (8);
    addAndMakeVisible (detect); addAndMakeVisible (extract); addAndMakeVisible (grooveToggle); addAndMakeVisible (status);
    grooveToggle.setToggleState (true, juce::dontSendNotification);
    status.setFont (font (10.5f)); status.setColour (juce::Label::textColourId, textDim); status.setJustificationType (juce::Justification::centredLeft);
    detect.onClick = [this] { processor.runSlicer ((float) sensitivity.getValue(), (int) maxPads.getValue()); repaint(); };
    extract.onClick = [this] { processor.extractSlicesToPads (grooveToggle.getToggleState()); };
    sensitivity.onDragEnd = [this] { detect.triggerClick(); };
    maxPads.onDragEnd = [this] { detect.triggerClick(); };
}

SlicerView::~SlicerView() { processor.slicerChanged.removeChangeListener (this); }

juce::Rectangle<int> SlicerView::waveArea() const { return getLocalBounds().reduced (40, 0).withTrimmedTop (28).withTrimmedBottom (110); }

void SlicerView::resized()
{
    auto r = getLocalBounds().reduced (40, 0);
    auto ctl = r.removeFromBottom (100).withTrimmedBottom (16);
    auto row1 = ctl.removeFromTop (30);
    sensitivity.setBounds (row1.removeFromLeft (300).withTrimmedLeft (90));
    row1.removeFromLeft (30);
    maxPads.setBounds (row1.removeFromLeft (260).withTrimmedLeft (90));
    row1.removeFromLeft (30);
    detect.setBounds (row1.removeFromLeft (110).reduced (0, 3));
    ctl.removeFromTop (10);
    auto row2 = ctl.removeFromTop (30);
    extract.setBounds (row2.removeFromLeft (190).reduced (0, 3));
    row2.removeFromLeft (16);
    grooveToggle.setBounds (row2.removeFromLeft (200));
    status.setBounds (row2);
}

void SlicerView::changeListenerCallback (juce::ChangeBroadcaster*) { rebuildPreview(); repaint(); }

void SlicerView::rebuildPreview()
{
    const int v = processor.loopVersion.load();
    if (v == previewVersion) return;
    previewVersion = v;
    const auto& a = processor.getLoopAudio();
    computePreview (a.data(), (int) a.size(), prevMin, prevMax, 1200);
}

void SlicerView::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    rebuildPreview();
    auto wa = waveArea();
    g.setColour (panel); g.fillRect (wa);
    g.setColour (line); g.drawRect (wa, 1);
    g.setFont (font (9.5f)); g.setColour (textDim);
    drawSpacedText (g, "SENSITIVITY", juce::Rectangle<float> (40.0f, (float) sensitivity.getY(), 86.0f, 30.0f), 1.0f, juce::Justification::centredLeft);
    drawSpacedText (g, "MAX PADS " + juce::String ((int) maxPads.getValue()), juce::Rectangle<float> ((float) maxPads.getX() - 90.0f, (float) maxPads.getY(), 86.0f, 30.0f), 1.0f, juce::Justification::centredLeft);

    if (! processor.hasLoop())
    {
        g.setFont (font (12.0f)); g.setColour (textDim);
        g.drawText ("Drop a drum loop here (or onto the pads). Vellum finds the hits, groups them by timbre,\nputs one resynthesised drum per group on the pads and extracts the groove.",
                    wa, juce::Justification::centred, true);
        g.setFont (font (10.0f)); g.setColour (textDim.withAlpha (0.7f));
        drawSpacedText (g, "CLICK TO BROWSE", wa.toFloat().removeFromBottom (30.0f), 1.5f);
        status.setText ("", juce::dontSendNotification);
        return;
    }
    // waveform
    const int n = (int) prevMin.size();
    float pk = 1e-4f; for (int i = 0; i < n; ++i) pk = std::max ({ pk, std::fabs (prevMin[(size_t) i]), std::fabs (prevMax[(size_t) i]) });
    const float cy = (float) wa.getCentreY(), hh = (float) wa.getHeight() * 0.46f / pk;
    g.setColour (colours::wave.withAlpha (0.8f));
    for (int i = 0; i < n; ++i)
    {
        const float x = (float) wa.getX() + 1.0f + (float) (wa.getWidth() - 2) * (float) i / (float) n;
        g.fillRect (x, cy - prevMax[(size_t) i] * hh, std::max (1.0f, (float) wa.getWidth() / (float) n), (prevMax[(size_t) i] - prevMin[(size_t) i]) * hh);
    }
    const auto& res = processor.getSliceResult();
    const double total = (double) processor.getLoopAudio().size();
    static const juce::Colour palette[kNumPads] = {
        juce::Colour (0xffe2a232), juce::Colour (0xff6fb1e0), juce::Colour (0xff8fd18a), juce::Colour (0xffe07a7a), juce::Colour (0xffc99be0), juce::Colour (0xff7ad9d0),
        juce::Colour (0xffe0c26f), juce::Colour (0xff9fa8e0), juce::Colour (0xffe09ac0), juce::Colour (0xffb0e07a), juce::Colour (0xffe0a07a), juce::Colour (0xffa0a0a8) };
    if (processor.isSlicerBusy())
    {
        g.setFont (font (11.0f)); g.setColour (accent); drawSpacedText (g, "SLICING", wa.toFloat().removeFromTop (24.0f), 1.5f);
    }
    for (size_t i = 0; i < res.slices.size(); ++i)
    {
        const auto& s = res.slices[i];
        const float x = (float) wa.getX() + (float) (s.startSample / total) * (float) wa.getWidth();
        const juce::Colour c = palette[s.cluster % kNumPads];
        g.setColour (c.withAlpha (0.85f)); g.fillRect (x, (float) wa.getY(), 1.0f, (float) wa.getHeight());
        g.setColour (c); g.fillRect (x, (float) wa.getY(), 6.0f, 3.0f);
        const bool rep = s.cluster < (int) res.representative.size() && res.representative[(size_t) s.cluster] == (int) i;
        if (rep)
        {
            g.setColour (c.withAlpha (0.16f));
            const float x2 = (float) wa.getX() + (float) ((s.startSample + s.length) / total) * (float) wa.getWidth();
            g.fillRect (x, (float) wa.getY(), x2 - x, (float) wa.getHeight());
            g.setFont (font (9.5f)); g.setColour (c);
            g.drawText (juce::String (s.cluster + 1) + " " + juce::String (res.clusterNames[(size_t) s.cluster]), (int) x + 3, wa.getY() + 6, 90, 12, juce::Justification::centredLeft, true);
        }
    }
    // beat grid
    if (res.valid())
    {
        const double stepSec = 60.0 / (res.bpm * 4.0), sr = processor.getLoopSampleRate();
        for (int st = 0; st < res.bars * 16; ++st)
        {
            const float x = (float) wa.getX() + (float) (st * stepSec * sr / total) * (float) wa.getWidth();
            g.setColour (textDim.withAlpha (st % 4 == 0 ? 0.35f : 0.12f));
            g.fillRect (x, (float) wa.getBottom() - (st % 4 == 0 ? 10.0f : 5.0f), 1.0f, st % 4 == 0 ? 10.0f : 5.0f);
        }
    }
    g.setFont (font (10.0f)); g.setColour (textDim);
    drawSpacedText (g, processor.getLoopName().toUpperCase(), juce::Rectangle<float> (40.0f, 6.0f, (float) getWidth() - 80.0f, 14.0f), 1.2f, juce::Justification::centredLeft);
    status.setText (res.valid() ? juce::String (res.slices.size()) + " hits  " + dot() + "  " + juce::String (res.numClusters) + " drums  " + dot() + "  "
                                      + juce::String (res.bpm, 1) + " bpm  " + dot() + "  " + juce::String (res.bars) + (res.bars == 1 ? " bar" : " bars")
                                : juce::String (processor.isSlicerBusy() ? "slicing..." : "no hits found - raise sensitivity"),
                    juce::dontSendNotification);
}

void SlicerView::mouseDown (const juce::MouseEvent& e)
{
    if (! waveArea().contains (e.getPosition())) return;
    if (processor.hasLoop())
    {
        // audition the slice under the cursor
        const auto& res = processor.getSliceResult();
        if (! res.valid()) return;
        const double frac = (double) (e.x - waveArea().getX()) / (double) waveArea().getWidth();
        const int sample = (int) (frac * (double) processor.getLoopAudio().size());
        for (size_t i = 0; i < res.slices.size(); ++i)
            if (sample >= res.slices[i].startSample && sample < res.slices[i].startSample + res.slices[i].length)
            {
                if (res.slices[i].cluster < kNumPads && processor.pad (res.slices[i].cluster).model) processor.triggerPad (res.slices[i].cluster, 0.9f);
                break;
            }
        return;
    }
    chooser = std::make_unique<juce::FileChooser> ("Load loop", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a;*.caf");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) processor.loadLoopFile (fc.getResult()); });
}

// ---------------------------------------------------------------------------
// GrooveView
GrooveView::GrooveView (VellumProcessor& p) : processor (p), swing (p.apvts, "seqSwing", "Swing", false)
{
    int id = 1;
    for (const auto& pr : groovePresets()) presets.addItem (pr.name, id++);
    presets.setTextWhenNothingSelected ("presets");
    presets.onChange = [this] { const int i = presets.getSelectedId() - 1; if (i >= 0) { processor.applyPreset (i); lengthBtn.setButtonText (juce::String (processor.getPattern().length)); repaint(); } };
    addAndMakeVisible (presets);
    lengthBtn.onClick = [this] { auto& pat = processor.getPattern(); pat.length = pat.length == 16 ? 32 : 16; lengthBtn.setButtonText (juce::String (pat.length)); repaint(); };
    clearBtn.onClick = [this] { processor.getPattern().clear(); processor.getPattern().name = "Empty"; presets.setSelectedId (0, juce::dontSendNotification); repaint(); };
    addAndMakeVisible (lengthBtn); addAndMakeVisible (clearBtn); addAndMakeVisible (swing);
    tempo.setFont (font (10.5f)); tempo.setColour (juce::Label::textColourId, textDim); tempo.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (tempo);
    lengthBtn.setButtonText (juce::String (processor.getPattern().length));
}

juce::Rectangle<int> GrooveView::gridArea() const { return getLocalBounds().reduced (40, 0).withTrimmedTop (44).withTrimmedBottom (14).withTrimmedLeft (86); }

void GrooveView::resized()
{
    auto top = getLocalBounds().reduced (40, 0).removeFromTop (40).withTrimmedTop (8);
    presets.setBounds (top.removeFromLeft (170).reduced (0, 4));
    top.removeFromLeft (10);
    lengthBtn.setBounds (top.removeFromLeft (44).reduced (0, 4));
    top.removeFromLeft (10);
    clearBtn.setBounds (top.removeFromLeft (70).reduced (0, 4));
    top.removeFromLeft (16);
    swing.setBounds (top.removeFromLeft (60).withHeight (52).withY (top.getY() - 8));
    tempo.setBounds (top);
}

bool GrooveView::cellAt (juce::Point<int> p, int& pad, int& step) const
{
    auto ga = gridArea();
    if (! ga.contains (p)) return false;
    const int len = processor.getPattern().length;
    pad = (p.y - ga.getY()) * kNumPads / ga.getHeight();
    step = (p.x - ga.getX()) * len / ga.getWidth();
    return pad >= 0 && pad < kNumPads && step >= 0 && step < len;
}

void GrooveView::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    auto ga = gridArea();
    const auto& pat = processor.getPattern();
    const int len = pat.length;
    const float cw = (float) ga.getWidth() / (float) len, rh = (float) ga.getHeight() / (float) kNumPads;
    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const float y = (float) ga.getY() + pad * rh;
        g.setFont (font (10.0f)); g.setColour (processor.pad (pad).model ? text.withAlpha (0.85f) : textDim.withAlpha (0.6f));
        g.drawText (processor.pad (pad).name, ga.getX() - 86, (int) y, 80, (int) rh, juce::Justification::centredRight, true);
        for (int s = 0; s < len; ++s)
        {
            auto cell = juce::Rectangle<float> ((float) ga.getX() + s * cw, y, cw, rh).reduced (1.5f, 1.5f);
            const bool beat = (s / 4) % 2 == 0;
            g.setColour (juce::Colour (beat ? 0xff222226 : 0xff1e1e22));
            g.fillRoundedRectangle (cell, 2.0f);
            const Step& st = pat.steps[pad][s];
            if (st.on)
            {
                g.setColour (accent.withAlpha (0.25f + 0.75f * st.vel));
                g.fillRoundedRectangle (cell.reduced (1.0f), 2.0f);
            }
            if (s == playStep) { g.setColour (juce::Colours::white.withAlpha (0.10f)); g.fillRoundedRectangle (cell, 2.0f); }
        }
    }
    g.setFont (font (10.0f)); g.setColour (textDim);
    drawSpacedText (g, juce::String (pat.name).toUpperCase(), juce::Rectangle<float> ((float) ga.getX(), 0.0f, 300.0f, 12.0f), 1.2f, juce::Justification::centredLeft);
}

void GrooveView::mouseDown (const juce::MouseEvent& e)
{
    int pad, step;
    dragged = false; pressPad = -1;
    if (! cellAt (e.getPosition(), pad, step)) return;
    auto& st = processor.getPattern().steps[pad][step];
    pressPad = pad; pressStep = step; pressWasOn = st.on; pressVel = st.on ? st.vel : 0.8f;
    if (! st.on) { st.on = true; st.vel = 0.8f; }
    repaint();
}

void GrooveView::mouseDrag (const juce::MouseEvent& e)
{
    if (pressPad < 0) return;
    if (std::abs (e.getDistanceFromDragStartY()) > 3) dragged = true;
    if (dragged)
    {
        auto& st = processor.getPattern().steps[pressPad][pressStep];
        st.on = true;
        st.vel = juce::jlimit (0.1f, 1.0f, pressVel - (float) e.getDistanceFromDragStartY() / 120.0f);
        repaint();
    }
}

void GrooveView::mouseUp (const juce::MouseEvent&)
{
    if (pressPad >= 0 && ! dragged && pressWasOn) processor.getPattern().steps[pressPad][pressStep].on = false;
    pressPad = -1;
    repaint();
}

void GrooveView::tick()
{
    const int s = processor.currentStep();
    if (s != playStep) { playStep = s; repaint (gridArea()); }
    tempo.setText (juce::String (processor.currentBpm(), 1) + " BPM  " + dot() + "  " + (processor.hostIsPlaying() ? "HOST" : processor.internalPlay.load() ? "INTERNAL" : "STOPPED"), juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
// AdvancedView
AdvancedView::AdvancedView (VellumProcessor& p)
{
    groups = {
        { "Strike", { { "velCurve", "Vel Curve" }, { "velBoost", "Vel Boost" }, { "velBright", "Vel>Bright" }, { "velGlide", "Vel>Glide" }, { "velSnap", "Vel>Snap" }, { "velNoise", "Vel>Noise" }, { "velDecay", "Vel>Decay" } }, 0 },
        { "Position", { { "posSpread", "Spread" }, { "posDrift", "Drift" }, { "posModes", "Pos>Modes" }, { "posTransient", "Pos>Attack" } }, 0 },
        { "Instrument drift", { { "tuneSpread", "Tune" }, { "tuneWalk", "Tension" }, { "inharmJitter", "Inharmonic" }, { "dampSpread", "Damping" }, { "dampCoherence", "Damp Indep" } }, 1 },
        { "Timing", { { "timeSpread", "Spread" }, { "timeBias", "Rush/Drag" }, { "velSpread", "Vel Spread" } }, 1 },
        { "Residual", { { "noiseRegen", "Regen" }, { "noiseDecaySpread", "Decay Var" }, { "noiseTiltSpread", "Tilt Var" } }, 1 },
        { "Attack", { { "attackSpread", "Length Var" }, { "attackTiltSpread", "Tilt Var" } }, 2 },
        { "Body", { { "glideBase", "Pitch Glide" }, { "glideTime", "Glide Time" }, { "decayScale", "Decay" }, { "modalTone", "Body/Noise" } }, 2 },
        { "Expression (CC11)", { { "exprAmp", "Level" }, { "exprDecay", "Decay" }, { "exprBright", "Bright" }, { "exprTransient", "Attack" }, { "exprPitch", "Pitch" } }, 2 },
        { "Source", { { "autoLevel", "Auto Level" } }, 2 },
    };
    for (auto& grp : groups)
        for (auto& pr : grp.params)
        {
            auto k = std::make_unique<Knob> (p.apvts, pr.first, pr.second, false);
            addAndMakeVisible (*k);
            knobs.push_back (std::move (k));
        }
}

void AdvancedView::resized()
{
    const int colW = 76, rowH = 112, x0 = 40, y0 = 26, gap = 14;
    int idx = 0;
    groupBounds.clear();
    int rowX[3] = { x0, x0, x0 };
    for (auto& grp : groups)
    {
        const int y = y0 + grp.row * rowH;
        int& x = rowX[grp.row];
        const int startX = x;
        for (size_t i = 0; i < grp.params.size(); ++i)
        {
            knobs[(size_t) idx++]->setBounds (x, y + 10, colW, 78);
            x += colW;
        }
        groupBounds.push_back ({ startX, y, x - startX, rowH - 8 });
        x += gap;
    }
}

void AdvancedView::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    for (size_t i = 0; i < groups.size(); ++i)
    {
        auto b = groupBounds[i];
        g.setFont (font (9.0f)); g.setColour (accentDim);
        drawSpacedText (g, groups[i].title.toUpperCase(), b.toFloat().withHeight (12.0f).translated (6.0f, -4.0f), 1.5f, juce::Justification::centredLeft);
        g.setColour (line); g.fillRect (b.getX() + 4, b.getY() + 9, b.getWidth() - 8, 1);
    }
    g.setFont (font (9.5f)); g.setColour (textDim.withAlpha (0.7f));
    drawSpacedText (g, "ADVANCED  " + dot() + "  SENSITIVITY SCALES THE VEL> ROWS, INSTABILITY SCALES EVERY SPREAD", juce::Rectangle<float> (40.0f, (float) getHeight() - 20.0f, (float) getWidth() - 80.0f, 12.0f), 1.0f, juce::Justification::centredLeft);
}

}} // namespace vellum::ui
