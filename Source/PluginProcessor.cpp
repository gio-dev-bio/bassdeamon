#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <optional>

namespace {
constexpr std::array<const char*, 47> ids{
    "glue.on", "glue.amount", "glue.lock", "phase.on", "phase.invert", "phase.delay",
    "phase.rotate", "phase.frequency", "unmask.on", "unmask.amount", "unmask.delta",
    "glue.agc", "glue.agcLock", "phase.agc", "phase.agcLock", "unmask.agc", "unmask.agcLock",
    "output.trim", "global.on", "unmask.track", "unmask.hold", "unmask.frequency",
    "unmask.width", "unmask.attack", "unmask.release", "unmask.maxCut",
    "tube.on","tube.drive","tube.amount","tube.source","tube.colour","tube.agc","tube.agcLock","tube.audition",
    "glue.timing","glue.attackMs","glue.releaseMs","glue.attackNote","glue.releaseNote",
    "unmask.timing","unmask.attackNote","unmask.releaseNote","view.range","view.hold",
    "view.spectrum","view.minHz","view.maxHz"};
}

BassDeamonProcessor::BassDeamonProcessor()
    : AudioProcessor(BusesProperties().withInput("Bass", juce::AudioChannelSet::stereo(), true)
         .withInput("Kick reference", juce::AudioChannelSet::stereo(), false)
         .withOutput("Bass output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, &undoManager, "BassDeamonState", layout()) {
    for (size_t i = 0; i < ids.size(); ++i) raw[i] = parameters.getRawParameterValue(ids[i]);
    const bassdeamon::Engine::RuntimeState initial{-24, 2, 20, 180, 0, 0, 0, 75,0};
    for (size_t i = 0; i < initial.size(); ++i) runtimeSnapshot[i].store(static_cast<float>(initial[i]));
    for (auto& v : spectrumBass) v.store(-120);
    for (auto& v : spectrumKick) v.store(-120);
    parameters.copyState();undoManager.clearUndoHistory();
    for(auto* parameter:getParameters())parameter->addListener(this);
    startTimerHz(30);
}
BassDeamonProcessor::~BassDeamonProcessor() { stopTimer();for(auto* parameter:getParameters())parameter->removeListener(this); }

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
    boolean("global.on", "Processing active", true);
    boolean("unmask.track", "UNMASK auto focus", true);
    boolean("unmask.hold", "UNMASK hold focus", false);
    continuous("unmask.frequency", "UNMASK manual focus (Hz)", 30, 250, 75);
    continuous("unmask.width", "UNMASK width", 0, 1, 0.55f);
    continuous("unmask.attack", "UNMASK attack (ms)", 1, 50, 8);
    continuous("unmask.release", "UNMASK release (ms)", 30, 500, 120);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"unmask.maxCut", 1},
        "UNMASK cut ceiling", juce::StringArray{"3 dB", "6 dB", "9 dB", "12 dB"}, 2));
    boolean("tube.on","Soft Tube active",false);
    continuous("tube.drive","Soft Tube drive (dB)",0,18,6);
    continuous("tube.amount","Soft Tube amount",0,1,.35f);
    continuous("tube.source","Soft Tube source low-pass (Hz)",60,500,180);
    continuous("tube.colour","Soft Tube colour",0,1,.45f);
    boolean("tube.agc","Soft Tube auto gain",false);
    boolean("tube.agcLock","Soft Tube gain lock",false);
    boolean("tube.audition","Soft Tube audition added branch",false);
    const juce::StringArray notes{"1/64","1/32","1/16","1/8","1/4","1/2","1/1","1/8 T","1/8 D"};
    const auto choice=[&](const char* id,const char* name,const juce::StringArray& values,int initial){
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id,1},name,values,initial));
    };
    choice("glue.timing","GLUE timing",{"AUTO","MS","SYNC"},0);
    continuous("glue.attackMs","GLUE manual attack (ms)",.1f,500,20);
    continuous("glue.releaseMs","GLUE manual release (ms)",1,4000,180);
    choice("glue.attackNote","GLUE attack note",notes,0);choice("glue.releaseNote","GLUE release note",notes,2);
    choice("unmask.timing","UNMASK timing",{"MS","SYNC"},0);
    choice("unmask.attackNote","UNMASK attack note",notes,0);choice("unmask.releaseNote","UNMASK release note",notes,2);
    choice("view.range","Spectrum range",{"0-1000 Hz","0-2500 Hz","0-5000 Hz"},0);
    boolean("view.hold","Scope hold",false);
    choice("view.spectrum","Spectrum signal",{"PRE","POST"},1);
    continuous("view.minHz","Spectrum from (Hz)",0,19980,0);
    continuous("view.maxHz","Spectrum to (Hz)",20,20000,1000);
    return p;
}

void BassDeamonProcessor::prepareToPlay(double sr, int maximumBlock) {
    cpuLoad.reset(sr,std::max(1,maximumBlock));
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
    // Canonicalise the declared 0.001 dB grid in double: JUCE's normalised
    // float roundtrip can otherwise turn nominal 0 dB into a tiny boost.
    s.outputDb = std::round(static_cast<double>(value(17)) * 1000.0) / 1000.0;
    s.unmaskTrack = value(19) > 0.5f; s.unmaskHold = value(20) > 0.5f;
    s.unmaskFrequency = value(21); s.unmaskWidth = value(22);
    s.unmaskAttackMs = value(23); s.unmaskReleaseMs = value(24);
    s.unmaskMaxCutDb = 3 * (1 + std::clamp(static_cast<int>(value(25)), 0, 3));
    s.active=value(18)>.5f;
    s.tube.enabled=value(26)>.5f;s.tube.driveDb=value(27);s.tube.amount=value(28);
    s.tube.sourceHz=value(29);s.tube.colour=value(30);s.tube.autoGain=value(31)>.5f;
    s.tube.gainLock=value(32)>.5f;s.tube.audition=value(33)>.5f;
    s.glueTiming=int(value(34));s.glueAttackMs=value(35);s.glueReleaseMs=value(36);
    s.glueAttackNote=int(value(37));s.glueReleaseNote=int(value(38));s.unmaskSync=value(39)>.5f;
    s.unmaskAttackNote=int(value(40));s.unmaskReleaseNote=int(value(41));
    return s;
}
void BassDeamonProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    std::optional<juce::AudioProcessLoadMeasurer::ScopedTimer> cpuTimer;
    if(buffer.getNumSamples()>0)cpuTimer.emplace(cpuLoad,buffer.getNumSamples());
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
    double bpm = 0,ppq=-1;
    if (auto* playhead = getPlayHead()) {
        if (const auto position = playhead->getPosition()) {
            if (const auto hostTempo = position->getBpm()) bpm = *hostTempo;
            const bool playing = position->getIsPlaying();
            if(playing)if(const auto q=position->getPpqPosition())ppq=*q;
            bool seek = false;
            if (const auto t = position->getTimeInSeconds()) {
                seek = playing && wasPlaying && lastTimeSeconds >= 0
                    && std::abs(*t - lastTimeSeconds - lastBlockSamples/getSampleRate()) > 0.1;
                lastTimeSeconds = *t;
            }
            if (seek || (wasPlaying && !playing)) {
                const bool keepLearn=seek&&playing&&engine.learning();
                engine.clearHistory(keepLearn);
                if(!keepLearn){wasLearning=false;learnStatus.store(0);}
            }
            wasPlaying = playing;
        }
    }
    tempo.store(float(std::isfinite(bpm)&&bpm>=20&&bpm<=400?bpm:120));
    tempoFromHost.store(std::isfinite(bpm)&&bpm>=20&&bpm<=400);
    lastBlockSamples = buffer.getNumSamples();
    if (wasBypassed) { engine.clearHistory(); wasLearning = false; learnStatus.store(0); wasBypassed = false; }
    const auto settings=readSettings();
    if(!settings.active){engine.cancelLearn();wasLearning=false;learnStatus.store(0);resultReady.store(false);}
    if (learnRequested.exchange(false)&&settings.active) {
        resultReady.store(false); engine.startLearn(); wasLearning = true; learnStatus.store(1);
    }
    const float* kick[2]{nullptr, nullptr};
    const int sideChannels = side.getNumChannels();
    for (int c = 0; c < sideChannels; ++c) kick[c] = side.getReadPointer(c);
    if(!analysisQueue.enabled.load(std::memory_order_relaxed)){
        engine.process(bass.getArrayOfWritePointers(),bass.getNumChannels(),
                       sideChannels>0?kick:nullptr,sideChannels,bass.getNumSamples(),settings,bpm);
    }else{
        // Fixed stack workspace handles host blocks larger than the prepare hint.
        constexpr int chunkSize=256;std::array<std::array<float,chunkSize>,2> before{};
        const int channels=bass.getNumChannels();
        for(int offset=0;offset<bass.getNumSamples();offset+=chunkSize){
            const int count=std::min(chunkSize,bass.getNumSamples()-offset);
            float* output[2]{};const float* reference[2]{};
            for(int c=0;c<channels;++c){output[c]=bass.getWritePointer(c)+offset;std::copy_n(output[c],count,before[size_t(c)].begin());}
            for(int c=0;c<sideChannels;++c)reference[c]=kick[c]+offset;
            engine.process(output,channels,sideChannels>0?reference:nullptr,sideChannels,count,settings,bpm);
            for(int n=0;n<count;++n){bassdeamon::AnalysisFrame frame;
                for(int c=0;c<2;++c){const int source=std::min(c,channels-1);
                    frame.pre[size_t(c)]=before[size_t(source)][size_t(n)];frame.post[size_t(c)]=output[source][n];
                    if(sideChannels>0)frame.kick[size_t(c)]=reference[std::min(c,sideChannels-1)][n];}
                frame.beat=ppq>=0?ppq+(offset+n)*tempo.load(std::memory_order_relaxed)/(60*getSampleRate()):-1;
                analysisQueue.push(frame);
            }
        }
    }
    if (wasLearning && !engine.learning()) {
        const auto r = engine.learnResult();
        learnCorrelation.store(static_cast<float>(r.correlation));
        if (r.valid) {
            resultDelay.store(static_cast<float>(r.delayMs)); resultInvert.store(r.invert);
            learnCorrelation.store(static_cast<float>(r.correlation));
            resultReady.store(true); learnStatus.store(5);
        } else learnStatus.store(r.rejection==bassdeamon::LearnRejection::ambiguous?3:r.rejection==bassdeamon::LearnRejection::unstable?4:2);
    }
    learnProgress.store(float(engine.learnProgress()));
    wasLearning = engine.learning(); publishMeters();
}
void BassDeamonProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    std::optional<juce::AudioProcessLoadMeasurer::ScopedTimer> cpuTimer;
    if(buffer.getNumSamples()>0)cpuTimer.emplace(cpuLoad,buffer.getNumSamples());
    if(analysisQueue.enabled.load(std::memory_order_relaxed)){
        auto bass=getBusBuffer(buffer,false,0);auto side=getBusBuffer(buffer,true,1);
        for(int n=0;n<buffer.getNumSamples();++n){bassdeamon::AnalysisFrame frame;
            for(int c=0;c<2;++c){const int source=std::min(c,bass.getNumChannels()-1);
                frame.pre[size_t(c)]=frame.post[size_t(c)]=bass.getSample(source,n);
                if(side.getNumChannels()>0)frame.kick[size_t(c)]=side.getSample(std::min(c,side.getNumChannels()-1),n);}
            analysisQueue.push(frame);
        }
    }
    midi.clear(); wasBypassed = true;
    // Matching main input/output layouts and zero algorithmic latency: leave bass untouched.
}
void BassDeamonProcessor::publishMeters() noexcept {
    const auto& m = engine.getMeters();
    tubeAddedDb.store(float(m.tubeAddedDb));unmaskAttack.store(float(m.unmaskAttackMs));unmaskRelease.store(float(m.unmaskReleaseMs));
    tubeGainDb.store(static_cast<float>(m.tubeAgcDb));
    glueReduction.store(static_cast<float>(m.glueReductionDb));
    unmaskReduction.store(static_cast<float>(m.unmaskReductionDb));
    outputPeak.store(static_cast<float>(m.peak));
    for (size_t i = 0; i < 3; ++i) agcMeters[i].store(static_cast<float>(m.agcDb[i]));
    kickPeak.store(static_cast<float>(m.kickPeak)); focusHz.store(static_cast<float>(m.focusHz));
    inputRmsDb.store(static_cast<float>(m.inputRmsDb)); outputRmsDb.store(static_cast<float>(m.outputRmsDb));
    glueAttack.store(static_cast<float>(m.attackMs)); glueRelease.store(static_cast<float>(m.releaseMs));
    unmaskQ.store(static_cast<float>(m.filterQ));
    for (size_t i = 0; i < bassdeamon::bandCount; ++i) {
        spectrumBass[i].store(static_cast<float>(m.bassDb[i])); spectrumKick[i].store(static_cast<float>(m.kickDb[i]));
        spectrumCut[i].store(static_cast<float>(m.cutsDb[i]));
    }
    const auto runtime = engine.runtimeState();
    for (size_t i = 0; i < runtime.size(); ++i) runtimeSnapshot[i].store(static_cast<float>(runtime[i]));
}
void BassDeamonProcessor::timerCallback() {
    refreshHistory();
    if (!resultReady.exchange(false)) return;
    undoManager.beginNewTransaction("Learn phase");
    // Host parameter notifications happen on the message thread, never in the audio callback.
    const auto set = [&](const char* id, float value) {
        auto* p = parameters.getParameter(id);
        p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(value)); p->endChangeGesture();
    };
    set("phase.delay", resultDelay.load()); set("phase.invert", resultInvert.load() ? 1.0f : 0.0f);
    parameters.copyState();undoManager.beginNewTransaction();
    // Learn does not silently enable PHASE. User can compare with the active switch.
}
void BassDeamonProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = parameters.copyState(); state.setProperty("schemaVersion", 5, nullptr);
    for (size_t i = 0; i < runtimeSnapshot.size(); ++i)
        state.setProperty("runtime" + juce::String(static_cast<int>(i)), runtimeSnapshot[i].load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}
void BassDeamonProcessor::setStateInformation(const void* data, int bytes) {
    if (auto xml = getXmlFromBinary(data, bytes)) {
        if (xml->hasTagName(parameters.state.getType()) && xml->getIntAttribute("schemaVersion", 1) <= 5) {
            if(xml->getIntAttribute("schemaVersion",1)<5)
                for(size_t i=44;i<ids.size();++i){auto* param=parameters.getParameter(ids[i]);param->setValueNotifyingHost(param->getDefaultValue());}
            if(xml->getIntAttribute("schemaVersion",1)<4){
                for(size_t i=34;i<ids.size();++i){auto* param=parameters.getParameter(ids[i]);param->setValueNotifyingHost(param->getDefaultValue());}
            }
            // A v1 state cannot carry the added controls: reset them deterministically.
            if (xml->getIntAttribute("schemaVersion", 1) == 1) {
                for (size_t i = 18; i < ids.size(); ++i) {
                    auto* parameter = parameters.getParameter(ids[i]);
                    parameter->setValueNotifyingHost(parameter->getDefaultValue());
                }
            }
            if(xml->getIntAttribute("schemaVersion",1)<3)
                for(size_t i=26;i<ids.size();++i){auto* parameter=parameters.getParameter(ids[i]);parameter->setValueNotifyingHost(parameter->getDefaultValue());}
            // Merge known parameter values into the current schema: v0.1 sessions lack v0.2 IDs.
            const auto incoming = juce::ValueTree::fromXml(*xml);
            for (int i = 0; i < incoming.getNumChildren(); ++i) {
                const auto child = incoming.getChild(i);
                if (auto* parameter = parameters.getParameter(child.getProperty("id").toString())) {
                    const float value = static_cast<float>(child.getProperty("value"));
                    if (std::isfinite(value)) parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
                }
            }
            if(xml->getIntAttribute("schemaVersion",1)<5){
                auto* limit=parameters.getParameter("view.maxHz");
                const int oldRange=int(parameters.getRawParameterValue("view.range")->load());
                limit->setValueNotifyingHost(limit->convertTo0to1(oldRange==0?1000.f:oldRange==1?2500.f:5000.f));
            }
            const bassdeamon::Engine::RuntimeState defaults{-24, 2, 20, 180, 0, 0, 0, 75,0};
            for (size_t i = 0; i < defaults.size(); ++i) {
                const double value = xml->getDoubleAttribute("runtime" + juce::String(static_cast<int>(i)), defaults[i]);
                restoredRuntime[i].store(static_cast<float>(std::isfinite(value) ? value : defaults[i]));
            }
            restoreRequested.store(true);
            historyResetRequested.store(true);
        }
    }
}
juce::AudioProcessorEditor* BassDeamonProcessor::createEditor() { return new BassDeamonEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BassDeamonProcessor(); }

void BassDeamonProcessor::selectAB(int slot) {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    slot = juce::jlimit(0, 1, slot);
    if (slot == abSlot) return;
    getStateInformation(abStates[static_cast<size_t>(abSlot)]);
    auto& target = abStates[static_cast<size_t>(slot)];
    if (target.getSize() == 0) target = abStates[static_cast<size_t>(abSlot)];
    setStateInformation(target.getData(), static_cast<int>(target.getSize())); abSlot = slot;
}
void BassDeamonProcessor::loadStartingPoint(int preset) {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    refreshHistory();undoManager.beginNewTransaction("Starting point");
    for (const auto* id : ids) {
        auto* p = parameters.getParameter(id);
        p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture();
    }
    const auto set = [&](const char* id, float value) {
        auto* p = parameters.getParameter(id);
        p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(value)); p->endChangeGesture();
    };
    if (preset == 1) { set("glue.amount", 0.45f); set("unmask.amount", 0.3f); set("unmask.width", 0.7f); set("unmask.release", 180); }
    if (preset == 2) { set("glue.amount", 0.25f); set("unmask.amount", 0.65f); set("unmask.attack", 3); set("unmask.release", 90); set("unmask.maxCut", 3); }
    if (preset == 3) { set("glue.amount", 0.2f); set("unmask.amount", 0.35f); set("unmask.track", 0); set("unmask.frequency", 48); set("unmask.width", 0.35f); set("unmask.release", 220); }
    const bassdeamon::Engine::RuntimeState initial{-24, 2, 20, 180, 0, 0, 0, 75,0};
    for (size_t i = 0; i < initial.size(); ++i) restoredRuntime[i].store(static_cast<float>(initial[i]));
    restoreRequested.store(true);
    parameters.copyState();undoManager.beginNewTransaction();
}

void BassDeamonProcessor::refreshHistory(){
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    parameters.copyState();
    if(historyResetRequested.exchange(false))undoManager.clearUndoHistory();
}
bool BassDeamonProcessor::undoEdit(){refreshHistory();return undoManager.undo();}
bool BassDeamonProcessor::redoEdit(){refreshHistory();return undoManager.redo();}

void BassDeamonProcessor::parameterGestureChanged(int,bool starting){
    // Flush UI gestures before another control starts a transaction, even if
    // two clicks occur inside one APVTS timer interval. Never lock audio.
    auto* messages=juce::MessageManager::getInstanceWithoutCreating();
    if(!starting&&messages!=nullptr&&messages->isThisTheMessageThread())
        refreshHistory();
}
