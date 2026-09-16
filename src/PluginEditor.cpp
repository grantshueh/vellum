#include "PluginEditor.h"

namespace vellum {

using namespace ui;

static void vellumLog (const juce::String& line)
{
    auto f = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Vellum").getChildFile ("drag-log.txt");
    f.appendText (juce::Time::getCurrentTime().toString (true, true) + "  [editor] " + line + "\n");
}


VellumEditor::VellumEditor (VellumProcessor& p)
    : AudioProcessorEditor (&p), processor (p), pads (p), tabs (p), samplesView (p), kitsView (p), slicerView (p), grooveView (p), advancedView (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setSize (kBaseW, kBaseH);
    for (auto* c : std::initializer_list<juce::Component*> { &header, &pads, &tabs, &samplesView, &kitsView, &slicerView, &grooveView, &advancedView })
        content.addChildComponent (c);
    header.setVisible (true); pads.setVisible (true); tabs.setVisible (true);

    header.setBounds (0, 0, kBaseW, 48);
    pads.setBounds (0, 48, kBaseW, 186);
    tabs.setBounds (0, 234, kBaseW, 44);
    const juce::Rectangle<int> main (0, 278, kBaseW, kBaseH - 278);
    for (auto* v : std::initializer_list<juce::Component*> { &samplesView, &kitsView, &slicerView, &grooveView, &advancedView }) v->setBounds (main);

    header.setKitName (processor.getKitName());
    header.prev.onClick = [this] { processor.loadKitByOffset (-1); };
    header.next.onClick = [this] { processor.loadKitByOffset (1); };
    header.save.onClick = [this]
    {
        auto* w = new juce::AlertWindow ("Save kit", "Kits store the pads (with audio), the pattern and every setting.", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", processor.getKitName());
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
        {
            if (r == 1) { processor.saveKit (w->getTextEditorContents ("name")); kitsView.refresh(); header.setKitName (processor.getKitName()); }
        }), true);
    };
    header.gear.onClick = [this] { setAdvanced (header.gear.getToggleState()); };
    tabs.onTab = [this] (int t) { showTab (t); };
    pads.onSelect = [this] (int pad) { samplesView.showPad (pad); };
    samplesView.showPad (0);
    showTab (0);

    processor.padsChanged.addChangeListener (this);
    processor.slicerChanged.addChangeListener (this);
    lastLoopVersion = processor.loopVersion.load();
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    setResizeLimits (700, 700 * kBaseH / kBaseW, 2000, 2000 * kBaseH / kBaseW);
    setSize (kBaseW, kBaseH);
    startTimerHz (30);
}

VellumEditor::~VellumEditor()
{
    processor.padsChanged.removeChangeListener (this);
    processor.slicerChanged.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void VellumEditor::paint (juce::Graphics& g) { g.fillAll (colours::bg); }

void VellumEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseW;
    content.setTransform (juce::AffineTransform::scale (scale));
}

void VellumEditor::showTab (int tab)
{
    tabs.setTab (tab, false);
    if (advanced) { header.gear.setToggleState (false, juce::dontSendNotification); advanced = false; }
    samplesView.setVisible (tab == 0); kitsView.setVisible (tab == 1); slicerView.setVisible (tab == 2); grooveView.setVisible (tab == 3);
    advancedView.setVisible (false);
    if (tab == 1) kitsView.refresh();
}

void VellumEditor::setAdvanced (bool on)
{
    advanced = on;
    if (on)
    {
        for (auto* v : std::initializer_list<juce::Component*> { &samplesView, &kitsView, &slicerView, &grooveView }) v->setVisible (false);
        advancedView.setVisible (true);
    }
    else showTab (tabs.currentTab());
}

void VellumEditor::timerCallback()
{
    const double t = now();
    HitEvent e;
    while (processor.popHitEvent (e))
    {
        pads.hit (e, t);
        samplesView.onHit (e, t);
    }
    pads.tick (t);
    samplesView.tick (t);
    if (grooveView.isVisible()) grooveView.tick();
    if (tabs.play.getToggleState() != processor.internalPlay.load()) tabs.play.setToggleState (processor.internalPlay.load(), juce::dontSendNotification);
}

void VellumEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &processor.slicerChanged)
    {
        const int v = processor.loopVersion.load();
        if (v != lastLoopVersion) { lastLoopVersion = v; showTab (2); }   // a new loop arrived: show it
        return;
    }
    vellumLog ("pads refreshed");
    pads.refresh();
    header.setKitName (processor.getKitName());
    samplesView.showPad (pads.selectedPad());
    grooveView.refreshNames();
    if (kitsView.isVisible()) kitsView.refresh();
}

// ---------------------------------------------------------------------------
juce::Point<int> VellumEditor::toContent (int x, int y) const
{
    const float scale = (float) getWidth() / (float) kBaseW;
    return { (int) (x / scale), (int) (y / scale) };
}

bool VellumEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        juce::File file (f);
        if (file.isDirectory() || file.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3;m4a;caf;vkit")) return true;
    }
    return false;
}

void VellumEditor::fileDragEnter (const juce::StringArray& files, int x, int y) { fileDragMove (files, x, y); }

void VellumEditor::fileDragMove (const juce::StringArray&, int x, int y)
{
    const auto p = toContent (x, y);
    const int pad = pads.getBounds().contains (p) ? pads.padAt (p.x - pads.getX()) : -1;
    if (pad != dropPad) { dropPad = pad; pads.setDropHighlight (pad); }
}

void VellumEditor::fileDragExit (const juce::StringArray&) { dropPad = -1; pads.setDropHighlight (-1); }

void VellumEditor::filesDropped (const juce::StringArray& fileNames, int x, int y)
{
    vellumLog ("filesDropped " + fileNames.joinIntoString (" | ") + " at " + juce::String (x) + "," + juce::String (y));
    pads.setDropHighlight (-1); dropPad = -1;
    juce::Array<juce::File> files;
    for (const auto& f : fileNames) files.add (juce::File (f));
    if (files.size() == 1 && files[0].hasFileExtension ("vkit")) { processor.loadKit (files[0]); return; }

    const auto p = toContent (x, y);
    if (slicerView.isVisible() && slicerView.getBounds().contains (p))
    {
        processor.loadLoopFile (files[0]);
        return;
    }
    const bool onPads = pads.getBounds().contains (p);
    const int startPad = onPads ? pads.padAt (p.x - pads.getX()) : 0;
    processor.loadFiles (files, startPad, ! onPads);   // a drop onto a pad always means "this one-shot goes here"
}

} // namespace vellum
