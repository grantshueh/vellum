# Vellum — drum resynthesis instrument (AU / standalone, macOS)

Bring any one-shot to life. Vellum analyses a drum sample into a physical-ish latent
model and plays back *neighbouring strikes on the same object* instead of the same file
with random detune.

```
x  --E-->  z  = { modes (f, τ, a, φ, attack) , transient , stochastic tail (24-band decays) }
z' = z + Δz    Δz shaped by velocity, strike position, tension drift, damping, stick contact
x̂  = D(z')     modal bank + phase-coherent residual playback + re-rendered noise tail
```

Δz = 0 reproduces the source at 95–100 dB SNR (see `make render-test`), so the model is
lossless at rest; every perturbation is a deliberate, physically-motivated departure.

## Build (Command Line Tools only)

```bash
make -j10 au standalone      # build/Vellum.component + build/Vellum.app
make install                 # copies the AU to ~/Library/Audio/Plug-Ins/Components
make validate                # auval -v aumu Vel1 Vlum
make tests                   # auto-level behaviour tests (pure C++)
make render-test && ./build/render_test some_snare.wav build/out   # engine check + wav renders
make slice-test  && ./build/slice_test  loop.wav 125              # slicer check
make ui-snapshot && ./build/ui_snapshot build/out a.wav b.wav     # offscreen UI PNGs
```

Restart Logic Pro after `make install` so it rescans Audio Units. Vellum shows up under
*AU Instruments ▸ Vellum*. Pads are C1 (36) upward; any octave wraps onto the 12 pads.
CC11 is expression.

## Layout

| Area | What |
|------|------|
| `src/dsp/Leveler.h`   | filename classification, peak / 200 ms RMS analysis, category targets, static `autoGainDb` |
| `src/dsp/GainStage.h` | source / performance / user gain architecture (velocity curve, expression) |
| `src/dsp/Analyzer.cpp`| E(x): STFT peak tracking → matched-filter refinement → joint least squares with attack ramps → residual-driven (f, τ) refinement → transient/tail split → noise-band model → stochastic variants |
| `src/dsp/Humanizer.cpp` | Δz: velocity, position random walk (nodal weighting), tension walk, damping, timing, expression routing |
| `src/dsp/Voice.cpp`   | D(z'): 48-mode recursive phasor bank, phase-coherent residual resampling, tail regen mix |
| `src/dsp/Slicer.cpp`  | superflux onsets, timbre features, k-means → pads, bar/tempo guess |
| `src/seq/Groove.cpp`  | 12×32 step pattern, 16 presets, host / internal clock with swing |
| `src/PluginProcessor.cpp` | voices, MIDI, sequencer, model hot-swap, bus (space, punch, drive, lift, ceiling), kits, state |
| `src/ui/`             | look-and-feel, pad strip with strike visualisation, samples / kits / slicer / groove / advanced views |

## Gain staging

`autoGainDb` is a property of the source sample (never recomputed for pitch, velocity or
expression). Final voice gain = `autoGainDb·autoLevel + velocityGainDb + expressionGainDb + padGainDb`.
Velocity uses `v^1.5` (0 dB at 127, no boost by default); expression maps 0/0.5/1 → −12/−6/0 dB and
can also be routed to decay, brightness, attack and pitch in the Advanced panel.

## Known limits

- Modes are fixed-frequency exponentials; strongly gliding tones (808s) end up mostly in the
  residual, which is still played back coherently but only tuning/glide perturbations apply to it.
- Loop extraction slices at onsets and picks one representative per timbre cluster; overlapping
  hits are not separated, their cut-off tails are extrapolated from the noise model.
- Mono engine (stereo sources are summed); SPACE adds early reflections and width.
