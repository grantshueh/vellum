// Vellum — parameter table (X-macro) shared by the processor and the UI.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Humanizer.h"

namespace vellum {

// field, id, display name, min, max, default, skew-centre (0 = linear)
#define VELLUM_FLOAT_PARAMS(X) \
    X(sensitivity,      "sensitivity",      "Sensitivity",        0.0f, 1.0f, 0.6f,  0.0f) \
    X(instability,      "instability",      "Instability",        0.0f, 1.0f, 0.5f,  0.0f) \
    X(space,            "space",            "Space",              0.0f, 1.0f, 0.2f,  0.0f) \
    X(autoLevel,        "autoLevel",        "Auto Level",         0.0f, 1.0f, 1.0f,  0.0f) \
    X(level,            "level",            "Level",            -24.0f, 12.0f, 0.0f, 0.0f) \
    X(punch,            "punch",            "Punch",              0.0f, 1.0f, 0.0f,  0.0f) \
    X(ceiling,          "ceiling",          "Ceiling",            0.0f, 1.0f, 0.3f,  0.0f) \
    X(lift,             "lift",             "Lift",               0.0f, 1.0f, 0.0f,  0.0f) \
    X(drive,            "drive",            "Drive",              0.0f, 1.0f, 0.0f,  0.0f) \
    X(velCurve,         "velCurve",         "Vel Curve",          0.5f, 3.0f, 1.5f,  1.5f) \
    X(velBoost,         "velBoost",         "Vel Boost",          0.0f, 6.0f, 0.0f,  0.0f) \
    X(velBright,        "velBright",        "Vel>Bright",         0.0f, 1.0f, 0.6f,  0.0f) \
    X(velGlide,         "velGlide",         "Vel>Glide",          0.0f, 1.0f, 0.5f,  0.0f) \
    X(velSnap,          "velSnap",          "Vel>Snap",           0.0f, 1.0f, 0.5f,  0.0f) \
    X(velNoise,         "velNoise",         "Vel>Noise",          0.0f, 1.0f, 0.5f,  0.0f) \
    X(velDecay,         "velDecay",         "Vel>Decay",         -1.0f, 1.0f, 0.2f,  0.0f) \
    X(posSpread,        "posSpread",        "Pos Spread",         0.0f, 1.0f, 0.4f,  0.0f) \
    X(posDrift,         "posDrift",         "Pos Drift",          0.0f, 1.0f, 0.6f,  0.0f) \
    X(posModes,         "posModes",         "Pos>Modes",          0.0f, 1.0f, 0.7f,  0.0f) \
    X(posTransient,     "posTransient",     "Pos>Attack",         0.0f, 1.0f, 0.5f,  0.0f) \
    X(tuneSpread,       "tuneSpread",       "Tune Spread",        0.0f, 1.0f, 0.3f,  0.0f) \
    X(tuneWalk,         "tuneWalk",         "Tension Walk",       0.0f, 1.0f, 0.4f,  0.0f) \
    X(inharmJitter,     "inharmJitter",     "Inharmonic",         0.0f, 1.0f, 0.3f,  0.0f) \
    X(dampSpread,       "dampSpread",       "Damp Spread",        0.0f, 1.0f, 0.4f,  0.0f) \
    X(dampCoherence,    "dampCoherence",    "Damp Indep",         0.0f, 1.0f, 0.5f,  0.0f) \
    X(timeSpread,       "timeSpread",       "Time Spread",        0.0f, 1.0f, 0.3f,  0.0f) \
    X(timeBias,         "timeBias",         "Rush/Drag",         -1.0f, 1.0f, 0.0f,  0.0f) \
    X(velSpread,        "velSpread",        "Vel Spread",         0.0f, 1.0f, 0.3f,  0.0f) \
    X(noiseRegen,       "noiseRegen",       "Noise Regen",        0.0f, 1.0f, 0.6f,  0.0f) \
    X(noiseDecaySpread, "noiseDecaySpread", "Noise Decay",        0.0f, 1.0f, 0.4f,  0.0f) \
    X(noiseTiltSpread,  "noiseTiltSpread",  "Noise Tilt",         0.0f, 1.0f, 0.3f,  0.0f) \
    X(attackSpread,     "attackSpread",     "Attack Var",         0.0f, 1.0f, 0.3f,  0.0f) \
    X(attackTiltSpread, "attackTiltSpread", "Attack Tilt",        0.0f, 1.0f, 0.3f,  0.0f) \
    X(glideBase,        "glideBase",        "Pitch Glide",        0.0f, 1.0f, 0.15f, 0.0f) \
    X(glideTime,        "glideTime",        "Glide Time",         0.0f, 1.0f, 0.4f,  0.0f) \
    X(decayScale,       "decayScale",       "Decay",              0.5f, 2.0f, 1.0f,  1.0f) \
    X(modalTone,        "modalTone",        "Body/Noise",        -1.0f, 1.0f, 0.0f,  0.0f) \
    X(exprAmp,          "exprAmp",          "Expr>Level",         0.0f, 1.0f, 1.0f,  0.0f) \
    X(exprDecay,        "exprDecay",        "Expr>Decay",        -1.0f, 1.0f, 0.0f,  0.0f) \
    X(exprBright,       "exprBright",       "Expr>Bright",       -1.0f, 1.0f, 0.0f,  0.0f) \
    X(exprTransient,    "exprTransient",    "Expr>Attack",       -1.0f, 1.0f, 0.0f,  0.0f) \
    X(exprPitch,        "exprPitch",        "Expr>Pitch",        -1.0f, 1.0f, 0.0f,  0.0f) \
    X(seqSwing,         "seqSwing",         "Swing",              0.0f, 1.0f, 0.0f,  0.0f)

#define VELLUM_BOOL_PARAMS(X) \
    X(seqOn, "seqOn", "Groove On", false)

struct ParamRefs
{
#define DECL(field, id, name, mn, mx, def, skew) std::atomic<float>* field = nullptr;
    VELLUM_FLOAT_PARAMS (DECL)
#undef DECL
#define DECLB(field, id, name, def) std::atomic<float>* field = nullptr;
    VELLUM_BOOL_PARAMS (DECLB)
#undef DECLB

    void attach (juce::AudioProcessorValueTreeState& apvts)
    {
#define GET(field, id, name, mn, mx, def, skew) field = apvts.getRawParameterValue (id);
        VELLUM_FLOAT_PARAMS (GET)
#undef GET
#define GETB(field, id, name, def) field = apvts.getRawParameterValue (id);
        VELLUM_BOOL_PARAMS (GETB)
#undef GETB
    }

    HumanizeSettings humanize() const
    {
        HumanizeSettings s;
        s.sensitivity = *sensitivity; s.instability = *instability; s.space = *space; s.autoLevel = *autoLevel;
        s.velCurve = *velCurve; s.velBoost = *velBoost; s.velBright = *velBright; s.velGlide = *velGlide;
        s.velSnap = *velSnap; s.velNoise = *velNoise; s.velDecay = *velDecay;
        s.posSpread = *posSpread; s.posDrift = *posDrift; s.posModes = *posModes; s.posTransient = *posTransient;
        s.tuneSpread = *tuneSpread; s.tuneWalk = *tuneWalk; s.inharmJitter = *inharmJitter;
        s.dampSpread = *dampSpread; s.dampCoherence = *dampCoherence;
        s.timeSpread = *timeSpread; s.timeBias = *timeBias; s.velSpread = *velSpread;
        s.noiseRegen = *noiseRegen; s.noiseDecaySpread = *noiseDecaySpread; s.noiseTiltSpread = *noiseTiltSpread;
        s.attackSpread = *attackSpread; s.attackTiltSpread = *attackTiltSpread;
        s.glideBase = *glideBase; s.glideTime = *glideTime; s.decayScale = *decayScale; s.modalTone = *modalTone;
        s.exprAmp = *exprAmp; s.exprDecay = *exprDecay; s.exprBright = *exprBright; s.exprTransient = *exprTransient; s.exprPitch = *exprPitch;
        return s;
    }
};

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
#define ADD(field, id, name, mn, mx, def, skew) \
    { juce::NormalisableRange<float> r (mn, mx); if (skew > 0.0f) r.setSkewForCentre (skew); \
      layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def)); }
    VELLUM_FLOAT_PARAMS (ADD)
#undef ADD
#define ADDB(field, id, name, def) layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    VELLUM_BOOL_PARAMS (ADDB)
#undef ADDB
    return layout;
}

} // namespace vellum
