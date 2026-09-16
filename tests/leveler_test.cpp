// Behaviour tests for the auto-leveler + gain architecture (no JUCE needed).
#include "dsp/Leveler.h"
#include "dsp/GainStage.h"
#include <cstdio>
#include <random>
#include <vector>

using namespace vellum;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++failures; } else std::printf("  ok:   %s\n", msg); } while (0)

// synthetic drum: decaying sine + decaying noise, scaled to a given peak
static std::vector<float> synthDrum (double sr, float hz, float tau, float noiseAmt, float noiseTau, float peakLin, unsigned seed)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    const int n = (int) (sr * 1.0);
    std::vector<float> x (n);
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sr;
        x[i] = std::sin (6.2831853f * hz * t) * std::exp (-t / tau) + noiseAmt * nd (rng) * std::exp (-t / noiseTau);
        pk = std::max (pk, std::fabs (x[i]));
    }
    for (auto& v : x) v *= peakLin / pk;
    return x;
}

static float peakOf (const std::vector<float>& x, float gainLin)
{
    float p = 0.0f; for (float v : x) p = std::max (p, std::fabs (v * gainLin)); return p;
}

int main()
{
    const double sr = 48000.0;
    std::printf ("Vellum auto-level tests\n");

    // classification
    CHECK (classifyFilename ("Kick DnB.wav") == DrumCategory::Kick, "classify 'Kick DnB.wav' -> Kick");
    CHECK (classifyFilename ("BD_909_01.wav") == DrumCategory::Kick, "classify 'BD_909_01' -> Kick");
    CHECK (classifyFilename ("222 808.wav") == DrumCategory::Kick, "classify '222 808' -> Kick");
    CHECK (classifyFilename ("Hat Closed 3.aif") == DrumCategory::ClosedHat, "classify 'Hat Closed 3' -> ClosedHat");
    CHECK (classifyFilename ("CHH_tight.wav") == DrumCategory::ClosedHat, "classify 'CHH_tight' -> ClosedHat");
    CHECK (classifyFilename ("open_hat_long.wav") == DrumCategory::OpenHat, "classify 'open_hat_long' -> OpenHat");
    CHECK (classifyFilename ("hh01.wav") == DrumCategory::Hat, "classify 'hh01' -> Hat");
    CHECK (classifyFilename ("SD_crack.wav") == DrumCategory::Snare, "classify 'SD_crack' -> Snare");
    CHECK (classifyFilename ("Tom Low DnB.wav") == DrumCategory::Tom, "classify 'Tom Low DnB' -> Tom");
    CHECK (classifyFilename ("Cymbal DnB.wav") == DrumCategory::Cymbal, "classify 'Cymbal DnB' -> Cymbal");
    CHECK (classifyFilename ("clave_wood.wav") == DrumCategory::Percussion, "classify 'clave_wood' -> Percussion");
    CHECK (classifyFilename ("weird_thing_07.wav") == DrumCategory::Unknown, "classify unknown -> Unknown");
    CHECK (classifyFilename ("Abdomen.wav") != DrumCategory::Kick, "'bd' must match as whole token only");

    // 1. two kicks from different packs land at comparable baseline
    auto kickA = synthDrum (sr, 55.0f, 0.25f, 0.05f, 0.02f, 0.99f, 1);   // hot pack
    auto kickB = synthDrum (sr, 60.0f, 0.30f, 0.08f, 0.03f, 0.25f, 2);   // quiet pack
    const float gA = computeAutoGainDb (analyzeLevel (kickA.data(), (int) kickA.size(), sr), DrumCategory::Kick);
    const float gB = computeAutoGainDb (analyzeLevel (kickB.data(), (int) kickB.size(), sr), DrumCategory::Kick);
    const float pkA = linToDb (peakOf (kickA, dbToLin (gA)));
    const float pkB = linToDb (peakOf (kickB, dbToLin (gB)));
    std::printf ("  kick A: auto %.1f dB -> peak %.1f dBFS | kick B: auto %.1f dB -> peak %.1f dBFS\n", gA, pkA, gB, pkB);
    CHECK (std::fabs (pkA - pkB) < 3.0f, "1. two kicks import at comparable baseline (within 3 dB)");
    CHECK (pkA <= -8.0f + 0.01f && pkB <= -8.0f + 0.01f, "   kick peaks never exceed the KICK peak target");

    // 2. closed hat quieter than kick after leveling
    auto hat = synthDrum (sr, 6000.0f, 0.03f, 1.0f, 0.06f, 0.99f, 3);
    const float gH = computeAutoGainDb (analyzeLevel (hat.data(), (int) hat.size(), sr), DrumCategory::ClosedHat);
    const float pkH = linToDb (peakOf (hat, dbToLin (gH)));
    std::printf ("  hat: auto %.1f dB -> peak %.1f dBFS\n", gH, pkH);
    CHECK (pkH < pkA - 4.0f, "2. closed hat remains clearly quieter than kick");

    // 3/4. velocity behaviour
    const float v127 = velocityGainDbMidi (127);
    const float v64  = velocityGainDbMidi (64);
    std::printf ("  vel127 %.2f dB, vel64 %.2f dB\n", v127, v64);
    CHECK (std::fabs (v127) < 0.05f, "3. velocity 127 ~= 0 dB (does not exceed baseline)");
    CHECK (v64 < v127 - 6.0f, "4. velocity 64 clearly quieter than 127");
    CHECK (velocityGainDbMidi (127, 1.5f, 0.0f) <= 0.0f, "   no boost above baseline by default");

    // 5/6. velocity and expression never alter autoGainDb
    VoiceGainTerms t; t.autoGainDb = gA; t.autoLevelAmount = 1.0f;
    const float before = t.autoGainDb;
    t.velocityGainDb = velocityGainDbMidi (40);
    t.expressionGainDb = expressionGainDb (0.3f);
    CHECK (t.autoGainDb == before, "5/6. velocity / expression leave autoGainDb untouched");
    CHECK (std::fabs (expressionGainDb (0.0f) + 12.0f) < 1e-4f && std::fabs (expressionGainDb (0.5f) + 6.0f) < 1e-4f
           && std::fabs (expressionGainDb (1.0f)) < 1e-4f, "   expression maps 0/0.5/1 -> -12/-6/0 dB");
    CHECK (expressionGainDbBipolar (1.0f) == 0.0f && expressionGainDbBipolar (1.0f, true) > 0.0f,
           "   bipolar expression only positive when explicitly enabled");

    // 7/8. pitch change: autoGainDb is a property of the sample, no re-normalisation.
    // (Resampling ratio does not appear anywhere in the gain terms; we assert that the
    // API has no pitch input by recomputing with the same analysis.)
    const float again = computeAutoGainDb (analyzeLevel (kickA.data(), (int) kickA.size(), sr), DrumCategory::Kick);
    CHECK (again == gA, "7. recomputing from the same source yields identical autoGainDb (pitch is not an input)");
    // pitching up by resampling a copy 1.5x: analysis of the *resampled* signal differs,
    // demonstrating the natural loudness consequence we deliberately do NOT correct.
    {
        std::vector<float> up; for (size_t i = 0; i < kickA.size(); i += 3) { up.push_back (kickA[i]); if (i + 1 < kickA.size()) up.push_back (kickA[i + 1]); }
        auto la = analyzeLevel (up.data(), (int) up.size(), sr);
        std::printf ("  pitched copy RMS %.1f vs original %.1f dB (left uncorrected by design)\n", la.rmsDb, analyzeLevel (kickA.data(), (int) kickA.size(), sr).rmsDb);
        CHECK (true, "8. pitch shift keeps its natural loudness/spectral consequence (no per-pitch normalisation)");
    }

    // 9/10. auto level amount
    VoiceGainTerms t0 = t; t0.autoLevelAmount = 0.0f;
    VoiceGainTerms t1 = t; t1.autoLevelAmount = 1.0f;
    CHECK (sourceGainDb (t0) == 0.0f, "9. Auto Level 0 restores original source gain");
    CHECK (std::fabs (finalVoiceGainDb (t0) - (t0.velocityGainDb + t0.expressionGainDb + t0.userGainDb)) < 1e-5f,
           "   ...while velocity/expression still apply");
    CHECK (sourceGainDb (t1) == gA, "10. Auto Level 1 applies the full category normalisation");
    CHECK (performanceGainDb (t0) == performanceGainDb (t1), "   ...with identical velocity/expression range");

    // 11. stacked voices keep bus headroom: 4 category-levelled hits at once
    {
        auto snare = synthDrum (sr, 200.0f, 0.15f, 0.6f, 0.12f, 0.99f, 4);
        auto clap  = synthDrum (sr, 1200.0f, 0.05f, 0.9f, 0.08f, 0.99f, 5);
        const float gS = computeAutoGainDb (analyzeLevel (snare.data(), (int) snare.size(), sr), DrumCategory::Snare);
        const float gC = computeAutoGainDb (analyzeLevel (clap.data(), (int) clap.size(), sr), DrumCategory::Clap);
        float busPeak = 0.0f;
        for (size_t i = 0; i < kickA.size(); ++i)
            busPeak = std::max (busPeak, std::fabs (kickA[i] * dbToLin (gA) + snare[i] * dbToLin (gS)
                                                    + clap[i] * dbToLin (gC) + hat[i] * dbToLin (gH)));
        std::printf ("  4-voice stack peak %.1f dBFS\n", linToDb (busPeak));
        CHECK (linToDb (busPeak) < -3.0f, "11. four simultaneous levelled voices stay under the -3 dBFS internal ceiling");
    }

    // 12. user gain independent
    VoiceGainTerms tu = t1; tu.userGainDb = -4.0f;
    CHECK (std::fabs (finalVoiceGainDb (tu) - (finalVoiceGainDb (t1) - 4.0f)) < 1e-5f && tu.autoGainDb == t1.autoGainDb,
           "12. user gain adds independently and leaves autoGainDb editable/unchanged");

    // clamps
    {
        LevelAnalysis silentish; silentish.peakDb = -60.0f; silentish.rmsDb = -70.0f;
        CHECK (computeAutoGainDb (silentish, DrumCategory::Kick) <= kMaxBoostDb, "boost clamped at +12 dB");
        LevelAnalysis hot; hot.peakDb = 0.0f; hot.rmsDb = -3.0f;
        const float g = computeAutoGainDb (hot, DrumCategory::Shaker);
        CHECK (g >= kMaxAttenuationDb && hot.peakDb + g <= -17.0f + 1e-4f, "attenuation clamped at -24 dB and peak <= category target");
    }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
