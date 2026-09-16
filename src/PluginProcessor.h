#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "dsp/Analyzer.h"
#include "dsp/Humanizer.h"
#include "dsp/Slicer.h"
#include "dsp/Voice.h"
#include "seq/Groove.h"
#include <array>
#include <random>

namespace vellum {

constexpr int kNumVoices = 48;
constexpr int kBaseNote = 36;   // C1 -> pad 1

struct HitEvent
{
    int pad = 0;
    float velocity = 0, posX = 0, posY = 0, tuneCents = 0, decayMul = 1, brightness = 0, timingMs = 0;
};

class VellumProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    VellumProcessor();
    ~VellumProcessor() override;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Vellum"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- parameters
    juce::AudioProcessorValueTreeState apvts;
    ParamRefs params;

    // ---- pads (message thread API)
    struct PadInfo
    {
        juce::String name;
        juce::String path;
        float userGainDb = 0.0f;
        int categoryOverride = -1;      // -1 = from filename / slicer
        DrumModelPtr model;
        bool analyzing = false;
    };
    const PadInfo& pad (int i) const { return pads[(size_t) i]; }
    void loadFiles (const juce::Array<juce::File>& files, int startPad, bool allowLoopDetection = true);   // bulk drop; loops may go to the slicer
    void loadFileToPad (const juce::File& file, int padIndex);
    void clearPad (int padIndex);
    void renamePad (int padIndex, const juce::String& name);
    void setPadGain (int padIndex, float db);
    void setPadCategory (int padIndex, int categoryOrMinusOne);   // re-levels
    void triggerPad (int padIndex, float velocity01);             // from the UI
    juce::ChangeBroadcaster padsChanged;                          // any pad metadata/model change

    // ---- slicer
    bool hasLoop() const { return ! loopAudio.empty(); }
    const std::vector<float>& getLoopAudio() const { return loopAudio; }
    double getLoopSampleRate() const { return loopSampleRate; }
    juce::String getLoopName() const { return loopName; }
    void setLoop (std::vector<float> mono, double sr, const juce::String& name);
    void loadLoopFile (const juce::File& file);                 // force a file into the slicer
    void runSlicer (float sensitivity, int maxPads);
    bool isSlicerBusy() const { return slicerBusy.load(); }
    const SliceResult& getSliceResult() const { return sliceResult; }
    void extractSlicesToPads (bool alsoExtractGroove);
    juce::ChangeBroadcaster slicerChanged;
    std::atomic<int> loopVersion { 0 };

    // ---- kits
    static juce::File kitsFolder();
    juce::Array<juce::File> listKits() const;
    void saveKit (const juce::String& name);
    bool loadKit (const juce::File& file);
    void loadKitByOffset (int delta);
    void resetToInit();                 // clear all pads, loop, pattern; all parameters back to default
    juce::String getKitName() const { return kitName; }
    void setKitName (const juce::String& n) { kitName = n; }

    // ---- sequencer
    Pattern& getPattern() { return pattern; }
    void applyPreset (int index);
    std::atomic<bool> internalPlay { false };
    int currentStep() const { return clock.currentStep(); }
    double currentBpm() const { return lastBpm.load(); }
    bool hostIsPlaying() const { return hostPlayingFlag.load(); }

    // ---- monitoring for the UI
    bool popHitEvent (HitEvent& e);
    float meterLeft() const { return meterL.load(); }
    float meterRight() const { return meterR.load(); }
    HitRealization lastRealization (int padIndex) const;
    DrumModelPtr modelForPad (int padIndex) const;
    float currentExpression() const { return expression.load(); }

    JUCE_DECLARE_WEAK_REFERENCEABLE (VellumProcessor)

private:
    void timerCallback() override;
    void syncModelsOnAudioThread();
    void installModel (int padIndex, std::shared_ptr<DrumModel> model, const juce::String& name, const juce::String& path, int categoryOverride);
    void startVoice (int padIndex, float velocity, int sampleOffset, const HumanizeSettings& s);
    void queueAnalysis (int padIndex, std::vector<float> mono, double sr, const juce::String& name, const juce::String& path, bool truncated, int categoryOverride);
    juce::ValueTree padsToTree() const;
    void padsFromTree (const juce::ValueTree& t);
    juce::ValueTree patternToTree() const;
    void patternFromTree (const juce::ValueTree& t);
    bool readAudioFile (const juce::File& f, std::vector<float>& mono, double& sr) const;

    // models: message-thread owned copies + audio-thread active set
    juce::SpinLock modelLock;
    std::array<DrumModelPtr, kNumPads> pendingModels;
    std::array<DrumModelPtr, kNumPads> activeModels;    // audio thread only
    std::vector<DrumModelPtr> retired;                  // freed on the message thread
    std::atomic<bool> modelsDirty { false };
    std::array<PadInfo, kNumPads> pads;
    std::array<float, kNumPads> padGainAtomic {};        // read on the audio thread
    std::array<std::atomic<float>, kNumPads> padGains;

    // voices
    std::array<Voice, kNumVoices> voices;
    Humanizer humanizer;
    uint64_t voiceOrder = 0;
    std::mt19937 rng { 99 };
    double sampleRate = 48000.0;
    double timeSeconds = 0.0;
    std::atomic<float> expression { 1.0f };

    // UI <-> audio queues
    struct ManualTrigger { int pad; float vel; };
    juce::AbstractFifo triggerFifo { 64 };
    std::array<ManualTrigger, 64> triggerSlots;
    juce::AbstractFifo hitFifo { 256 };
    std::array<HitEvent, 256> hitSlots;
    mutable juce::SpinLock realizationLock;
    std::array<HitRealization, kNumPads> lastHits;

    // sequencer
    Pattern pattern;
    GrooveClock clock;
    std::vector<Trigger> seqTriggers;
    std::atomic<double> lastBpm { 120.0 };
    std::atomic<bool> hostPlayingFlag { false };

    // bus
    struct Space
    {
        void prepare (double sr);
        void process (float* L, float* R, int n, float amount);
        std::vector<float> bufL, bufR; int wpos = 0, size = 1;
        int tapDelay[8]; float tapGain[8], tapPanL[8], tapPanR[8];
        int combDelayL = 1, combDelayR = 1; float lpL = 0, lpR = 0;
    } space;
    juce::dsp::IIR::Filter<float> liftL, liftR;
    float envFast = 0.0f, envSlow = 0.0f;
    std::atomic<float> meterL { 0.0f }, meterR { 0.0f };
    juce::AudioBuffer<float> scratch;

    // analysis / slicer jobs
    juce::ThreadPool pool { 1 };
    juce::AudioFormatManager formatManager;
    std::vector<float> loopAudio; double loopSampleRate = 48000.0; juce::String loopName;
    SliceResult sliceResult;
    std::atomic<bool> slicerBusy { false };
    int slicerJobCounter = 0;
    juce::String kitName { "Init" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VellumProcessor)
};

} // namespace vellum
