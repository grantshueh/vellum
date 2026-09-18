// Vellum — onboard groove sequencer: pattern model, presets, and the block clock.
#pragma once
#include "dsp/Humanizer.h"
#include <random>
#include <string>
#include <vector>

namespace vellum {

constexpr int kMaxSteps = 32;

// pad roles used by the presets (and default pad names)
enum PadRole { RoleKick = 0, RoleSnare, RoleRim, RoleClap, RoleHatClosed, RoleHatOpen, RolePerc, RoleTomHi, RoleTomMid, RoleTomLo, RoleRide, RoleCrash };
const char* defaultPadName (int pad);
DrumCategory defaultPadCategory (int pad);

struct Step
{
    bool  on = false;
    float vel = 0.8f;
    float prob = 1.0f;
};

struct Pattern
{
    int   length = 16;        // 1..32 steps
    bool  triplet = false;    // false: step = 1/16 note, true: step = 1/12 note (8th-note triplet / 12/8 feel)
    float swing = 0.0f;       // 0..1  (50% .. 75%)
    Step  steps[kNumPads][kMaxSteps];
    std::string name = "Empty";

    double stepBeats() const { return triplet ? 1.0 / 3.0 : 0.25; }
    double loopBeats() const { return length * stepBeats(); }
    void clear() { for (auto& row : steps) for (auto& s : row) s = Step{}; }
};

struct GroovePreset
{
    const char* name;
    const char* region;           // menu section
    int length;
    bool triplet;
    float swing;
    const char* rows[kNumPads];   // 'X' 1.0, 'x' 0.8, 'o' 0.55, '-' 0.35, '.' off ; nullptr = empty row
};

const std::vector<GroovePreset>& groovePresets();
Pattern patternFromPreset (const GroovePreset& p);

struct Trigger { int pad; float vel; int sampleOffset; int step; };

class GrooveClock
{
public:
    void reset() { internalPpq = 0.0; }
    void process (const Pattern& p, bool hostPlaying, double hostPpq, double bpm, bool internalPlaying,
                  double sampleRate, int numSamples, std::vector<Trigger>& out, std::mt19937& rng);
    int  currentStep() const noexcept { return displayStep; }

private:
    double internalPpq = 0.0;
    int displayStep = -1;
};

} // namespace vellum
