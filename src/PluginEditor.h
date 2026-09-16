#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Views.h"

namespace vellum {

class VellumEditor : public juce::AudioProcessorEditor,
                     private juce::Timer,
                     private juce::ChangeListener,
                     public juce::FileDragAndDropTarget
{
public:
    explicit VellumEditor (VellumProcessor&);
    ~VellumEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int x, int y) override;
    void fileDragMove (const juce::StringArray&, int x, int y) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // test hooks (offscreen snapshots)
    void debugShowTab (int t) { showTab (t); }
    void debugSetAdvanced (bool on) { header.gear.setToggleState (on, juce::dontSendNotification); setAdvanced (on); }
    void debugSelectPad (int p) { pads.select (p); }

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showTab (int tab);
    void setAdvanced (bool on);
    juce::Point<int> toContent (int x, int y) const;
    static double now() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

    VellumProcessor& processor;
    ui::VellumLookAndFeel lnf;
    juce::Component content;
    ui::HeaderBar header;
    ui::PadStrip pads;
    ui::TabBar tabs;
    ui::SamplesView samplesView;
    ui::KitsView kitsView;
    ui::SlicerView slicerView;
    ui::GrooveView grooveView;
    ui::AdvancedView advancedView;
    juce::TooltipWindow tooltips { this, 600 };
    bool advanced = false;
    int dropPad = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VellumEditor)
};

} // namespace vellum
