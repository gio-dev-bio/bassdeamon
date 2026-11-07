#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void set(BassDeamonProcessor& p, const char* id, float value) {
    auto* parameter = p.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
int captureGui(const juce::File&);
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String(argv[1]) == "--capture") return captureGui(juce::File(argv[2]));
    try {
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
        p.processBlock(bass, midi);
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
        restored.processBlock(routed, midi);
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
        std::cout << "PASS plugin layouts / routing / state / A-B / legacy migration\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
