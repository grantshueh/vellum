// Renders every editor view offscreen to PNG (no window / screen-recording permission needed).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
#include <CoreFoundation/CoreFoundation.h>

static void pump (double ms)
{
    const double end = juce::Time::getMillisecondCounterHiRes() + ms;
    while (juce::Time::getMillisecondCounterHiRes() < end)
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.01, false);
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: ui_snapshot <outdir> [samples...]  (env LOOP=path to load a loop)\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    juce::File outDir (argv[1]); outDir.createDirectory();

    vellum::VellumProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    std::unique_ptr<vellum::VellumEditor> ed (dynamic_cast<vellum::VellumEditor*> (proc.createEditor()));
    ed->setSize (1000, 640);

    juce::Array<juce::File> files;
    for (int i = 2; i < argc; ++i) files.add (juce::File (argv[i]));
    if (! files.isEmpty()) proc.loadFiles (files, 0);
    if (const char* loop = std::getenv ("LOOP")) proc.loadLoopFile (juce::File (loop));
    pump (3000);

    auto save = [&] (const char* name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), false, 2.0f);
        juce::File f = outDir.getChildFile (name); f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };

    // fire a few hits through the audio path so the pads and the modal view light up
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 37, (juce::uint8) 118), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 70), 100);
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 200);
    proc.processBlock (buf, midi);
    for (int i = 0; i < 3; ++i) { juce::MidiBuffer none; proc.processBlock (buf, none); }
    ed->debugSelectPad (1);
    pump (120);
    save ("ui_samples.png");
    ed->debugShowTab (1); pump (60); save ("ui_kits.png");
    ed->debugShowTab (2); pump (60); save ("ui_slicer.png");
    ed->debugShowTab (3); proc.applyPreset (32); pump (60); save ("ui_groove.png");
    {
        auto mid = proc.exportPatternMidi (outDir.getChildFile ("pattern.mid"));
        juce::FileInputStream in (mid); juce::MidiFile mf; mf.readFrom (in);
        int notes = 0; for (int t = 0; t < mf.getNumTracks(); ++t) for (int i = 0; i < mf.getTrack (t)->getNumEvents(); ++i) if (mf.getTrack (t)->getEventPointer (i)->message.isNoteOn()) ++notes;
        std::printf ("midi export: %s, %d tracks, %d note-ons, tpq %d\n", mid.getFileName().toRawUTF8(), mf.getNumTracks(), notes, (int) mf.getTimeFormat());
    }
    ed->debugSetAdvanced (true); pump (60); save ("ui_advanced.png");
    ed.reset();
    return 0;
}
