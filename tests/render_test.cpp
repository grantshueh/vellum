// Offline check of the resynthesis engine: analyse a one-shot, verify the identity
// render reproduces it, then render humanised hits and report how they differ.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/Analyzer.h"
#include "dsp/Humanizer.h"
#include "dsp/Voice.h"
#include <cstdio>

using namespace vellum;

static void writeWav (const juce::File& f, const std::vector<float>& L, const std::vector<float>& R, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    if (! w) return;
    juce::AudioBuffer<float> buf (2, (int) L.size());
    buf.copyFrom (0, 0, L.data(), (int) L.size());
    buf.copyFrom (1, 0, R.data(), (int) R.size());
    w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

static float centroid (const float* x, int n, double sr)
{
    juce::dsp::FFT fft (12);
    const int N = 4096;
    std::vector<float> buf ((size_t) N * 2, 0.0f);
    for (int i = 0; i < std::min (n, N); ++i) buf[(size_t) i] = x[i] * (0.5f - 0.5f * std::cos (6.2831853f * i / N));
    fft.performRealOnlyForwardTransform (buf.data(), true);
    double num = 0, den = 0;
    for (int b = 1; b < N / 2; ++b) { const double p = buf[(size_t) b * 2] * buf[(size_t) b * 2] + buf[(size_t) b * 2 + 1] * buf[(size_t) b * 2 + 1]; num += p * b * sr / N; den += p; }
    return den > 0 ? (float) (num / den) : 0.0f;
}

static float rmsDb (const std::vector<float>& x) { double s = 0; for (float v : x) s += (double) v * v; return (float) (10.0 * std::log10 (std::max (s / std::max<size_t> (1, x.size()), 1e-20))); }

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: render_test <in.wav> <outdir>\n"); return 1; }
    juce::File in (argv[1]); juce::File outDir (argv[2]); outDir.createDirectory();
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (in));
    if (! reader) { std::printf ("cannot read %s\n", argv[1]); return 1; }
    const int n = (int) std::min<int64_t> (reader->lengthInSamples, (int64_t) (reader->sampleRate * 8));
    juce::AudioBuffer<float> buf ((int) reader->numChannels, n);
    reader->read (&buf, 0, n, 0, true, true);
    std::vector<float> mono ((size_t) n, 0.0f);
    for (int c = 0; c < (int) reader->numChannels; ++c) for (int i = 0; i < n; ++i) mono[(size_t) i] += buf.getReadPointer (c)[i] / (float) reader->numChannels;
    const double sr = reader->sampleRate;

    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto model = analyzeDrum (mono.data(), n, sr, in.getFileNameWithoutExtension().toStdString(), in.getFullPathName().toStdString());
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    std::printf ("%s\n  %d samples @ %.0f Hz, analysed in %.0f ms\n", in.getFileName().toRawUTF8(), model->numSamples, sr, ms);
    std::printf ("  category %s, peak %.1f dBFS, rms200 %.1f dBFS -> autoGain %+.1f dB\n", categoryName (model->category), model->peakDb, model->rmsDb, model->autoGainDb);
    std::printf ("  %zu modes explain %.0f%% of the energy; lowest %.1f Hz; tail tau %.0f ms; transient %zu smp; tail %d(+%zu ext) smp; duration %.2f s\n",
                 model->modes.size(), model->modalEnergyRatio * 100, model->lowestModeHz, model->tailTau * 1000, model->transient.size(),
                 model->tailOriginalLength, model->tail.size() - (size_t) model->tailOriginalLength, model->durationSec);
    if (! model->tail.empty())
    {
        const int L50 = std::min ((int) (sr * 0.05), (int) model->tail.size());
        std::printf ("  tail flatness %.3f (%s); centroid tail %.0f Hz", model->tailFlatness, model->tailStochastic ? "stochastic" : "tonal, kept verbatim", centroid (model->tail.data(), L50, sr));
        if (! model->tailVariants.empty() && ! model->tailVariants[0].empty()) std::printf (", variant %.0f Hz", centroid (model->tailVariants[0].data(), std::min (L50, (int) model->tailVariants[0].size()), sr));
        std::printf ("; transient centroid %.0f Hz\n", centroid (model->transient.data(), (int) model->transient.size(), sr));
    }
    for (size_t k = 0; k < std::min<size_t> (6, model->modes.size()); ++k)
        std::printf ("    mode %zu: %.1f Hz, tau %.0f ms, amp %.3f\n", k, model->modes[k].freq, model->modes[k].tau * 1000, model->modes[k].amp);

    // ---- identity render
    Humanizer hum; hum.prepare (sr);
    HumanizeSettings ident; ident.instability = 0; ident.sensitivity = 0; ident.autoLevel = 0; ident.noiseRegen = 0; ident.glideBase = 0; ident.velBoost = 0; ident.velCurve = 1.5f;
    ident.timeSpread = 0; ident.timeBias = 0; ident.velSpread = 0; ident.posSpread = 0; ident.space = 0;
    const int len = model->numSamples;
    auto render = [&] (const HitRealization& h, int length, std::vector<float>& L, std::vector<float>& R)
    {
        Voice v; v.prepare (sr); v.start (model, h, 1);
        L.assign ((size_t) length, 0.0f); R.assign ((size_t) length, 0.0f);
        for (int i = 0; i < length && v.isActive(); i += 256) v.render (L.data() + i, R.data() + i, std::min (256, length - i));
    };
    std::vector<float> L, R;
    HitRealization hid = hum.realize (0, 1.0f, 1.0f, *model, ident, 0.0f, 0.0);
    render (hid, len, L, R);
    double ex = 0, ee = 0;
    for (int i = 0; i < len; ++i) { const double y = (L[(size_t) i] + R[(size_t) i]) / std::sqrt (2.0); const double x = model->source[(size_t) i]; ex += x * x; ee += (x - y) * (x - y); }
    std::printf ("  identity render SNR: %.1f dB (Δz = 0 reproduces the source)\n", 10.0 * std::log10 (ex / std::max (ee, 1e-20)));
    {
        // modal-only diagnostic: voice with the residual removed vs (source - residual)
        auto modalOnly = std::make_shared<DrumModel> (*model);
        modalOnly->transient.clear(); modalOnly->tail.clear(); modalOnly->tailVariants.clear();
        Voice v; v.prepare (sr); v.start (modalOnly, hid, 1);
        std::vector<float> ML ((size_t) len, 0.0f), MR ((size_t) len, 0.0f);
        for (int i = 0; i < len && v.isActive(); i += 256) v.render (ML.data() + i, MR.data() + i, std::min (256, len - i));
        double em = 0, ed = 0;
        for (int i = 0; i < len; ++i)
        {
            double ref = model->source[(size_t) i];
            if (i < (int) model->transient.size()) ref -= model->transient[(size_t) i];
            const int ti = i - model->tailOffset;
            if (ti >= 0 && ti < (int) model->tail.size()) ref -= model->tail[(size_t) ti];
            const double y = (ML[(size_t) i] + MR[(size_t) i]) / std::sqrt (2.0);
            em += ref * ref; ed += (ref - y) * (ref - y);
        }
        std::printf ("  modal-only SNR vs analysis reconstruction: %.1f dB (modal energy %.1f dB rel. source)\n", 10.0 * std::log10 (em / std::max (ed, 1e-20)), 10.0 * std::log10 (em / std::max (ex, 1e-20)));
    }
    writeWav (outDir.getChildFile (in.getFileNameWithoutExtension() + "_identity.wav"), L, R, sr);

    // ---- humanised family
    HumanizeSettings hs; hs.autoLevel = 0; hs.timeSpread = 0; hs.timeBias = 0;
    hum.reset();
    const int hitLen = (int) (sr * std::min (2.0, (double) model->durationSec * 1.5 + 0.2));
    const float c0 = centroid (model->source.data(), std::min (len, 4096), sr);
    std::vector<float> roll, rollR;
    const int spacing = (int) (sr * 0.5);
    roll.assign ((size_t) (spacing * 9 + hitLen), 0.0f); rollR = roll;
    std::vector<std::vector<float>> hits;
    std::printf ("  humanised hits at velocity 0.85 (instability 0.5):\n");
    for (int k = 0; k < 8; ++k)
    {
        HitRealization h = hum.realize (0, 0.85f, 1.0f, *model, hs, 0.0f, k * 0.5);
        render (h, hitLen, L, R);
        if (k == 0)
        {
            // which component moves? render each alone with the same realisation
            auto part = [&] (bool keepModes, bool keepTr, bool keepTail, const char* label, const float* ref, int refLen)
            {
                auto pm = std::make_shared<DrumModel> (*model);
                if (! keepModes) pm->modes.clear();
                if (! keepTr) pm->transient.clear();
                if (! keepTail) { pm->tail.clear(); pm->tailVariants.clear(); }
                Voice v; v.prepare (sr); v.start (pm, h, 1);
                std::vector<float> PL ((size_t) hitLen, 0.0f), PR ((size_t) hitLen, 0.0f);
                for (int i = 0; i < hitLen && v.isActive(); i += 256) v.render (PL.data() + i, PR.data() + i, std::min (256, hitLen - i));
                std::vector<float> pmono ((size_t) hitLen); for (int i = 0; i < hitLen; ++i) pmono[(size_t) i] = PL[(size_t) i] + PR[(size_t) i];
                const int off = keepTail && ! keepTr && ! keepModes ? model->tailOffset : 0;
                std::printf ("      %-9s centroid %5.0f Hz vs source component %5.0f Hz, rms %.1f dB vs %.1f dB\n", label,
                             centroid (pmono.data() + off, std::min (hitLen - off, 4096), sr), centroid (ref, std::min (refLen, 4096), sr),
                             rmsDb (pmono), 10.0 * std::log10 (std::max (1e-20, [&] { double s = 0; for (int i = 0; i < refLen; ++i) s += (double) ref[i] * ref[i]; return s / std::max (1, hitLen); }())) + 3.0);
            };
            std::vector<float> modalRef ((size_t) len);
            for (int i = 0; i < len; ++i)
            {
                double r = model->source[(size_t) i];
                if (i < (int) model->transient.size()) r -= model->transient[(size_t) i];
                const int ti = i - model->tailOffset;
                if (ti >= 0 && ti < (int) model->tail.size()) r -= model->tail[(size_t) ti];
                modalRef[(size_t) i] = (float) r;
            }
            part (true, false, false, "modal", modalRef.data(), len);
            part (false, true, false, "transient", model->transient.data(), (int) model->transient.size());
            part (false, false, true, "tail", model->tail.data(), std::min ((int) model->tail.size(), model->tailOriginalLength));
        }
        std::vector<float> m ((size_t) hitLen); for (int i = 0; i < hitLen; ++i) m[(size_t) i] = L[(size_t) i] + R[(size_t) i];
        const float c = centroid (m.data(), std::min (hitLen, 4096), sr);
        double corr = 0, ea = 0, eb = 0;
        for (int i = 0; i < std::min (hitLen, len); ++i) { corr += m[(size_t) i] * model->source[(size_t) i]; ea += m[(size_t) i] * m[(size_t) i]; eb += model->source[(size_t) i] * model->source[(size_t) i]; }
        std::printf ("    hit %d: tune %+5.1f c  pos (%+.2f,%+.2f)  decay x%.2f  bright %+.2f  centroid %5.0f Hz (%+.0f%%)  rms %.1f dB  corr(x) %.3f\n",
                     k + 1, h.tuneCents, h.posX, h.posY, h.decayMul, h.brightness, c, (c / c0 - 1) * 100, rmsDb (m), corr / std::sqrt (ea * eb + 1e-20));
        for (int i = 0; i < hitLen; ++i) { roll[(size_t) (k * spacing + i)] += L[(size_t) i]; rollR[(size_t) (k * spacing + i)] += R[(size_t) i]; }
        hits.push_back (std::move (m));
    }
    // original at the end for comparison
    for (int i = 0; i < len && 8 * spacing + i < (int) roll.size(); ++i) { roll[(size_t) (8 * spacing + i)] += model->source[(size_t) i] * 0.7071f; rollR[(size_t) (8 * spacing + i)] += model->source[(size_t) i] * 0.7071f; }
    writeWav (outDir.getChildFile (in.getFileNameWithoutExtension() + "_family.wav"), roll, rollR, sr);

    // ---- velocity sweep
    std::vector<float> sw, swR; sw.assign ((size_t) (spacing * 6 + hitLen), 0.0f); swR = sw;
    hum.reset();
    HumanizeSettings vs; vs.autoLevel = 0; vs.instability = 0.15f; vs.timeSpread = 0;
    for (int k = 0; k < 6; ++k)
    {
        const float vel = 0.2f + 0.16f * k;
        HitRealization h = hum.realize (0, vel, 1.0f, *model, vs, 0.0f, k * 0.5);
        render (h, hitLen, L, R);
        std::vector<float> m ((size_t) hitLen); for (int i = 0; i < hitLen; ++i) m[(size_t) i] = L[(size_t) i] + R[(size_t) i];
        std::printf ("    vel %.2f -> gain %+.1f dB, rms %.1f dB, centroid %.0f Hz, glide %.1f%%\n", vel, 20 * std::log10 (h.gainLin), rmsDb (m), centroid (m.data(), std::min (hitLen, 4096), sr), h.glideAmount * 100);
        for (int i = 0; i < hitLen; ++i) { sw[(size_t) (k * spacing + i)] += L[(size_t) i]; swR[(size_t) (k * spacing + i)] += R[(size_t) i]; }
    }
    writeWav (outDir.getChildFile (in.getFileNameWithoutExtension() + "_velocity.wav"), sw, swR, sr);
    std::printf ("  wrote %s/{identity,family,velocity}.wav\n", outDir.getFullPathName().toRawUTF8());
    return 0;
}
