// Vellum — voice gain architecture (pure C++, no JUCE).
//
//   sourceGainDb      = autoGainDb * autoLevelAmount          (SOURCE BALANCE)
//   performanceGainDb = velocityGainDb + expressionGainDb     (PERFORMANCE)
//   finalVoiceGainDb  = sourceGainDb + performanceGainDb + userGainDb   (MIXING)
//
// None of these are recomputed when pitch / resampling ratio changes, and the
// auto-level term is never touched by velocity or expression.
#pragma once
#include <algorithm>
#include <cmath>

namespace vellum {

constexpr float kDefaultVelocityExponent = 1.5f;

inline float clamp01 (float v) { return std::min (1.0f, std::max (0.0f, v)); }

// 6. Velocity: v^exp, 0 dB at full velocity (+ optional boost, default 0).
inline float velocityGainDb (float velocity01, float exponent = kDefaultVelocityExponent, float maxBoostDb = 0.0f)
{
    const float v = clamp01 (velocity01);
    const float amp = std::pow (v, std::max (0.05f, exponent));
    return 20.0f * std::log10 (std::max (amp, 1e-5f)) + maxBoostDb * v;
}

inline float velocityGainDbMidi (int velocity0to127, float exponent = kDefaultVelocityExponent, float maxBoostDb = 0.0f)
{
    return velocityGainDb ((float) velocity0to127 / 127.0f, exponent, maxBoostDb);
}

// 7. Expression amplitude component. Unipolar 0..1 -> -12..0 dB (linear in dB).
inline float expressionGainDb (float expression01, float rangeDb = 12.0f)
{
    return -rangeDb * (1.0f - clamp01 (expression01));
}

// Bipolar variant -1..1 -> -12..+3 dB; positive gain only if allowed.
inline float expressionGainDbBipolar (float expressionPm1, bool allowPositive = false)
{
    const float e = std::min (1.0f, std::max (-1.0f, expressionPm1));
    if (e <= 0.0f) return 12.0f * e;                 // -12 .. 0
    return allowPositive ? 3.0f * e : 0.0f;
}

struct VoiceGainTerms
{
    float autoGainDb       = 0.0f;   // belongs to the sample
    float autoLevelAmount  = 1.0f;   // global 0..1
    float velocityGainDb   = 0.0f;   // per voice
    float expressionGainDb = 0.0f;   // per voice (amplitude component only)
    float userGainDb       = 0.0f;   // per pad
};

inline float sourceGainDb (const VoiceGainTerms& t)      { return t.autoGainDb * clamp01 (t.autoLevelAmount); }
inline float performanceGainDb (const VoiceGainTerms& t) { return t.velocityGainDb + t.expressionGainDb; }
inline float finalVoiceGainDb (const VoiceGainTerms& t)  { return sourceGainDb (t) + performanceGainDb (t) + t.userGainDb; }
inline float finalVoiceGainLinear (const VoiceGainTerms& t) { return std::pow (10.0f, finalVoiceGainDb (t) / 20.0f); }

} // namespace vellum
