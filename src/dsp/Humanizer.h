// Vellum — Δz generator. Turns MIDI velocity / expression plus per-pad player
// state into a physically-shaped perturbation of the drum model: a neighbouring
// strike on the same object rather than a random detune.
#pragma once
#include "DrumModel.h"
#include "GainStage.h"
#include <random>

namespace vellum {

constexpr int kNumPads = 12;

struct HumanizeSettings
{
    // macros
    float sensitivity = 0.6f, instability = 0.5f, space = 0.2f, autoLevel = 1.0f;
    // strike (velocity -> X, scaled by sensitivity)
    float velCurve = 1.5f, velBoost = 0.0f, velBright = 0.6f, velGlide = 0.5f, velSnap = 0.5f, velNoise = 0.5f, velDecay = 0.2f;
    // position
    float posSpread = 0.4f, posDrift = 0.6f, posModes = 0.7f, posTransient = 0.5f;
    // instrument drift
    float tuneSpread = 0.3f, tuneWalk = 0.4f, inharmJitter = 0.3f, dampSpread = 0.4f, dampCoherence = 0.5f;
    // timing
    float timeSpread = 0.3f, timeBias = 0.0f, velSpread = 0.3f;
    // residual
    float noiseRegen = 0.6f, noiseDecaySpread = 0.4f, noiseTiltSpread = 0.3f;
    // transient
    float attackSpread = 0.3f, attackTiltSpread = 0.3f;
    // body
    float glideBase = 0.15f, glideTime = 0.4f, decayScale = 1.0f, modalTone = 0.0f;
    // expression routing
    float exprAmp = 1.0f, exprDecay = 0.0f, exprBright = 0.0f, exprTransient = 0.0f, exprPitch = 0.0f;
};

struct HitRealization
{
    int   pad = 0;
    float velocity = 1.0f, expression = 1.0f;
    float gainLin = 1.0f;                 // finalVoiceGain (source + performance + user)
    float freqScale = 1.0f;               // global tuning of all modes
    float glideAmount = 0.0f, glideTau = 0.03f;
    float modeAmpMul[kMaxModes], modeFreqMul[kMaxModes], modeDecayMul[kMaxModes];
    float transientTau = 0.0f, transientGain = 1.0f, transientTiltDb = 0.0f;   // tau <= 0: full-length transient
    int   tailVariant = 0;
    float tailRegen = 0.5f, tailGain = 1.0f, tailDecayMul = 1.0f, tailTiltDb = 0.0f;
    float pan = 0.0f, modalGain = 1.0f, residualGain = 1.0f;
    int   delaySamples = 0;
    // for display
    float posX = 0.0f, posY = 0.0f, tuneCents = 0.0f, decayMul = 1.0f, brightness = 0.0f, timingMs = 0.0f;
};

class Humanizer
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset();
    HitRealization realize (int pad, float velocity01, float expression01, const DrumModel& model,
                            const HumanizeSettings& s, float padUserGainDb, double nowSeconds);

private:
    struct PadState { float posX = 0, posY = 0, tuneWalkCents = 0; double lastHit = -1.0; };
    PadState pads[kNumPads];
    std::mt19937 rng { 2024 };
    std::normal_distribution<float> normal { 0.0f, 1.0f };
    double sr = 48000.0;
    float N (float sigma) { return sigma > 0.0f ? normal (rng) * sigma : 0.0f; }
};

} // namespace vellum
