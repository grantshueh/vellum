// Vellum — the latent representation z of one drum sample.
//
//   x  --E-->  z = { modes (freq, decay, amp, phase, attack), residual transient,
//                   residual tail + parametric noise-band model }
//
// The modal part is fully parametric (a bank of exponentially decaying
// sinusoids, phase-coherent with the source via least squares). The residual
// (x minus modal reconstruction) is split into a short transient (stick /
// beater contact) and a stochastic tail (snare wires, shell noise, room) that
// is also described by per-band exponential decays so it can be re-rendered
// as a *different* realisation of the same process, or extended past a
// truncated slice end.
#pragma once
#include "Leveler.h"
#include <memory>
#include <string>
#include <vector>

namespace vellum {

constexpr int kMaxModes      = 48;
constexpr int kNumTailVariants = 4;
constexpr float kTailStretch = 1.6f;    // variants are rendered with decays * stretch

struct Mode
{
    float freq  = 100.0f;   // Hz
    float tau   = 0.2f;     // amplitude decay time constant, seconds
    float amp   = 0.0f;     // linear amplitude at t = 0 (after attack ramp)
    float phase = 0.0f;     // radians
    float attackTau = 0.002f; // onset ramp time constant, seconds
};

struct NoiseBand
{
    float centreHz = 1000.0f;
    float ampDb0   = -120.0f;  // band RMS (dB) at tail start
    float tau      = 0.1f;     // seconds
};

struct DrumModel
{
    double sampleRate = 48000.0;
    int    numSamples = 0;                  // trimmed source length

    // ---- modal component
    std::vector<Mode> modes;                // sorted ascending by frequency
    float modalEnergyRatio = 0.0f;          // fraction of source energy explained by the modes
    float lowestModeHz = 0.0f;

    // ---- residual component
    std::vector<float> transient;           // residual head (starts at t = 0), windowed out
    int   tailOffset = 0;                   // sample index where the tail starts
    std::vector<float> tail;                // residual tail: original + stochastic extension if truncated
    int   tailOriginalLength = 0;           // samples of genuine source tail inside `tail`
    std::vector<NoiseBand> bands;           // parametric noise model of the tail
    float tailTau = 0.1f;                   // energy-weighted noise decay
    bool  tailStochastic = true;            // false: tonal residual (e.g. 808 glide) -> never regenerate
    float tailFlatness = 1.0f;              // spectral flatness of the tail (1 = white noise)
    std::vector<std::vector<float>> tailVariants; // stochastic re-renders, decays * kTailStretch

    // ---- descriptors
    std::string name;
    std::string sourcePath;
    DrumCategory category = DrumCategory::Unknown;
    float autoGainDb = 0.0f;                // static source baseline (Leveler)
    float peakDb = -120.0f, rmsDb = -120.0f;
    float spectralCentroidHz = 0.0f;
    float durationSec = 0.0f;               // time to fall 60 dB below peak (or source length)
    bool  truncated = false;                // came from a loop slice cut by the next onset

    std::vector<float> source;              // trimmed mono source (kept for state saving + display)
    std::vector<float> previewMin, previewMax; // 256-point envelope for the UI

    bool isValid() const { return numSamples > 0 && (! modes.empty() || ! transient.empty() || ! tail.empty()); }
};

using DrumModelPtr = std::shared_ptr<const DrumModel>;

} // namespace vellum
