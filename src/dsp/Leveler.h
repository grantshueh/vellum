// Vellum — automatic drum-sample leveling (pure C++, no JUCE).
//
// Establishes a *static* per-sample baseline gain (autoGainDb) from filename
// classification + peak / short-term RMS analysis of the untouched source.
// Velocity, expression, resampling and everything downstream move away from
// that baseline deliberately and are never renormalised (see GainStage.h).
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace vellum {

enum class DrumCategory {
    Kick, Snare, Clap, Rim, Tom, ClosedHat, OpenHat, Hat, Ride, Cymbal,
    Shaker, Tambourine, Percussion, FX, Unknown, Count
};

inline const char* categoryName (DrumCategory c)
{
    switch (c)
    {
        case DrumCategory::Kick:       return "Kick";
        case DrumCategory::Snare:      return "Snare";
        case DrumCategory::Clap:       return "Clap";
        case DrumCategory::Rim:        return "Rim";
        case DrumCategory::Tom:        return "Tom";
        case DrumCategory::ClosedHat:  return "Closed Hat";
        case DrumCategory::OpenHat:    return "Open Hat";
        case DrumCategory::Hat:        return "Hat";
        case DrumCategory::Ride:       return "Ride";
        case DrumCategory::Cymbal:     return "Cymbal";
        case DrumCategory::Shaker:     return "Shaker";
        case DrumCategory::Tambourine: return "Tambourine";
        case DrumCategory::Percussion: return "Percussion";
        case DrumCategory::FX:         return "FX";
        default:                       return "Unknown";
    }
}

struct CategoryTargets { float peakDb; float rmsDb; };

inline CategoryTargets targetsFor (DrumCategory c)
{
    switch (c)
    {
        case DrumCategory::Kick:       return { -8.0f,  -16.0f };
        case DrumCategory::Snare:      return { -10.0f, -18.0f };
        case DrumCategory::Clap:       return { -11.0f, -19.0f };
        case DrumCategory::Rim:        return { -13.0f, -21.0f };
        case DrumCategory::Tom:        return { -11.0f, -18.0f };
        case DrumCategory::ClosedHat:  return { -15.0f, -24.0f };
        case DrumCategory::OpenHat:    return { -14.0f, -22.0f };
        case DrumCategory::Hat:        return { -15.0f, -23.0f };
        case DrumCategory::Ride:       return { -15.0f, -23.0f };
        case DrumCategory::Cymbal:     return { -14.0f, -23.0f };
        case DrumCategory::Shaker:     return { -17.0f, -25.0f };
        case DrumCategory::Tambourine: return { -16.0f, -24.0f };
        case DrumCategory::Percussion: return { -14.0f, -21.0f };
        case DrumCategory::FX:         return { -16.0f, -23.0f };
        default:                       return { -13.0f, -21.0f };
    }
}

// ---------------------------------------------------------------------------
// 1. Filename classification
// ---------------------------------------------------------------------------
namespace detail {

// lower-case, non-alphanumerics -> space, split letter/digit boundaries.
inline std::string normaliseName (const std::string& in)
{
    std::string out;
    out.reserve (in.size() + 8);
    char prev = ' ';
    for (unsigned char ch : in)
    {
        char c = (char) std::tolower (ch);
        if (! std::isalnum ((unsigned char) c)) c = ' ';
        const bool prevAlpha = std::isalpha ((unsigned char) prev) != 0;
        const bool prevDigit = std::isdigit ((unsigned char) prev) != 0;
        if ((std::isdigit ((unsigned char) c) && prevAlpha) || (std::isalpha ((unsigned char) c) && prevDigit))
            out.push_back (' ');
        out.push_back (c);
        prev = c;
    }
    return " " + out + " ";
}

inline bool hasToken (const std::string& padded, const char* tok)      // whole word
{
    return padded.find (" " + std::string (tok) + " ") != std::string::npos;
}
inline bool hasSub (const std::string& padded, const char* sub)        // substring
{
    return padded.find (sub) != std::string::npos;
}
inline bool hasPhrase (const std::string& padded, const char* a, const char* b) // "a b" or "ab"
{
    return hasSub (padded, (std::string (a) + " " + b).c_str()) || hasSub (padded, (std::string (a) + b).c_str());
}
} // namespace detail

inline DrumCategory classifyFilename (const std::string& filenameOrName)
{
    using namespace detail;
    // strip directory + extension
    std::string s = filenameOrName;
    if (auto p = s.find_last_of ("/\\"); p != std::string::npos) s = s.substr (p + 1);
    if (auto p = s.find_last_of ('.'); p != std::string::npos && p > 0) s = s.substr (0, p);
    const std::string n = normaliseName (s);

    // specific hats first, then everything in the order of the spec
    if (hasSub (n, "chh") || hasPhrase (n, "closed", "hat") || hasPhrase (n, "hat", "closed")
        || hasPhrase (n, "closed", "hh") || hasPhrase (n, "hh", "closed") || hasPhrase (n, "hihat", "closed")
        || hasPhrase (n, "hat", "cl") || hasPhrase (n, "hh", "cl") || hasPhrase (n, "cl", "hat"))
        return DrumCategory::ClosedHat;
    if (hasSub (n, "ohh") || hasPhrase (n, "open", "hat") || hasPhrase (n, "hat", "open")
        || hasPhrase (n, "open", "hh") || hasPhrase (n, "hh", "open") || hasPhrase (n, "hihat", "open"))
        return DrumCategory::OpenHat;

    if (hasSub (n, "kick") || hasToken (n, "bd") || hasToken (n, "808") || hasPhrase (n, "bass", "drum")) return DrumCategory::Kick;
    if (hasSub (n, "snare") || hasToken (n, "sd") || hasToken (n, "snr"))           return DrumCategory::Snare;
    if (hasSub (n, "clap") || hasToken (n, "cp"))                                    return DrumCategory::Clap;
    if (hasSub (n, "rimshot") || hasSub (n, "rim") || hasPhrase (n, "side", "stick") || hasSub (n, "stick"))
        return DrumCategory::Rim;
    if (hasSub (n, "tom") || hasSub (n, "floor"))                                    return DrumCategory::Tom;
    if (hasToken (n, "hh") || hasPhrase (n, "hi", "hat") || hasSub (n, "hat"))       return DrumCategory::Hat;
    if (hasSub (n, "ride"))                                                          return DrumCategory::Ride;
    if (hasSub (n, "crash") || hasSub (n, "cymbal") || hasToken (n, "cym") || hasSub (n, "cym "))
        return DrumCategory::Cymbal;
    if (hasSub (n, "shaker") || hasSub (n, "shake"))                                 return DrumCategory::Shaker;
    if (hasSub (n, "tamb"))                                                          return DrumCategory::Tambourine;
    if (hasSub (n, "perc") || hasSub (n, "cowbell") || hasSub (n, "clave") || hasSub (n, "woodblock") || hasSub (n, "block"))
        return DrumCategory::Percussion;
    if (hasToken (n, "fx") || hasSub (n, "texture") || hasSub (n, "noise") || hasSub (n, "foley"))
        return DrumCategory::FX;
    return DrumCategory::Unknown;
}

// ---------------------------------------------------------------------------
// 2. Analysis of the original (un-resampled) source
// ---------------------------------------------------------------------------
struct LevelAnalysis
{
    float peakDb = -120.0f;   // sample peak of the non-silent region
    float rmsDb  = -120.0f;   // RMS over the loudest ~200 ms window
    int   start  = 0;         // first non-silent sample
    int   end    = 0;         // one past the last non-silent sample
};

inline float linToDb (double lin) { return (float) (20.0 * std::log10 (std::max (lin, 1e-9))); }
inline float dbToLin (float db)   { return std::pow (10.0f, db / 20.0f); }

inline LevelAnalysis analyzeLevel (const float* x, int n, double sampleRate, float silenceDb = -60.0f)
{
    LevelAnalysis a;
    if (n <= 0) return a;

    double peak = 0.0;
    for (int i = 0; i < n; ++i) peak = std::max (peak, (double) std::fabs (x[i]));
    if (peak <= 0.0) return a;

    const double gate = peak * std::pow (10.0, silenceDb / 20.0);
    int s = 0, e = n;
    while (s < n && std::fabs (x[s]) < gate) ++s;
    while (e > s && std::fabs (x[e - 1]) < gate) --e;
    a.start = s; a.end = e;
    a.peakDb = linToDb (peak);

    // loudest 200 ms window (sliding sum of squares)
    const int win = std::max (1, std::min (e - s, (int) std::lround (0.2 * sampleRate)));
    double sum = 0.0, best = 0.0;
    for (int i = s; i < s + win; ++i) sum += (double) x[i] * x[i];
    best = sum;
    for (int i = s + win; i < e; ++i)
    {
        sum += (double) x[i] * x[i] - (double) x[i - win] * x[i - win];
        best = std::max (best, sum);
    }
    a.rmsDb = linToDb (std::sqrt (std::max (best, 0.0) / win));
    return a;
}

// ---------------------------------------------------------------------------
// 3/4. Static source gain
// ---------------------------------------------------------------------------
constexpr float kMaxBoostDb        =  12.0f;
constexpr float kMaxAttenuationDb  = -24.0f;
constexpr float kAbsoluteCeilingDb =  -3.0f;

inline float computeAutoGainDb (const LevelAnalysis& a, DrumCategory cat)
{
    if (a.peakDb <= -119.0f) return 0.0f;           // silence: leave alone
    const CategoryTargets t = targetsFor (cat);
    const float rmsCorrection  = t.rmsDb  - a.rmsDb;
    const float peakCorrection = t.peakDb - a.peakDb;
    float g = 0.65f * rmsCorrection + 0.35f * peakCorrection;
    g = std::min (g, kMaxBoostDb);
    g = std::max (g, kMaxAttenuationDb);
    // never push the peak above the category target...
    g = std::min (g, t.peakDb - a.peakDb);
    // ...or above the absolute safety ceiling
    g = std::min (g, kAbsoluteCeilingDb - a.peakDb);
    return g;
}

} // namespace vellum
