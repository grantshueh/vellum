#include "Slicer.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <random>

namespace vellum {

namespace {

float rms (const float* x, int n) { if (n <= 0) return 0.0f; double s = 0; for (int i = 0; i < n; ++i) s += (double) x[i] * x[i]; return (float) std::sqrt (s / n); }

struct Features { float centroid, low, high, decay, energyDb; };

Features featuresOf (const float* x, int n, double sr, juce::dsp::FFT& fft, int N)
{
    Features f {};
    const int L = std::min (n, N);
    std::vector<float> buf ((size_t) N * 2, 0.0f);
    for (int i = 0; i < L; ++i) buf[(size_t) i] = x[i] * (0.5f - 0.5f * std::cos (6.2831853f * i / (float) N));
    fft.performRealOnlyForwardTransform (buf.data(), true);
    double num = 0, den = 0, low = 0, high = 0;
    const double binHz = sr / N;
    for (int b = 1; b < N / 2; ++b)
    {
        const double p = (double) buf[(size_t) b * 2] * buf[(size_t) b * 2] + (double) buf[(size_t) b * 2 + 1] * buf[(size_t) b * 2 + 1];
        const double hz = b * binHz;
        num += p * hz; den += p;
        if (hz < 180.0) low += p;
        if (hz > 5000.0) high += p;
    }
    f.centroid = den > 0 ? (float) (num / den) : 1000.0f;
    f.low = den > 0 ? (float) (low / den) : 0.0f;
    f.high = den > 0 ? (float) (high / den) : 0.0f;
    // decay: time for 2 ms RMS envelope to fall 20 dB below its peak
    const int blk = std::max (8, (int) (0.002 * sr));
    float pk = 0.0f; std::vector<float> env;
    for (int i = 0; i + blk <= n; i += blk) { env.push_back (rms (x + i, blk)); pk = std::max (pk, env.back()); }
    int idx = 0; while (idx < (int) env.size() && env[(size_t) idx] < pk) ++idx;
    while (idx < (int) env.size() && env[(size_t) idx] > pk * 0.1f) ++idx;
    f.decay = (float) (idx * blk / sr);
    f.energyDb = 20.0f * std::log10 (std::max (pk, 1e-6f));
    return f;
}

} // namespace

std::vector<int> detectOnsets (const float* x, int n, double sr, float sensitivity)
{
    std::vector<int> onsets;
    const int N = 1024, hop = 256;
    if (n < N * 2) return onsets;
    juce::dsp::FFT fft (10);
    const int frames = (n - N) / hop + 1, bins = N / 2 + 1;
    std::vector<float> prev ((size_t) bins, 0.0f), cur ((size_t) bins), buf ((size_t) N * 2), flux ((size_t) frames, 0.0f);
    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * i / (float) N);

    for (int m = 0; m < frames; ++m)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = x[m * hop + i] * win[(size_t) i];
        fft.performRealOnlyForwardTransform (buf.data(), true);
        float f = 0.0f;
        for (int b = 1; b < bins; ++b)
        {
            const float mag = std::sqrt (buf[(size_t) b * 2] * buf[(size_t) b * 2] + buf[(size_t) b * 2 + 1] * buf[(size_t) b * 2 + 1]);
            cur[(size_t) b] = std::log1p (mag * 20.0f);
            // superflux-style: compare with local max of previous frame (+-1 bin) to suppress vibrato
            const float pm = std::max ({ prev[(size_t) b], prev[(size_t) std::max (1, b - 1)], prev[(size_t) std::min (bins - 1, b + 1)] });
            f += std::max (0.0f, cur[(size_t) b] - pm);
        }
        flux[(size_t) m] = f;
        std::swap (prev, cur);
    }
    // adaptive threshold
    float mean = 0.0f; for (float v : flux) mean += v; mean /= (float) std::max (1, frames);
    const float k = 1.0f + (1.0f - sensitivity) * 2.5f;   // 1.0 (sensitive) .. 3.5 (strict)
    const int R = 8, minGap = (int) (0.045 * sr / hop);
    int last = -1000;
    for (int m = 1; m < frames - 1; ++m)
    {
        float local = 0.0f; int c = 0;
        for (int j = std::max (0, m - R); j <= std::min (frames - 1, m + R); ++j) { local += flux[(size_t) j]; ++c; }
        local /= (float) std::max (1, c);
        const float thr = local * k + mean * 0.15f;
        if (flux[(size_t) m] > thr && flux[(size_t) m] >= flux[(size_t) m - 1] && flux[(size_t) m] >= flux[(size_t) m + 1] && m - last >= minGap)
        {
            // refine to the sample where the energy actually starts rising within this frame span
            int pos = m * hop;
            const int lo = std::max (0, pos - hop), hi = std::min (n - 1, pos + hop);
            float pk = 0.0f; for (int i = lo; i < hi; ++i) pk = std::max (pk, std::fabs (x[i]));
            int s = lo; while (s < hi && std::fabs (x[s]) < pk * 0.15f) ++s;
            const int onset = std::max (0, s - (int) (0.001 * sr));
            // real attacks rise out of something quieter; flux inside a sustained tone does not
            // (checked on the raw signal and on its first difference, so a hat riding on a bass note counts)
            float pre = 0.0f, preHF = 0.0f, post = 0.0f, postHF = 0.0f;
            for (int i = std::max (1, onset - (int) (0.02 * sr)); i < std::max (1, onset - (int) (0.002 * sr)); ++i)
            { pre = std::max (pre, std::fabs (x[i])); preHF = std::max (preHF, std::fabs (x[i] - x[i - 1])); }
            for (int i = std::max (1, onset); i < std::min (n, onset + (int) (0.03 * sr)); ++i)
            { post = std::max (post, std::fabs (x[i])); postHF = std::max (postHF, std::fabs (x[i] - x[i - 1])); }
            if (onset > (int) (0.02 * sr) && post < pre * 2.0f && postHF < preHF * 2.0f) continue;
            onsets.push_back (onset);
            last = m;
        }
    }
    // include a loop-start onset if the file starts hot
    if (! onsets.empty() && onsets.front() > (int) (0.03 * sr) && rms (x, (int) (0.01 * sr)) > 0.05f) onsets.insert (onsets.begin(), 0);
    return onsets;
}

SliceResult sliceLoop (const float* x, int n, double sr, const SlicerOptions& opt)
{
    SliceResult r;
    r.loopSeconds = n / sr;
    auto onsets = detectOnsets (x, n, sr, opt.sensitivity);
    if (onsets.empty()) return r;

    juce::dsp::FFT fft (11);
    const int N = 2048;
    std::vector<Features> feats;
    for (size_t i = 0; i < onsets.size(); ++i)
    {
        SliceResult::Slice s;
        s.startSample = onsets[i];
        const int end = (i + 1 < onsets.size()) ? onsets[i + 1] : n;
        s.length = std::max (1, end - s.startSample);
        Features f = featuresOf (x + s.startSample, s.length, sr, fft, N);
        s.energy = std::pow (10.0f, f.energyDb / 20.0f);
        s.centroidHz = f.centroid; s.lowRatio = f.low; s.highRatio = f.high; s.decaySec = f.decay;
        r.slices.push_back (s);
        feats.push_back (f);
    }

    // ---- feature matrix (standardised)
    const int M = (int) feats.size(), D = 4;
    std::vector<float> X ((size_t) M * D);
    for (int i = 0; i < M; ++i)
    {
        X[(size_t) i * D + 0] = std::log2 (std::max (feats[(size_t) i].centroid, 30.0f));
        X[(size_t) i * D + 1] = feats[(size_t) i].low * 3.0f;
        X[(size_t) i * D + 2] = feats[(size_t) i].high * 3.0f;
        X[(size_t) i * D + 3] = std::log2 (std::max (feats[(size_t) i].decay, 0.005f));
    }
    for (int d = 0; d < D; ++d)
    {
        double mu = 0, var = 0;
        for (int i = 0; i < M; ++i) mu += X[(size_t) i * D + d]; mu /= M;
        for (int i = 0; i < M; ++i) { const double v = X[(size_t) i * D + d] - mu; var += v * v; }
        const double sd = std::sqrt (var / std::max (1, M)) + 1e-6;
        for (int i = 0; i < M; ++i) X[(size_t) i * D + d] = (float) ((X[(size_t) i * D + d] - mu) / sd);
    }

    // ---- k-means for k = 1..maxK, pick smallest k explaining 75% of the variance
    const int maxK = std::max (1, std::min (opt.maxPads, M));
    std::vector<int> bestLabels ((size_t) M, 0);
    double sse1 = 0.0;
    std::mt19937 rng (7);
    int chosenK = 1;
    for (int k = 1; k <= maxK; ++k)
    {
        std::vector<float> cent ((size_t) k * D);
        // k-means++ init
        std::vector<int> labels ((size_t) M, 0);
        std::vector<int> picked;
        picked.push_back ((int) (rng() % (unsigned) M));
        while ((int) picked.size() < k)
        {
            std::vector<double> dmin ((size_t) M, 1e30);
            for (int i = 0; i < M; ++i)
                for (int p : picked)
                {
                    double d = 0; for (int dd = 0; dd < D; ++dd) { const double v = X[(size_t) i * D + dd] - X[(size_t) p * D + dd]; d += v * v; }
                    dmin[(size_t) i] = std::min (dmin[(size_t) i], d);
                }
            int best = 0; for (int i = 1; i < M; ++i) if (dmin[(size_t) i] > dmin[(size_t) best]) best = i;
            picked.push_back (best);
        }
        for (int c = 0; c < k; ++c) for (int d = 0; d < D; ++d) cent[(size_t) c * D + d] = X[(size_t) picked[(size_t) c] * D + d];
        double sse = 0.0;
        for (int it = 0; it < 25; ++it)
        {
            sse = 0.0;
            for (int i = 0; i < M; ++i)
            {
                int bc = 0; double bd = 1e30;
                for (int c = 0; c < k; ++c)
                {
                    double d = 0; for (int dd = 0; dd < D; ++dd) { const double v = X[(size_t) i * D + dd] - cent[(size_t) c * D + dd]; d += v * v; }
                    if (d < bd) { bd = d; bc = c; }
                }
                labels[(size_t) i] = bc; sse += bd;
            }
            std::vector<double> acc ((size_t) k * D, 0.0); std::vector<int> cnt ((size_t) k, 0);
            for (int i = 0; i < M; ++i) { ++cnt[(size_t) labels[(size_t) i]]; for (int d = 0; d < D; ++d) acc[(size_t) labels[(size_t) i] * D + d] += X[(size_t) i * D + d]; }
            for (int c = 0; c < k; ++c) if (cnt[(size_t) c] > 0) for (int d = 0; d < D; ++d) cent[(size_t) c * D + d] = (float) (acc[(size_t) c * D + d] / cnt[(size_t) c]);
        }
        if (k == 1) sse1 = std::max (sse, 1e-9);
        chosenK = k; bestLabels = labels;
        if (sse / sse1 < 0.25) break;
    }
    // remove empty clusters / relabel
    std::vector<int> remap ((size_t) chosenK, -1); int kk = 0;
    for (int i = 0; i < M; ++i) { int& l = bestLabels[(size_t) i]; if (remap[(size_t) l] < 0) remap[(size_t) l] = kk++; l = remap[(size_t) l]; }
    r.numClusters = kk;
    for (int i = 0; i < M; ++i) r.slices[(size_t) i].cluster = bestLabels[(size_t) i];

    // ---- order clusters by mean centroid (low -> high) and choose representatives
    std::vector<double> meanCent ((size_t) kk, 0.0), meanLow ((size_t) kk, 0.0), meanDecay ((size_t) kk, 0.0), meanHigh ((size_t) kk, 0.0);
    std::vector<int> count ((size_t) kk, 0);
    for (int i = 0; i < M; ++i)
    {
        const int c = r.slices[(size_t) i].cluster;
        meanCent[(size_t) c] += std::log2 (std::max (feats[(size_t) i].centroid, 30.0f)); meanLow[(size_t) c] += feats[(size_t) i].low;
        meanDecay[(size_t) c] += feats[(size_t) i].decay; meanHigh[(size_t) c] += feats[(size_t) i].high; ++count[(size_t) c];
    }
    for (int c = 0; c < kk; ++c) { meanCent[(size_t) c] /= count[(size_t) c]; meanLow[(size_t) c] /= count[(size_t) c]; meanDecay[(size_t) c] /= count[(size_t) c]; meanHigh[(size_t) c] /= count[(size_t) c]; }
    std::vector<int> order ((size_t) kk); for (int c = 0; c < kk; ++c) order[(size_t) c] = c;
    std::sort (order.begin(), order.end(), [&] (int a, int b) { return meanCent[(size_t) a] < meanCent[(size_t) b]; });
    std::vector<int> newIndex ((size_t) kk); for (int i = 0; i < kk; ++i) newIndex[(size_t) order[(size_t) i]] = i;
    for (auto& s : r.slices) s.cluster = newIndex[(size_t) s.cluster];

    r.representative.assign ((size_t) kk, -1);
    r.clusterNames.resize ((size_t) kk); r.clusterCategories.resize ((size_t) kk);
    int percCount = 0;
    for (int ci = 0; ci < kk; ++ci)
    {
        const int oc = order[(size_t) ci];
        // median energy of the cluster
        std::vector<float> es; for (int i = 0; i < M; ++i) if (r.slices[(size_t) i].cluster == ci) es.push_back (r.slices[(size_t) i].energy);
        std::nth_element (es.begin(), es.begin() + (long) es.size() / 2, es.end());
        const float med = es[es.size() / 2];
        double bestScore = -1.0; int best = -1;
        for (int i = 0; i < M; ++i)
        {
            const auto& s = r.slices[(size_t) i];
            if (s.cluster != ci) continue;
            const double dur = std::min (0.7, s.length / sr);
            const double closeness = 1.0 - std::min (1.0, (double) (std::fabs (s.energy - med) / std::max (med, 1e-6f)));
            const double score = dur * (0.4 + 0.6 * closeness) * (0.5 + 0.5 * std::min (1.0f, s.energy / std::max (med, 1e-6f)));
            if (score > bestScore) { bestScore = score; best = i; }
        }
        r.representative[(size_t) ci] = best;

        const double cent = std::pow (2.0, meanCent[(size_t) oc]);
        DrumCategory cat; std::string name;
        if ((meanLow[(size_t) oc] > 0.35 && cent < 400.0) || (meanLow[(size_t) oc] > 0.5 && cent < 900.0)) { cat = DrumCategory::Kick; name = "Kick"; }
        else if (cent < 2500.0 && meanHigh[(size_t) oc] < 0.35 && meanDecay[(size_t) oc] > 0.03) { cat = DrumCategory::Snare; name = "Snare"; }
        else if (cent > 3000.0 && meanDecay[(size_t) oc] < 0.08)                { cat = DrumCategory::ClosedHat; name = "Hat Closed"; }
        else if (cent > 3000.0 && meanDecay[(size_t) oc] >= 0.08)               { cat = DrumCategory::OpenHat;   name = "Hat Open"; }
        else                                                                    { cat = DrumCategory::Percussion; name = "Perc " + std::to_string (++percCount); }
        r.clusterCategories[(size_t) ci] = cat; r.clusterNames[(size_t) ci] = name;
    }
    // disambiguate duplicate names
    for (int a = 0; a < kk; ++a)
    {
        int dup = 1;
        for (int b = a + 1; b < kk; ++b)
            if (r.clusterNames[(size_t) b] == r.clusterNames[(size_t) a]) r.clusterNames[(size_t) b] += " " + std::to_string (++dup);
    }

    // ---- bars & tempo: assume the file is a whole number of bars
    const double L = r.loopSeconds;
    double bestErr = 1e9;
    for (int bars : { 1, 2, 4, 8 })
    {
        const double bpm = bars * 4.0 * 60.0 / L;
        double err = std::fabs (std::log (bpm / opt.hostBpm));
        if (bpm < 60.0 || bpm > 200.0) err += 2.0;
        if (err < bestErr) { bestErr = err; r.bars = bars; r.bpm = bpm; }
    }
    return r;
}

} // namespace vellum
