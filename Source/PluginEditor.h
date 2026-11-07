#pragma once
#include "PluginProcessor.h"
#include "ConsoleSkin.h"

class BassDeamonEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit BassDeamonEditor(BassDeamonProcessor&);
    ~BassDeamonEditor() override;
    void paint(juce::Graphics& g) override {g.fillAll(console::ink);}
    void resized() override;
private:
    using State=juce::AudioProcessorValueTreeState;
    struct Face final : juce::Component {
        explicit Face(BassDeamonEditor& e):editor(e){}
        void paint(juce::Graphics& g) override {editor.paintConsole(g);}
        BassDeamonEditor& editor;
    };
    struct Knob final : juce::Component {
        Knob(State&,const char*,const char*,const char*,bool);
        void resized() override;
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<State::SliderAttachment> attachment;
    };
    struct Switch {juce::ToggleButton button;std::unique_ptr<State::ButtonAttachment> attachment;};
    void addSwitch(const char*,const char*,juce::Rectangle<int>,const char* tooltip);
    Knob& addKnob(const char*,const char*,const char*,bool,juce::Rectangle<int>,const char*);
    void paintConsole(juce::Graphics&);
    void paintSpectrum(juce::Graphics&);
    void paintVU(juce::Graphics&);
    void timerCallback() override;
    void createTexture();
    BassDeamonProcessor& processor;
    console::Skin skin;
    Face face{*this};
    juce::TooltipWindow tooltips{this,600};
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    juce::TextButton learn{"LEARN / LOCK"},aButton{"A"},bButton{"B"};
    juce::ComboBox presets,ceiling;
    std::unique_ptr<State::ComboBoxAttachment> ceilingAttachment;
    juce::Slider output;
    std::unique_ptr<State::SliderAttachment> outputAttachment;
    Knob* frequencyKnob=nullptr;
    juce::Image grain;
    std::array<float,bassdeamon::bandCount> bass{},kick{},cut{};
    std::array<float,3> compensation{};
    float vu=0, focus=75,peak=-120,inputDb=-120,outputDb=-120,sidePeak=0,q=2;
    int clipFrames=0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassDeamonEditor)
};
