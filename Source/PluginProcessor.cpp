#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
constexpr std::array<const char*, 18> ids{
    "glue.on", "glue.amount", "glue.lock", "phase.on", "phase.invert", "phase.delay",
    "phase.rotate", "phase.frequency", "unmask.on", "unmask.amount", "unmask.delta",
    "glue.agc", "glue.agcLock", "phase.agc", "phase.agcLock", "unmask.agc", "unmask.agcLock",
    "output.trim"};
}

BassDeamonProcessor::BassDeamonProcessor()
    : AudioProcessor(BusesProperties().withInput("Bass", juce::AudioChannelSet::stereo(), true)
         .withInput("Kick reference", juce::AudioChannelSet::stereo(), false)
         .withOutput("Bass output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "BassDeamonState", layout()) {
    for (size_t i = 0; i < ids.size(); ++i) raw[i] = parameters.getRawParameterValue(ids[i]);
    const bassdeamon::Engine::RuntimeState initial{-24, 2, 20, 180, 0, 0, 0};
    for (size_t i = 0; i < initial.size(); ++i) runtimeSnapshot[i].store(static_cast<float>(initial[i]));
    startTimerHz(30);
}
BassDeamonProcessor::~BassDeamonProcessor() { stopTimer(); }

juce::AudioProcessorValueTreeState::ParameterLayout BassDeamonProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    const auto boolean = [&](const char* id, const char* name, bool initial) {
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, 1}, name, initial));
    };
    const auto continuous = [&](const char* id, const char* name, float lo, float hi, float initial) {
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name,
              juce::NormalisableRange<float>(lo, hi, 0.001f), initial));
    };
    boolean("glue.on", "GLUE active", true);
    continuous("glue.amount", "GLUE amount", 0, 1, 0.35f);
    boolean("glue.lock", "GLUE freeze adaptation", false);
    boolean("phase.on", "PHASE active", false);
    boolean("phase.invert", "PHASE invert polarity", false);
    continuous("phase.delay", "PHASE delay (ms)", 0, 5, 0);
    boolean("phase.rotate", "PHASE all-pass active", false);
    continuous("phase.frequency", "PHASE all-pass corner (Hz)", 20, 500, 90);
    boolean("unmask.on", "UNMASK active", true);
    continuous("unmask.amount", "UNMASK amount", 0, 1, 0.4f);
    boolean("unmask.delta", "UNMASK audition delta", false);
    for (const auto* module : {"glue", "phase", "unmask"}) {
        const juce::String prefix(module);
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{prefix + ".agc", 1}, prefix + " auto gain", false));
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{prefix + ".agcLock", 1}, prefix + " lock auto gain", false));
    }
    continuous("output.trim", "Output trim (dB)", -24, 12, 0);
    return p;
}

void BassDeamonProcessor::prepareToPlay(double sr, int) {
    engine.prepare(sr); setLatencySamples(0);
    bassdeamon::Engine::RuntimeState r{};
    const bool restoring = restoreRequested.load();
    for (size_t i = 0; i < r.size(); ++i) r[i] = restoring ? restoredRuntime[i].load() : runtimeSnapshot[i].load();
    engine.restoreRuntime(r);
    wasLearning = wasPlaying = wasBypassed = false; lastTimeSeconds = -1; lastBlockSamples = 0;
    learnStatus.store(0); resultReady.store(false);
}
bool BassDeamonProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    const auto main = b.getMainInputChannelSet();
    if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo()) return false;
    if (b.getMainOutputChannelSet() != main) return false;
    const auto side = b.getChannelSet(true, 1);
    return side.isDisabled() || side == juce::AudioChannelSet::mono() || side == juce::AudioChannelSet::stereo();
}
bassdeamon::Settings BassDeamonProcessor::readSettings() const noexcept {
    const auto value = [&](size_t i) { return raw[i]->load(std::memory_order_relaxed); };
    bassdeamon::Settings s;
    s.glue = value(0) > 0.5f; s.glueAmount = value(1); s.glueLocked = value(2) > 0.5f;
    s.phase = value(3) > 0.5f; s.invert = value(4) > 0.5f; s.delayMs = value(5);
    s.rotate = value(6) > 0.5f; s.rotationHz = value(7);
    s.unmask = value(8) > 0.5f; s.unmaskAmount = value(9); s.delta = value(10) > 0.5f;
    for (size_t i = 0; i < 3; ++i) s.agc[i] = {value(11+2*i) > 0.5f, value(12+2*i) > 0.5f};
    s.outputDb = value(17); return s;
}
void BassDeamonProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    auto bass = getBusBuffer(buffer, false, 0);
    auto side = getBusBuffer(buffer, true, 1);
    if (restoreRequested.exchange(false)) {
        bassdeamon::Engine::RuntimeState restored{};
        for (size_t i = 0; i < restored.size(); ++i) restored[i] = restoredRuntime[i].load();
        engine.restoreRuntime(restored); engine.clearHistory(); wasLearning = false;
        resultReady.store(false); learnStatus.store(0);
    }
    double bpm = 0;
    if (auto* playhead = getPlayHead()) {
        if (const auto position = playhead->getPosition()) {
            if (const auto tempo = position->getBpm()) bpm = *tempo;
            const bool playing = position->getIsPlaying();
            bool seek = false;
            if (const auto t = position->getTimeInSeconds()) {
                seek = playing && wasPlaying && lastTimeSeconds >= 0
                    && std::abs(*t - lastTimeSeconds - lastBlockSamples/getSampleRate()) > 0.1;
                lastTimeSeconds = *t;
            }
            if (seek || (wasPlaying && !playing)) {
                engine.clearHistory(); wasLearning = false; learnStatus.store(0);
            }
            wasPlaying = playing;
        }
    }
    lastBlockSamples = buffer.getNumSamples();
    if (wasBypassed) { engine.clearHistory(); wasLearning = false; learnStatus.store(0); wasBypassed = false; }
    if (learnRequested.exchange(false)) {
        resultReady.store(false); engine.startLearn(); wasLearning = true; learnStatus.store(1);
    }
    const float* kick[2]{nullptr, nullptr};
    const int sideChannels = side.getNumChannels();
    for (int c = 0; c < sideChannels; ++c) kick[c] = side.getReadPointer(c);
    engine.process(bass.getArrayOfWritePointers(), bass.getNumChannels(),
                   sideChannels > 0 ? kick : nullptr, sideChannels, bass.getNumSamples(), readSettings(), bpm);
    if (wasLearning && !engine.learning()) {
        const auto r = engine.learnResult();
        if (r.valid) {
            resultDelay.store(static_cast<float>(r.delayMs)); resultInvert.store(r.invert);
            learnCorrelation.store(static_cast<float>(r.correlation));
            resultReady.store(true); learnStatus.store(0);
        } else learnStatus.store(2);
    }
    wasLearning = engine.learning(); publishMeters();
}
void BassDeamonProcessor::processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer& midi) {
    midi.clear(); wasBypassed = true;
    // Matching main input/output layouts and zero algorithmic latency: leave bass untouched.
}
void BassDeamonProcessor::publishMeters() noexcept {
    const auto& m = engine.getMeters();
    glueReduction.store(static_cast<float>(m.glueReductionDb));
    unmaskReduction.store(static_cast<float>(m.unmaskReductionDb));
    outputPeak.store(static_cast<float>(m.peak));
    for (size_t i = 0; i < 3; ++i) agcMeters[i].store(static_cast<float>(m.agcDb[i]));
    const auto runtime = engine.runtimeState();
    for (size_t i = 0; i < runtime.size(); ++i) runtimeSnapshot[i].store(static_cast<float>(runtime[i]));
}
void BassDeamonProcessor::timerCallback() {
    if (!resultReady.exchange(false)) return;
    // Host parameter notifications happen on the message thread, never in the audio callback.
    const auto set = [&](const char* id, float value) {
        auto* p = parameters.getParameter(id);
        p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(value)); p->endChangeGesture();
    };
    set("phase.delay", resultDelay.load()); set("phase.invert", resultInvert.load() ? 1.0f : 0.0f);
    // Learn does not silently enable PHASE. User can compare with the active switch.
}
void BassDeamonProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = parameters.copyState(); state.setProperty("schemaVersion", 1, nullptr);
    for (size_t i = 0; i < runtimeSnapshot.size(); ++i)
        state.setProperty("runtime" + juce::String(static_cast<int>(i)), runtimeSnapshot[i].load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}
void BassDeamonProcessor::setStateInformation(const void* data, int bytes) {
    if (auto xml = getXmlFromBinary(data, bytes)) {
        if (xml->hasTagName(parameters.state.getType()) && xml->getIntAttribute("schemaVersion", 1) == 1) {
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
            const bassdeamon::Engine::RuntimeState defaults{-24, 2, 20, 180, 0, 0, 0};
            for (size_t i = 0; i < defaults.size(); ++i) {
                const double value = xml->getDoubleAttribute("runtime" + juce::String(static_cast<int>(i)), defaults[i]);
                restoredRuntime[i].store(static_cast<float>(std::isfinite(value) ? value : defaults[i]));
            }
            restoreRequested.store(true);
        }
    }
}
juce::AudioProcessorEditor* BassDeamonProcessor::createEditor() { return new BassDeamonEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BassDeamonProcessor(); }
