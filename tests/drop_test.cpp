// Exercise the editor's file-drop path directly (bypasses the OS drag layer).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <CoreFoundation/CoreFoundation.h>
#include <cstdio>
static void pump (double ms) { const double end = juce::Time::getMillisecondCounterHiRes() + ms; while (juce::Time::getMillisecondCounterHiRes() < end) CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.01, false); }
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    vellum::VellumProcessor proc; proc.prepareToPlay (48000.0, 512);
    std::unique_ptr<vellum::VellumEditor> ed (dynamic_cast<vellum::VellumEditor*> (proc.createEditor()));
    ed->setSize (1000, 640);
    juce::StringArray files; for (int i = 1; i < argc; ++i) files.add (argv[i]);
    std::printf ("interested: %d\n", (int) ed->isInterestedInFileDrag (files));
    ed->filesDropped (files, 300, 150);   // over pad 4
    pump (2500);
    for (int i = 0; i < vellum::kNumPads; ++i)
        if (proc.pad (i).model) std::printf ("pad %d: %s (%zu modes)\n", i + 1, proc.pad (i).name.toRawUTF8(), proc.pad (i).model->modes.size());
    return 0;
}
