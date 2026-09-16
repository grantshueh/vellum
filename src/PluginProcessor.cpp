#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace vellum {

namespace {
juce::String base64FromMono (const std::vector<float>& x)
{
    juce::MemoryBlock mb (x.size() * sizeof (int16_t));
    auto* d = static_cast<int16_t*> (mb.getData());
    for (size_t i = 0; i < x.size(); ++i) d[i] = (int16_t) juce::jlimit (-32768.0f, 32767.0f, std::round (x[i] * 32767.0f));
    return mb.toBase64Encoding();
}

std::vector<float> monoFromBase64 (const juce::String& s)
{
    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding (s)) return {};
    const auto* d = static_cast<const int16_t*> (mb.getData());
    std::vector<float> out (mb.getSize() / sizeof (int16_t));
    for (size_t i = 0; i < out.size(); ++i) out[i] = (float) d[i] / 32767.0f;
    return out;
}

struct AnalysisJob : public juce::ThreadPoolJob
{
    AnalysisJob (juce::WeakReference<VellumProcessor> p, std::function<void (VellumProcessor&)> fn)
        : juce::ThreadPoolJob ("vellum-job"), proc (p), work (std::move (fn)) {}
    JobStatus runJob() override
    {
        if (auto* p = proc.get()) work (*p);
        return jobHasFinished;
    }
    juce::WeakReference<VellumProcessor> proc;
    std::function<void (VellumProcessor&)> work;
};
} // namespace

// ---------------------------------------------------------------------------
VellumProcessor::VellumProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    params.attach (apvts);
    formatManager.registerBasicFormats();
    retired.reserve (256);
    seqTriggers.reserve (256);
    for (int i = 0; i < kNumPads; ++i)
    {
        pads[(size_t) i].name = defaultPadName (i);
        padGains[(size_t) i].store (0.0f);
    }
    pattern = patternFromPreset (groovePresets()[0]);
    startTimer (200);
}

VellumProcessor::~VellumProcessor()
{
    stopTimer();
    pool.removeAllJobs (true, 8000);
}

bool VellumProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void VellumProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    for (auto& v : voices) v.prepare (sr);
    humanizer.prepare (sr);
    space.prepare (sr);
    liftL.reset(); liftR.reset();
    liftL.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 8000.0, 0.7f, 1.0f);
    liftR.coefficients = liftL.coefficients;
    scratch.setSize (2, std::max (16, samplesPerBlock));
    envFast = envSlow = 0.0f;
    clock.reset();
    modelsDirty.store (true);
}

// ---------------------------------------------------------------------------
// Space: early reflections + two damped combs
void VellumProcessor::Space::prepare (double sr)
{
    size = std::max (16, (int) (sr * 0.12));
    bufL.assign ((size_t) size, 0.0f); bufR.assign ((size_t) size, 0.0f);
    static const float ms[8] = { 7.3f, 11.9f, 17.1f, 23.4f, 29.8f, 36.5f, 43.7f, 52.2f };
    for (int i = 0; i < 8; ++i)
    {
        tapDelay[i] = std::min (size - 1, (int) (ms[i] * 0.001 * sr));
        tapGain[i] = 0.65f * std::pow (0.78f, (float) i);
        tapPanL[i] = (i & 1) ? 0.45f : 0.85f;
        tapPanR[i] = (i & 1) ? 0.85f : 0.45f;
    }
    combDelayL = std::min (size - 1, (int) (0.041 * sr));
    combDelayR = std::min (size - 1, (int) (0.047 * sr));
    wpos = 0; lpL = lpR = 0.0f;
}

void VellumProcessor::Space::process (float* L, float* R, int n, float amount)
{
    if (amount <= 0.0005f) return;
    const float wet = amount * 0.55f;
    const float widen = 1.0f + amount * 0.6f;
    for (int i = 0; i < n; ++i)
    {
        const float m = 0.5f * (L[i] + R[i]);
        // early reflections from the mono history (bufL doubles as the input history)
        float eL = 0.0f, eR = 0.0f;
        for (int t = 0; t < 8; ++t)
        {
            int idx = wpos - tapDelay[t]; if (idx < 0) idx += size;
            const float s = bufL[(size_t) idx] * tapGain[t];
            eL += s * tapPanL[t]; eR += s * tapPanR[t];
        }
        // damped feedback combs (bufR holds comb L in first half? keep separate rings via offsets)
        int il = wpos - combDelayL; if (il < 0) il += size;
        int ir = wpos - combDelayR; if (ir < 0) ir += size;
        const float cl = bufR[(size_t) il], cr = bufR[(size_t) ir];
        lpL += 0.25f * (cl - lpL); lpR += 0.25f * (cr - lpR);
        const float fbL = m + lpL * 0.42f, fbR = m + lpR * 0.42f;
        bufL[(size_t) wpos] = m;
        bufR[(size_t) wpos] = 0.5f * (fbL + fbR);
        if (++wpos >= size) wpos = 0;

        const float mid = 0.5f * (L[i] + R[i]), side = 0.5f * (L[i] - R[i]) * widen;
        L[i] = mid + side + wet * (eL + lpL * 0.5f);
        R[i] = mid - side + wet * (eR + lpR * 0.5f);
    }
}

// ---------------------------------------------------------------------------
void VellumProcessor::syncModelsOnAudioThread()
{
    if (! modelsDirty.load()) return;
    juce::SpinLock::ScopedTryLockType lock (modelLock);
    if (! lock.isLocked()) return;
    for (int i = 0; i < kNumPads; ++i)
    {
        if (pendingModels[(size_t) i] != activeModels[(size_t) i])
        {
            if (activeModels[(size_t) i] && retired.size() < retired.capacity()) retired.push_back (activeModels[(size_t) i]);
            activeModels[(size_t) i] = pendingModels[(size_t) i];
        }
    }
    modelsDirty.store (false);
}

void VellumProcessor::startVoice (int padIndex, float velocity, int offset, const HumanizeSettings& s)
{
    if (padIndex < 0 || padIndex >= kNumPads) return;
    const auto& model = activeModels[(size_t) padIndex];
    if (! model || ! model->isValid()) return;

    // per-pad polyphony limit: fade the oldest voice of this pad
    int count = 0; Voice* oldestOfPad = nullptr;
    for (auto& v : voices)
        if (v.isActive() && v.pad() == padIndex) { ++count; if (! oldestOfPad || v.order() < oldestOfPad->order()) oldestOfPad = &v; }
    if (count >= 6 && oldestOfPad) oldestOfPad->fastRelease();

    Voice* target = nullptr;
    for (auto& v : voices) if (! v.isActive()) { target = &v; break; }
    if (! target)
    {
        target = &voices[0];
        for (auto& v : voices) if (v.order() < target->order()) target = &v;
    }

    HitRealization h = humanizer.realize (padIndex, velocity, expression.load(), *model, s,
                                          padGains[(size_t) padIndex].load(), timeSeconds + offset / sampleRate);
    h.delaySamples += offset;
    target->start (model, h, ++voiceOrder);

    {
        juce::SpinLock::ScopedTryLockType l (realizationLock);
        if (l.isLocked()) lastHits[(size_t) padIndex] = h;
    }
    int s1, n1, s2, n2;
    hitFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0)
    {
        HitEvent& e = hitSlots[(size_t) s1];
        e.pad = padIndex; e.velocity = h.velocity; e.posX = h.posX; e.posY = h.posY;
        e.tuneCents = h.tuneCents; e.decayMul = h.decayMul; e.brightness = h.brightness; e.timingMs = h.timingMs;
        hitFifo.finishedWrite (1);
    }
}

void VellumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (scratch.getNumSamples() < n) scratch.setSize (2, n, false, false, true);
    scratch.clear();
    float* L = scratch.getWritePointer (0);
    float* R = scratch.getWritePointer (1);

    syncModelsOnAudioThread();
    const HumanizeSettings s = params.humanize();

    // ---- transport
    double bpm = 120.0, ppq = 0.0; bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b > 0 ? *b : 120.0;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            hostPlaying = pos->getIsPlaying();
        }
    lastBpm.store (bpm); hostPlayingFlag.store (hostPlaying);

    // ---- sequencer
    seqTriggers.clear();
    const bool seqOn = params.seqOn->load() > 0.5f;
    if (seqOn)
    {
        const_cast<Pattern&> (pattern).swing = *params.seqSwing;
        clock.process (pattern, hostPlaying, ppq, bpm, internalPlay.load(), sampleRate, n, seqTriggers, rng);
    }
    else if (! internalPlay.load()) clock.reset();
    for (const auto& t : seqTriggers) startVoice (t.pad, t.vel, t.sampleOffset, s);

    // ---- MIDI
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int padIndex = ((m.getNoteNumber() - kBaseNote) % kNumPads + kNumPads) % kNumPads;
            startVoice (padIndex, m.getFloatVelocity(), meta.samplePosition, s);
        }
        else if (m.isController() && m.getControllerNumber() == 11)
            expression.store ((float) m.getControllerValue() / 127.0f);
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& v : voices) v.fastRelease();
    }

    // ---- manual triggers from the UI
    {
        int s1, n1, s2, n2;
        triggerFifo.prepareToRead (triggerFifo.getNumReady(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) startVoice (triggerSlots[(size_t) (s1 + i)].pad, triggerSlots[(size_t) (s1 + i)].vel, 0, s);
        for (int i = 0; i < n2; ++i) startVoice (triggerSlots[(size_t) (s2 + i)].pad, triggerSlots[(size_t) (s2 + i)].vel, 0, s);
        triggerFifo.finishedRead (n1 + n2);
    }

    // ---- voices
    for (auto& v : voices) if (v.isActive()) v.render (L, R, n);

    // ---- bus: space, punch, drive, lift, ceiling, level
    space.process (L, R, n, *params.space);

    const float punch = *params.punch;
    const float drive = *params.drive;
    const float lift = *params.lift;
    const float ceiling = *params.ceiling;
    const float level = juce::Decibels::decibelsToGain (params.level->load());
    if (lift > 0.001f)
    {
        *liftL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 8000.0, 0.7f, juce::Decibels::decibelsToGain (6.0f * lift));
        liftR.coefficients = liftL.coefficients;
    }
    const float aFast = 1.0f - std::exp (-1.0f / (0.0003f * (float) sampleRate)), rFast = 1.0f - std::exp (-1.0f / (0.015f * (float) sampleRate));
    const float aSlow = 1.0f - std::exp (-1.0f / (0.008f * (float) sampleRate)), rSlow = 1.0f - std::exp (-1.0f / (0.12f * (float) sampleRate));
    const float g = 1.0f + 6.0f * drive, gComp = std::pow (g, -0.6f);
    const float T = std::pow (10.0f, -12.0f * ceiling / 20.0f);
    float pkL = 0.0f, pkR = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        if (punch > 0.001f)
        {
            const float a = std::fabs (l) + std::fabs (r);
            envFast += (a > envFast ? aFast : rFast) * (a - envFast);
            envSlow += (a > envSlow ? aSlow : rSlow) * (a - envSlow);
            const float ratio = (envFast + 1e-5f) / (envSlow + 1e-5f);
            const float pg = juce::jlimit (0.5f, 3.0f, std::pow (ratio, punch * 0.8f));
            l *= pg; r *= pg;
        }
        if (drive > 0.001f)
        {
            l = l + drive * (std::tanh (g * l) * gComp - l);
            r = r + drive * (std::tanh (g * r) * gComp - r);
        }
        if (lift > 0.001f) { l = liftL.processSample (l); r = liftR.processSample (r); }
        l *= level; r *= level;
        // ceiling: soft clip above 0.5 T, saturating at T
        auto soft = [T] (float x)
        {
            const float ax = std::fabs (x), half = 0.5f * T;
            if (ax <= half) return x;
            const float y = half + half * std::tanh ((ax - half) / half);
            return x < 0 ? -y : y;
        };
        l = soft (l); r = soft (r);
        l = juce::jlimit (-0.999f, 0.999f, l); r = juce::jlimit (-0.999f, 0.999f, r);   // output protection
        L[i] = l; R[i] = r;
        pkL = std::max (pkL, std::fabs (l)); pkR = std::max (pkR, std::fabs (r));
    }
    meterL.store (std::max (pkL, meterL.load() * 0.85f));
    meterR.store (std::max (pkR, meterR.load() * 0.85f));

    if (buffer.getNumChannels() >= 2) { buffer.copyFrom (0, 0, L, n); buffer.copyFrom (1, 0, R, n); }
    else if (buffer.getNumChannels() == 1) { buffer.copyFrom (0, 0, L, n); buffer.addFrom (0, 0, R, n); buffer.applyGain (0.5f); }

    timeSeconds += n / sampleRate;
    midi.clear();
}

// ---------------------------------------------------------------------------
void VellumProcessor::timerCallback()
{
    std::vector<DrumModelPtr> toFree;
    {
        juce::SpinLock::ScopedLockType lock (modelLock);
        toFree.swap (retired);
        retired.reserve (256);
    }
    toFree.clear();
}

bool VellumProcessor::popHitEvent (HitEvent& e)
{
    int s1, n1, s2, n2;
    hitFifo.prepareToRead (1, s1, n1, s2, n2);
    if (n1 <= 0) return false;
    e = hitSlots[(size_t) s1];
    hitFifo.finishedRead (1);
    return true;
}

HitRealization VellumProcessor::lastRealization (int padIndex) const
{
    juce::SpinLock::ScopedLockType l (realizationLock);
    return lastHits[(size_t) juce::jlimit (0, kNumPads - 1, padIndex)];
}

DrumModelPtr VellumProcessor::modelForPad (int padIndex) const
{
    if (padIndex < 0 || padIndex >= kNumPads) return nullptr;
    return pads[(size_t) padIndex].model;
}

void VellumProcessor::triggerPad (int padIndex, float velocity01)
{
    int s1, n1, s2, n2;
    triggerFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) { triggerSlots[(size_t) s1] = { juce::jlimit (0, kNumPads - 1, padIndex), juce::jlimit (0.02f, 1.0f, velocity01) }; triggerFifo.finishedWrite (1); }
}

// ---------------------------------------------------------------------------
// pads
bool VellumProcessor::readAudioFile (const juce::File& f, std::vector<float>& mono, double& sr) const
{
    std::unique_ptr<juce::AudioFormatReader> reader (const_cast<juce::AudioFormatManager&> (formatManager).createReaderFor (f));
    if (! reader) return false;
    const int64_t len = std::min<int64_t> (reader->lengthInSamples, (int64_t) (reader->sampleRate * 60.0));
    if (len <= 0) return false;
    juce::AudioBuffer<float> buf ((int) reader->numChannels, (int) len);
    reader->read (&buf, 0, (int) len, 0, true, true);
    mono.assign ((size_t) len, 0.0f);
    const float g = 1.0f / (float) std::max (1u, reader->numChannels);
    for (int c = 0; c < (int) reader->numChannels; ++c)
    {
        const float* d = buf.getReadPointer (c);
        for (int i = 0; i < (int) len; ++i) mono[(size_t) i] += d[i] * g;
    }
    sr = reader->sampleRate;
    return true;
}

void VellumProcessor::queueAnalysis (int padIndex, std::vector<float> mono, double sr, const juce::String& name,
                                     const juce::String& path, bool truncated, int categoryOverride)
{
    if (padIndex < 0 || padIndex >= kNumPads) return;
    pads[(size_t) padIndex].analyzing = true;
    pads[(size_t) padIndex].name = name;
    padsChanged.sendChangeMessage();
    auto data = std::make_shared<std::vector<float>> (std::move (mono));
    juce::WeakReference<VellumProcessor> weak (this);
    pool.addJob (new AnalysisJob (weak, [=] (VellumProcessor&)
    {
        AnalysisOptions opt; opt.truncated = truncated; opt.variantSeed = 1234 + padIndex * 31;
        const DrumCategory forced = categoryOverride >= 0 ? (DrumCategory) categoryOverride : DrumCategory::Count;
        auto model = analyzeDrum (data->data(), (int) data->size(), sr, name.toStdString(), path.toStdString(), opt, forced);
        juce::MessageManager::callAsync ([weak, model, padIndex, name, path, categoryOverride]
        {
            if (auto* p = weak.get()) p->installModel (padIndex, model, name, path, categoryOverride);
        });
    }), true);
}

void VellumProcessor::installModel (int padIndex, std::shared_ptr<DrumModel> model, const juce::String& name, const juce::String& path, int categoryOverride)
{
    auto& p = pads[(size_t) padIndex];
    p.analyzing = false;
    p.name = name; p.path = path; p.categoryOverride = categoryOverride;
    p.model = model;
    {
        juce::SpinLock::ScopedLockType lock (modelLock);
        pendingModels[(size_t) padIndex] = model;
    }
    modelsDirty.store (true);
    padsChanged.sendChangeMessage();
}

void VellumProcessor::loadFileToPad (const juce::File& file, int padIndex)
{
    juce::Array<juce::File> files; files.add (file);
    loadFiles (files, padIndex);
}

void VellumProcessor::loadFiles (const juce::Array<juce::File>& filesIn, int startPad, bool allowLoopDetection)
{
    // flatten folders, keep only readable audio, natural sort
    juce::Array<juce::File> files;
    for (const auto& f : filesIn)
    {
        if (f.isDirectory())
        {
            auto inner = f.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a;*.caf");
            inner.sort();
            for (const auto& g : inner) files.addIfNotAlreadyThere (g);
        }
        else if (formatManager.findFormatForFileExtension (f.getFileExtension()))
            files.addIfNotAlreadyThere (f);
    }
    if (filesIn.size() > 1) files.sort();

    int cursor = juce::jlimit (0, kNumPads - 1, startPad);
    for (const auto& f : files)
    {
        if (cursor >= kNumPads) break;
        const int padIndex = cursor++;
        const juce::String name = f.getFileNameWithoutExtension();
        const juce::String path = f.getFullPathName();
        pads[(size_t) padIndex].analyzing = true;
        pads[(size_t) padIndex].name = name;
        padsChanged.sendChangeMessage();

        juce::WeakReference<VellumProcessor> weak (this);
        pool.addJob (new AnalysisJob (weak, [=] (VellumProcessor& proc)
        {
            std::vector<float> mono; double sr = 48000.0;
            const bool ok = proc.readAudioFile (f, mono, sr);
            {
                auto log = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Vellum").getChildFile ("drag-log.txt");
                log.appendText (juce::Time::getCurrentTime().toString (true, true) + "  [load] pad " + juce::String (padIndex + 1) + " " + f.getFullPathName()
                                + (ok ? " read OK (" + juce::String (mono.size()) + " smp)" : " READ FAILED exists=" + juce::String ((int) f.existsAsFile())) + "\n");
            }
            if (! ok)
            {
                juce::MessageManager::callAsync ([weak, padIndex]
                {
                    // leave a visible trace instead of silently reverting (sandboxed hosts can stat but not read)
                    if (auto* p = weak.get()) { p->pads[(size_t) padIndex].analyzing = false; p->pads[(size_t) padIndex].name = "Can't read file"; p->padsChanged.sendChangeMessage(); }
                });
                return;
            }
            // long file with many strong hits spread across it -> a loop: send it to the slicer instead of a pad
            bool isLoop = false;
            if (allowLoopDetection && mono.size() > (size_t) (sr * 1.5))
            {
                auto onsets = detectOnsets (mono.data(), (int) mono.size(), sr, 0.5f);
                float globalPeak = 0.0f; for (float v : mono) globalPeak = std::max (globalPeak, std::fabs (v));
                int strong = 0, lastStrong = 0;
                for (int o : onsets)
                {
                    float pk = 0.0f;
                    for (int i = o; i < std::min ((int) mono.size(), o + (int) (0.03 * sr)); ++i) pk = std::max (pk, std::fabs (mono[(size_t) i]));
                    if (pk > 0.25f * globalPeak) { ++strong; lastStrong = o; }
                }
                isLoop = strong >= 6 && lastStrong > (int) (mono.size() * 0.4);
            }
            {
                auto log = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Vellum").getChildFile ("drag-log.txt");
                log.appendText (juce::Time::getCurrentTime().toString (true, true) + "  [load] " + name + (isLoop ? " -> LOOP (slicer)" : " -> one-shot, analysing") + "\n");
            }
            if (isLoop)
            {
                auto shared = std::make_shared<std::vector<float>> (std::move (mono));
                juce::MessageManager::callAsync ([weak, shared, sr, name, padIndex]
                {
                    if (auto* p = weak.get())
                    {
                        auto& pad = p->pads[(size_t) padIndex];
                        pad.analyzing = false;
                        pad.name = pad.model ? juce::String (pad.model->name) : juce::String (defaultPadName (padIndex));
                        p->setLoop (std::move (*shared), sr, name);
                        p->padsChanged.sendChangeMessage();
                    }
                });
                return;
            }
            AnalysisOptions opt; opt.variantSeed = 1234 + padIndex * 31;
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            auto model = analyzeDrum (mono.data(), (int) mono.size(), sr, name.toStdString(), path.toStdString(), opt);
            {
                auto log = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Vellum").getChildFile ("drag-log.txt");
                log.appendText (juce::Time::getCurrentTime().toString (true, true) + "  [analysis] " + name + ": " + juce::String (model->modes.size()) + " modes, valid="
                                + juce::String ((int) model->isValid()) + ", " + juce::String (juce::Time::getMillisecondCounterHiRes() - t0, 0) + " ms\n");
            }
            juce::MessageManager::callAsync ([weak, model, padIndex, name, path]
            {
                if (auto* p = weak.get())
                {
                    p->installModel (padIndex, model, name, path, -1);
                    auto log = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Vellum").getChildFile ("drag-log.txt");
                    log.appendText (juce::Time::getCurrentTime().toString (true, true) + "  [install] pad " + juce::String (padIndex + 1) + " <- " + name + "\n");
                }
            });
        }), true);
    }
}

void VellumProcessor::loadLoopFile (const juce::File& f)
{
    if (! formatManager.findFormatForFileExtension (f.getFileExtension())) return;
    juce::WeakReference<VellumProcessor> weak (this);
    const juce::String name = f.getFileNameWithoutExtension();
    pool.addJob (new AnalysisJob (weak, [=] (VellumProcessor& proc)
    {
        std::vector<float> mono; double sr = 48000.0;
        if (! proc.readAudioFile (f, mono, sr)) return;
        auto shared = std::make_shared<std::vector<float>> (std::move (mono));
        juce::MessageManager::callAsync ([weak, shared, sr, name]
        {
            if (auto* p = weak.get()) p->setLoop (std::move (*shared), sr, name);
        });
    }), true);
}

void VellumProcessor::clearPad (int padIndex)
{
    if (padIndex < 0 || padIndex >= kNumPads) return;
    auto& p = pads[(size_t) padIndex];
    p.model = nullptr; p.path.clear(); p.name = defaultPadName (padIndex); p.analyzing = false; p.categoryOverride = -1;
    { juce::SpinLock::ScopedLockType lock (modelLock); pendingModels[(size_t) padIndex] = nullptr; }
    modelsDirty.store (true);
    padsChanged.sendChangeMessage();
}

void VellumProcessor::renamePad (int padIndex, const juce::String& name)
{
    if (padIndex < 0 || padIndex >= kNumPads) return;
    pads[(size_t) padIndex].name = name;
    padsChanged.sendChangeMessage();
}

void VellumProcessor::setPadGain (int padIndex, float db)
{
    if (padIndex < 0 || padIndex >= kNumPads) return;
    pads[(size_t) padIndex].userGainDb = db;
    padGains[(size_t) padIndex].store (db);
    padsChanged.sendChangeMessage();
}

void VellumProcessor::setPadCategory (int padIndex, int cat)
{
    if (padIndex < 0 || padIndex >= kNumPads) return;
    auto& p = pads[(size_t) padIndex];
    p.categoryOverride = cat;
    if (p.model)
    {
        auto copy = std::make_shared<DrumModel> (*p.model);
        copy->category = cat >= 0 ? (DrumCategory) cat : classifyFilename (copy->name);
        LevelAnalysis la; la.peakDb = copy->peakDb; la.rmsDb = copy->rmsDb;
        copy->autoGainDb = computeAutoGainDb (la, copy->category);
        installModel (padIndex, copy, p.name, p.path, cat);
    }
}

// ---------------------------------------------------------------------------
// slicer
void VellumProcessor::setLoop (std::vector<float> mono, double sr, const juce::String& name)
{
    loopAudio = std::move (mono); loopSampleRate = sr; loopName = name;
    sliceResult = SliceResult{};
    loopVersion.fetch_add (1);
    slicerChanged.sendChangeMessage();
    runSlicer (0.5f, 8);
}

void VellumProcessor::runSlicer (float sensitivity, int maxPads)
{
    if (loopAudio.empty()) return;
    slicerBusy.store (true);
    auto data = std::make_shared<std::vector<float>> (loopAudio);
    const double sr = loopSampleRate;
    const int version = loopVersion.load();
    const int job = ++slicerJobCounter;
    SlicerOptions opt; opt.sensitivity = sensitivity; opt.maxPads = juce::jlimit (1, kNumPads, maxPads); opt.hostBpm = lastBpm.load();
    juce::WeakReference<VellumProcessor> weak (this);
    pool.addJob (new AnalysisJob (weak, [=] (VellumProcessor&)
    {
        auto result = std::make_shared<SliceResult> (sliceLoop (data->data(), (int) data->size(), sr, opt));
        juce::MessageManager::callAsync ([weak, result, version, job]
        {
            if (auto* p = weak.get())
            {
                if (version == p->loopVersion.load()) p->sliceResult = *result;      // ignore results for a replaced loop
                if (job == p->slicerJobCounter) p->slicerBusy.store (false);        // last requested job finished
                p->slicerChanged.sendChangeMessage();
            }
        });
    }), true);
}

void VellumProcessor::extractSlicesToPads (bool alsoGroove)
{
    if (! sliceResult.valid() || loopAudio.empty()) return;
    const int k = std::min (kNumPads, sliceResult.numClusters);
    for (int c = 0; c < k; ++c)
    {
        const int si = sliceResult.representative[(size_t) c];
        if (si < 0) continue;
        const auto& s = sliceResult.slices[(size_t) si];
        const int start = juce::jlimit (0, (int) loopAudio.size() - 1, s.startSample);
        const int len = juce::jlimit (1, (int) loopAudio.size() - start, s.length);
        std::vector<float> audio (loopAudio.begin() + start, loopAudio.begin() + start + len);
        queueAnalysis (c, std::move (audio), loopSampleRate, sliceResult.clusterNames[(size_t) c],
                       loopName + " #" + juce::String (c + 1), true, (int) sliceResult.clusterCategories[(size_t) c]);
    }
    if (alsoGroove)
    {
        Pattern p;
        p.length = juce::jlimit (16, kMaxSteps, sliceResult.bars * 16);
        p.name = loopName.toStdString();
        const double stepSec = 60.0 / (sliceResult.bpm * 4.0);
        float maxE = 1e-6f; for (const auto& s : sliceResult.slices) maxE = std::max (maxE, s.energy);
        for (const auto& s : sliceResult.slices)
        {
            const int step = (int) std::lround (s.startSample / loopSampleRate / stepSec);
            if (step < 0 || step >= p.length || s.cluster >= kNumPads) continue;
            Step& st = p.steps[s.cluster][step];
            st.on = true; st.vel = juce::jlimit (0.2f, 1.0f, 0.3f + 0.7f * s.energy / maxE);
        }
        pattern = p;
        if (auto* sw = apvts.getParameter ("seqSwing")) sw->setValueNotifyingHost (0.0f);
    }
}

// ---------------------------------------------------------------------------
// sequencer
void VellumProcessor::applyPreset (int index)
{
    const auto& presets = groovePresets();
    if (index < 0 || index >= (int) presets.size()) return;
    pattern = patternFromPreset (presets[(size_t) index]);
    if (auto* sw = apvts.getParameter ("seqSwing")) sw->setValueNotifyingHost (presets[(size_t) index].swing);
}

// ---------------------------------------------------------------------------
// state / kits
juce::ValueTree VellumProcessor::padsToTree() const
{
    juce::ValueTree t ("PADS");
    for (int i = 0; i < kNumPads; ++i)
    {
        const auto& p = pads[(size_t) i];
        juce::ValueTree pt ("PAD");
        pt.setProperty ("idx", i, nullptr);
        pt.setProperty ("name", p.name, nullptr);
        pt.setProperty ("path", p.path, nullptr);
        pt.setProperty ("gain", p.userGainDb, nullptr);
        pt.setProperty ("cat", p.categoryOverride, nullptr);
        if (p.model && p.model->isValid())
        {
            pt.setProperty ("sr", p.model->sampleRate, nullptr);
            pt.setProperty ("truncated", p.model->truncated, nullptr);
            pt.setProperty ("audio", base64FromMono (p.model->source), nullptr);
        }
        t.appendChild (pt, nullptr);
    }
    return t;
}

void VellumProcessor::padsFromTree (const juce::ValueTree& t)
{
    for (int i = 0; i < kNumPads; ++i) clearPad (i);
    for (const auto& pt : t)
    {
        const int i = (int) pt.getProperty ("idx", -1);
        if (i < 0 || i >= kNumPads) continue;
        auto& p = pads[(size_t) i];
        p.name = pt.getProperty ("name", defaultPadName (i)).toString();
        p.path = pt.getProperty ("path", "").toString();
        p.userGainDb = (float) pt.getProperty ("gain", 0.0f);
        padGains[(size_t) i].store (p.userGainDb);
        p.categoryOverride = (int) pt.getProperty ("cat", -1);
        const juce::String audio = pt.getProperty ("audio", "").toString();
        if (audio.isNotEmpty())
        {
            auto mono = monoFromBase64 (audio);
            queueAnalysis (i, std::move (mono), (double) pt.getProperty ("sr", 48000.0), p.name, p.path,
                           (bool) pt.getProperty ("truncated", false), p.categoryOverride);
        }
    }
    padsChanged.sendChangeMessage();
}

juce::ValueTree VellumProcessor::patternToTree() const
{
    juce::ValueTree t ("PATTERN");
    t.setProperty ("length", pattern.length, nullptr);
    t.setProperty ("name", juce::String (pattern.name), nullptr);
    for (int pad = 0; pad < kNumPads; ++pad)
    {
        juce::String row;
        for (int s = 0; s < kMaxSteps; ++s)
        {
            const Step& st = pattern.steps[pad][s];
            if (st.on) row << s << ":" << juce::String (st.vel, 3) << ":" << juce::String (st.prob, 3) << ";";
        }
        if (row.isNotEmpty()) { juce::ValueTree r ("ROW"); r.setProperty ("pad", pad, nullptr); r.setProperty ("steps", row, nullptr); t.appendChild (r, nullptr); }
    }
    return t;
}

void VellumProcessor::patternFromTree (const juce::ValueTree& t)
{
    Pattern p;
    p.length = juce::jlimit (1, kMaxSteps, (int) t.getProperty ("length", 16));
    p.name = t.getProperty ("name", "Pattern").toString().toStdString();
    for (const auto& r : t)
    {
        const int pad = (int) r.getProperty ("pad", -1);
        if (pad < 0 || pad >= kNumPads) continue;
        juce::StringArray items; items.addTokens (r.getProperty ("steps", "").toString(), ";", "");
        for (const auto& it : items)
        {
            juce::StringArray f; f.addTokens (it, ":", "");
            if (f.size() < 3) continue;
            const int s = f[0].getIntValue();
            if (s < 0 || s >= kMaxSteps) continue;
            p.steps[pad][s].on = true; p.steps[pad][s].vel = f[1].getFloatValue(); p.steps[pad][s].prob = f[2].getFloatValue();
        }
    }
    pattern = p;
}

void VellumProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("Vellum");
    root.setProperty ("kit", kitName, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (padsToTree(), nullptr);
    root.appendChild (patternToTree(), nullptr);
    juce::MemoryOutputStream mos (destData, false);
    root.writeToStream (mos);
}

void VellumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ValueTree root = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (! root.isValid() || ! root.hasType ("Vellum")) return;
    kitName = root.getProperty ("kit", "Init").toString();
    auto ps = root.getChildWithName (apvts.state.getType());
    if (ps.isValid()) apvts.replaceState (ps);
    auto pt = root.getChildWithName ("PADS");
    if (pt.isValid()) padsFromTree (pt);
    auto pat = root.getChildWithName ("PATTERN");
    if (pat.isValid()) patternFromTree (pat);
    padsChanged.sendChangeMessage();
}

juce::File VellumProcessor::kitsFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Vellum").getChildFile ("Kits");
    f.createDirectory();
    return f;
}

juce::Array<juce::File> VellumProcessor::listKits() const
{
    auto files = kitsFolder().findChildFiles (juce::File::findFiles, false, "*.vkit");
    files.sort();
    return files;
}

void VellumProcessor::saveKit (const juce::String& name)
{
    const juce::String clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return;
    kitName = clean;
    juce::MemoryBlock mb;
    getStateInformation (mb);
    kitsFolder().getChildFile (clean + ".vkit").replaceWithData (mb.getData(), mb.getSize());
    padsChanged.sendChangeMessage();
}

bool VellumProcessor::loadKit (const juce::File& file)
{
    juce::MemoryBlock mb;
    if (! file.loadFileAsData (mb)) return false;
    setStateInformation (mb.getData(), (int) mb.getSize());
    kitName = file.getFileNameWithoutExtension();
    padsChanged.sendChangeMessage();
    return true;
}

void VellumProcessor::resetToInit()
{
    for (int i = 0; i < kNumPads; ++i) clearPad (i);
    for (int i = 0; i < kNumPads; ++i) { pads[(size_t) i].userGainDb = 0.0f; padGains[(size_t) i].store (0.0f); }
    loopAudio.clear(); loopName.clear(); sliceResult = SliceResult{}; loopVersion.fetch_add (1);
    pattern = Pattern{}; pattern.clear(); pattern.name = "Empty";
    internalPlay.store (false);
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
    kitName = "Init";
    humanizer.reset();
    slicerChanged.sendChangeMessage();
    padsChanged.sendChangeMessage();
}

void VellumProcessor::loadKitByOffset (int delta)
{
    auto kits = listKits();
    if (kits.isEmpty()) return;
    int idx = -1;
    for (int i = 0; i < kits.size(); ++i) if (kits[i].getFileNameWithoutExtension() == kitName) idx = i;
    idx = idx < 0 ? (delta > 0 ? 0 : kits.size() - 1) : ((idx + delta) % kits.size() + kits.size()) % kits.size();
    loadKit (kits[idx]);
}

juce::AudioProcessorEditor* VellumProcessor::createEditor() { return new VellumEditor (*this); }

} // namespace vellum

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new vellum::VellumProcessor(); }
