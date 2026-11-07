#include "PluginEditor.h"

namespace { const juce::Colour amber{0xffedb76a}, ink{0xff161918}, panel{0xff242824}; }
BassDeamonEditor::BassDeamonEditor(BassDeamonProcessor& p) : AudioProcessorEditor(p), processor(p) {
    style.setColour(juce::Slider::thumbColourId, amber);
    style.setColour(juce::Slider::trackColourId, juce::Colour(0xff7e593a));
    style.setColour(juce::Slider::textBoxTextColourId, amber);
    style.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    style.setColour(juce::ToggleButton::textColourId, amber);
    style.setColour(juce::ToggleButton::tickColourId, amber);
    style.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff755237));
    setLookAndFeel(&style);
    addToggle("glue.on", "ACTIVE", 0, 0);
    addSlider("glue.amount", "AMOUNT", "", 0, 1);
    addToggle("glue.lock", "LOCK ADAPTATION", 0, 3);
    addToggle("glue.agc", "AUTO GAIN", 0, 7);
    addToggle("glue.agcLock", "LOCK AUTO GAIN", 0, 8);
    addToggle("phase.on", "ACTIVE", 1, 0);
    addToggle("phase.invert", "INVERT POLARITY", 1, 2);
    addSlider("phase.delay", "CAUSAL DELAY", " ms", 1, 3);
    addToggle("phase.rotate", "ALL-PASS", 1, 5);
    addSlider("phase.frequency", "ALL-PASS CORNER", " Hz", 1, 6);
    addToggle("phase.agc", "AUTO GAIN", 1, 8);
    addToggle("phase.agcLock", "LOCK AUTO GAIN", 1, 9);
    addToggle("unmask.on", "ACTIVE", 2, 0);
    addSlider("unmask.amount", "AMOUNT", "", 2, 1);
    addToggle("unmask.delta", "AUDITION DELTA", 2, 3);
    addToggle("unmask.agc", "AUTO GAIN", 2, 7);
    addToggle("unmask.agcLock", "LOCK AUTO GAIN", 2, 8);
    addSlider("output.trim", "OUTPUT", " dB", 2, 10);
    addAndMakeVisible(learn);
    learn.onClick = [this] { processor.requestLearn(); };
    for (auto* label : {&status, &meters}) {
        label->setColour(juce::Label::textColourId, amber); addAndMakeVisible(*label);
    }
    setSize(960, 690); startTimerHz(15);
}
BassDeamonEditor::~BassDeamonEditor() { stopTimer(); setLookAndFeel(nullptr); }
void BassDeamonEditor::addToggle(const char* id, const char* name, int col, int row) {
    auto t = std::make_unique<Toggle>(); t->column = col; t->row = row;
    t->button.setButtonText(name); addAndMakeVisible(t->button);
    t->attachment = std::make_unique<State::ButtonAttachment>(processor.parameters, id, t->button);
    toggles.push_back(std::move(t));
}
void BassDeamonEditor::addSlider(const char* id, const char* name, const char* suffix, int col, int row) {
    auto s = std::make_unique<Slider>(); s->column = col; s->row = row;
    s->label.setText(name, juce::dontSendNotification); s->label.setColour(juce::Label::textColourId, amber);
    s->control.setSliderStyle(juce::Slider::LinearHorizontal);
    s->control.setTextBoxStyle(juce::Slider::TextBoxRight, false, 85, 24);
    s->control.setTextValueSuffix(suffix); s->control.setNumDecimalPlacesToDisplay(2);
    addAndMakeVisible(s->label); addAndMakeVisible(s->control);
    s->attachment = std::make_unique<State::SliderAttachment>(processor.parameters, id, s->control);
    sliders.push_back(std::move(s));
}
void BassDeamonEditor::paint(juce::Graphics& g) {
    g.fillAll(ink);
    g.setColour(amber); g.setFont(juce::FontOptions(34.0f, juce::Font::bold));
    g.drawText("BASSDEAMON", 28, 18, 450, 50, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(14.0f));
    g.drawText("By Guy_BioRg   /   DSP PROTOTYPE 0.1", 550, 32, 380, 24, juce::Justification::centredRight);
    const char* titles[]{"01 / GLUE", "02 / PHASE", "03 / UNMASK"};
    for (int i = 0; i < 3; ++i) {
        const int x = 20 + i*310;
        g.setColour(panel); g.fillRoundedRectangle(static_cast<float>(x), 88, 300, 525, 5);
        g.setColour(amber.withAlpha(0.35f)); g.drawRect(x+1, 89, 298, 523);
        g.setColour(amber); g.setFont(juce::FontOptions(21.0f, juce::Font::bold));
        g.drawText(titles[i], x+14, 100, 260, 32, juce::Justification::centredLeft);
        for (const auto point : {juce::Point<int>{x+8,96}, {x+292,96}, {x+8,605}, {x+292,605}}) {
            g.setColour(juce::Colour(0xff827866)); g.fillEllipse(static_cast<float>(point.x-2), static_cast<float>(point.y-2), 4, 4);
        }
    }
    g.setFont(juce::FontOptions(12.0f)); g.setColour(amber.withAlpha(0.65f));
    g.drawText("BASS INSERT  /  KICK SIDECHAIN REFERENCE  /  LEARN + LOCK  /  NO LIMITER", 28, 659, 900, 20, juce::Justification::centredLeft);
}
void BassDeamonEditor::resized() {
    for (auto& t : toggles) t->button.setBounds(34+t->column*310, 142+t->row*36, 270, 30);
    for (auto& s : sliders) {
        const int x = 34+s->column*310, y = 142+s->row*36;
        s->label.setBounds(x, y, 265, 20); s->control.setBounds(x, y+20, 268, 34);
    }
    learn.setBounds(344, 178, 126, 28); status.setBounds(472, 178, 150, 28);
    meters.setBounds(28, 620, 905, 32);
}
void BassDeamonEditor::timerCallback() {
    const int state = processor.learnStatus.load();
    learn.setEnabled(state != 1);
    status.setText(state == 1 ? "ANALYSING..." : state == 2 ? "NO RELIABLE MATCH" : "LOCKED / MANUAL", juce::dontSendNotification);
    juce::String text = "GLUE GR " + juce::String(processor.glueReduction.load(), 1) + " dB    |    UNMASK max band "
        + juce::String(processor.unmaskReduction.load(), 1) + " dB    |    AGC ";
    for (auto& m : processor.agcMeters) text += juce::String(m.load(), 1) + " / ";
    text += "dB    |    PEAK " + juce::String(bassdeamon::db(processor.outputPeak.load()), 1) + " dBFS";
    meters.setText(text, juce::dontSendNotification);
}
