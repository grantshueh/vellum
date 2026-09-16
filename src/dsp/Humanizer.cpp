#include "Humanizer.h"
#include <algorithm>
#include <cmath>

namespace vellum {

void Humanizer::reset()
{
    for (auto& p : pads) p = PadState{};
}

static inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }

HitRealization Humanizer::realize (int pad, float vel01, float expr01, const DrumModel& model,
                                   const HumanizeSettings& s, float userGainDb, double now)
{
    HitRealization h;
    h.pad = pad;
    PadState& ps = pads[std::max (0, std::min (kNumPads - 1, pad))];
    const float inst = s.instability, sens = s.sensitivity;

    // ---- velocity & expression
    const float v = clamp01 (vel01 + N (s.velSpread * inst * 0.12f));
    const float e = clamp01 (expr01);
    h.velocity = v; h.expression = e;
    const float vd = v - 0.7f;            // deviation from a "normal" hit
    const float ed = e - 1.0f;            // expression deviation from full (<= 0)

    // ---- gain architecture: source baseline, performance, mixing (never renormalised)
    VoiceGainTerms gt;
    gt.autoGainDb = model.autoGainDb;
    gt.autoLevelAmount = s.autoLevel;
    gt.velocityGainDb = velocityGainDb (v, s.velCurve, s.velBoost);
    gt.expressionGainDb = expressionGainDb (e) * clamp01 (s.exprAmp);
    gt.userGainDb = userGainDb;
    h.gainLin = finalVoiceGainLinear (gt);

    // ---- strike position: random walk inside the head (unit disc)
    const float sigmaPos = s.posSpread * inst * 0.35f;
    ps.posX = ps.posX * s.posDrift + N (sigmaPos);
    ps.posY = ps.posY * s.posDrift + N (sigmaPos);
    {
        const float dev = std::sqrt (ps.posX * ps.posX + ps.posY * ps.posY);
        if (dev > 0.6f) { ps.posX *= 0.6f / dev; ps.posY *= 0.6f / dev; }
    }
    h.posX = ps.posX; h.posY = ps.posY;
    constexpr float r0 = 0.3f;            // assumed radius of the original strike
    const float rad = std::min (0.95f, std::sqrt ((r0 + ps.posX) * (r0 + ps.posX) + ps.posY * ps.posY));
    const float dr = rad - r0;            // >0: towards the rim, <0: towards the centre (0 at the original spot)

    // ---- tension drift (slow walk) + per-hit tuning noise
    const double dt = ps.lastHit >= 0.0 ? std::max (0.0, now - ps.lastHit) : 1.0;
    ps.lastHit = now;
    const float memory = (float) std::exp (-dt / 20.0);                  // walk forgets over ~20 s
    ps.tuneWalkCents = clampf (ps.tuneWalkCents * memory + N (s.tuneWalk * inst * 2.5f), -20.0f, 20.0f);
    const float cents = ps.tuneWalkCents + N (s.tuneSpread * inst * 12.0f);
    h.tuneCents = cents;
    h.freqScale = std::pow (2.0f, cents / 1200.0f);

    // ---- pitch glide (membrane tension modulation: harder hit -> more initial pitch rise)
    h.glideTau = 0.01f + 0.07f * s.glideTime;
    h.glideAmount = clampf (s.glideBase * 0.04f + s.velGlide * sens * std::max (0.0f, vd + 0.3f) * 0.06f
                            + s.exprPitch * ed * -0.04f, 0.0f, 0.15f);

    // ---- damping
    const float dampSigma = s.dampSpread * inst * 0.22f;
    const float coh = clamp01 (s.dampCoherence);
    const float shared = std::exp (N (dampSigma * (1.0f - 0.5f * coh)));
    const float velDecay = 1.0f + s.velDecay * sens * vd * 0.6f;
    const float exprDecay = std::pow (2.0f, s.exprDecay * ed);
    h.decayMul = clampf (shared * s.decayScale * velDecay * exprDecay, 0.2f, 4.0f);

    // ---- brightness: spectral tilt of the modal part (dB per octave)
    const float bright = s.velBright * sens * vd * 1.2f + s.exprBright * ed;
    h.brightness = bright;

    // ---- per-mode perturbations
    const int K = (int) std::min<size_t> (model.modes.size(), (size_t) kMaxModes);
    const float fLow = std::max (20.0f, model.lowestModeHz);
    for (int k = 0; k < kMaxModes; ++k) { h.modeAmpMul[k] = 1.0f; h.modeFreqMul[k] = 1.0f; h.modeDecayMul[k] = h.decayMul; }
    for (int k = 0; k < K; ++k)
    {
        const float rel = K > 1 ? (float) k / (float) (K - 1) : 0.0f;
        const float octaves = std::log2 (std::max (model.modes[(size_t) k].freq, fLow) / fLow);
        float amp = std::pow (2.0f, bright * octaves * 0.5f);             // +-3 dB/oct at |bright| = 1
        // strike position: rim excites high modes, centre favours the fundamental; nodal-line dips
        const float q = 0.6f * std::sqrt ((float) (k + 1)) + 0.3f;             // radial order grows ~sqrt(mode index)
        const float nodal = std::cos (3.14159265f * q * rad) - std::cos (3.14159265f * q * r0);
        const float w = 1.0f + s.posModes * (2.0f * dr * (rel - 0.35f) + 0.35f * nodal);
        amp *= clampf (w, 0.15f, 3.0f);
        h.modeAmpMul[k] = amp;
        h.modeFreqMul[k] = 1.0f + N (s.inharmJitter * inst * 0.0035f * (0.5f + rel));
        h.modeDecayMul[k] = clampf (h.decayMul * std::exp (N (dampSigma * coh)), 0.15f, 5.0f);
    }

    // ---- transient (stick / beater contact)
    {
        // stick contact / rim proximity shorten the transient via an envelope (no resampling, so the
        // residual stays phase-coherent with the modal part)
        const float shorten = clampf (std::exp (N (s.attackSpread * inst * 0.35f)) * (1.0f - s.posTransient * dr * 0.6f), 0.35f, 1.0f);
        h.transientTau = shorten < 0.98f ? 0.012f * shorten / std::max (0.02f, 1.0f - shorten) : 0.0f;
    }
    h.transientGain = clampf ((1.0f + s.velSnap * sens * vd * 1.2f) * (1.0f + s.exprTransient * ed), 0.1f, 3.0f);
    h.transientTiltDb = N (s.attackTiltSpread * inst * 5.0f) + s.posTransient * dr * 6.0f + s.velBright * sens * vd * 6.0f;

    // ---- stochastic tail (snare wires, shell noise)
    h.tailVariant = (int) (rng() % (unsigned) kNumTailVariants);
    h.tailRegen = clamp01 (s.noiseRegen);
    h.tailGain = clampf (1.0f + s.velNoise * sens * vd * 1.2f, 0.05f, 3.0f);
    h.tailDecayMul = clampf (h.decayMul * std::exp (N (s.noiseDecaySpread * inst * 0.25f)), 0.3f, kTailStretch);
    h.tailTiltDb = N (s.noiseTiltSpread * inst * 5.0f) + bright * 4.0f;

    // ---- tone balance
    h.modalGain = s.modalTone >= 0.0f ? 1.0f : 1.0f + s.modalTone;
    h.residualGain = s.modalTone <= 0.0f ? 1.0f : 1.0f - s.modalTone;

    // ---- timing (only non-negative offsets are realisable in real time)
    const float sigmaMs = s.timeSpread * inst * 12.0f;
    const float ms = std::max (0.0f, sigmaMs + s.timeBias * 20.0f + N (sigmaMs));
    h.timingMs = ms;
    h.delaySamples = (int) (ms * 0.001 * sr);

    // ---- placement
    h.pan = clampf (ps.posX * 0.35f * (0.3f + s.space), -1.0f, 1.0f);
    return h;
}

} // namespace vellum
