#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void set(BassDeamonProcessor& p, const char* id, float value) {
    auto* parameter = p.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
int main() {
    juce::ScopedJuceInitialiser_GUI init;
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
        juce::MemoryBlock state; p.getStateInformation(state);
        BassDeamonProcessor restored; restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        require(restored.parameters.getRawParameterValue("phase.delay")->load() == 2.25f, "Delay state lost");
        require(restored.parameters.getRawParameterValue("phase.invert")->load() == 1, "Polarity state lost");
        require(restored.parameters.getRawParameterValue("unmask.agcLock")->load() == 1, "AGC lock state lost");
        layout = restored.getBusesLayout();
        layout.inputBuses.set(1, juce::AudioChannelSet::mono());
        require(restored.setBusesLayout(layout), "Mono sidechain rejected");
        set(restored, "glue.on", 0); set(restored, "unmask.on", 0);
        restored.prepareToPlay(48000, 256);
        juce::AudioBuffer<float> routed(3, 256); routed.clear();
        for (int i = 0; i < 256; ++i) routed.setSample(2, i, 0.5f);
        restored.processBlock(routed, midi);
        require(routed.getMagnitude(0,0,256) == 0 && routed.getMagnitude(1,0,256) == 0, "Sidechain leaked");
        std::cout << "PASS plugin layouts / routing / state roundtrip\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
