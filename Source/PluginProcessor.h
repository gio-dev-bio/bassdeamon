#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"
#include "Analyzer.h"
#include "KickEqLearn.h"
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
    int analyzerRate() const noexcept { return visualAnalyzerRate.load(std::memory_order_relaxed); }
    void setAnalyzerRate(int value) noexcept { visualAnalyzerRate.store(std::clamp(value,0,3),std::memory_order_relaxed); }
    int spectrumView() const noexcept { return visualSpectrumView.load(std::memory_order_relaxed); }
    void setSpectrumView(int view) noexcept { visualSpectrumView.store(juce::jlimit(0,3,view),std::memory_order_relaxed); }
    int selectedAB() const noexcept { return abSlot; }
    void requestKickEqLearn() noexcept { kickEqLearnRequested.store(true); }
    std::atomic<bool> compareDry{false}, compareMatch{false};
    std::atomic<float> comparisonGainDb{0};
    std::atomic<int> kickEqStatus{0}; // idle, recording, failed, locked
    void requestLearn() noexcept { learnRequested.store(true); }
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
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    bassdeamon::Settings readSettings() const noexcept;
    void timerCallback() override;
    void parameterValueChanged(int,float) override {}
    void parameterGestureChanged(int,bool starting) override;
    void publishMeters() noexcept;
    bassdeamon::Engine engine;
    bassdeamon::KickEqCapture kickEqCapture;
    enum class CaptureState { idle, recording, ready };
    std::atomic<CaptureState> kickEqCaptureState{CaptureState::idle};
    std::atomic<bool> kickEqLearnRequested{false};
    std::atomic<unsigned> kickEqGeneration{0};
    unsigned captureGeneration = 0; // published with CaptureState::ready
    float captureStartFrequency = 0;
    std::array<std::atomic<float>*, 55> raw{};
    std::atomic<bool> learnRequested{false}, resultReady{false};
    std::atomic<float> resultDelay{0};
    std::atomic<bool> resultInvert{false};
    std::array<std::atomic<float>, 9> runtimeSnapshot{}, restoredRuntime{};
    std::array<juce::MemoryBlock, 2> abStates;
    std::atomic<int> visualSpectrumView{1},visualAnalyzerRate{2};
    int abSlot = 0; // message-thread only; A/B survives editor close/reopen.
    std::atomic<bool> restoreRequested{false};
    std::atomic<bool> historyResetRequested{false};
    bool wasLearning = false, wasPlaying = false, wasBypassed = false;
    double lastTimeSeconds = -1;
    int lastBlockSamples = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassDeamonProcessor)
};
