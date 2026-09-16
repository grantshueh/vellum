// Vellum — loop slicer: onset detection, timbre clustering into pads, groove extraction.
#pragma once
#include "DrumModel.h"
#include <string>
#include <vector>

namespace vellum {

struct SlicerOptions
{
    float  sensitivity = 0.5f;     // 0..1, higher = more onsets
    int    maxPads = 8;            // clusters
    double hostBpm = 120.0;        // used to pick the bar count
};

struct SliceResult
{
    struct Slice
    {
        int   startSample = 0;
        int   length = 0;          // to the next onset (or loop end)
        int   cluster = 0;
        float energy = 0.0f;       // peak RMS, linear
        float centroidHz = 0.0f, lowRatio = 0.0f, highRatio = 0.0f, decaySec = 0.0f;
    };
    std::vector<Slice> slices;                // one per onset
    int numClusters = 0;
    std::vector<int> representative;          // slice index per cluster (ordered low -> high)
    std::vector<std::string> clusterNames;
    std::vector<DrumCategory> clusterCategories;
    double bpm = 120.0;
    int bars = 1;
    double loopSeconds = 0.0;
    bool valid() const { return ! slices.empty() && numClusters > 0; }
};

std::vector<int> detectOnsets (const float* x, int n, double sr, float sensitivity);   // sample positions
SliceResult sliceLoop (const float* x, int n, double sr, const SlicerOptions& options);

} // namespace vellum
