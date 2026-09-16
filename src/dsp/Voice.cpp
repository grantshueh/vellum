#include "Voice.h"
#include <cmath>

namespace vellum {

namespace {
constexpr int kSubBlock = 32;
inline float lerpAt (const std::vector<float>& v, double p)
{
    const int i = (int) p;
    if (i < 0 || i + 1 >= (int) v.size()) return (i >= 0 && i < (int) v.size()) ? v[(size_t) i] : 0.0f;
    const float f = (float) (p - i);
    return v[(size_t) i] + f * (v[(size_t) i + 1] - v[(size_t) i]);
}
inline void tiltGains (float tiltDb, float& lo, float& hi)
{
    lo = std::pow (10.0f, -tiltDb / 40.0f);
    hi = std::pow (10.0f,  tiltDb / 40.0f);
}
}

void Voice::prepare (double hostSampleRate)
{
    sr = hostSampleRate;
    active = false;
}

void Voice::start (DrumModelPtr m, const HitRealization& h, uint64_t order)
{
    model = std::move (m);
    hit = h;
    startOrder = order;
    if (! model || ! model->isValid()) { active = false; return; }

    delay = std::max (0, hit.delaySamples);
    pos = 0; releaseGain = 1.0f; releaseStep = 0.0f;
    const float pan = 0.5f * (hit.pan + 1.0f);
    gL = std::cos (pan * 1.5707963f) * hit.gainLin;
    gR = std::sin (pan * 1.5707963f) * hit.gainLin;

    // ---- modal bank
    K = (int) std::min<size_t> (model->modes.size(), (size_t) kMaxModes);
    double longest = 0.0;
    for (int k = 0; k < K; ++k)
    {
        const Mode& md = model->modes[(size_t) k];
        const float amp = md.amp * hit.modeAmpMul[k] * hit.modalGain;
        re[k] = amp * std::cos (md.phase);
        im[k] = amp * std::sin (md.phase);
        att[k] = 0.0f;
        attCoef[k] = 1.0f - std::exp (-1.0f / (md.attackTau * (float) sr));
        baseW[k] = (float) (6.283185307 * md.freq * hit.freqScale * hit.modeFreqMul[k] / sr);
        const double tau = std::max (0.002, (double) md.tau * hit.modeDecayMul[k]);
        decayPerSample[k] = (float) std::exp (-1.0 / (tau * sr));
        longest = std::max (longest, tau * 12.0);      // ~ -104 dB
    }
    subCounter = 0;
    updateRotations();

    // ---- transient
    const double rateBase = model->sampleRate / sr;
    trPos = 0.0;
    trRateBase = rateBase; trRate = trRateBase * hit.freqScale;
    trEnv = 1.0f; trDec = hit.transientTau > 0.0f ? (float) std::exp (-1.0 / ((double) hit.transientTau * sr)) : 1.0f;
    trLP.z = 0.0f; trLP.a = 1.0f - std::exp (-6.2831853f * 1200.0f / (float) sr);
    tiltGains (hit.transientTiltDb, trLo, trHi);
    const double trDur = model->transient.empty() ? 0.0 : (double) model->transient.size() / (trRate * sr);

    // ---- tail
    tailStart = (double) model->tailOffset / rateBase;
    tailPos = 0.0; tailRateBase = rateBase; tailRate = rateBase * hit.freqScale;
    tailB = nullptr;
    if (! model->tailVariants.empty())
    {
        const auto& cand = model->tailVariants[(size_t) (hit.tailVariant % (int) model->tailVariants.size())];
        if (! cand.empty()) tailB = &cand;
    }
    const float regen = tailB ? hit.tailRegen : 0.0f;
    gA = std::cos (regen * 1.5707963f); gB = std::sin (regen * 1.5707963f);
    const double tau = std::max (0.005, (double) model->tailTau);
    const double tauA = tau * std::min (1.0, (double) hit.tailDecayMul);           // original can only be shortened
    const double tauB = tau * std::min ((double) kTailStretch, (double) hit.tailDecayMul);
    envA = envB = 1.0f;
    decA = (float) std::exp (-(1.0 / tauA - 1.0 / tau) / sr);
    decB = (float) std::exp (-(1.0 / tauB - 1.0 / (tau * kTailStretch)) / sr);
    tailLP.z = 0.0f; tailLP.a = trLP.a;
    tiltGains (hit.tailTiltDb, tailLo, tailHi);
    double tailDur = 0.0;
    if (! model->tail.empty()) tailDur = std::max (tailDur, (double) model->tail.size() / (tailRate * sr));
    if (tailB) tailDur = std::max (tailDur, (double) tailB->size() / (tailRate * sr));

    const double total = std::max ({ longest, trDur, tailStart / sr + tailDur }) + 0.01;
    remaining = (long long) (std::min (total, 12.0) * sr) + delay;
    active = remaining > 0;
}

void Voice::updateRotations()
{
    const float t = (float) (pos / sr);
    const float glide = 1.0f + hit.glideAmount * std::exp (-t / std::max (0.001f, hit.glideTau));
    trRate = trRateBase * hit.freqScale * glide;
    tailRate = tailRateBase * hit.freqScale * glide;
    for (int k = 0; k < K; ++k)
    {
        const float w = baseW[k] * glide;
        rotRe[k] = decayPerSample[k] * std::cos (w);
        rotIm[k] = decayPerSample[k] * std::sin (w);
    }
}

void Voice::fastRelease()
{
    if (! active) return;
    releaseStep = 1.0f / std::max (1.0f, 0.003f * (float) sr);
}

void Voice::render (float* L, float* R, int numSamples)
{
    if (! active) return;
    int i = 0;
    if (delay > 0)
    {
        const int skip = std::min (delay, numSamples);
        delay -= skip; i = skip;
        remaining -= skip;
    }
    const auto& tr = model->transient;
    const auto& tailA = model->tail;
    const bool hasTr = ! tr.empty();
    const float resGain = hit.residualGain;

    for (; i < numSamples; ++i)
    {
        if (subCounter == 0) updateRotations();
        if (++subCounter >= kSubBlock) subCounter = 0;

        // modal
        float s = 0.0f;
        for (int k = 0; k < K; ++k)
        {
            att[k] += (1.0f - att[k]) * attCoef[k];
            s += re[k] * att[k];
            const float nr = re[k] * rotRe[k] - im[k] * rotIm[k];
            const float ni = re[k] * rotIm[k] + im[k] * rotRe[k];
            re[k] = nr; im[k] = ni;
        }

        // transient
        if (hasTr && trPos < (double) tr.size())
        {
            const float x = lerpAt (tr, trPos) * hit.transientGain * trEnv;
            const float lo = trLP.process (x);
            s += (lo * trLo + (x - lo) * trHi) * resGain;
            trPos += trRate; trEnv *= trDec;
        }

        // tail
        if ((double) pos >= tailStart)
        {
            float x = 0.0f;
            if (gA > 0.0f && tailPos < (double) tailA.size()) x += gA * envA * lerpAt (tailA, tailPos);
            if (tailB && gB > 0.0f && tailPos < (double) tailB->size()) x += gB * envB * lerpAt (*tailB, tailPos);
            envA *= decA; envB *= decB;
            tailPos += tailRate;
            x *= hit.tailGain;
            const float lo = tailLP.process (x);
            s += (lo * tailLo + (x - lo) * tailHi) * resGain;
        }

        if (releaseStep > 0.0f)
        {
            releaseGain -= releaseStep;
            if (releaseGain <= 0.0f) { active = false; return; }
            s *= releaseGain;
        }

        L[i] += s * gL;
        R[i] += s * gR;
        ++pos;
        if (--remaining <= 0) { active = false; return; }
    }
}

} // namespace vellum
