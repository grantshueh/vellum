#include "Analyzer.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>
#include <random>

namespace vellum {

namespace {

constexpr double kTwoPi = 6.283185307179586;

// ---------------------------------------------------------------------------
// STFT helpers
// ---------------------------------------------------------------------------
struct Spectrogram
{
    int N = 0, hop = 0, bins = 0, numFrames = 0;
    std::vector<float> db;         // numFrames * bins, dB of (|X| * 4 / N)  => dB of sinusoid amplitude
    float at (int m, int b) const { return db[(size_t) m * (size_t) bins + (size_t) b]; }
    double frameCentreTime (int m, double sr) const { return ((double) m * hop + 0.5 * N) / sr; }
};

Spectrogram computeSpectrogram (const float* x, int n, int N, int hop, double /*sr*/)
{
    Spectrogram s;
    s.N = N; s.hop = hop; s.bins = N / 2 + 1;
    s.numFrames = std::max (1, (n + hop - 1) / hop);
    s.db.assign ((size_t) s.numFrames * (size_t) s.bins, -160.0f);

    const int order = (int) std::lround (std::log2 ((double) N));
    juce::dsp::FFT fft (order);
    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos ((float) (kTwoPi * i / N));

    std::vector<float> buf ((size_t) N * 2);
    const float scale = 4.0f / (float) N;
    for (int m = 0; m < s.numFrames; ++m)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        const int start = m * hop - N / 2;          // frames centred on m*hop
        for (int i = 0; i < N; ++i)
        {
            const int idx = start + i;
            if (idx >= 0 && idx < n) buf[(size_t) i] = x[idx] * win[(size_t) i];
        }
        fft.performRealOnlyForwardTransform (buf.data(), true);
        for (int b = 0; b < s.bins; ++b)
        {
            const float re = buf[(size_t) b * 2], im = buf[(size_t) b * 2 + 1];
            const float mag = std::sqrt (re * re + im * im) * scale;
            s.db[(size_t) m * (size_t) s.bins + (size_t) b] = 20.0f * std::log10 (std::max (mag, 1e-8f));
        }
    }
    return s;
}

// ---------------------------------------------------------------------------
// Peak tracking -> mode candidates
// ---------------------------------------------------------------------------
struct Track
{
    std::vector<int>   frames;
    std::vector<float> freqs;
    std::vector<float> dbs;
    float lastFreq = 0.0f;
    int   lastFrame = -1;
    bool  alive = true;
};

struct Candidate
{
    float freq, tau, amp0;   // amp0 linear (normalised domain)
    float energy;
};

std::vector<Candidate> findModeCandidates (const Spectrogram& S, double sr, int n)
{
    const float binHz = (float) (sr / S.N);
    float globalMax = -200.0f;
    for (float v : S.db) globalMax = std::max (globalMax, v);

    std::vector<Track> tracks;
    struct Peak { float f, db; };
    std::vector<Peak> peaks;
    std::vector<float> floorDb ((size_t) S.bins);

    for (int m = 0; m < S.numFrames; ++m)
    {
        // local spectral floor: running mean in dB over +-24 bins (cheap median substitute)
        const int R = 24;
        double acc = 0.0; int cnt = 0;
        for (int b = 0; b < std::min (S.bins, R); ++b) { acc += S.at (m, b); ++cnt; }
        float frameMax = -200.0f;
        for (int b = 0; b < S.bins; ++b)
        {
            const int addB = b + R, remB = b - R - 1;
            if (addB < S.bins) { acc += S.at (m, addB); ++cnt; }
            if (remB >= 0)     { acc -= S.at (m, remB); --cnt; }
            floorDb[(size_t) b] = (float) (acc / std::max (1, cnt));
            frameMax = std::max (frameMax, S.at (m, b));
        }
        if (frameMax < globalMax - 80.0f) { for (auto& t : tracks) if (t.alive && m - t.lastFrame > 2) t.alive = false; continue; }

        peaks.clear();
        for (int b = 2; b < S.bins - 2; ++b)
        {
            const float beta = S.at (m, b);
            if (beta < floorDb[(size_t) b] + 8.0f || beta < frameMax - 60.0f || beta < globalMax - 80.0f) continue;
            const float alpha = S.at (m, b - 1), gamma = S.at (m, b + 1);
            if (! (beta > alpha && beta >= gamma)) continue;
            const float denom = alpha - 2.0f * beta + gamma;
            const float p = (std::fabs (denom) > 1e-6f) ? 0.5f * (alpha - gamma) / denom : 0.0f;
            const float f = ((float) b + p) * binHz;
            const float pdb = beta - 0.25f * (alpha - gamma) * p;
            if (f < 18.0f || f > 0.45f * (float) sr) continue;
            peaks.push_back ({ f, pdb });
        }
        if (peaks.size() > 80)
        {
            std::partial_sort (peaks.begin(), peaks.begin() + 80, peaks.end(), [] (const Peak& a, const Peak& b) { return a.db > b.db; });
            peaks.resize (80);
        }

        // greedy matching to alive tracks
        std::vector<char> used (peaks.size(), 0);
        for (auto& t : tracks)
        {
            if (! t.alive) continue;
            const float tol = std::max (binHz * 1.0f, 0.012f * t.lastFreq);
            int best = -1; float bestD = tol;
            for (size_t i = 0; i < peaks.size(); ++i)
            {
                if (used[i]) continue;
                const float d = std::fabs (peaks[i].f - t.lastFreq);
                if (d < bestD) { bestD = d; best = (int) i; }
            }
            if (best >= 0)
            {
                used[(size_t) best] = 1;
                t.frames.push_back (m); t.freqs.push_back (peaks[(size_t) best].f); t.dbs.push_back (peaks[(size_t) best].db);
                t.lastFreq = peaks[(size_t) best].f; t.lastFrame = m;
            }
            else if (m - t.lastFrame > 2) t.alive = false;
        }
        for (size_t i = 0; i < peaks.size(); ++i)
        {
            if (used[i]) continue;
            Track t; t.frames.push_back (m); t.freqs.push_back (peaks[i].f); t.dbs.push_back (peaks[i].db);
            t.lastFreq = peaks[i].f; t.lastFrame = m;
            tracks.push_back (std::move (t));
        }
    }

    std::vector<Candidate> out;
    for (auto& t : tracks)
    {
        const int len = (int) t.frames.size();
        if (len < 5) continue;
        const int i0 = len >= 8 ? 2 : 1;
        // linear regression of dB vs time
        double st = 0, sy = 0, stt = 0, sty = 0, syy = 0; int cnt = 0;
        for (int i = i0; i < len; ++i)
        {
            const double tt = S.frameCentreTime (t.frames[(size_t) i], sr), y = t.dbs[(size_t) i];
            st += tt; sy += y; stt += tt * tt; sty += tt * y; syy += y * y; ++cnt;
        }
        if (cnt < 3) continue;
        const double varT = stt - st * st / cnt, covTY = sty - st * sy / cnt, varY = syy - sy * sy / cnt;
        if (varT <= 1e-12) continue;
        const double slope = covTY / varT;                 // dB per second
        const double intercept = (sy - slope * st) / cnt;  // dB at t = 0
        const double corr = (varY > 1e-9) ? covTY / std::sqrt (varT * varY) : 0.0;

        float tau;
        if (slope < -1.0)          tau = (float) (-8.685889638 / slope);
        else                        tau = 4.0f;             // effectively sustained within the sample
        if (slope < -1.0 && corr > -0.5 && len < 15) continue;   // noisy, not a clean decaying partial
        tau = std::min (std::max (tau, 0.004f), 8.0f);

        std::vector<float> fs (t.freqs.begin() + i0, t.freqs.end());
        std::nth_element (fs.begin(), fs.begin() + (long) fs.size() / 2, fs.end());
        const float freq = fs[fs.size() / 2];
        const float amp0 = std::pow (10.0f, (float) intercept / 20.0f);
        Candidate c { freq, tau, amp0, amp0 * amp0 * std::min (tau, (float) n / (float) sr) };
        out.push_back (c);
    }
    std::sort (out.begin(), out.end(), [] (const Candidate& a, const Candidate& b) { return a.energy > b.energy; });
    return out;
}

// merge near-duplicates (keep stronger), then keep top maxModes
std::vector<Candidate> mergeCandidates (std::vector<Candidate> c, float binHz, int maxModes)
{
    std::vector<Candidate> kept;
    for (auto& cand : c)
    {
        bool dup = false;
        for (auto& k : kept)
            if (std::fabs (k.freq - cand.freq) < std::max (binHz * 1.0f, 0.015f * k.freq)) { dup = true; break; }
        if (! dup) kept.push_back (cand);
        if ((int) kept.size() >= maxModes) break;
    }
    return kept;
}

// matched-filter score for a decaying sinusoid at (f, tau)
double matchScore (const float* x, int L, double sr, double f, double tau)
{
    const double w = kTwoPi * f / sr;
    const double d = std::exp (-1.0 / (tau * sr));
    std::complex<double> rot (d * std::cos (w), -d * std::sin (w));  // e^{-iw} * decay
    std::complex<double> ph (1.0, 0.0), acc (0.0, 0.0);
    double envE = 0.0;
    for (int i = 0; i < L; ++i)
    {
        acc += (double) x[i] * ph;
        envE += std::norm (ph);
        ph *= rot;
        if ((i & 1023) == 1023) { /* keep magnitude sane */ }
    }
    return std::norm (acc) / std::max (envE, 1e-12);
}

void refineCandidate (Candidate& c, const float* x, int n, double sr, float binHz)
{
    const int L = std::min (n, (int) (sr * std::min (1.0, (double) c.tau * 4.0 + 0.05)));
    if (L < 64) return;
    auto searchF = [&] (double halfRange, int steps)
    {
        double bestF = c.freq, bestS = -1.0;
        for (int i = 0; i < steps; ++i)
        {
            const double f = c.freq + (-halfRange + 2.0 * halfRange * i / (steps - 1));
            if (f < 15.0) continue;
            const double s = matchScore (x, L, sr, f, c.tau);
            if (s > bestS) { bestS = s; bestF = f; }
        }
        c.freq = (float) bestF;
    };
    auto searchTau = [&]
    {
        static const double muls[] = { 0.6, 0.8, 1.0, 1.25, 1.6 };
        double bestT = c.tau, bestS = -1.0;
        for (double m : muls)
        {
            const double t = std::min (8.0, std::max (0.004, (double) c.tau * m));
            const double s = matchScore (x, L, sr, c.freq, t);
            if (s > bestS) { bestS = s; bestT = t; }
        }
        c.tau = (float) bestT;
    };
    searchF (0.5 * binHz, 7);
    searchTau();
    searchF (0.12 * binHz, 5);
}

// ---------------------------------------------------------------------------
// Least-squares fit of complex amplitudes with attack ramps
// ---------------------------------------------------------------------------
float attackTauFor (float freq) { return std::min (0.006f, std::max (0.0003f, 0.25f / std::max (freq, 20.0f))); }

// Cholesky solve of (A + lambda I) c = b, A symmetric positive semi-definite. Returns false on failure.
bool solveSPD (std::vector<double>& A, std::vector<double>& b, int K, double lambda)
{
    for (int i = 0; i < K; ++i) A[(size_t) i * K + i] += lambda;
    // in-place Cholesky
    for (int j = 0; j < K; ++j)
    {
        double s = A[(size_t) j * K + j];
        for (int k = 0; k < j; ++k) s -= A[(size_t) j * K + k] * A[(size_t) j * K + k];
        if (s <= 1e-14) return false;
        const double d = std::sqrt (s);
        A[(size_t) j * K + j] = d;
        for (int i = j + 1; i < K; ++i)
        {
            double t = A[(size_t) i * K + j];
            for (int k = 0; k < j; ++k) t -= A[(size_t) i * K + k] * A[(size_t) j * K + k];
            A[(size_t) i * K + j] = t / d;
        }
    }
    // forward
    for (int i = 0; i < K; ++i)
    {
        double s = b[(size_t) i];
        for (int k = 0; k < i; ++k) s -= A[(size_t) i * K + k] * b[(size_t) k];
        b[(size_t) i] = s / A[(size_t) i * K + i];
    }
    // backward
    for (int i = K - 1; i >= 0; --i)
    {
        double s = b[(size_t) i];
        for (int k = i + 1; k < K; ++k) s -= A[(size_t) k * K + i] * b[(size_t) k];
        b[(size_t) i] = s / A[(size_t) i * K + i];
    }
    return true;
}

void synthesiseModes (const std::vector<Mode>& modes, double sr, float* out, int n)
{
    std::fill (out, out + n, 0.0f);
    for (const auto& m : modes)
    {
        const double w = kTwoPi * m.freq / sr;
        const double d = std::exp (-1.0 / ((double) m.tau * sr));
        const double ca = 1.0 - std::exp (-1.0 / ((double) m.attackTau * sr));
        std::complex<double> rot (d * std::cos (w), d * std::sin (w));
        std::complex<double> ph (std::cos (m.phase), std::sin (m.phase));
        double att = 0.0;
        for (int i = 0; i < n; ++i)
        {
            att += (1.0 - att) * ca;
            out[i] += (float) (m.amp * att * ph.real());
            ph *= rot;
            if (std::norm (ph) < 1e-20) break;
        }
    }
}

std::vector<Mode> fitModes (const std::vector<Candidate>& cands, const float* x, int n, double sr)
{
    const int K = (int) cands.size();
    std::vector<Mode> modes;
    if (K == 0) return modes;

    const int F = std::min (n, (int) (sr * 1.5));
    const int R = 2 * K;
    // basis: rows u_k (cos), v_k (sin), each of length F
    std::vector<float> basis ((size_t) R * (size_t) F);
    for (int k = 0; k < K; ++k)
    {
        const double w = kTwoPi * cands[(size_t) k].freq / sr;
        const double d = std::exp (-1.0 / ((double) cands[(size_t) k].tau * sr));
        const float atk = attackTauFor (cands[(size_t) k].freq);
        const double ca = 1.0 - std::exp (-1.0 / ((double) atk * sr));
        std::complex<double> rot (d * std::cos (w), d * std::sin (w)), ph (1.0, 0.0);
        double att = 0.0;
        float* u = &basis[(size_t) (2 * k) * F];
        float* v = &basis[(size_t) (2 * k + 1) * F];
        for (int i = 0; i < F; ++i)
        {
            att += (1.0 - att) * ca;
            u[i] = (float) (att * ph.real());
            v[i] = (float) (-att * ph.imag());   // x ~ a*env*cos - b*env*sin
            ph *= rot;
        }
    }
    std::vector<double> G ((size_t) R * R, 0.0), rhs ((size_t) R, 0.0);
    for (int i = 0; i < R; ++i)
    {
        const float* bi = &basis[(size_t) i * F];
        double s = 0.0;
        for (int t = 0; t < F; ++t) s += (double) bi[t] * x[t];
        rhs[(size_t) i] = s;
        for (int j = 0; j <= i; ++j)
        {
            const float* bj = &basis[(size_t) j * F];
            double g = 0.0;
            for (int t = 0; t < F; ++t) g += (double) bi[t] * bj[t];
            G[(size_t) i * R + j] = g; G[(size_t) j * R + i] = g;
        }
    }
    double trace = 0.0; for (int i = 0; i < R; ++i) trace += G[(size_t) i * R + i];
    if (! solveSPD (G, rhs, R, 1e-4 * trace / R + 1e-9)) return modes;

    for (int k = 0; k < K; ++k)
    {
        const double a = rhs[(size_t) 2 * k], b = rhs[(size_t) 2 * k + 1];
        Mode m;
        m.freq = cands[(size_t) k].freq; m.tau = cands[(size_t) k].tau;
        m.amp = (float) std::hypot (a, b); m.phase = (float) std::atan2 (b, a);
        m.attackTau = attackTauFor (m.freq);
        if (m.amp > 1e-4f) modes.push_back (m);
    }
    std::sort (modes.begin(), modes.end(), [] (const Mode& p, const Mode& q) { return p.freq < q.freq; });
    return modes;
}

// Residual-driven refinement: for each mode, try nearby (freq, tau), refit its amplitude/phase
// against (x - all other modes) and keep whatever lowers the residual energy. Corrects the
// STFT-induced bias in frequency and decay so the residual loses its tonal content.
void synthesiseOneMode (const Mode& m, double sr, float* out, int n)
{
    const double w = kTwoPi * m.freq / sr, d = std::exp (-1.0 / ((double) m.tau * sr));
    const double ca = 1.0 - std::exp (-1.0 / ((double) m.attackTau * sr));
    std::complex<double> rot (d * std::cos (w), d * std::sin (w)), ph (std::cos (m.phase), std::sin (m.phase));
    double att = 0.0;
    for (int i = 0; i < n; ++i)
    {
        att += (1.0 - att) * ca;
        out[i] = (float) (m.amp * att * ph.real());
        ph *= rot;
        if (std::norm (ph) < 1e-20) { std::fill (out + i + 1, out + n, 0.0f); break; }
    }
}

// 2x2 least squares of a*u - b*v against target, returns residual energy and fills amp/phase
double fitOneMode (Mode& m, const float* target, int n, double sr)
{
    const double w = kTwoPi * m.freq / sr, d = std::exp (-1.0 / ((double) m.tau * sr));
    const double ca = 1.0 - std::exp (-1.0 / ((double) m.attackTau * sr));
    std::complex<double> rot (d * std::cos (w), d * std::sin (w)), ph (1.0, 0.0);
    double att = 0.0, uu = 0, vv = 0, uv = 0, ut = 0, vt = 0, tt = 0;
    const int L = std::min (n, (int) (sr * std::min (2.0, (double) m.tau * 14.0 + 0.02)));
    for (int i = 0; i < L; ++i)
    {
        att += (1.0 - att) * ca;
        const double u = att * ph.real(), v = -att * ph.imag(), t = target[i];
        uu += u * u; vv += v * v; uv += u * v; ut += u * t; vt += v * t; tt += t * t;
        ph *= rot;
    }
    const double det = uu * vv - uv * uv;
    if (det < 1e-18) return tt;
    const double a = (ut * vv - vt * uv) / det, b = (vt * uu - ut * uv) / det;
    m.amp = (float) std::hypot (a, b); m.phase = (float) std::atan2 (b, a);
    // residual energy over L
    return tt - (a * ut + b * vt);
}

void refineModesByResidual (std::vector<Mode>& modes, const float* x, int n, double sr, int passes)
{
    if (modes.empty()) return;
    std::vector<float> synth ((size_t) n), residual (x, x + n), target ((size_t) n), one ((size_t) n);
    synthesiseModes (modes, sr, synth.data(), n);
    for (int i = 0; i < n; ++i) residual[(size_t) i] = x[i] - synth[(size_t) i];

    for (int pass = 0; pass < passes; ++pass)
    {
        const double fStep = pass == 0 ? 0.004 : pass == 1 ? 0.0015 : 0.0005;
        const double tMul = pass == 0 ? 1.35 : pass == 1 ? 1.15 : 1.06;
        for (auto& m : modes)
        {
            synthesiseOneMode (m, sr, one.data(), n);
            for (int i = 0; i < n; ++i) target[(size_t) i] = residual[(size_t) i] + one[(size_t) i];
            Mode best = m;
            double bestE = fitOneMode (best, target.data(), n, sr);
            const Mode base = m;
            const double fCands[] = { base.freq * (1.0 - fStep), base.freq * (1.0 + fStep) };
            const double tCands[] = { base.tau / tMul, base.tau * tMul };
            for (double f : fCands) { Mode c = base; c.freq = (float) f; const double e = fitOneMode (c, target.data(), n, sr); if (e < bestE) { bestE = e; best = c; } }
            for (double t : tCands) { Mode c = base; c.tau = (float) std::min (8.0, std::max (0.002, t)); const double e = fitOneMode (c, target.data(), n, sr); if (e < bestE) { bestE = e; best = c; } }
            m = best;
            synthesiseOneMode (m, sr, one.data(), n);
            for (int i = 0; i < n; ++i) residual[(size_t) i] = target[(size_t) i] - one[(size_t) i];
        }
    }
    modes.erase (std::remove_if (modes.begin(), modes.end(), [] (const Mode& m) { return m.amp < 1e-4f; }), modes.end());
    std::sort (modes.begin(), modes.end(), [] (const Mode& p, const Mode& q) { return p.freq < q.freq; });
}

// ---------------------------------------------------------------------------
// Noise-band model + stochastic rendering
// ---------------------------------------------------------------------------
constexpr int kNoiseN = 1024, kNoiseHop = 256, kNumBands = 24;

std::vector<float> bandEdges (double sr)
{
    std::vector<float> e ((size_t) kNumBands + 1);
    const double lo = 40.0, hi = sr * 0.5;
    for (int j = 0; j <= kNumBands; ++j) e[(size_t) j] = (float) (lo * std::pow (hi / lo, (double) j / kNumBands));
    return e;
}

std::vector<NoiseBand> fitNoiseBands (const float* tail, int len, double sr, float tauMax, float& globalTau, float& flatness)
{
    flatness = 1.0f;
    std::vector<NoiseBand> bands ((size_t) kNumBands);
    const auto edges = bandEdges (sr);
    for (int j = 0; j < kNumBands; ++j) bands[(size_t) j].centreHz = std::sqrt (edges[(size_t) j] * edges[(size_t) j + 1]);
    globalTau = 0.1f;
    if (len < kNoiseHop * 3) { for (auto& b : bands) b.ampDb0 = -120.0f; return bands; }

    Spectrogram S = computeSpectrogram (tail, len, kNoiseN, kNoiseHop, sr);
    const float binHz = (float) (sr / kNoiseN);
    // band power per frame (dB)
    std::vector<std::vector<float>> bp ((size_t) kNumBands, std::vector<float> ((size_t) S.numFrames, -160.0f));
    for (int m = 0; m < S.numFrames; ++m)
        for (int j = 0; j < kNumBands; ++j)
        {
            const int b0 = std::max (1, (int) (edges[(size_t) j] / binHz)), b1 = std::min (S.bins - 1, std::max (b0 + 1, (int) (edges[(size_t) j + 1] / binHz)));
            double p = 0.0; int c = 0;
            for (int b = b0; b < b1; ++b) { const float d = S.at (m, b); p += std::pow (10.0, d / 10.0); ++c; }
            bp[(size_t) j][(size_t) m] = (float) (10.0 * std::log10 (std::max (p / std::max (1, c), 1e-16)));
        }
    // broadband envelope tau as fallback
    auto regress = [&] (const std::vector<float>& y, float& tau, float& a0) -> bool
    {
        float mx = -200.0f; for (float v : y) mx = std::max (mx, v);
        double st = 0, sy = 0, stt = 0, sty = 0; int cnt = 0;
        for (int m = 0; m < (int) y.size(); ++m)
        {
            if (y[(size_t) m] < mx - 50.0f) continue;
            const double t = (double) m * kNoiseHop / sr;   // frames centred at m*hop -> time from tail start
            st += t; sy += y[(size_t) m]; stt += t * t; sty += t * y[(size_t) m]; ++cnt;
        }
        if (cnt < 3) return false;
        const double varT = stt - st * st / cnt;
        if (varT < 1e-12) return false;
        const double slope = (sty - st * sy / cnt) / varT;   // dB(power) per second
        const double intercept = (sy - slope * st) / cnt;
        if (slope >= -0.5) { tau = tauMax; a0 = (float) intercept; return true; }
        tau = (float) std::min ((double) tauMax, std::max (0.005, -8.685889638 / slope));   // power dB: 10log10(e^{-2t/tau}) = -8.686 t/tau
        a0 = (float) intercept;
        return true;
    };
    std::vector<float> broadband ((size_t) S.numFrames);
    for (int m = 0; m < S.numFrames; ++m)
    {
        double p = 0.0; for (int j = 0; j < kNumBands; ++j) p += std::pow (10.0, bp[(size_t) j][(size_t) m] / 10.0);
        broadband[(size_t) m] = (float) (10.0 * std::log10 (std::max (p, 1e-16)));
    }
    float bbTau = 0.1f, bbA0 = 0.0f;
    if (! regress (broadband, bbTau, bbA0)) bbTau = std::min (0.1f, tauMax);

    // within-band spectral flatness of the first frames, energy weighted: noise ~0.4-0.6, a tonal
    // residual (808 glide, unmodelled ring) < 0.2. Measured per band so spectral tilt does not count.
    {
        double wsumF = 0.0, fsum = 0.0;
        for (int m = 0; m < std::min (S.numFrames, 3); ++m)
            for (int j = 0; j < kNumBands; ++j)
            {
                const int b0 = std::max (1, (int) (edges[(size_t) j] / binHz));
                const int b1 = std::min (S.bins - 1, std::max (b0 + 6, (int) (edges[(size_t) j + 1] / binHz)));
                if (b1 - b0 < 4) continue;
                double logSum = 0.0, linSum = 0.0; int cnt = 0;
                for (int b = b0; b < b1; ++b) { const double p = std::pow (10.0, S.at (m, b) / 10.0) + 1e-16; logSum += std::log (p); linSum += p; ++cnt; }
                const double am = linSum / cnt, gm = std::exp (logSum / cnt);
                wsumF += am; fsum += am * (gm / am);
            }
        if (wsumF > 0.0) flatness = (float) (fsum / wsumF);
    }

    double wsum = 0.0, tsum = 0.0;
    for (int j = 0; j < kNumBands; ++j)
    {
        auto& b = bands[(size_t) j];
        float tau, a0;
        if (regress (bp[(size_t) j], tau, a0)) { b.tau = tau; b.ampDb0 = a0; }
        else { b.tau = std::min (bbTau, tauMax); b.ampDb0 = bp[(size_t) j][0]; }
        const double w = std::pow (10.0, b.ampDb0 / 10.0) * b.tau;
        wsum += w; tsum += w * b.tau;
    }
    globalTau = (float) (wsum > 0 ? tsum / wsum : bbTau);
    globalTau = std::min (tauMax, std::max (0.005f, globalTau));
    // a band cannot ring absurdly longer than the tail as a whole (noise-floor flattening)
    for (auto& b : bands) b.tau = std::min (b.tau, std::max (0.01f, globalTau * 3.0f));
    return bands;
}

// render stochastic noise from the band model, starting at `startTime` seconds into the tail
std::vector<float> renderNoise (const std::vector<NoiseBand>& bands, double sr, float stretch, double startTime,
                                int maxSamples, unsigned seed)
{
    std::vector<float> out;
    if (bands.empty() || maxSamples <= 0) return out;
    const int N = kNoiseN, hop = N / 4, bins = N / 2 + 1;
    juce::dsp::FFT fft (10);
    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos ((float) (kTwoPi * i / N));
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> uni (0.0f, (float) kTwoPi);
    const float binHz = (float) (sr / N);

    std::vector<float> buf ((size_t) N * 2), bandDb ((size_t) bands.size());
    out.assign ((size_t) maxSamples + N, 0.0f);
    int written = 0;
    float peak0 = -300.0f; for (const auto& b : bands) peak0 = std::max (peak0, b.ampDb0);
    for (int m = 0; m * hop < maxSamples; ++m)
    {
        const double t = startTime + (double) m * hop / sr;
        float mx = -300.0f;
        for (size_t j = 0; j < bands.size(); ++j)
        {
            bandDb[j] = bands[j].ampDb0 - (float) (8.685889638 * t / ((double) bands[j].tau * stretch));
            mx = std::max (mx, bandDb[j]);
        }
        if (mx < peak0 - 90.0f) break;
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int b = 1; b < bins - 1; ++b)
        {
            const float f = (float) b * binHz;
            // interpolate band dB (in log-frequency)
            float db;
            if (f <= bands.front().centreHz) db = bandDb.front();
            else if (f >= bands.back().centreHz) db = bandDb.back();
            else
            {
                size_t j = 0; while (j + 1 < bands.size() && bands[j + 1].centreHz < f) ++j;
                const float a = std::log (f / bands[j].centreHz) / std::log (bands[j + 1].centreHz / bands[j].centreHz);
                db = bandDb[j] + a * (bandDb[j + 1] - bandDb[j]);
            }
            const float mag = std::pow (10.0f, db / 20.0f);
            const float phi = uni (rng);
            buf[(size_t) b * 2] = mag * std::cos (phi);
            buf[(size_t) b * 2 + 1] = mag * std::sin (phi);
        }
        fft.performRealOnlyInverseTransform (buf.data());
        const int start = m * hop;
        for (int i = 0; i < N && start + i < (int) out.size(); ++i) out[(size_t) (start + i)] += buf[(size_t) i] * win[(size_t) i];
        written = std::min ((int) out.size(), start + N);
    }
    out.resize ((size_t) std::max (0, std::min (written, maxSamples)));
    return out;
}

float rmsOf (const float* x, int n)
{
    if (n <= 0) return 0.0f;
    double s = 0.0; for (int i = 0; i < n; ++i) s += (double) x[i] * x[i];
    return (float) std::sqrt (s / n);
}

// spectral centroid (Hz) of the first `n` samples via the difference-energy ratio
// (cheap, monotonic in brightness, good enough to steer a tilt correction)
float roughCentroid (const float* x, int n, double sr)
{
    if (n < 4) return 1000.0f;
    double e = 0.0, de = 0.0;
    for (int i = 1; i < n; ++i) { const double d = x[i] - x[i - 1]; e += (double) x[i] * x[i]; de += d * d; }
    if (e <= 0.0) return 1000.0f;
    return (float) (sr / kTwoPi * std::sqrt (de / e));
}

// first-order tilt around 1 kHz, in place
void applyTilt (std::vector<float>& x, float tiltDb, double sr)
{
    const float lo = std::pow (10.0f, -tiltDb / 40.0f), hi = std::pow (10.0f, tiltDb / 40.0f);
    const float a = 1.0f - std::exp (-6.2831853f * 1200.0f / (float) sr);
    float z = 0.0f;
    for (auto& v : x) { z += a * (v - z); v = z * lo + (v - z) * hi; }
}

// match the brightness of a stochastic render to the reference over the first `len` samples
void matchBrightness (std::vector<float>& variant, const float* ref, int len, double sr)
{
    if (variant.empty() || len < 64) return;
    const float cRef = roughCentroid (ref, len, sr);
    const std::vector<float> pristine = variant;
    float tilt = 0.0f;
    for (int it = 0; it < 4; ++it)
    {
        const float cVar = roughCentroid (variant.data(), std::min (len, (int) variant.size()), sr);
        if (cVar <= 0.0f || cRef <= 0.0f) return;
        const float step = 8.0f * std::log2 (cRef / cVar);
        if (std::fabs (step) < 0.15f) break;
        tilt = std::min (12.0f, std::max (-12.0f, tilt + step));
        variant = pristine;
        applyTilt (variant, tilt, sr);
    }
}

} // namespace

// ---------------------------------------------------------------------------
void computePreview (const float* x, int n, std::vector<float>& mn, std::vector<float>& mx, int points)
{
    mn.assign ((size_t) points, 0.0f); mx.assign ((size_t) points, 0.0f);
    if (n <= 0) return;
    for (int p = 0; p < points; ++p)
    {
        const int a = (int) ((long long) p * n / points), b = std::max (a + 1, (int) ((long long) (p + 1) * n / points));
        float lo = 0.0f, hi = 0.0f;
        for (int i = a; i < b && i < n; ++i) { lo = std::min (lo, x[i]); hi = std::max (hi, x[i]); }
        mn[(size_t) p] = lo; mx[(size_t) p] = hi;
    }
}

std::shared_ptr<DrumModel> analyzeDrum (const float* monoIn, int numIn, double sr, const std::string& name,
                                        const std::string& sourcePath, const AnalysisOptions& opt, DrumCategory forcedCategory)
{
    auto model = std::make_shared<DrumModel>();
    model->sampleRate = sr; model->name = name; model->sourcePath = sourcePath;
    if (numIn <= 0 || monoIn == nullptr) return model;

    // ---- trim & leveling analysis (on the untouched source)
    LevelAnalysis la = analyzeLevel (monoIn, numIn, sr, -60.0f);
    if (la.end <= la.start) return model;
    int start = std::max (0, la.start - (int) (0.002 * sr));
    int end = std::min (numIn, la.end + (int) (0.01 * sr));
    end = std::min (end, start + (int) (opt.maxSeconds * sr));
    const int n = end - start;
    if (n < 32) return model;

    model->source.assign (monoIn + start, monoIn + end);
    model->numSamples = n;
    model->category = forcedCategory == DrumCategory::Count ? classifyFilename (name) : forcedCategory;
    model->peakDb = la.peakDb; model->rmsDb = la.rmsDb;
    model->autoGainDb = computeAutoGainDb (la, model->category);
    model->truncated = opt.truncated;
    computePreview (model->source.data(), n, model->previewMin, model->previewMax, 256);

    // ---- normalised working copy
    float peak = 0.0f; for (float v : model->source) peak = std::max (peak, std::fabs (v));
    const float norm = peak > 0 ? 1.0f / peak : 1.0f;
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = model->source[(size_t) i] * norm;

    // ---- modal analysis
    const int N = 2048, hop = 256;
    Spectrogram S = computeSpectrogram (x.data(), n, N, hop, sr);
    const float binHz = (float) (sr / N);
    auto cands = findModeCandidates (S, sr, n);
    cands = mergeCandidates (std::move (cands), binHz, opt.maxModes);
    for (auto& c : cands) refineCandidate (c, x.data(), n, sr, binHz);
    cands = mergeCandidates (std::move (cands), binHz, opt.maxModes);   // refinement may have converged pairs
    model->modes = fitModes (cands, x.data(), n, sr);

    std::vector<float> modal ((size_t) n), residual ((size_t) n);
    auto computeResidual = [&] (double& ex, double& er)
    {
        synthesiseModes (model->modes, sr, modal.data(), n);
        ex = 0.0; er = 0.0;
        for (int i = 0; i < n; ++i)
        {
            residual[(size_t) i] = x[(size_t) i] - modal[(size_t) i];
            ex += (double) x[(size_t) i] * x[(size_t) i]; er += (double) residual[(size_t) i] * residual[(size_t) i];
        }
    };
    double ex = 0.0, er = 0.0;
    computeResidual (ex, er);

    // second pass: partials the first pass missed show up as peaks in the residual
    if ((int) model->modes.size() < opt.maxModes && er > 0.05 * ex)
    {
        Spectrogram S2 = computeSpectrogram (residual.data(), n, N, hop, sr);
        auto more = findModeCandidates (S2, sr, n);
        std::vector<Candidate> combined;
        for (const auto& m : model->modes) combined.push_back ({ m.freq, m.tau, m.amp, m.amp * m.amp * m.tau });
        for (auto& c : more)
        {
            bool dup = false;
            for (const auto& k : combined) if (std::fabs (k.freq - c.freq) < std::max (binHz * 0.8f, 0.01f * k.freq)) { dup = true; break; }
            if (dup) continue;
            refineCandidate (c, residual.data(), n, sr, binHz);
            combined.push_back (c);
            if ((int) combined.size() >= opt.maxModes) break;
        }
        if (combined.size() > model->modes.size())
        {
            auto refit = fitModes (combined, x.data(), n, sr);
            const auto saved = model->modes;
            model->modes = refit;
            double ex2, er2;
            computeResidual (ex2, er2);
            if (er2 > er) { model->modes = saved; computeResidual (ex, er); }   // keep the better fit
            else er = er2;
        }
    }
    refineModesByResidual (model->modes, x.data(), n, sr, 3);
    computeResidual (ex, er);
    if (er > ex * 1.02)   // fit diverged (should not happen) -> fall back to pure residual playback
    {
        model->modes.clear(); residual = x; er = ex;
    }
    model->modalEnergyRatio = (float) std::max (0.0, std::min (1.0, 1.0 - er / std::max (ex, 1e-12)));
    model->lowestModeHz = model->modes.empty() ? 0.0f : model->modes.front().freq;

    // ---- residual split: transient head / stochastic tail
    const int blk = std::max (8, (int) (0.001 * sr));
    const int nb = (n + blk - 1) / blk;
    std::vector<float> env ((size_t) nb, 0.0f);
    for (int b = 0; b < nb; ++b) env[(size_t) b] = rmsOf (residual.data() + b * blk, std::min (blk, n - b * blk));
    int pk = 0; for (int b = 1; b < nb; ++b) if (env[(size_t) b] > env[(size_t) pk]) pk = b;
    int boundary = pk;
    while (boundary < nb && env[(size_t) boundary] > env[(size_t) pk] * 0.25f) ++boundary;   // -12 dB
    int bSamples = boundary * blk;
    bSamples = std::min (std::max (bSamples, (int) (0.004 * sr)), (int) (0.040 * sr));
    bSamples = std::min (bSamples, n);
    const int fade = std::min ((int) (0.005 * sr), std::max (1, n - bSamples));

    model->transient.assign (residual.begin(), residual.begin() + std::min (n, bSamples + fade));
    for (int i = 0; i < fade && bSamples + i < (int) model->transient.size(); ++i)
    {
        const float g = 0.5f + 0.5f * std::cos ((float) (3.14159265 * (i + 1) / (fade + 1)));
        model->transient[(size_t) (bSamples + i)] *= g;
    }
    model->tailOffset = bSamples;
    std::vector<float> tail (residual.begin() + bSamples, residual.end());
    for (int i = 0; i < fade && i < (int) tail.size(); ++i)
        tail[(size_t) i] *= 0.5f - 0.5f * std::cos ((float) (3.14159265 * (i + 1) / (fade + 1)));
    const int tailLen = (int) tail.size();
    model->tailOriginalLength = tailLen;

    // ---- is the tail cut off by the next hit (loop slice / hot sample end)?
    bool needExtension = opt.truncated;
    if (tailLen > (int) (0.03 * sr))
    {
        const float endRms = rmsOf (tail.data() + tailLen - (int) (0.02 * sr), (int) (0.02 * sr));
        float pkRms = 0.0f;
        for (int i = 0; i + blk <= tailLen; i += blk) pkRms = std::max (pkRms, rmsOf (tail.data() + i, blk));
        if (pkRms > 0 && endRms > pkRms * 0.0158f) needExtension = true;    // still > -36 dB at the end: cut off
    }

    // ---- noise-band model. A tail that ended naturally (trimmed at -60 dB) cannot decay slower
    // than its own length allows; a truncated tail keeps the regression estimate.
    float tailTau = 0.1f, flatness = 1.0f;
    const bool longRinger = model->category == DrumCategory::Cymbal || model->category == DrumCategory::Ride
                         || model->category == DrumCategory::OpenHat || model->category == DrumCategory::FX || model->category == DrumCategory::Unknown;
    const float tauMax = needExtension ? (longRinger ? 4.0f : 1.2f) : std::max (0.01f, (float) tailLen / (float) sr / 3.0f);
    model->bands = fitNoiseBands (tail.data(), tailLen, sr, tauMax, tailTau, flatness);
    model->tailTau = tailTau;
    model->tailStochastic = flatness > 0.2f;
    model->tailFlatness = flatness;
    if (! model->tailStochastic) needExtension = false;   // cannot extend a tonal residual with noise

    // ---- extend a truncated tail with a stochastic continuation
    if (needExtension && tailLen > (int) (0.02 * sr))
    {
        const int xf = (int) (0.010 * sr);
        const double t0 = (double) (tailLen - xf) / sr;
        auto ext = renderNoise (model->bands, sr, 1.0f, t0, (int) (3.0 * sr), (unsigned) opt.variantSeed * 7919u + 17u);
        matchBrightness (ext, tail.data() + std::max (0, tailLen - (int) (0.03 * sr)), std::min (tailLen, (int) (0.03 * sr)), sr);
        const int cal = std::min (xf, (int) ext.size());
        const float target = rmsOf (tail.data() + tailLen - xf, xf), got = rmsOf (ext.data(), cal);
        if (got > 1e-9f && target > 0)
        {
            const float g = target / got;
            for (auto& v : ext) v *= g;
            std::vector<float> merged (tail.begin(), tail.end() - xf);
            for (int i = 0; i < (int) ext.size(); ++i)
            {
                float v = ext[(size_t) i];
                if (i < xf)
                {
                    const float a = (float) (i + 1) / (xf + 1);
                    v = v * std::sqrt (a) + tail[(size_t) (tailLen - xf + i)] * std::sqrt (1.0f - a);
                }
                merged.push_back (v);
            }
            tail = std::move (merged);
        }
    }
    model->tail = tail;

    // ---- stochastic variants (decays stretched so the voice can shorten them per hit)
    if (model->tailStochastic && tailLen > (int) (0.005 * sr))
    {
        const int calLen = std::min (tailLen, (int) (0.05 * sr));
        const float target = rmsOf (tail.data(), calLen);
        for (int v = 0; v < kNumTailVariants; ++v)
        {
            auto var = renderNoise (model->bands, sr, kTailStretch, 0.0, (int) (4.0 * sr), (unsigned) (opt.variantSeed + 101 * v));
            matchBrightness (var, tail.data(), calLen, sr);
            const float got = rmsOf (var.data(), std::min (calLen, (int) var.size()));
            if (got > 1e-9f && target > 0) { const float g = target / got; for (auto& s : var) s *= g; }
            else var.clear();
            model->tailVariants.push_back (std::move (var));
        }
    }

    // ---- descriptors
    {
        double num = 0.0, den = 0.0;
        const int frames = std::min (S.numFrames, std::max (1, (int) (0.1 * sr / hop)));
        for (int m = 0; m < frames; ++m)
            for (int b = 1; b < S.bins; ++b) { const double p = std::pow (10.0, S.at (m, b) / 10.0); num += p * b * binHz; den += p; }
        model->spectralCentroidHz = den > 0 ? (float) (num / den) : 0.0f;
        std::vector<float> fullEnv ((size_t) nb);
        for (int b = 0; b < nb; ++b) fullEnv[(size_t) b] = rmsOf (x.data() + b * blk, std::min (blk, n - b * blk));
        float epk = 0.0f; for (float v : fullEnv) epk = std::max (epk, v);
        int last = nb - 1; while (last > 0 && fullEnv[(size_t) last] < epk * 0.001f) --last;
        model->durationSec = (float) ((last + 1) * blk / sr);
    }

    // ---- undo normalisation so the model reproduces the source at unity
    for (auto& m : model->modes) m.amp *= peak;
    for (auto& v : model->transient) v *= peak;
    for (auto& v : model->tail) v *= peak;
    for (auto& var : model->tailVariants) for (auto& v : var) v *= peak;
    return model;
}

} // namespace vellum
