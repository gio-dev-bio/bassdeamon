#include "PluginProcessor.h"
#include <iostream>
#include "RealtimeAllocation.h"
#include <stdexcept>
#include <cstring>

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void set(BassDeamonProcessor& p, const char* id, float value) {
    auto* parameter = p.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void guardedProcess(BassDeamonProcessor& processor, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    const auto before = allocation::count; allocation::watch = true;
    processor.processBlock(buffer, midi);
    allocation::watch = false;
    require(allocation::count == before, "C++ allocation inside plugin callback");
}
int captureGui(const juce::File&);
int captureLoopGui(const juce::File&,const juce::File&,const juce::File&);
int interactionTests();
int pluginSignalTests(const std::string&);
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String(argv[1]) == "--capture") return captureGui(juce::File(argv[2]));
    if (argc > 2 && juce::String(argv[1]) == "--signals") return pluginSignalTests(argv[2]);
    try {
        if (argc==5 && juce::String(argv[1])=="--real-loop")
            return captureLoopGui(juce::File(argv[2]),juce::File(argv[3]),juce::File(argv[4]));
        interactionTests();
        BassDeamonProcessor p;
        auto layout = p.getBusesLayout();
        layout.inputBuses.set(0, juce::AudioChannelSet::mono());
        layout.outputBuses.set(0, juce::AudioChannelSet::mono());
        layout.inputBuses.set(1, juce::AudioChannelSet::disabled());
        require(p.setBusesLayout(layout), "Mono layout rejected");
        set(p, "glue.on", 0); set(p, "unmask.on", 0);
        p.prepareToPlay(48000, 256);
        juce::AudioBuffer<float> bass(1, 256); juce::MidiBuffer midi;
        for (int i = 0; i < 256; ++i) bass.setSample(0, i, 0.125f);
        guardedProcess(p, bass, midi);
        for (int i = 0; i < 256; ++i) require(bass.getSample(0,i) == 0.125f, "Plugin bypass not unity");
        set(p, "phase.delay", 2.25f); set(p, "phase.invert", 1); set(p, "unmask.agcLock", 1);
        set(p, "unmask.hold", 1); set(p, "unmask.width", 0.75f);
        p.selectAB(1); set(p, "phase.delay", 3.5f); p.selectAB(0);
        require(std::abs(p.parameters.getRawParameterValue("phase.delay")->load()-2.25f) < 0.001f, "A/B recall failed");
        juce::MemoryBlock state; p.getStateInformation(state);
        BassDeamonProcessor restored; restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        require(restored.parameters.getRawParameterValue("phase.delay")->load() == 2.25f, "Delay state lost");
        require(restored.parameters.getRawParameterValue("phase.invert")->load() == 1, "Polarity state lost");
        require(restored.parameters.getRawParameterValue("unmask.agcLock")->load() == 1, "AGC lock state lost");
        require(restored.parameters.getRawParameterValue("unmask.hold")->load() == 1, "Focus HOLD state lost");
        require(std::abs(restored.parameters.getRawParameterValue("unmask.width")->load()-0.75f) < 0.001f, "Width state lost");
        layout = restored.getBusesLayout();
        layout.inputBuses.set(1, juce::AudioChannelSet::mono());
        require(restored.setBusesLayout(layout), "Mono sidechain rejected");
        set(restored, "glue.on", 0); set(restored, "unmask.on", 0);
        restored.prepareToPlay(48000, 256);
        juce::AudioBuffer<float> routed(3, 256); routed.clear();
        for (int i = 0; i < 256; ++i) routed.setSample(2, i, 0.5f);
        guardedProcess(restored, routed, midi);
        require(routed.getMagnitude(0,0,256) == 0 && routed.getMagnitude(1,0,256) == 0, "Sidechain leaked");
        auto legacy = p.parameters.copyState(); legacy.setProperty("schemaVersion", 1, nullptr);
        const juce::StringArray newIds{"global.on", "unmask.track", "unmask.hold", "unmask.frequency", "unmask.width", "unmask.attack", "unmask.release", "unmask.maxCut"};
        for (int i = legacy.getNumChildren()-1; i >= 0; --i)
            if (newIds.contains(legacy.getChild(i).getProperty("id").toString())) legacy.removeChild(i, nullptr);
        juce::MemoryBlock legacyBytes;
        auto legacyXml = legacy.createXml(); juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyBytes);
        restored.setStateInformation(legacyBytes.getData(), static_cast<int>(legacyBytes.getSize()));
        require(std::abs(restored.parameters.getRawParameterValue("unmask.width")->load()-0.55f) < 0.001f, "Legacy width default was not restored");
        require(restored.parameters.getRawParameterValue("unmask.hold")->load() == 0, "Legacy HOLD default was not restored");
        // New state fields and legacy migration on an already configured instance.
        set(p,"tube.on",1);set(p,"tube.drive",9);set(p,"tube.source",210);set(p,"tube.agcLock",1);
        p.getStateInformation(state);restored.setStateInformation(state.getData(),int(state.getSize()));
        require(restored.parameters.getRawParameterValue("tube.on")->load()==1,"Tube state lost");
        require(std::abs(restored.parameters.getRawParameterValue("tube.drive")->load()-9)<.001f,"Tube Drive state lost");
        require(restored.parameters.getRawParameterValue("tube.agcLock")->load()==1,"Tube gain lock lost");
        auto v2=p.parameters.copyState();v2.setProperty("schemaVersion",2,nullptr);
        for(int i=v2.getNumChildren()-1;i>=0;--i)if(v2.getChild(i).getProperty("id").toString().startsWith("tube."))v2.removeChild(i,nullptr);
        auto xml2=v2.createXml();juce::MemoryBlock bytes2;juce::AudioProcessor::copyXmlToBinary(*xml2,bytes2);
        restored.setStateInformation(bytes2.getData(),int(bytes2.getSize()));
        require(restored.parameters.getRawParameterValue("tube.on")->load()==0,"v2 migration must disable Tube");
        // Independent, identical processors: opening the analyzer only taps the
        // actual samples. Oversized host blocks exercise bounded chunk storage.
        for(int channels:{1,2})for(int block:{17,128,511,2048}){
            BassDeamonProcessor closed,opened;
            for(auto* processor:{&closed,&opened}){
                auto l=processor->getBusesLayout();l.inputBuses.set(0,channels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
                l.outputBuses.set(0,l.inputBuses[0]);l.inputBuses.set(1,juce::AudioChannelSet::mono());
                require(processor->setBusesLayout(l),"Analyzer layout rejected");set(*processor,"tube.on",1);set(*processor,"phase.on",1);set(*processor,"phase.delay",1.25f);
                processor->prepareToPlay(48000,64);
            }
            opened.analysisQueue.enabled.store(true);
            juce::AudioBuffer<float> a(channels+1,block),b(channels+1,block);
            for(int pass=0;pass<12;++pass){
                for(int c=0;c<a.getNumChannels();++c)for(int n=0;n<block;++n){const float value=float(.25*std::sin(2*bassdeamon::pi*(55+17*c)*(pass*block+n)/48000.));a.setSample(c,n,value);b.setSample(c,n,value);}
                guardedProcess(closed,a,midi);guardedProcess(opened,b,midi);
                for(int c=0;c<channels;++c)require(std::memcmp(a.getReadPointer(c),b.getReadPointer(c),size_t(block)*sizeof(float))==0,"Analyzer changes audio");
                bassdeamon::AnalysisFrame frame;int frames=0;
                while(opened.analysisQueue.pop(frame)){for(int c=0;c<2;++c)require(frame.post[size_t(c)]==b.getSample(std::min(c,channels-1),frames),"Analyzer POST differs from actual output");++frames;}
                require(frames==block,"Analyzer loses frames without pressure");
            }
        }
        std::cout << "PASS plugin layouts / routing / state / A-B / legacy migration\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
