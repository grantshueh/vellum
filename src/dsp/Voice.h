// Vellum — D(z'): real-time resynthesis of one perturbed hit.
#pragma once
#include "DrumModel.h"
#include "Humanizer.h"

namespace vellum {

class Voice
{
public:
    void prepare (double hostSampleRate);
    void start (DrumModelPtr model, const HitRealization& hit, uint64_t order);
    void render (float* left, float* right, int numSamples);   // accumulates into the buffers
    void fastRelease();                                        // steal: 3 ms fade then free

    bool isActive() const noexcept { return active; }
    int  pad() const noexcept { return hit.pad; }
    uint64_t order() const noexcept { return startOrder; }
    const HitRealization& realization() const noexcept { return hit; }

private:
    struct OnePole { float z = 0.0f, a = 0.0f; float process (float x) { z += a * (x - z); return z; } };

    void updateRotations();

    DrumModelPtr model;
    HitRealization hit;
    double sr = 48000.0;
    bool active = false;
    uint64_t startOrder = 0;
    int delay = 0;
    long long pos = 0;           // host samples since onset
    long long remaining = 0;
    float releaseGain = 1.0f, releaseStep = 0.0f;
    float gL = 1.0f, gR = 1.0f;

    // modal bank
    int K = 0;
    float re[kMaxModes], im[kMaxModes], rotRe[kMaxModes], rotIm[kMaxModes];
    float att[kMaxModes], attCoef[kMaxModes], baseW[kMaxModes], decayPerSample[kMaxModes];
    int subCounter = 0;

    // transient
    double trPos = 0.0, trRate = 1.0, trRateBase = 1.0;
    float trEnv = 1.0f, trDec = 1.0f;
    OnePole trLP; float trLo = 1.0f, trHi = 1.0f;

    // tail
    double tailStart = 0.0, tailPos = 0.0, tailRate = 1.0, tailRateBase = 1.0;
    float gA = 1.0f, gB = 0.0f, envA = 1.0f, envB = 1.0f, decA = 1.0f, decB = 1.0f;
    const std::vector<float>* tailB = nullptr;
    OnePole tailLP; float tailLo = 1.0f, tailHi = 1.0f;
};

} // namespace vellum
