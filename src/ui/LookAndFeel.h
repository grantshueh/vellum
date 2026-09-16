// Vellum — visual language: dark charcoal, thin arcs, amber accents.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace vellum { namespace ui {

namespace colours
{
    const juce::Colour bg        (0xff18181a);
    const juce::Colour header    (0xff1e1e21);
    const juce::Colour panel     (0xff1c1c1f);
    const juce::Colour padTop    (0xff2c2c30);
    const juce::Colour padBottom (0xff1d1d20);
    const juce::Colour line      (0xff2c2c31);
    const juce::Colour textDim   (0xff7b7b83);
    const juce::Colour text      (0xffd4d4d8);
    const juce::Colour accent    (0xffe2a232);
    const juce::Colour accentDim (0xff8c6520);
    const juce::Colour wave      (0xff75757c);
    const juce::Colour arc       (0xffdadadd);
    const juce::Colour good      (0xff7fc27a);
}

inline juce::String dot() { return juce::String::charToString ((juce::juce_wchar) 0xB7); }

inline juce::Font font (float height, bool bold = false)
{
    auto opts = juce::FontOptions().withHeight (height);
    if (bold) opts = opts.withStyle ("Bold");
    return juce::Font (opts);
}

inline float spacedTextWidth (const juce::Font& f, const juce::String& text, float spacing)
{
    float w = 0.0f;
    for (int i = 0; i < text.length(); ++i)
        w += juce::GlyphArrangement::getStringWidth (f, text.substring (i, i + 1)) + (i + 1 < text.length() ? spacing : 0.0f);
    return w;
}

// letter-spaced text, centred or left-aligned in the area
inline void drawSpacedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                            float spacing, juce::Justification just = juce::Justification::centred)
{
    const juce::Font f = g.getCurrentFont();
    const float total = spacedTextWidth (f, text, spacing);
    float x = just.testFlags (juce::Justification::left) ? area.getX()
            : just.testFlags (juce::Justification::right) ? area.getRight() - total
            : area.getCentreX() - total * 0.5f;
    for (int i = 0; i < text.length(); ++i)
    {
        const juce::String ch = text.substring (i, i + 1);
        const float w = juce::GlyphArrangement::getStringWidth (f, ch);
        g.drawText (ch, juce::Rectangle<float> (x, area.getY(), w + 2.0f, area.getHeight()), juce::Justification::centredLeft, false);
        x += w + spacing;
    }
}

class VellumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VellumLookAndFeel()
    {
        using namespace colours;
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff232327));
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff232327));
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::arrowColourId, textDim);
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff232327));
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, line);
        setColour (juce::TextEditor::focusedOutlineColourId, accentDim);
        setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.3f));
        setColour (juce::CaretComponent::caretColourId, accent);
        setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff3a3a40));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff232327));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2e2e33));
        setColour (juce::TextButton::textColourOffId, textDim);
        setColour (juce::TextButton::textColourOnId, text);
        setColour (juce::ToggleButton::textColourId, textDim);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::ToggleButton::tickDisabledColourId, textDim);
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff2a2a2f));
        setColour (juce::Slider::trackColourId, accent);
        setColour (juce::Slider::thumbColourId, arc);
        setColour (juce::Label::textColourId, text);
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff232327));
        setColour (juce::AlertWindow::textColourId, text);
        setColour (juce::AlertWindow::outlineColourId, line);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff232327));
        setColour (juce::TooltipWindow::textColourId, text);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (11.0f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.0f); }
    juce::Font getPopupMenuFont() override { return font (12.5f); }
    juce::Font getLabelFont (juce::Label&) override { return font (12.0f); }
    juce::Font getAlertWindowTitleFont() override { return font (15.0f, true); }
    juce::Font getAlertWindowMessageFont() override { return font (13.0f); }
    juce::Font getAlertWindowFont() override { return font (12.0f); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool pill = b.getComponentID() == "pill";
        const bool tab = b.getComponentID() == "tab";
        const bool on = b.getToggleState();
        if (pill)
        {
            g.setColour (juce::Colour (0xff2a2a2f).withAlpha (down ? 0.9f : hover ? 0.7f : 0.0f));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (colours::textDim.withAlpha (hover ? 0.9f : 0.55f));
            g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            return;
        }
        if (tab)
        {
            g.setColour (on ? juce::Colour (0xff2b2b30) : hover ? juce::Colour (0xff26262a) : juce::Colour (0xff222226));
            g.fillRect (r);
            g.setColour (colours::line);
            g.drawRect (r, 1.0f);
            return;
        }
        g.setColour (on ? juce::Colour (0xff2f2f35) : (down ? juce::Colour (0xff2c2c31) : hover ? juce::Colour (0xff27272b) : juce::Colour (0xff222226)));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? colours::accentDim : colours::line);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool hover, bool) override
    {
        const bool on = b.getToggleState();
        const bool pill = b.getComponentID() == "pill";
        g.setFont (font (pill ? 10.0f : 11.0f));
        g.setColour (on ? colours::text : hover ? colours::text.withAlpha (0.85f) : colours::textDim);
        drawSpacedText (g, b.getButtonText().toUpperCase(), b.getLocalBounds().toFloat(), pill ? 1.6f : 1.1f);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const float d = (float) std::min (w, h) - 6.0f;
        const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
        const bool big = d > 70.0f;
        const float R = d * 0.5f;                                  // arc radius
        const float r = R - (big ? 11.0f : 7.0f);                   // knob radius
        const float thick = big ? 2.4f : 1.9f;

        // body
        juce::ColourGradient body (juce::Colour (0xff36363b), cx - r * 0.45f, cy - r * 0.55f,
                                   juce::Colour (0xff141416), cx + r * 0.7f, cy + r * 0.9f, true);
        g.setGradientFill (body);
        g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (cx - r, cy - r, 2 * r, 2 * r, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillEllipse (cx - r * 0.55f, cy - r * 0.75f, r * 0.9f, r * 0.55f);

        // arc track
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float angle = startAngle + pos * (endAngle - startAngle);
        juce::Path track;
        track.addCentredArc (cx, cy, R, R, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float from = bipolar ? 0.5f * (startAngle + endAngle) : startAngle;
        if (std::fabs (angle - from) > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc (cx, cy, R, R, 0.0f, std::min (from, angle), std::max (from, angle), true);
            g.setColour (big ? colours::arc : colours::accent);
            g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        // glowing tip
        const float tx = cx + R * std::sin (angle), ty = cy - R * std::cos (angle);
        g.setColour (colours::accent.withAlpha (0.25f));
        g.fillEllipse (tx - thick * 2.2f, ty - thick * 2.2f, thick * 4.4f, thick * 4.4f);
        g.setColour (big ? colours::accent : juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (tx - thick * 0.9f, ty - thick * 0.9f, thick * 1.8f, thick * 1.8f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (juce::Colour (0xff2a2a2f));
        g.fillRoundedRectangle ((float) x, cy - 1.5f, (float) w, 3.0f, 1.5f);
        const float x0 = s.getMinimum() < 0.0 ? (float) x + (float) w * 0.5f : (float) x;
        g.setColour (colours::accent);
        g.fillRoundedRectangle (std::min (x0, sliderPos), cy - 1.5f, std::fabs (sliderPos - x0), 3.0f, 1.5f);
        g.setColour (colours::arc);
        g.fillEllipse (sliderPos - 5.0f, cy - 5.0f, 10.0f, 10.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (juce::Colour (0xff232327)); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (colours::line); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        juce::Path p; const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (colours::textDim); g.fillPath (p);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const float box = 12.0f, by = r.getCentreY() - box * 0.5f;
        g.setColour (b.getToggleState() ? colours::accent : (hover ? colours::text : colours::textDim));
        g.drawRoundedRectangle (2.0f, by, box, box, 2.0f, 1.0f);
        if (b.getToggleState()) g.fillRoundedRectangle (5.0f, by + 3.0f, box - 6.0f, box - 6.0f, 1.0f);
        g.setFont (font (10.5f));
        g.setColour (hover ? colours::text : colours::textDim);
        drawSpacedText (g, b.getButtonText().toUpperCase(), r.withTrimmedLeft (box + 8.0f), 1.2f, juce::Justification::centredLeft);
    }
};

}} // namespace vellum::ui
