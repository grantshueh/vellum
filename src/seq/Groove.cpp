#include "Groove.h"
#include <cmath>

namespace vellum {

const char* defaultPadName (int pad)
{
    static const char* names[kNumPads] = { "Kick", "Snare", "Rim", "Clap", "Hat Closed", "Hat Open",
                                           "Perc", "Tom Hi", "Tom Mid", "Tom Lo", "Ride", "Crash" };
    return names[std::max (0, std::min (kNumPads - 1, pad))];
}

DrumCategory defaultPadCategory (int pad)
{
    static const DrumCategory cats[kNumPads] = { DrumCategory::Kick, DrumCategory::Snare, DrumCategory::Rim, DrumCategory::Clap,
                                                 DrumCategory::ClosedHat, DrumCategory::OpenHat, DrumCategory::Percussion, DrumCategory::Tom,
                                                 DrumCategory::Tom, DrumCategory::Tom, DrumCategory::Ride, DrumCategory::Cymbal };
    return cats[std::max (0, std::min (kNumPads - 1, pad))];
}

// rows: Kick, Snare, Rim, Clap, HatC, HatO, Perc, TomH, TomM, TomL, Ride, Crash
const std::vector<GroovePreset>& groovePresets()
{
    static const std::vector<GroovePreset> presets = {
        { "Boom Bap", 16, 0.18f, {
            "X.....x...X...x.", "....X.......X..-", nullptr, nullptr,
            "x.o.x.o.x.o.x.o.", "..............o.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Trap", 32, 0.0f, {
            "X......x..x.....X.........x.....", nullptr, "..-...-...-...-...-...-...-...-.", "........X...............X.......",
            "x.x.x.x.x.x.x.x.x.x.x.xxx.x.x.xx", "......o.......................o.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "House", 16, 0.05f, {
            "X...X...X...X...", nullptr, nullptr, "....X.......X...",
            "x.x.x.x.x.x.x.x.", "..o...o...o...o.", "o.-.o.-.o.-.o.-.", nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Techno", 16, 0.0f, {
            "X...X...X...X...", nullptr, "....x.......x...", "....o.......o...",
            "x.x.x.x.x.x.x.x.", "..o...o...o...o.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Breakbeat", 16, 0.1f, {
            "X..X....X.X.....", "....X......X..-.", nullptr, nullptr,
            "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Jungle", 32, 0.0f, {
            "X.........X.....X.........X.....", "....X.......X..-....X.......X...", "..-.....-.....-...-.....-.-.....", nullptr,
            "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Half-Time", 16, 0.0f, {
            "X.....x.........", "........X.......", nullptr, nullptr,
            "x.x.x.x.x.x.x.x.", ".......o.......o", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Dilla", 16, 0.6f, {
            "X..x.....x.X....", "....X.......X...", nullptr, nullptr,
            "x.-.x.-.x.-.x.-.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Dembow", 16, 0.0f, {
            "X...X...X...X...", "...X..X....X..X.", nullptr, nullptr,
            "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Afrobeat", 16, 0.0f, {
            "X.....X...X.X...", nullptr, ".x.x..x..x.x..x.", nullptr,
            "x.x.x.x.x.x.x.x.", nullptr, "..o..o..o..o.o..", nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Funk 16ths", 16, 0.1f, {
            "X..X..X...X.....", "....X..o....X..o", nullptr, nullptr,
            "xoxoxoxoxoxoxoxo", "......o.......o.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Disco", 16, 0.0f, {
            "X...X...X...X...", "....X.......X...", nullptr, "....o.......o...",
            "x...x...x...x...", "..o...o...o...o.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Bossa", 16, 0.0f, {
            "X..x..X.X..x..X.", nullptr, "x..x..x...x..x..", nullptr,
            "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "2-Step", 16, 0.25f, {
            "X.....X....X....", "....X.......X...", nullptr, nullptr,
            "x.x.x.x.x.x.x.x.", "..o.......o.....", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Motown", 16, 0.05f, {
            "X...X...X...X...", "X...X...X...X...", nullptr, nullptr,
            nullptr, nullptr, "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Tom Fill", 16, 0.0f, {
            "X...............", nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, "....X.x.........", "........X.x.....", "............X.x.", nullptr, "X..............." } },
    };
    return presets;
}

Pattern patternFromPreset (const GroovePreset& p)
{
    Pattern out;
    out.length = p.length; out.swing = p.swing; out.name = p.name;
    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const char* row = p.rows[pad];
        if (row == nullptr) continue;
        for (int s = 0; s < p.length && row[s] != 0; ++s)
        {
            Step& st = out.steps[pad][s];
            switch (row[s])
            {
                case 'X': st.on = true; st.vel = 1.0f; break;
                case 'x': st.on = true; st.vel = 0.8f; break;
                case 'o': st.on = true; st.vel = 0.55f; break;
                case '-': st.on = true; st.vel = 0.35f; break;
                default: break;
            }
        }
    }
    return out;
}

void GrooveClock::process (const Pattern& p, bool hostPlaying, double hostPpq, double bpm, bool internalPlaying,
                           double sr, int numSamples, std::vector<Trigger>& out, std::mt19937& rng)
{
    const bool running = hostPlaying || internalPlaying;
    if (! running || bpm <= 0.0) { displayStep = -1; return; }

    const double ppq = hostPlaying ? hostPpq : internalPpq;
    const int length = std::max (1, std::min (kMaxSteps, p.length));
    const double loopBeats = length * 0.25;
    const double blockBeats = numSamples * bpm / (60.0 * sr);
    double start = std::fmod (ppq, loopBeats);
    if (start < 0.0) start += loopBeats;
    const double end = start + blockBeats;
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);

    for (int s = 0; s < length; ++s)
    {
        const double t = s * 0.25 + ((s & 1) ? p.swing * 0.125 : 0.0);
        for (double off : { 0.0, loopBeats })
        {
            const double tt = t + off;
            if (tt < start || tt >= end) continue;
            const int offset = std::min (numSamples - 1, std::max (0, (int) ((tt - start) * 60.0 * sr / bpm)));
            for (int pad = 0; pad < kNumPads; ++pad)
            {
                const Step& st = p.steps[pad][s];
                if (! st.on) continue;
                if (st.prob < 1.0f && uni (rng) > st.prob) continue;
                out.push_back ({ pad, st.vel, offset, s });
            }
        }
    }
    displayStep = (int) (start / 0.25) % length;
    if (! hostPlaying) internalPpq += blockBeats;
}

} // namespace vellum
