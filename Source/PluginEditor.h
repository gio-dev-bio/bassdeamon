#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class BassDeamonEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit BassDeamonEditor(BassDeamonProcessor&);
    ~BassDeamonEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using State = juce::AudioProcessorValueTreeState;
    struct Toggle {
        juce::ToggleButton button;
        std::unique_ptr<State::ButtonAttachment> attachment;
        int column = 0, row = 0;
    };
    struct Slider {
        juce::Label label;
        juce::Slider control;
        std::unique_ptr<State::SliderAttachment> attachment;
        int column = 0, row = 0;
    };
    void addToggle(const char*, const char*, int, int);
    void addSlider(const char*, const char*, const char*, int, int);
    void timerCallback() override;
    BassDeamonProcessor& processor;
    juce::LookAndFeel_V4 style;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::vector<std::unique_ptr<Slider>> sliders;
    juce::TextButton learn{"LEARN 4 s"};
    juce::Label status, meters;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassDeamonEditor)
};
