#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"
#include "Analyzer.h"
#include "AutoMix.h"
#include <future>
#include <atomic>

class BassDeamonProcessor final : public juce::AudioProcessor, private juce::Timer, private juce::AudioProcessorParameter::Listener {
public:
    BassDeamonProcessor();
    ~BassDeamonProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "BassDeamon"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void selectAB(int slot);
    void loadStartingPoint(int preset);
    double analyzerRefreshHz() const noexcept { return visualAnalyzerHz.load(std::memory_order_relaxed); }
    void setAnalyzerRefreshHz(double hz) noexcept {
        visualAnalyzerHz.store(float(std::isfinite(hz) ? std::round(std::clamp(hz,1.,30.) * 10) / 10 : 8.),
                               std::memory_order_relaxed);
    }
    bassdeamon::UnmaskResponse consumeUnmaskResponse() noexcept;
    bool usesRawTheme() const noexcept { return rawTheme.load(std::memory_order_relaxed); }
    void setRawTheme(bool useRaw) noexcept { rawTheme.store(useRaw,std::memory_order_relaxed); }
    int spectrumView() const noexcept { return visualSpectrumView.load(std::memory_order_relaxed); }
    void setSpectrumView(int view) noexcept { visualSpectrumView.store(view == 0 ? 0 : 1,std::memory_order_relaxed); }
    std::atomic<float> phaseReferenceHz{60};
    std::atomic<bool> phaseScopeEnabled{false};
    int selectedAB() const noexcept { return abSlot; }
    void requestKickEqLearn() noexcept { kickEqProgress.store(0); kickEqStatus.store(5); kickEqLearnRequested.store(true); }
    std::atomic<bool> compareDry{false}, compareMatch{false};
    std::atomic<float> comparisonGainDb{0};
    std::atomic<int> kickEqStatus{0}; // idle, recording, failed, locked, no sidechain, waiting
    std::atomic<float> kickEqProgress{0};
    void requestLearn() noexcept { learnStatus.store(7); learnRequested.store(true); }
    enum class AutoMixState { idle, armed, recording, captured, analyzing, ready };
    AutoMixState autoMixState() const noexcept { return autoState.load(std::memory_order_acquire); }
    void startAutoMix(int bars);
    void cancelAutoMix() noexcept;
    void applyAutoMix();
    void auditionAutoMix(int side, bool match) noexcept;
    const bassdeamon::AutoMixResult& autoMixResult() const noexcept { return autoResult; }
    std::atomic<float> autoMixProgress{0};
    bool undoEdit();
    bool redoEdit();
    void refreshHistory();
    bool canUndo() const { return !historyResetRequested.load()&&undoManager.canUndo(); }
    bool canRedo() const { return !historyResetRequested.load()&&undoManager.canRedo(); }
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState parameters;
    juce::AudioProcessLoadMeasurer cpuLoad;
    bassdeamon::AnalysisQueue analysisQueue;
    std::atomic<float> tubeGainDb{0},tubeAddedDb{-120},tempo{120},unmaskAttack{8},unmaskRelease{120},learnProgress{0};
    std::atomic<bool> tempoFromHost{false};
    std::atomic<float> glueReduction{0}, unmaskReduction{0}, outputPeak{0};
    std::array<std::atomic<float>, 3> agcMeters{};
    std::array<std::atomic<float>, bassdeamon::bandCount> spectrumBass{}, spectrumKick{}, spectrumCut{};
    std::atomic<float> kickPeak{0}, focusHz{75}, inputRmsDb{-120}, outputRmsDb{-120};
    std::atomic<float> glueAttack{20}, glueRelease{180}, unmaskQ{2};
    // 0 idle, 1 learning, 2 weak/insufficient, 3 ambiguous, 4 unstable, 5 accepted.
    std::atomic<int> learnStatus{0};
    std::atomic<float> learnCorrelation{0};
private:
    friend int interactionTests();
    friend int captureLoopGui(const juce::File&, const juce::File&, const juce::File&, int);
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    bassdeamon::Settings readSettings(bool proposal = false) const noexcept;
    void timerCallback() override;
    void parameterValueChanged(int,float) override {}
    void parameterGestureChanged(int,bool starting) override;
    void publishMeters() noexcept;
    void captureAutoMix(const juce::AudioBuffer<float>& bass, const juce::AudioBuffer<float>& kick,
                        bool playing, double bpm, double beatsPerBar, const bassdeamon::Settings& settings);
    void updateAutoMix();
    bassdeamon::Settings auditionSettings(bassdeamon::Settings settings) const noexcept;
    std::atomic<AutoMixState> autoState{AutoMixState::idle};
    std::atomic<bool> autoCancelled{false};
    std::atomic<int> autoAudition{0};
    std::atomic<bool> autoLevelMatch{true};
    std::vector<bassdeamon::AutoMixFrame> autoAudio;
    std::future<bassdeamon::AutoMixResult> autoWorker;
    bassdeamon::AutoMixResult autoResult;
    std::array<std::atomic<float>, 57> autoParameters{};
    std::array<float, 57> autoOriginal{};
    std::atomic<float> autoDryDb{0}, autoWetDb{0};
    bassdeamon::Settings autoBase;
    bassdeamon::Engine::RuntimeState autoRuntime{};
    size_t autoCount = 0, autoTarget = 0;
    int autoBars = 8, autoChannels = 2;
    bool autoLengthUnsupported = false;
    double autoRate = 48000, autoBpm = 120;
    bassdeamon::Engine engine;
    bassdeamon::KickEqCapture kickEqCapture;
    enum class CaptureState { idle, recording, ready };
    std::atomic<CaptureState> kickEqCaptureState{CaptureState::idle};
    std::atomic<bool> kickEqLearnRequested{false};
    std::atomic<unsigned> kickEqGeneration{0};
    unsigned captureGeneration = 0; // published with CaptureState::ready
    float captureStartFrequency = 0;
    std::array<std::atomic<float>*, 57> raw{};
    std::atomic<bool> learnRequested{false}, resultReady{false};
    std::atomic<float> resultDelay{0};
    std::atomic<bool> resultInvert{false};
    std::array<std::atomic<float>, 9> runtimeSnapshot{}, restoredRuntime{};
    std::array<juce::MemoryBlock, 2> abStates;
    void pushUnmaskResponse(const bassdeamon::UnmaskResponse&) noexcept;
    juce::AbstractFifo responseFifo{128};
    std::array<bassdeamon::UnmaskResponse, 128> responseFrames{};
    std::atomic<bool> rawTheme{false};
    std::atomic<int> visualSpectrumView{1};
    std::atomic<float> visualAnalyzerHz{8};
    int abSlot = 0; // message-thread only; A/B survives editor close/reopen.
    std::atomic<bool> restoreRequested{false};
    std::atomic<bool> historyResetRequested{false};
    bool wasLearning = false, wasPlaying = false, wasBypassed = false;
    double lastTimeSeconds = -1;
    int lastBlockSamples = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassDeamonProcessor)
};
