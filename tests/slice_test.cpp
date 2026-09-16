// Offline check of the loop slicer: onsets, clusters, representatives, tempo.
#include <juce_audio_formats/juce_audio_formats.h>
#include "dsp/Slicer.h"
#include <cstdio>

using namespace vellum;

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: slice_test <loop.wav> [hostBpm]\n"); return 1; }
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (juce::File (argv[1])));
    if (! reader) { std::printf ("cannot read\n"); return 1; }
    const int n = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> buf ((int) reader->numChannels, n);
    reader->read (&buf, 0, n, 0, true, true);
    std::vector<float> mono ((size_t) n, 0.0f);
    for (int c = 0; c < (int) reader->numChannels; ++c) for (int i = 0; i < n; ++i) mono[(size_t) i] += buf.getReadPointer (c)[i] / (float) reader->numChannels;
    SlicerOptions opt; opt.hostBpm = argc > 2 ? atof (argv[2]) : 120.0;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto r = sliceLoop (mono.data(), n, reader->sampleRate, opt);
    std::printf ("%zu onsets, %d clusters, %.1f bpm, %d bars (%.2f s) in %.0f ms\n", r.slices.size(), r.numClusters, r.bpm, r.bars, r.loopSeconds, juce::Time::getMillisecondCounterHiRes() - t0);
    for (int c = 0; c < r.numClusters; ++c)
    {
        int count = 0; for (const auto& s : r.slices) if (s.cluster == c) ++count;
        const auto& rep = r.slices[(size_t) r.representative[(size_t) c]];
        std::printf ("  pad %2d %-12s (%s) x%d  rep @ %.3f s len %.0f ms  centroid %.0f Hz low %.2f decay %.0f ms\n", c + 1, r.clusterNames[(size_t) c].c_str(),
                     categoryName (r.clusterCategories[(size_t) c]), count, rep.startSample / reader->sampleRate, rep.length * 1000.0 / reader->sampleRate, rep.centroidHz, rep.lowRatio, rep.decaySec * 1000);
    }
    const double stepSec = 60.0 / (r.bpm * 4.0);
    std::printf ("  grid: ");
    for (const auto& s : r.slices) std::printf ("%d@%.2f ", s.cluster + 1, s.startSample / reader->sampleRate / stepSec);
    std::printf ("\n");
    return 0;
}
