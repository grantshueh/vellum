// Vellum — UI components.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"

namespace vellum { namespace ui {

constexpr int kBaseW = 1000, kBaseH = 640;

// ---------------------------------------------------------------------------
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& label, bool big);
    void resized() override;
    void paint (juce::Graphics& g) override;
    juce::Slider slider;
private:
    juce::String valueText() const;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::String label, paramId;
    bool big = false, dragging = false;
};

// ---------------------------------------------------------------------------
class IconButton : public juce::Button
{
public:
    enum Kind { Power, Play, Gear, ArrowLeft, ArrowRight };
    IconButton (Kind k, const juce::String& name) : juce::Button (name), kind (k) {}
    void paintButton (juce::Graphics& g, bool hover, bool down) override;
private:
    Kind kind;
};

// ---------------------------------------------------------------------------
class HeaderBar : public juce::Component
{
public:
    HeaderBar();
    void paint (juce::Graphics& g) override;
    void resized() override;
    void setKitName (const juce::String& n) { if (kitName != n) { kitName = n; repaint(); } }
    IconButton prev { IconButton::ArrowLeft, "prev" }, next { IconButton::ArrowRight, "next" }, gear { IconButton::Gear, "advanced" };
    juce::TextButton save { "save kit" }, newKit { "new" };
private:
    juce::String kitName;
};

// ---------------------------------------------------------------------------
class PadCell : public juce::Component
{
public:
    PadCell (VellumProcessor& p, int index);
    void refresh();
    void hit (const HitEvent& e, double now);
    bool tick (double now);          // returns true if still animating
    void setSelected (bool s) { if (selected != s) { selected = s; repaint(); } }
    void setDropHighlight (bool h) { if (dropHighlight != h) { dropHighlight = h; repaint(); } }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    std::function<void (int)> onSelect;
private:
    juce::Rectangle<int> waveArea() const;
    juce::Rectangle<int> menuArea() const;
    void showMenu();
    VellumProcessor& processor;
    int index;
    DrumModelPtr model;
    juce::String name;
    bool analyzing = false, selected = false, dropHighlight = false;
    double hitTime = -10.0; float hitVel = 0, hitX = 0, hitY = 0;
    std::unique_ptr<juce::FileChooser> chooser;
};

class PadStrip : public juce::Component
{
public:
    PadStrip (VellumProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    int padAt (int x) const;
    void refresh();
    void hit (const HitEvent& e, double now) { cells[(size_t) juce::jlimit (0, kNumPads - 1, e.pad)]->hit (e, now); }
    void tick (double now) { for (auto& c : cells) c->tick (now); }
    void select (int pad);
    int selectedPad() const { return selected; }
    void setDropHighlight (int pad) { for (int i = 0; i < kNumPads; ++i) cells[(size_t) i]->setDropHighlight (i == pad); }
    std::function<void (int)> onSelect;
private:
    [[maybe_unused]] VellumProcessor& processor;
    std::vector<std::unique_ptr<PadCell>> cells;
    int selected = 0;
};

// ---------------------------------------------------------------------------
class TabBar : public juce::Component
{
public:
    TabBar (VellumProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void setTab (int t, bool notify = true);
    int currentTab() const { return tab; }
    std::function<void (int)> onTab;
    juce::TextButton samples { "samples" }, kits { "kits" }, slicer { "slicer" }, groove { "groove" };
    IconButton power { IconButton::Power, "groove on" }, play { IconButton::Play, "play" };
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    int tab = 0;
};

// ---------------------------------------------------------------------------
class ModalViz : public juce::Component
{
public:
    void setModel (DrumModelPtr m) { model = std::move (m); repaint(); }
    void setHit (const HitRealization& h, double t) { hit = h; hitTime = t; hasHit = true; }
    void tick (double now) { this->now = now; if (hasHit && now - hitTime < 6.0) repaint(); }
    void paint (juce::Graphics& g) override;
private:
    DrumModelPtr model; HitRealization hit; bool hasHit = false; double hitTime = 0, now = 0;
};

class MeterPair : public juce::Component
{
public:
    void setLevels (float l, float r) { L = l; R = r; repaint(); }
    void paint (juce::Graphics& g) override;
private:
    float L = 0, R = 0;
};

class SamplesView : public juce::Component
{
public:
    SamplesView (VellumProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void tick (double now);
    void showPad (int pad);                    // selected / last hit pad
    void onHit (const HitEvent& e, double now);
private:
    VellumProcessor& processor;
    Knob sensitivity, instability, space, level, punch, ceiling, lift, drive;
    ModalViz viz; MeterPair meters;
    juce::String readout;
    int shownPad = 0;
};

// ---------------------------------------------------------------------------
class KitsView : public juce::Component, private juce::ListBoxModel
{
public:
    KitsView (VellumProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void refresh();
    void visibilityChanged() override { if (isVisible()) refresh(); }
private:
    int getNumRows() override { return kits.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int) override {}
    VellumProcessor& processor;
    juce::ListBox list;
    juce::Array<juce::File> kits;
    juce::TextEditor nameEditor;
    juce::TextButton saveBtn { "save" }, loadBtn { "load" }, folderBtn { "open folder" }, deleteBtn { "delete" }, newBtn { "new kit (clear all)" };
public:
    std::function<void()> onNewKit;
private:
};

// ---------------------------------------------------------------------------
class SlicerView : public juce::Component, private juce::ChangeListener
{
public:
    SlicerView (VellumProcessor& p);
    ~SlicerView() override;
    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuildPreview();
    juce::Rectangle<int> waveArea() const;
    VellumProcessor& processor;
    juce::Slider sensitivity, maxPads;
    juce::TextButton detect { "detect" }, extract { "extract to pads" };
    juce::ToggleButton grooveToggle { "also extract groove" };
    juce::Label status;
    std::vector<float> prevMin, prevMax;
    int previewVersion = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};

// ---------------------------------------------------------------------------
// drag this out to a DAW track to drop a .mid of the current pattern
class MidiDragButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<juce::File()> makeFile;   // writes and returns the file to drag
    std::function<void()> onClick;          // plain click -> export dialog
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { dragging = false; }
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
private:
    bool dragging = false;
};

class GrooveView : public juce::Component
{
public:
    GrooveView (VellumProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void tick();
    void refreshNames() { syncControls(); repaint(); }
private:
    juce::Rectangle<int> gridArea() const;
    bool cellAt (juce::Point<int> p, int& pad, int& step) const;
    VellumProcessor& processor;
    juce::ComboBox presets, lengthBox;
    juce::TextButton tripletBtn { "1/12" }, clearBtn { "clear" };
    MidiDragButton midiDrag;
    Knob swing;
    juce::Label tempo;
    std::unique_ptr<juce::FileChooser> chooser;
    void syncControls();
    int playStep = -1;
    int pressPad = -1, pressStep = -1; bool pressWasOn = false, dragged = false; float pressVel = 0.8f;
};

// ---------------------------------------------------------------------------
class AdvancedView : public juce::Component
{
public:
    AdvancedView (VellumProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
private:
    struct Group { juce::String title; std::vector<std::pair<juce::String, juce::String>> params; int row; };
    std::vector<Group> groups;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<juce::Rectangle<int>> groupBounds;
};

}} // namespace vellum::ui
