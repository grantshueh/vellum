// Vellum — E(x): analysis of a one-shot into a DrumModel. Runs off the audio thread.
#pragma once
#include "DrumModel.h"
#include <juce_dsp/juce_dsp.h>

namespace vellum {

struct AnalysisOptions
{
    int   maxModes = kMaxModes;
    float maxSeconds = 4.0f;          // analyse at most this much audio
    bool  truncated = false;          // source is a loop slice cut by the next onset
    int   variantSeed = 1234;
};

// mono input at sampleRate. name/path are descriptive only (name drives classification
// unless `category` is forced).
std::shared_ptr<DrumModel> analyzeDrum (const float* mono, int numSamples, double sampleRate,
                                        const std::string& name, const std::string& sourcePath,
                                        const AnalysisOptions& options = {},
                                        DrumCategory forcedCategory = DrumCategory::Count);

// helpers shared with the slicer
void computePreview (const float* x, int n, std::vector<float>& mn, std::vector<float>& mx, int points = 256);

} // namespace vellum
