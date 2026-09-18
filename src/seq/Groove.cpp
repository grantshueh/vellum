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
// World grooves map traditional instruments onto the kit: low drums -> Kick / Tom Lo, high drums -> Tom Hi/Mid,
// bells -> Ride, claves / sticks -> Rim, shakers / guira -> Hat Closed, hand claps -> Clap.
const std::vector<GroovePreset>& groovePresets()
{
    static const std::vector<GroovePreset> presets = {
        // ---- studio
        { "Boom Bap", "Studio", 16, false, 0.18f, {
            "X.....x...X...x.", "....X.......X..-", nullptr, nullptr,
            "x.o.x.o.x.o.x.o.", "..............o.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Trap", "Studio", 32, false, 0.0f, {
            "X......x..x.....X.........x.....", nullptr, "..-...-...-...-...-...-...-...-.", "........X...............X.......",
            "x.x.x.x.x.x.x.x.x.x.x.xxx.x.x.xx", "......o.......................o.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "House", "Studio", 16, false, 0.05f, {
            "X...X...X...X...", nullptr, nullptr, "....X.......X...",
            "x.x.x.x.x.x.x.x.", "..o...o...o...o.", "o.-.o.-.o.-.o.-.", nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Techno", "Studio", 16, false, 0.0f, {
            "X...X...X...X...", nullptr, "....x.......x...", "....o.......o...",
            "x.x.x.x.x.x.x.x.", "..o...o...o...o.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Breakbeat", "Studio", 16, false, 0.1f, {
            "X..X....X.X.....", "....X......X..-.", nullptr, nullptr,
            "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Jungle", "Studio", 32, false, 0.0f, {
            "X.........X.....X.........X.....", "....X.......X..-....X.......X...", "..-.....-.....-...-.....-.-.....", nullptr,
            "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Half-Time", "Studio", 16, false, 0.0f, {
            "X.....x.........", "........X.......", nullptr, nullptr,
            "x.x.x.x.x.x.x.x.", ".......o.......o", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Dilla", "Studio", 16, false, 0.6f, {
            "X..x.....x.X....", "....X.......X...", nullptr, nullptr,
            "x.-.x.-.x.-.x.-.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Dembow", "Studio", 16, false, 0.0f, {
            "X...X...X...X...", "...X..X....X..X.", nullptr, nullptr,
            "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Afrobeat", "Studio", 16, false, 0.0f, {
            "X.....X...X.X...", nullptr, ".x.x..x..x.x..x.", nullptr,
            "x.x.x.x.x.x.x.x.", nullptr, "..o..o..o..o.o..", nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Funk 16ths", "Studio", 16, false, 0.1f, {
            "X..X..X...X.....", "....X..o....X..o", nullptr, nullptr,
            "xoxoxoxoxoxoxoxo", "......o.......o.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Disco", "Studio", 16, false, 0.0f, {
            "X...X...X...X...", "....X.......X...", nullptr, "....o.......o...",
            "x...x...x...x...", "..o...o...o...o.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Bossa", "Studio", 16, false, 0.0f, {
            "X..x..X.X..x..X.", nullptr, "x..x..x...x..x..", nullptr,
            "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "2-Step", "Studio", 16, false, 0.25f, {
            "X.....X....X....", "....X.......X...", nullptr, nullptr,
            "x.x.x.x.x.x.x.x.", "..o.......o.....", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Motown", "Studio", 16, false, 0.05f, {
            "X...X...X...X...", "X...X...X...X...", nullptr, nullptr,
            nullptr, nullptr, "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Tom Fill", "Studio", 16, false, 0.0f, {
            "X...............", nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, "....X.x.........", "........X.x.....", "............X.x.", nullptr, "X..............." } },

        // ---- Brazil
        { "Samba (batucada)", "Brazil", 32, false, 0.0f, {
            "....o...X...........o...X.......",          // surdo: marcação on 2, answer on 4
            "x-x-Xx-x-x-Xx-x-x-x-Xx-x-x-x-Xx-",          // caixa
            nullptr, nullptr,
            "xoxoxoxoxoxoxoxoxoxoxoxoxoxoxoxo",          // chocalho / ganzá
            nullptr,
            "x..x..x.x..x..x.x..x..x.x..x..x.",          // tamborim carreteiro
            nullptr, nullptr, nullptr,
            "x..x.x..x..x.x..x..x.x..x..x.x..",          // agogô
            nullptr } },
        { "Maracatu (baque virado)", "Brazil", 16, false, 0.0f, {
            "X..X..X.X..X....", "xoxoXoxoxoxoXoxo", nullptr, nullptr,
            "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr,
            "x..x..x...x..x..",                          // gonguê bell
            nullptr } },
        // ---- Cuba / Caribbean
        { "Rumba Clave (guaguancó)", "Cuba", 16, false, 0.0f, {
            nullptr, nullptr,
            "X..X...X..X.X...",                          // rumba clave 3-2
            nullptr,
            "x.x.x.x.x.x.x.x.",                          // shaker
            nullptr,
            "x.xx.x.xx.xx.x.x",                          // palitos / catá
            "..........o.o...",                          // quinto (improvised)
            "..x...x...x...x.",                          // conga (segundo)
            "X.......x.......",                          // tumba
            nullptr, nullptr } },
        { "Cascara (2-3 son)", "Cuba", 16, false, 0.0f, {
            "...X......X.....",                          // bombo / bass tumbao
            nullptr,
            "..X.X...X..X..X.",                          // son clave 2-3
            nullptr, nullptr, nullptr, nullptr, nullptr,
            "..x...x...x...x.",                          // conga
            "......X.......X.",                          // tumba open tones
            "x.xx.x.xx.xx.x.x",                          // cáscara on the shell -> ride
            nullptr } },
        { "Bomba (sicá)", "Puerto Rico", 16, false, 0.0f, {
            "X.....X...X.X...",                          // buleador
            nullptr,
            "x.x.xx.xx.x.xx.x",                          // cuá sticks
            nullptr,
            "x.x.x.x.x.x.x.x.",                          // maraca
            nullptr, nullptr,
            "..o.....o...o...",                          // primo / subidor
            nullptr, nullptr, nullptr, nullptr } },
        { "Merengue", "Dominican Rep.", 16, false, 0.0f, {
            "X.......X.X.....",                          // tambora low
            nullptr,
            "....x.x.....x.x.",                          // tambora slaps
            nullptr,
            "xoxoxoxoxoxoxoxo",                          // güira
            nullptr, nullptr, nullptr,
            "..x...x...x...x.",                          // conga
            nullptr, nullptr, nullptr } },
        { "Cumbia", "Colombia", 16, false, 0.0f, {
            "X...x...X...x...",                          // tambora
            nullptr, nullptr, nullptr,
            "x-x-x-x-x-x-x-x-",                          // guacharaca
            nullptr, nullptr,
            "....x.x.....x.x.",                          // alegre
            "..x...x...x...x.",                          // llamador off-beats
            nullptr, nullptr, nullptr } },
        // ---- Africa
        { "Kpanlogo", "Ghana", 16, false, 0.0f, {
            "X.....X.X.....X.", nullptr, nullptr, "....X.......X...",
            "x.x.x.x.x.x.x.x.",                          // shekere
            nullptr, nullptr, nullptr,
            "..x..x....x..x..",
            nullptr,
            "x.xx.x.xx.xx.x.x",                          // gankogui bell
            nullptr } },
        { "Semba", "Angola", 16, false, 0.0f, {
            "X..X....X..X....", nullptr, "..x...x...x..x..", nullptr,
            "x.x.x.x.x.x.x.x.",                          // dikanza
            nullptr, nullptr, nullptr, "....x.......x...", nullptr, nullptr, nullptr } },
        { "Gqom", "South Africa", 32, false, 0.0f, {
            "X.....X.....X...X.....X.....X...",          // broken 3-against-4 kick
            nullptr, nullptr, "........X...............X.......",
            "....x.......x.......x.......x...", nullptr,
            "..x.....x.....x...x.....x.....x.", nullptr, nullptr,
            "X...............X...............", nullptr, nullptr } },
        { "Amapiano", "South Africa", 32, false, 0.0f, {
            "o.......o.......o.......o.......", nullptr, nullptr, "....o.......o.......o.......o...",
            "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.", nullptr,
            "......x.......x.......x.......x.", nullptr, nullptr,
            "..X...X.....X.....X...X.....X...",          // log drum
            nullptr, nullptr } },
        // ---- Middle East / Turkey
        { "Maqsum", "Egypt", 16, false, 0.0f, {
            "X.......X.......",                          // dum
            nullptr,
            "..X...X.....X...",                          // tek
            nullptr,
            "x.x.x.x.x.x.x.x.", nullptr,
            "-.-.-.-.-.-.-.-.",                          // riq
            nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Aksak 9/8", "Turkey", 18, false, 0.0f, {
            "X...X...X...X.....",                        // 2+2+2+3
            nullptr,
            "..X...X...X...X.X.",
            nullptr,
            "x.x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        // ---- India (one matra = 2 steps for Jhaptal, 1 step for Teental)
        { "Jhaptal (10)", "India", 20, false, 0.0f, {
            "X...X.X.......X.X...",                      // dhi
            nullptr,
            "..X.....X...X.....X.",                      // na
            nullptr, nullptr, nullptr,
            "..........X.........",                      // ti
            nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Teental (16)", "India", 16, false, 0.0f, {
            "X..XX..XX......X",                          // dha
            nullptr,
            ".........XXXX...",                          // tin / ta (khali)
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            ".XX..XX......XX.",                          // dhin
            nullptr, nullptr } },
        // ---- East Asia
        { "Taiko", "Japan", 16, false, 0.0f, {
            "X.....X.X.....X.",                          // odaiko don
            nullptr,
            "x.x.x.x.x.x.x.x.",                          // shime-daiko
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            "..x.x.....x.x...",                          // doko
            "X.......X.......",                          // atarigane
            nullptr } },
        { "Samulnori (12/8)", "Korea", 12, true, 0.0f, {
            "X..X..X..X..",                              // buk
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            "..x..x..xx.x",                              // janggu (chae)
            "X.x.X.x.X.x.",                              // janggu (gungchae)
            nullptr,
            "x.xx.xx.xx.x",                              // kkwaenggwari
            "X..........." } },                          // jing
        { "Eisa", "Okinawa", 16, false, 0.0f, {
            "X...X...X..X....", nullptr,
            "x.x.x.x.x.x.x.x.",                          // shime
            nullptr, nullptr, nullptr, nullptr,
            "....x.......x.x.",                          // paranku
            nullptr, nullptr, nullptr, nullptr } },
        // ---- Jamaica
        { "Reggae (one drop)", "Jamaica", 16, false, 0.05f, {
            "........X.......",
            nullptr,
            "........X.......",                          // cross-stick
            nullptr,
            "x.x.x.x.x.x.x.x.", "......o.......o.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
        { "Dancehall", "Jamaica", 16, false, 0.0f, {
            "X.....X.X.....X.", "...X.......X....", nullptr, "...X..X....X..X.",
            "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } },
    };
    return presets;
}

Pattern patternFromPreset (const GroovePreset& p)
{
    Pattern out;
    out.length = std::max (1, std::min (kMaxSteps, p.length)); out.triplet = p.triplet; out.swing = p.swing; out.name = p.name;
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
    const double stepBeats = p.stepBeats();
    const double loopBeats = length * stepBeats;
    const double blockBeats = numSamples * bpm / (60.0 * sr);
    double start = std::fmod (ppq, loopBeats);
    if (start < 0.0) start += loopBeats;
    const double end = start + blockBeats;
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);

    for (int s = 0; s < length; ++s)
    {
        const double t = s * stepBeats + ((s & 1) ? p.swing * stepBeats * 0.5 : 0.0);
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
    displayStep = (int) (start / stepBeats) % length;
    if (! hostPlaying) internalPpq += blockBeats;
}

} // namespace vellum
