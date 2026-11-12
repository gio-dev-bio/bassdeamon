#include "PluginEditor.h"
namespace {
const juce::Colour eqTrace(0xffffde78);
const juce::Colour unmaskTrace(0xff69f3d2);
}

BassDeamonEditor::Knob::Knob(State& state,const char* id,const char* name,const char* unit,bool percent) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,88,23);
    slider.setScrollWheelEnabled(false);
    attachment=std::make_unique<State::SliderAttachment>(state,id,slider);
    if(percent) {
        slider.textFromValueFunction=[](double v){return juce::String(juce::roundToInt(v*100))+" %";};
        slider.valueFromTextFunction=[](const juce::String& s){return s.getDoubleValue()/100;};
    } else {
        const juce::String suffix(unit);
        slider.textFromValueFunction=[suffix](double v){return juce::String(v,suffix==" ms"?2:1)+suffix;};
        slider.valueFromTextFunction=[](const juce::String& s){return s.getDoubleValue();};
    }
    slider.updateText();
    label.setText(name,juce::dontSendNotification);label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(slider);addAndMakeVisible(label);
}
void BassDeamonEditor::Knob::resized() {slider.setBounds(getLocalBounds().withTrimmedBottom(25));label.setBounds(0,getHeight()-24,getWidth(),22);}

BassDeamonEditor::BassDeamonEditor(BassDeamonProcessor& p):AudioProcessorEditor(p),processor(p) {
    setLookAndFeel(&skin); addAndMakeVisible(face);face.setOpaque(true);createTexture();
    presets.addItemList({"INIT / CLEAN LOW END","AFRO / ROUND & STEADY","PUNCH / MAKE ROOM","SUB / GENTLE CONTROL"},1);
    presets.setSelectedId(1,juce::dontSendNotification);presets.setBounds(628,33,310,38);face.addAndMakeVisible(presets);
    presets.setTooltip("Starting points, not automatic mix decisions. PHASE remains off until you choose to enable it.");
    presets.onChange=[this]{processor.loadStartingPoint(presets.getSelectedId()-1);};
    aButton.setBounds(958,33,44,38);bButton.setBounds(1010,33,44,38);
    for(auto* b:{&aButton,&bButton}) {face.addAndMakeVisible(*b);b->setTooltip("Compare two complete settings at their actual output levels. Auto Gain can help level matching.");}
    aButton.onClick=[this]{processor.selectAB(0);};bButton.onClick=[this]{processor.selectAB(1);};
    undoButton.setBounds(402,80,88,25);redoButton.setBounds(498,80,88,25);
    for(auto* b:{&undoButton,&redoButton})face.addAndMakeVisible(*b);
    undoButton.setTooltip("Undo the last parameter edit or starting point. History lasts until session recall or A/B switch.");
    redoButton.setTooltip("Redo the last undone edit. A new edit replaces the redo branch.");
    presets.setTextWhenNothingSelected("CUSTOM");
    undoButton.onClick=[this]{processor.undoEdit();presets.setSelectedId(0,juce::dontSendNotification);timerCallback();};
    redoButton.onClick=[this]{processor.redoEdit();presets.setSelectedId(0,juce::dontSendNotification);timerCallback();};
    cpuLabel.setBounds(628,80,430,26);cpuLabel.setFont(console::font(13));face.addAndMakeVisible(cpuLabel);
    cpuLabel.setTooltip("DSP CPU: audio callback wall time / current block duration, averaged. Not total system CPU. ANALYZER: message-thread analysis milliseconds per update, measured separately. OVR: callback budget exceeded; not a hardware dropout count.");
    addSwitch("global.on","POWER",{1082,27,88,50},"20 ms transition, then exact dry bypass. Processing settings are retained.");
    const char* modules[]{"glue","phase","unmask"};
    for(int i=0;i<3;++i) {
        const int x=24+i*392;
        const auto prefix=juce::String(modules[i]);
        addSwitch((prefix+".on").toRawUTF8(),"ON",{x+290,354,55,28},"Enable this module. Its bypass also excludes its Auto Gain.");
        addSwitch((prefix+".agc").toRawUTF8(),"AUTO GAIN",{x+22,403,108,28},"Slow input/output energy compensation. It is not a peak limiter.");
        addSwitch((prefix+".agcLock").toRawUTF8(),"GAIN LOCK",{x+140,403,100,28},"Freeze this module's learned compensation gain.");
    }
    addKnob("glue.amount","AMOUNT","",true,{44,451,190,223},"Bass-led adaptive compression. Kick and host tempo only provide timing context.");
    addSwitch("glue.lock","LOCK ADAPTATION",{249,690,122,36},"Freeze threshold, ratio, attack and release. The compressor envelope keeps moving.");
    addKnob("phase.delay","DELAY"," ms",false,{438,450,154,192},"Intentional causal delay on the bass. No advance and no host latency compensation of this musical offset.");
    addKnob("phase.frequency","ALL-PASS CORNER"," Hz",false,{609,450,154,192},"Corner frequency of a first-order all-pass. Rotation depends on frequency, not a constant angle.");
    addSwitch("phase.invert","POLARITY -",{438,654,150,32},"Invert both bass channels together.");
    addSwitch("phase.rotate","ALL-PASS",{610,654,150,32},"Enable the manual all-pass stage.");
    learn.setBounds(438,704,150,38);face.addAndMakeVisible(learn);learn.onClick=[this]{processor.requestLearn();};
    learn.setTooltip("Analyse four seconds. A reliable result sets delay and polarity and enables PHASE. Uncertain results keep the current settings.");
    depthKnob=&addKnob("unmask.amount","DEPTH","",true,{830,448,170,184},"Maximum spectral unmasking amount. The kick is never summed into the bass.");
    attenuationKnob=&addKnob("unmask.attenuation","ATTENUATION"," dB",false,{830,448,170,184},"Maximum dynamic attenuation at the focus, 0-24 dB. Actual reduction follows shared signal activity. Before Auto Gain: turn Auto Gain off to measure the requested cut.");
    attenuationKnob->slider.textFromValueFunction=[](double value){return (value>0?juce::String("-"):juce::String{})+juce::String(value,1)+" dB";};
    attenuationKnob->slider.valueFromTextFunction=[](const juce::String& text){return std::abs(text.getDoubleValue());};attenuationKnob->slider.updateText();
    frequencyKnob=&addKnob("unmask.frequency","MANUAL FOCUS"," Hz",false,{1026,460,118,120},"Manual focus frequency, available when AUTO FOCUS is off.");
    addKnob("unmask.width","WIDTH","",true,{1026,580,118,120},"Width of the focus region and dynamic EQ bands.");
    unmaskAttackKnob=&addKnob("unmask.attack","ATTACK"," ms",false,{824,670,99,95},"Dynamic cut attack, 1 to 50 ms.");
    unmaskReleaseKnob=&addKnob("unmask.release","RELEASE"," ms",false,{926,670,99,95},"Dynamic cut recovery, 30 to 500 ms.");
    addSwitch("unmask.controlDb","dB CONTROL",{832,640,116,25},"On: calibrated attenuation knob in dB. Off: previous DEPTH percentage algorithm, retained for existing sessions.");
    addSwitch("unmask.track","AUTO FOCUS",{1032,708,118,25},"Follow the strongest overlapping low-end resonance, with hysteresis and slow movement. This is not pitch/fundamental detection.");
    addSwitch("unmask.hold","HOLD FOCUS",{1032,741,118,25},"Hold the current auto focus frequency. The dynamic gain reduction remains active.");
    addSwitch("unmask.delta","DELTA",{950,640,58,25},"Audition the bass removed by UNMASK, before its Auto Gain and output trim.");
    output.setSliderStyle(juce::Slider::LinearHorizontal);output.setTextBoxStyle(juce::Slider::TextBoxRight,false,78,24);
    output.setTextValueSuffix(" dB");output.setNumDecimalPlacesToDisplay(1);output.setScrollWheelEnabled(false);
    output.setBounds(824,808,260,28);face.addAndMakeVisible(output);
    outputAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"output.trim",output);
    output.textFromValueFunction=[](double value){return juce::String(value,1);};
    output.valueFromTextFunction=[](const juce::String& value){return value.getDoubleValue();}; output.updateText();
    output.setTooltip("Final trim. No limiter is applied; red peak indication means you must reduce the level.");
    for(size_t i=0;i<bass.size();++i){bass[i]=processor.spectrumBass[i].load();kick[i]=processor.spectrumKick[i].load();cut[i]=processor.spectrumCut[i].load();}
    // Keep existing modules together; add scope above and tube below.
    for(auto* child:face.getChildren())if(child->getY()>=340)child->setTopLeftPosition(child->getX(),child->getY()+(child->getY()>=798?320:160));
    addSwitch("tube.on","ON",{53,970,64,28},"Enable parallel Soft Tube. Off by default; the settled off state leaves the old chain unchanged.");
    addSwitch("tube.agc","AUTO GAIN",{52,1016,120,28},"Slow stereo-linked compensation, max +/-3 dB, shared positive-gain budget. Disabled during tube audition.");
    addSwitch("tube.agcLock","GAIN LOCK",{52,1054,120,28},"Freeze the learned Soft Tube compensation.");
    addKnob("tube.drive","DRIVE"," dB",false,{203,967,135,125},"Soft asymmetric waveshaping. 4x oversampling in this added branch only. Not a circuit model.");
    addKnob("tube.amount","AMOUNT","",true,{365,967,135,125},"How much nonlinear residual is added in parallel. The added branch may retain fundamental energy.");
    addKnob("tube.source","SOURCE LP"," Hz",false,{527,967,135,125},"Select the low-frequency source, 60-500 Hz. This does not track individual notes.");
    addKnob("tube.colour","COLOUR","",true,{689,967,135,125},"Smoothly varies asymmetry and the added-branch tone filter.");
    addSwitch("tube.audition","ADDED ONLY",{872,1005,145,32},"Send the added residual through UNMASK/output, with tube AGC disabled. It may retain fundamental energy.");
    sumButton.setBounds(1002,336,145,26);face.addAndMakeVisible(sumButton);
    sumButton.setTooltip("Show or hide the kick + bass sum graph. Complete 1/4-note captures aligned to the project beat. Display only: audio is unchanged.");
    sumButton.onClick=[this]{showSum=!showSum;sumButton.setToggleState(showSum,juce::dontSendNotification);face.repaint();};
    sumButton.setToggleState(true,juce::dontSendNotification);
    addSwitch("view.hold","HOLD",{904,336,88,26},"Freeze the last complete quarter-note capture; audio is unaffected.");
    spectrumPre.setBounds(72,136,66,26);spectrumPost.setBounds(145,136,72,26);
    for(auto* button:{&spectrumPre,&spectrumPost}){
        face.addAndMakeVisible(*button);
        button->setTooltip("Show bass before or after processing. The kick stays visible as the reference.");
    }
    const auto selectSpectrum=[this](int mode){
        // Local display state only. No host gesture, APVTS/audio parameter or
        // DSP state change, and no synchronous FFT when switching pictures.
        processor.setSpectrumView(mode);
        spectrumPre.setToggleState(mode==0,juce::dontSendNotification);
        spectrumPost.setToggleState(mode==1,juce::dontSendNotification);
        face.repaint();
    };
    spectrumPre.onClick=[selectSpectrum]{selectSpectrum(0);};
    spectrumPost.onClick=[selectSpectrum]{selectSpectrum(1);};
    spectrumHold.setBounds(427,166,75,22);
    spectrumHold.setTooltip("Freeze the spectrum and its measurements.");
    spectrumHold.setClickingTogglesState(true);
    spectrumHold.onClick=[this]{
        if(!spectrumHold.getToggleState()){
            spectrumDisplay.reset();
            pendingUnmaskResponse={};
        }
        face.repaint();
    };
    face.addAndMakeVisible(spectrumHold);
    analyzerRefresh.setSliderStyle(juce::Slider::LinearBar);
    analyzerRefresh.setTextBoxStyle(juce::Slider::TextBoxLeft,false,125,22);
    analyzerRefresh.setRange(1,30,.1);
    analyzerRefresh.setTextValueSuffix(" fps");
    analyzerRefresh.valueFromTextFunction=[](const juce::String& text) {
        return text.replaceCharacter(',', '.').getDoubleValue();
    };
    analyzerRefresh.setScrollWheelEnabled(false);
    syncedRefreshHz = processor.analyzerRefreshHz();
    analyzerRefresh.setValue(syncedRefreshHz,juce::dontSendNotification);
    analyzerRefresh.setBounds(290,166,125,22);
    analyzerRefresh.setTooltip("Drag or type a refresh rate from 1 to 30 fps.");
    analyzerRefresh.onValueChange=[this] {
        processor.setAnalyzerRefreshHz(analyzerRefresh.getValue());
        syncedRefreshHz = processor.analyzerRefreshHz();
    };
    face.addAndMakeVisible(analyzerRefresh);
    for(auto* slider:{&spectrumMin,&spectrumMax}){
        slider->setSliderStyle(juce::Slider::LinearBar);
        slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,130,26);
        slider->setTextValueSuffix(" Hz");slider->setNumDecimalPlacesToDisplay(1);slider->setScrollWheelEnabled(false);
        slider->setTooltip("Drag or type a frequency in Hz. Set FROM below TO; a minimum 20 Hz window is displayed. Display only.");
        face.addAndMakeVisible(*slider);
    }
    spectrumMin.setBounds(526,136,132,26);spectrumMax.setBounds(682,136,132,26);
    spectrumMinAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"view.minHz",spectrumMin);
    spectrumMaxAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"view.maxHz",spectrumMax);
    spectrumMin.onValueChange=[this]{
        if(spectrumMin.getValue()>spectrumMax.getValue()-20)
            spectrumMax.setValue(spectrumMin.getValue()+20,juce::sendNotificationSync);
    };
    spectrumMax.onValueChange=[this]{
        if(spectrumMax.getValue()<spectrumMin.getValue()+20)
            spectrumMin.setValue(spectrumMax.getValue()-20,juce::sendNotificationSync);
    };
    for (auto* limit : {&focusMin, &focusMax}) {
        limit->setSliderStyle(juce::Slider::LinearBar);
        limit->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 76, 22);
        limit->setTextValueSuffix(" Hz");
        limit->setScrollWheelEnabled(false);
        face.addAndMakeVisible(*limit);
    }
    focusMin.setBounds(892,541,70,22);
    focusMax.setBounds(966,541,70,22);
    focusMin.setTooltip("Lower frequency limit for Auto Focus.");
    focusMax.setTooltip("Upper frequency limit for Auto Focus.");
    focusMinAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"unmask.focusMinHz",focusMin);
    focusMaxAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"unmask.focusMaxHz",focusMax);
    focusMin.setNumDecimalPlacesToDisplay(1);
    focusMax.setNumDecimalPlacesToDisplay(1);
    addChoice("unmask.stereoMode",{"STEREO","MID","SIDE"},{1044,541,108,22});
    choices.back()->box.setTooltip("UNMASK target. MID processes the common signal; SIDE processes the difference. SIDE has no effect on mono bass.");
    addChoice("glue.timing",{"AUTO","MS","SYNC"},{249,623,122,26});
    addChoice("unmask.timing",{"MS","SYNC"},{832,918,185,22});
    const juce::StringArray notes{"1/64","1/32","1/16","1/8","1/4","1/2","1/1","1/8 T","1/8 D"};
    addChoice("glue.releaseNote",notes,{249,759,122,32});
    addChoice("unmask.releaseNote",notes,{926,862,99,30});
    for(auto* slider:{&glueAttackSlider,&glueReleaseSlider}){
        slider->setSliderStyle(juce::Slider::LinearBar);slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,122,32);
        slider->setTextValueSuffix(" ms");slider->setNumDecimalPlacesToDisplay(1);slider->setScrollWheelEnabled(false);face.addAndMakeVisible(slider);
    }
    glueAttackSlider.setBounds(249,680,122,32);glueReleaseSlider.setBounds(249,759,122,32);
    glueAttackSlider.setTooltip("Attack is always in milliseconds in MS and SYNC. Only release follows note values in SYNC.");
    glueAttackAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"glue.attackMs",glueAttackSlider);
    glueReleaseAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"glue.releaseMs",glueReleaseSlider);
    // A request to listen to the added signal must turn on its generator and
    // leave competing monitoring modes. The main POWER switch stays explicit.
    for(auto& sw:switches){const auto id=sw->button.getComponentID();
        if(id=="tube.audition"||id=="tube.on"||id=="unmask.delta")sw->button.onClick=[this,id,&button=sw->button]{
            if(!button.userGesture)return;
            const auto set=[&](const char* key,float value){auto* parameter=processor.parameters.getParameter(key);
                parameter->beginChangeGesture();parameter->setValueNotifyingHost(value);parameter->endChangeGesture();};
            if(id=="tube.audition"&&button.getToggleState()){set("tube.on",1);set("unmask.delta",0);}
            if(id=="tube.on"&&!button.getToggleState())set("tube.audition",0);
            if(id=="unmask.delta"&&button.getToggleState())set("tube.audition",0);
        };
    }
    visualPre.fill(-120);visualPost.fill(-120);visualKick.fill(-120);
    analyzer.prepare(processor.getSampleRate()>0?processor.getSampleRate():48000);
    lastAnalysisTime=juce::Time::getMillisecondCounterHiRes();
    bassdeamon::AnalysisFrame oldFrame;const auto pending=processor.analysisQueue.pending();
    for(uint32_t i=0;i<pending&&processor.analysisQueue.pop(oldFrame);++i){}
    processor.analysisQueue.enabled.store(true,std::memory_order_release);
    vu=processor.glueReduction.load();
    for(auto* child:face.getChildren())if(child->getY()>=328)child->setTopLeftPosition(child->getX(),child->getY()+72);
    for (auto* child : face.getChildren())
        if (child->getY() >= 570) child->setTopLeftPosition(child->getX(),child->getY()+64);
    addSwitch("kickEq.on","ON",{227,568,52,28},"Enable the static bell ahead of GLUE. Q is fixed at 3.");
    kickEqLearn.setBounds(293,568,90,28);
    kickEqLearn.setTooltip("Capture four seconds of kick. A successful learn locks one frequency; an uncertain result keeps the previous setting.");
    kickEqLearn.onClick=[this]{processor.requestKickEqLearn();};
    dryCompare.setBounds(939,568,61,28);
    matchCompare.setBounds(1011,568,139,28);
    dryCompare.setTooltip("Switch between original bass and processed bass. The kick sidechain is unchanged.");
    matchCompare.setTooltip("Match processed bass RMS to original bass RMS. Let it settle before comparing. No limiter.");
    dryCompare.onClick=[this]{processor.compareDry.store(!processor.compareDry.load());};
    matchCompare.onClick=[this]{processor.compareMatch.store(!processor.compareMatch.load());};
    for (auto* button : {&kickEqLearn,&dryCompare,&matchCompare}) face.addAndMakeVisible(*button);
    for (auto* slider : {&kickEqFrequency,&kickEqCut}) {
        slider->setSliderStyle(juce::Slider::LinearBar);
        slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,100,28);
        slider->setScrollWheelEnabled(false);
        face.addAndMakeVisible(*slider);
    }
    kickEqFrequency.setBounds(497,568,104,28);
    kickEqCut.setBounds(647,568,96,28);
    kickEqFrequencyAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"kickEq.frequency",kickEqFrequency);
    kickEqCutAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"kickEq.cut",kickEqCut);
    kickEqFrequency.textFromValueFunction=[](double hz){return hz<30?"-- Hz":juce::String(hz,1)+" Hz";};
    kickEqFrequency.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue();};
    kickEqCut.textFromValueFunction=[](double cutDb){return juce::String(-cutDb,1)+" dB";};
    kickEqCut.valueFromTextFunction=[](const juce::String& text){return std::abs(text.getDoubleValue());};
    kickEqFrequency.updateText();kickEqCut.updateText();
    kickEqFrequency.setTooltip("Locked frequency in Hz. Type 30 to 130 Hz for manual control.");
    kickEqCut.setTooltip("Static cut from 0 to -4 dB.");
    for (auto* child : face.getChildren())
        if (child->getY() >= 400) child->setTopLeftPosition(child->getX(),child->getY()+60);
    setResizable(true,true);setResizeLimits(780,894,1560,1789);
    if(auto* c=getConstrainer())c->setFixedAspectRatio(1200.0/1376.0);
    setSize(900,1032);timerCallback();startTimerHz(60);
}
BassDeamonEditor::~BassDeamonEditor(){stopTimer();processor.analysisQueue.enabled.store(false,std::memory_order_release);setLookAndFeel(nullptr);}
void BassDeamonEditor::resized(){face.setBounds(0,0,1200,1376);face.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth())/1200.0f));}
void BassDeamonEditor::addSwitch(const char* id,const char* name,juce::Rectangle<int> bounds,const char* tip){
    auto s=std::make_unique<Switch>();s->button.setButtonText(name);s->button.setComponentID(id);s->button.setBounds(bounds);s->button.setTooltip(tip);
    face.addAndMakeVisible(s->button);s->attachment=std::make_unique<State::ButtonAttachment>(processor.parameters,id,s->button);switches.push_back(std::move(s));
}
BassDeamonEditor::Knob& BassDeamonEditor::addKnob(const char* id,const char* name,const char* unit,bool pct,juce::Rectangle<int> r,const char* tip){
    auto k=std::make_unique<Knob>(processor.parameters,id,name,unit,pct);k->setBounds(r);k->slider.setTooltip(tip);face.addAndMakeVisible(*k);
    auto& ref=*k;knobs.push_back(std::move(k));return ref;
}
void BassDeamonEditor::addChoice(const char* id,const juce::StringArray& values,juce::Rectangle<int> bounds){
    auto c=std::make_unique<Choice>();c->box.addItemList(values,1);c->box.setBounds(bounds);c->box.setComponentID(id);
    face.addAndMakeVisible(c->box);c->attachment=std::make_unique<State::ComboBoxAttachment>(processor.parameters,id,c->box);choices.push_back(std::move(c));
}
void BassDeamonEditor::createTexture(){
    grain=juce::Image(juce::Image::ARGB,1200,1376,true);juce::Random random(0xBADB00B);
    juce::Image::BitmapData data(grain,juce::Image::BitmapData::writeOnly);
    for(int y=0;y<1376;++y)for(int x=0;x<1200;++x){const int n=random.nextInt(9);data.setPixelColour(x,y,juce::Colour::fromRGBA(230,216,180,static_cast<juce::uint8>(n)));}
}
void BassDeamonEditor::paintConsole(juce::Graphics& g){
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff302e2a),0,0,console::ink,1200,1376,false));g.fillAll();
    console::panel(g,{5,5,1190,1366});
    g.setColour(console::copper.withAlpha(0.35f));g.drawRoundedRectangle(juce::Rectangle<float>(9,9,1182,1358),10,2);
    juce::Random scratches(1829);
    for(int i=0;i<160;++i){const float x=static_cast<float>(scratches.nextInt(1190)+5),y=i%2==0?static_cast<float>(scratches.nextInt(10)+5):static_cast<float>(scratches.nextInt(10)+1235);g.setColour(console::copper.withAlpha(0.4f));g.drawLine(x,y,x+static_cast<float>(scratches.nextInt(12)+2),y+1);}
    console::demon(g,{30,22,80,86});
    g.setColour(console::cream);g.setFont(console::font(49,true));g.drawText("BASSDEAMON",124,19,470,61,juce::Justification::centredLeft);
    console::text(g,"By Guy_BioRg",{128,80,240,22},16,console::copper.brighter());
    console::panel(g,{24,128,820,320},true);paintSpectrum(g);
    console::panel(g,{862,128,314,320});{juce::Graphics::ScopedSaveState v(g);g.addTransform(juce::AffineTransform::translation(0,66));paintVU(g);}
    {
    juce::Graphics::ScopedSaveState eqLayout(g);
    g.addTransform(juce::AffineTransform::translation(0,60));
    console::panel(g,{24,552,1152,62});
    console::text(g,"KICK EQ",{46,561,156,25},23);
    console::text(g,"PRE-GLUE / Q 3",{47,587,150,15},11,console::muted);
    console::text(g,"FREQUENCY",{397,570,98,24},12,console::muted);
    console::text(g,"CUT",{615,570,32,24},12,console::muted);
    const int kickState = processor.kickEqStatus.load();
    const bool hasFrequency = processor.parameters.getRawParameterValue("kickEq.frequency")->load() >= 30;
    const juce::String eqStatus = kickState == 5 ? "PRESS PLAY" : kickState == 1 ? "LEARNING "+juce::String(int(processor.kickEqProgress.load()*100))+"%" : kickState == 4 ? "NO KICK SIDECHAIN" : kickState == 2 ? "NO RELIABLE PEAK" : hasFrequency ? "FREQUENCY SET" : "READY TO LEARN";
    console::text(g,eqStatus,{756,570,176,24},12,(kickState==2||kickState==4)?console::orange:console::cream);
    if (processor.compareMatch.load())
        console::text(g,"MATCH "+juce::String(processor.comparisonGainDb.load(),1)+" dB",{1011,597,140,14},10,console::muted,juce::Justification::centred);
    }
    {juce::Graphics::ScopedSaveState body(g);g.addTransform(juce::AffineTransform::translation(0,132));
    console::panel(g,{24,328,1152,144},true);paintWave(g);
    g.addTransform(juce::AffineTransform::translation(0,64));
    {juce::Graphics::ScopedSaveState moduleScope(g);g.addTransform(juce::AffineTransform::translation(0,160));
    const char* titles[]{"01  GLUE","02  PHASE","03  UNMASK"};
    const char* descriptions[]{"ADAPTIVE BASS COMPRESSION","LEARN + LOCK / MANUAL PHASE","DYNAMIC SPECTRAL SPACE"};
    for(int i=0;i<3;++i){const float x=24+static_cast<float>(i)*392;
        console::panel(g,{x,340,368,442});console::text(g,titles[i],{x+22,351,255,35},28);
        if (i < 2) console::text(g,descriptions[i],{x+22,383,320,17},12,console::muted);
        else console::text(g,"FOCUS Hz",{x+22,383,60,17},11,console::muted);
        g.setColour(console::copper.withAlpha(0.45f));g.drawLine(x+20,438,x+348,438,1);
        g.setColour(console::ink);g.fillRoundedRectangle(x+250,403,95,28,4);
        console::text(g,juce::String(compensation[static_cast<size_t>(i)],1)+" dB",{x+253,405,89,24},15,console::amber,juce::Justification::centred);
    }
    
    const auto readout=[&](const juce::String& title,const juce::String& value,float y){
        console::text(g,title,{249,y,122,20},12,console::muted);if(processor.parameters.getRawParameterValue("glue.timing")->load()<.5f){g.setColour(console::ink);g.fillRoundedRectangle(249,y+24,122,32,4);
        console::text(g,value,{253,y+27,114,26},18,console::amber,juce::Justification::centred);}
    };
    readout("ATTACK",juce::String(processor.glueAttack.load(),1)+" ms",496);
    readout("RELEASE",juce::String(juce::roundToInt(processor.glueRelease.load()))+" ms",575);
    console::text(g,"GLUE GR  "+juce::String(processor.glueReduction.load(),1)+" dB",{46,724,190,24},17,console::amber);
    console::text(g,"BASS-LED / STEREO LINKED",{46,752,294,18},12,console::muted);
    const int state=processor.learnStatus.load();
    const auto match=processor.learnCorrelation.load();
    const bool phaseOn=processor.parameters.getRawParameterValue("phase.on")->load()>.5f;
    const juce::String status=state==7?"PRESS PLAY":state==1?"LEARNING "+juce::String(int(processor.learnProgress.load()*100))+"%":state==2?"NO RELIABLE MATCH":state==3?"AMBIGUOUS MATCH":state==4?"UNSTABLE MATCH":state==6?"NO KICK SIDECHAIN":state==5?(phaseOn?"APPLIED / r ":"READY / PHASE OFF / r ")+juce::String(match,2):"READY";
    console::text(g,status,{604,707,170,28},12,(state==6||(state>=2&&state<=4))?console::orange:console::amber);
    const juce::String hint=state==7?"Start playback with kick and bass":state==1?"Keep bass + kick playing for 4 seconds":state==5&&!phaseOn?"Enable PHASE to hear the learned settings":state==6?"Check the kick send to the plugin sidechain":state>=2&&state<=4?"Settings kept: use manual delay / polarity":"Learn sets delay + polarity; all-pass stays manual";
    console::text(g,hint,{438,746,325,22},12,console::cream);
    console::text(g,"GR -"+juce::String(processor.unmaskReduction.load(),2)+" dB",{832,439,185,18},12,console::cream);
    console::text(g,"FOCUS  "+juce::String(focus,1)+" Hz",{1026,439,118,18},13,console::amber);
    }
    if(processor.parameters.getRawParameterValue("glue.timing")->load()>1.5f){
        console::text(g,juce::String(processor.glueRelease.load(),1)+" ms",{249,793,122,18},12,console::cream);
    }
    if(processor.parameters.getRawParameterValue("unmask.timing")->load()>.5f){
        console::text(g,"R "+juce::String(processor.unmaskRelease.load(),1)+" ms",{926,838,101,20},12,console::cream);
        console::text(g,"RELEASE",{926,896,99,18},12,console::cream);
    }
    console::panel(g,{24,960,1152,144});
    console::text(g,"04  SOFT TUBE",{52,944,300,14},13,console::copper);
    const bool tubeOn=processor.parameters.getRawParameterValue("tube.on")->load()>.5f;
    const bool audition=processor.parameters.getRawParameterValue("tube.audition")->load()>.5f;
    console::text(g,!tubeOn?"TUBE OFF":audition?"LISTEN: ADDED SIGNAL":"4x / PARALLEL LOW-BAND",{870,1044,285,20},13,audition?console::amber:console::cream);
    console::text(g,"ADDED "+juce::String(processor.tubeAddedDb.load(),1)+" dBFS",{870,1071,285,18},12,console::cream);
    console::text(g,"AGC "+juce::String(processor.tubeGainDb.load(),1)+" dB",{1035,1007,125,24},14,console::amber);
    console::panel(g,{24,1118,1152,46},true);
    {juce::Graphics::ScopedSaveState footerScope(g);g.addTransform(juce::AffineTransform::translation(0,320));
    g.setColour(sidePeak>0.0003f?console::amber:console::muted.darker());g.fillEllipse(45,816,9,9);
    console::text(g,sidePeak>0.0003f?"KICK SC ACTIVE":"KICK SC IDLE",{63,808,144,26},13,console::cream);
    console::text(g,"IN "+juce::String(std::max(inputDb,-99.0f),1)+"  /  OUT "+juce::String(std::max(outputDb,-99.0f),1)+" dBFS RMS",{221,808,315,26},13,console::muted);
    console::text(g,"PEAK "+juce::String(std::max(peak,-99.0f),1)+" dBFS",{548,808,180,26},14,clipFrames>0?console::orange:console::cream);
    console::text(g,"TRIM",{762,808,60,26},13,console::muted);
    console::text(g,"v0.2.14",{1090,808,66,26},13,console::muted,juce::Justification::centredRight);
    }
    }
    g.drawImageAt(grain,0,0);
}
bool BassDeamonEditor::spectrumIsPost() const {
    return processor.spectrumView()!=0;
}
std::array<double,2> BassDeamonEditor::spectrumLimits() const {
    const double ceiling=std::min(20000.0,analyzer.sampleRate()*.5);
    const double minimum=juce::jlimit(0.0,ceiling-20.0,double(processor.parameters.getRawParameterValue("view.minHz")->load()));
    const double maximum=juce::jlimit(minimum+20.0,ceiling,double(processor.parameters.getRawParameterValue("view.maxHz")->load()));
    return {minimum,maximum};
}
void BassDeamonEditor::paintSpectrum(juce::Graphics& g) {
    const bool post = spectrumIsPost();
    const juce::Colour bassBlue(0xff8faef1);
    const auto legend = [&](const juce::String& label, float x, float width, juce::Colour colour) {
        g.setColour(colour);
        g.drawLine(x,150,x+12,150,2);
        console::text(g,label,{x+17,140,width-17,20},13,colour);
    };
    legend(post?"BASS POST":"BASS PRE",235,94,bassBlue);
    legend("KICK",330,55,juce::Colour(0xffd59666));
    legend("KICK EQ",386,65,eqTrace);
    if (post) legend("UNMASK",454,71,unmaskTrace);
    console::text(g,"FROM",{526,166,130,15},11,console::muted);
    console::text(g,"TO",{682,166,132,15},11,console::muted);
    console::text(g,"RATE",{235,167,52,20},13,console::muted);
    const int clashes = paintSpectrumPlot(g,{72,190,742,160},post);
    const juce::String status = !spectrumDisplay.ready ? "COLLECTING AUDIO"
        : clashes > 0 ? juce::String(clashes) + (clashes==1?" COLLISION":" COLLISIONS")
        : post ? "NO SIGNIFICANT RESIDUAL" : "NO COLLISION IN VIEW";
    const auto mode = visualUnmaskResponse.mode;
    const juce::String component = mode == bassdeamon::UnmaskStereoMode::mid ? " MID"
        : mode == bassdeamon::UnmaskStereoMode::side ? " SIDE" : "";
    const juce::String comparison = processor.compareDry.load() ? "  /  DRY AUDITION" : "";
    console::text(g,status+comparison,{72,433,430,14},13,console::muted);
    if (post) console::text(g,"UNMASK"+component+" PEAK  "+juce::String(visualUnmaskGr,1)+" dB",
                            {510,433,304,14},13,unmaskTrace,juce::Justification::centredRight);
}

int BassDeamonEditor::paintSpectrumPlot(juce::Graphics& g,juce::Rectangle<float> plot,bool post) {
    const auto limits=spectrumLimits();const double minimum=limits[0],maximum=limits[1];
    // Log-like bass zoom preserves a usable zero-Hz endpoint.
    const double start = std::log1p(minimum/20);
    const double span = std::log1p(maximum/20)-start;
    const auto fx=[&](double hz){return plot.getX()+float((std::log1p(hz/20)-start)/span)*plot.getWidth();};
    const auto fy=[&](float level){return plot.getBottom()-juce::jlimit(0.f,1.f,(level+96)/96)*plot.getHeight();};
    g.setColour(juce::Colour(0xff11151a));
    g.fillRect(plot);
    for(int level:{-96,-72,-48,-24,0}){
        const float y=fy(float(level));
        g.setColour(console::cream.withAlpha(.11f));
        g.drawHorizontalLine(juce::roundToInt(y),plot.getX(),plot.getRight());
        console::text(g,juce::String(level),{plot.getX()-38,y-7,33,14},12,console::muted,juce::Justification::centredRight);
    }
    float lastTick=plot.getX()-64;
    for(double hz:{0.,20.,30.,40.,50.,60.,80.,100.,150.,200.,300.,500.,750.,1000.,1500.,2000.,3000.,5000.,7500.,10000.,15000.,20000.}){
        if(hz<minimum||hz>maximum)continue;
        const float x=fx(hz);
        if(x-lastTick<59)continue;
        lastTick=x;
        g.setColour(console::cream.withAlpha(.10f));
        g.drawVerticalLine(juce::roundToInt(x),plot.getY(),plot.getBottom());
        console::text(g,juce::String(int(hz))+" Hz",{x-29,353,58,17},12,console::muted,juce::Justification::centred);
    }
    const juce::Colour bassBlue(0xff8faef1),red(0xffff655a);
    const auto& selected=post?visualPost:visualPre;
    const auto& collisions=post?visualMaskPost:visualMaskPre;
    const double binWidth=analyzer.sampleRate()/bassdeamon::RelationAnalyzer::fftSize;
    struct Region { size_t first,last,peak; double hz; float strength; };
    std::vector<Region> regions;
    const auto collect = [&](const auto& values,const auto& mask,bool resolved) {

    for(size_t b=1;b+1<values.size();){
        if(mask[b]<.12f){++b;continue;}
        const size_t first=b;size_t peak=b;
        while(b+1<values.size()&&mask[b]>=.12f){if(values[b]>values[peak])peak=b;++b;}
        const size_t last=b-1;
        if((double(last)+.5)*binWidth<minimum||(double(first)-.5)*binWidth>maximum)continue;
        if (resolved && std::any_of(visualMaskPost.begin()+first,visualMaskPost.begin()+last+1,
                                    [](float strength){ return strength >= .12f; })) continue;
        const double left=values[peak-1],centre=values[peak],right=values[peak+1],den=left-2*centre+right;
        const double offset=std::abs(den)>1e-12?std::clamp(.5*(left-right)/den,-.5,.5):0;
        regions.push_back({first,last,peak,(double(peak)+offset)*binWidth,resolved?0.f:mask[peak]});
    }
    };
    collect(selected,collisions,false);
    const int activeRegions = int(regions.size());
    if (post) collect(visualPre,visualMaskPre,true);
    // Give priority to the most significant peaks and leave room between markers.
    std::sort(regions.begin(),regions.end(),[&](const Region& a,const Region& b){
        const auto rank=[&](const Region& r){return (r.strength>0?200.f:0.f)+
            std::min(visualPre[r.peak],visualKick[r.peak])+10*r.strength;};
        return rank(a)>rank(b);
    });
    std::array<const Region*,1> visibleRegions{};
    size_t count=0;
    for(const auto& region:regions){
        const float x=fx(region.hz);
        if(x<plot.getX()||x>plot.getRight())continue;
        const auto end=visibleRegions.begin()+count;
        if(std::any_of(visibleRegions.begin(),end,[&](const Region* r){return std::abs(x-fx(r->hz))<27;}))continue;
        visibleRegions[count++]=&region;
        if(count==visibleRegions.size())break;
    }
    {
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(plot.toNearestInt());
        for(int trace=0;trace<2;++trace){
            const auto& values=trace==0?visualKick:selected;
            const auto colour=trace==0?juce::Colour(0xffd59666):bassBlue;
            juce::Path curve;
            const size_t first=size_t(std::max(0.,std::floor(minimum/binWidth)));
            const size_t last=std::min(values.size()-1,size_t(std::ceil(maximum/binWidth)));
            curve.startNewSubPath(fx(first*binWidth),fy(values[first]));
            for(size_t b=first+1;b<=last;++b)curve.lineTo(fx(b*binWidth),fy(values[b]));
            auto fill=curve;
            fill.lineTo(fx(last*binWidth),plot.getBottom());
            fill.lineTo(fx(first*binWidth),plot.getBottom());
            fill.closeSubPath();
            g.setGradientFill(juce::ColourGradient(colour.withAlpha(.40f),0,plot.getY(),
                                                  colour.withAlpha(.09f),0,plot.getBottom(),false));
            g.fillPath(fill);
            g.setColour(colour.withAlpha(.92f));
            g.strokePath(curve,juce::PathStrokeType(1.45f));
        }
        for(size_t i=0;i<count;++i){
            const auto& region=*visibleRegions[i];
            const float x=fx(region.hz);
            const float y=fy(selected[region.peak]);
            const bool active=region.strength>0;
            const auto accent=active?red:console::muted;
            g.setColour(accent.withAlpha(.30f));
            g.drawVerticalLine(juce::roundToInt(x),y,plot.getBottom());
            g.setColour(accent);
            if(active)g.fillEllipse(x-3,y-3,6,6);
            else g.drawEllipse(x-3,y-3,6,6,1);
        }
    }
    const double eqHz=processor.parameters.getRawParameterValue("kickEq.frequency")->load();
    const double eqCut=processor.parameters.getRawParameterValue("kickEq.cut")->load();
    const bool showEq=processor.parameters.getRawParameterValue("kickEq.on")->load()>.5f
        && processor.parameters.getRawParameterValue("global.on")->load()>.5f
        && !processor.compareDry.load() && eqHz>=30 && eqCut>0;
    if (post || showEq) {
        const float top=plot.getY()+8;
        const float depth=plot.getHeight()-16;
        const auto gainY=[&](double change) {
            return top+float(std::clamp(-change,0.,24.)/24.)*depth;
        };
        console::text(g,"dB",{816,173,25,15},11,console::muted,juce::Justification::centred);
        console::text(g,"dBFS",{27,173,42,15},11,console::muted,juce::Justification::centredRight);
        for (int level : {0,-4,-12,-24}) {
            const float y=gainY(level);
            g.setColour(console::cream.withAlpha(.35f));
            g.drawHorizontalLine(juce::roundToInt(y),plot.getRight()-4,plot.getRight()+3);
            console::text(g,juce::String(level),{816,y-7,25,14},11,console::muted,juce::Justification::centred);
        }

        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(plot.toNearestInt());
        const auto responsePath=[&](const auto& gainAt, double* deepest) {
            juce::Path curve;
            for (int i=0;i<=240;++i) {
                const double fraction=i/240.;
                const double hz=20*std::expm1(start+span*fraction);
                const double change=gainAt(hz);
                if (deepest) *deepest=std::min(*deepest,change);
                const float x=plot.getX()+float(fraction)*plot.getWidth();
                const float y=gainY(change);
                if (i==0) curve.startNewSubPath(x,y); else curve.lineTo(x,y);
            }
            return curve;
        };
        if (post) {
            if (unmaskPathDirty || unmaskPathLimits!=limits) {
                unmaskPathLimits=limits;
                unmaskPathDirty=false;
                visualUnmaskPath.clear();
                double deepest=0;
                if (visualUnmaskResponse.wet>1e-5) {
                    visualUnmaskPath=responsePath([&](double hz) {
                        return visualUnmaskResponse.gainDb(hz,analyzer.sampleRate());
                    },&deepest);
                }
                if (deepest>-.05) visualUnmaskPath.clear();
                visualUnmaskGr=deepest<-.05 ? float(deepest) : 0;
            }
            if (!visualUnmaskPath.isEmpty()) {
                auto shade=visualUnmaskPath;
                shade.lineTo(plot.getRight(),top);
                shade.lineTo(plot.getX(),top);
                shade.closeSubPath();
                g.setColour(unmaskTrace.withAlpha(.12f));
                g.fillPath(shade);
                g.setColour(console::ink);
                g.strokePath(visualUnmaskPath,juce::PathStrokeType(4.4f));
                g.setColour(unmaskTrace);
                g.strokePath(visualUnmaskPath,juce::PathStrokeType(2.2f));
            }
        }
        if (showEq) {
            const double amplitude=std::pow(10.,-eqCut/40);
            const double halfRateAngle=bassdeamon::pi/analyzer.sampleRate();
            const double centre=std::tan(eqHz*halfRateAngle);
            const auto eqPath=responsePath([&](double hz) {
                const double ratio=std::tan(hz*halfRateAngle)/centre;
                const double real=1-ratio*ratio;
                const double damping=ratio*ratio/9.;
                const double power=real*real;
                return 10*std::log10((power+damping*amplitude*amplitude)/(power+damping/(amplitude*amplitude)));
            },nullptr);
            constexpr float dashes[]{7,4};
            juce::Path dashed;
            juce::PathStrokeType(2).createDashedStroke(dashed,eqPath,dashes,2);
            g.setColour(eqTrace);
            g.fillPath(dashed);
        }
    }
    {
        const auto formatDb=[](float value){
            return (value>=0?juce::String("+"):juce::String{})+juce::String(value,1)+" dB";
        };
        // Keep readouts below the plot, leaving the actual peaks visible.
        const float width=740.f;
        for (size_t i=0;i<count;++i) {
            const auto& region=*visibleRegions[i];
            const auto bin=region.peak;
            const auto& pairs=spectrumDisplay.paired;
            const float pre=pairs[0][bin], after=pairs[1][bin], kickDb=pairs[2][bin];
            const float bassDb=post?after:pre;
            const auto relation=bassDb>-110 && kickDb>-110 ? formatDb(bassDb-kickDb) : juce::String("--");
            const auto delta=pre>-110 && after>-110 ? formatDb(after-pre) : juce::String("--");
            const float boxX=plot.getX()+float(i)*(width+4);
            const bool resolved=region.strength==0;
            const auto accent=resolved?juce::Colour(0xffa8bdb0):red.withAlpha(.45f+.55f*region.strength);
            juce::Rectangle<float> label(boxX,374,width,50);
            g.setColour(console::muted.withAlpha(.25f));g.drawHorizontalLine(374,label.getX(),label.getRight());
            console::text(g,resolved?"BELOW THRESHOLD":"MAIN COLLISION",label.withWidth(190).withHeight(17),12,console::muted);
            console::text(g,juce::String(region.hz,1)+" Hz",label.withWidth(190).withTrimmedTop(17),21,accent);
            console::text(g,"BASS - KICK",{boxX+235,374,185,17},12,console::muted);
            console::text(g,relation,{boxX+235,391,185,29},20,console::cream);
            if(post){
                console::text(g,"POST - PRE",{boxX+470,374,185,17},12,console::muted);
                console::text(g,delta,{boxX+470,391,185,29},20,console::cream);
            }

        }
    }
    return activeRegions;
}
void BassDeamonEditor::paintWave(juce::Graphics& g){
    console::text(g,showSum?"SCOPE / KICK + BASS PRE & POST":"SCOPE / SUM K+B OFF",{47,337,650,20},13,console::cream);
    if(!showSum){
        console::text(g,"SUM GRAPH HIDDEN / AUDIO UNCHANGED",{48,390,1104,32},16,console::muted,juce::Justification::centred);
        return;
    }
    const juce::Rectangle<float> plot(48,374,1104,75);
    float maximum=.05f;for(const auto& w:visibleScope.high)for(float v:w)maximum=std::max(maximum,std::abs(v));
    for(const auto& w:visibleScope.low)for(float v:w)maximum=std::max(maximum,std::abs(v));
    scopeScale=maximum;
    for(int i=0;i<=4;++i){const float x=plot.getX()+i*plot.getWidth()/4;g.setColour(console::cream.withAlpha(.15f));g.drawVerticalLine(int(x),plot.getY(),plot.getBottom());}
    g.setColour(console::cream.withAlpha(.2f));g.drawHorizontalLine(int(plot.getCentreY()),plot.getX(),plot.getRight());
    for(int trace=0;trace<2;++trace){const size_t stream=size_t(trace+3);
        g.setColour(trace==0?console::cream.withAlpha(.65f):trace==1?juce::Colour(0xff56e6c0):console::amber);
        for(size_t i=0;i<bassdeamon::BeatScope::size;++i){const float x=plot.getX()+float(i)*plot.getWidth()/float(bassdeamon::BeatScope::size-1);
            const float y1=plot.getCentreY()-visibleScope.low[stream][i]/scopeScale*plot.getHeight()*.48f;
            const float y2=plot.getCentreY()-visibleScope.high[stream][i]/scopeScale*plot.getHeight()*.48f;
            g.drawLine(x,y1,x,y2,1.4f);}
    }
    const bool host=processor.tempoFromHost.load();
    console::text(g,"1/4 NOTE / "+juce::String(60000/processor.tempo.load(),1)+" ms / "+(host?"PROJECT ":"FALLBACK ")+juce::String(processor.tempo.load(),1)+" BPM / GREY: PRE  GREEN: POST  AMBER: KICK / visual sum",{48,451,1080,14},11,console::cream);
}
void BassDeamonEditor::paintVU(juce::Graphics& g){
    const juce::Rectangle<float> r(878,143,282,157);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffedcb88),r.getCentreX(),r.getCentreY(),juce::Colour(0xffa7793d),r.getX(),r.getY(),true));g.fillRoundedRectangle(r,7);
    juce::Graphics::ScopedSaveState scope(g);g.reduceClipRegion(r.toNearestInt());
    const float cx=1019,cy=323,radius=139;
    juce::Path arc;arc.addCentredArc(cx,cy,radius,radius,0,-0.78f,0.78f,true);g.setColour(console::ink);g.strokePath(arc,juce::PathStrokeType(2));
    for(float d:{20.0f,10.0f,6.0f,3.0f,1.0f,0.0f}){
        const float a=0.78f-1.56f*std::sqrt(d/20);
        g.drawLine(cx+std::sin(a)*(radius-4),cy-std::cos(a)*(radius-4),cx+std::sin(a)*(radius+7),cy-std::cos(a)*(radius+7),1.3f);
        console::text(g,juce::String(d,0),{cx+std::sin(a)*(radius+21)-13,cy-std::cos(a)*(radius+21)-10,26,20},14,console::ink,juce::Justification::centred);
    }
    const float angle=0.78f-1.56f*std::sqrt(juce::jlimit(0.0f,20.0f,vu)/20);
    g.setColour(juce::Colours::black.withAlpha(0.2f));g.drawLine(cx+3,cy,cx+3+std::sin(angle)*153,cy-std::cos(angle)*153,4);
    g.setColour(juce::Colour(0xff6e3219));g.drawLine(cx,cy,cx+std::sin(angle)*153,cy-std::cos(angle)*153,2.2f);
    console::text(g,"GAIN REDUCTION",{891,252,257,25},19,console::ink,juce::Justification::centred);
    console::text(g,"GLUE  /  dB",{891,276,257,18},13,console::ink.withAlpha(0.75f),juce::Justification::centred);
}
void BassDeamonEditor::timerCallback(){refreshVisuals(juce::Time::getMillisecondCounterHiRes());}
void BassDeamonEditor::refreshVisuals(double clockMilliseconds){
    const double rate=processor.getSampleRate()>0?processor.getSampleRate():48000;
    if(rate!=analyzer.sampleRate()){analyzer.prepare(rate);spectrumDisplay.reset();}
    const bool poll = lastAnalysisPoll < 0 || clockMilliseconds < lastAnalysisPoll
        || clockMilliseconds - lastAnalysisPoll >= 30;
    uint64_t received=0;
    if (poll) {
        lastAnalysisPoll=clockMilliseconds;
        const double analysisStart=juce::Time::getMillisecondCounterHiRes();
        const auto beforeSequence=analyzer.streamSequence();
        const auto beforeConsumed=analyzer.consumedFrames;
        analyzer.consume(processor.analysisQueue,processor.tempo.load(),processor.parameters.getRawParameterValue("view.hold")->load()>.5f);
        if (analyzer.streamSequence()!=beforeSequence) {
            lastAnalysisTime=clockMilliseconds;
            staleAnalysis=false;
            received=analyzer.consumedFrames-beforeConsumed;
        } else if (!staleAnalysis && clockMilliseconds-lastAnalysisTime>500) {
            analyzer.prepare(rate);
            staleAnalysis=true;
        }
        analyzerMilliseconds=.85*analyzerMilliseconds+.15*(juce::Time::getMillisecondCounterHiRes()-analysisStart);
    }
    const double currentRate = processor.analyzerRefreshHz();
    if (currentRate != syncedRefreshHz) {
        syncedRefreshHz = currentRate;
        analyzerRefresh.setValue(currentRate,juce::dontSendNotification);
    }
    const auto response = processor.consumeUnmaskResponse();
    if (!spectrumHold.getToggleState() && response.activity >= pendingUnmaskResponse.activity)
        pendingUnmaskResponse=response;
    const auto revision=spectrumDisplay.revision;
    const bool wasReady=spectrumDisplay.ready;
    if(!spectrumHold.getToggleState())spectrumDisplay.pushAtRate(analyzer,received,clockMilliseconds*.001,currentRate);
    const bool newFrame=revision!=spectrumDisplay.revision || (wasReady&&!spectrumDisplay.ready);
    if(!spectrumHold.getToggleState() && newFrame){
        visualPre=spectrumDisplay.shown[0];visualPost=spectrumDisplay.shown[1];visualKick=spectrumDisplay.shown[2];
        visualMaskPre=spectrumDisplay.shown[3];visualMaskPost=spectrumDisplay.shown[4];
        visualUnmaskResponse=spectrumDisplay.ready ? pendingUnmaskResponse : bassdeamon::UnmaskResponse{};
        pendingUnmaskResponse={};
        unmaskPathDirty=true;
        visibleScope=analyzer.scope;
    }
    if (!poll && !newFrame) return;
    processor.refreshHistory();undoButton.setEnabled(processor.canUndo());redoButton.setEnabled(processor.canRedo());
    cpuLabel.setText("DSP CPU "+juce::String(processor.cpuLoad.getLoadAsPercentage(),1)+"%  |  ANALYZER "+juce::String(analyzerMilliseconds,1)+" ms  |  OVR "+juce::String(processor.cpuLoad.getXRunCount()),juce::dontSendNotification);
    const int timing=int(processor.parameters.getRawParameterValue("glue.timing")->load());
    const bool sync=processor.parameters.getRawParameterValue("unmask.timing")->load()>.5f;
    const bool direct=processor.parameters.getRawParameterValue("unmask.controlDb")->load()>.5f;
    attenuationKnob->setVisible(direct);depthKnob->setVisible(!direct);
    spectrumPre.setToggleState(!spectrumIsPost(),juce::dontSendNotification);
    spectrumPost.setToggleState(processor.spectrumView()==1,juce::dontSendNotification);
    const bool powered=processor.parameters.getRawParameterValue("global.on")->load()>.5f;
    const bool canCompare=powered && processor.parameters.getRawParameterValue("unmask.delta")->load()<.5f &&
        !(processor.parameters.getRawParameterValue("tube.on")->load()>.5f &&
          processor.parameters.getRawParameterValue("tube.audition")->load()>.5f);
    kickEqLearn.setEnabled(powered && processor.kickEqStatus.load()!=1);
    dryCompare.setEnabled(canCompare);matchCompare.setEnabled(canCompare);
    dryCompare.setToggleState(processor.compareDry.load(),juce::dontSendNotification);
    matchCompare.setToggleState(processor.compareMatch.load(),juce::dontSendNotification);
    const bool focusAuto=processor.parameters.getRawParameterValue("unmask.track")->load()>.5f;
    focusMin.setEnabled(focusAuto);focusMax.setEnabled(focusAuto);
    glueAttackSlider.setVisible(timing!=0);glueReleaseSlider.setVisible(timing==1);
    unmaskAttackKnob->setVisible(true);unmaskReleaseKnob->setVisible(!sync);
    for(auto& c:choices){const auto id=c->box.getComponentID();
        if(id=="glue.releaseNote")c->box.setVisible(timing==2);
        if(id=="unmask.releaseNote")c->box.setVisible(sync);
    }
    for(size_t i=0;i<bass.size();++i){bass[i]=0.7f*bass[i]+0.3f*processor.spectrumBass[i].load();kick[i]=0.7f*kick[i]+0.3f*processor.spectrumKick[i].load();cut[i]=processor.spectrumCut[i].load();}
    for(size_t i=0;i<3;++i)compensation[i]=processor.agcMeters[i].load();
    vu=0.65f*vu+0.35f*processor.glueReduction.load();focus=processor.focusHz.load();q=processor.unmaskQ.load();
    peak=static_cast<float>(bassdeamon::db(processor.outputPeak.load()));sidePeak=processor.kickPeak.load();inputDb=processor.inputRmsDb.load();outputDb=processor.outputRmsDb.load();
    if(peak>=0)clipFrames=90;else clipFrames=std::max(0,clipFrames-1);
    for(auto& sw:switches)if(sw->button.getComponentID()=="tube.audition")sw->button.setEnabled(processor.parameters.getRawParameterValue("global.on")->load()>.5f);
    learn.setEnabled(processor.learnStatus.load()!=1&&processor.parameters.getRawParameterValue("global.on")->load()>.5f);
    aButton.setToggleState(processor.selectedAB()==0,juce::dontSendNotification);bButton.setToggleState(processor.selectedAB()==1,juce::dontSendNotification);
    frequencyKnob->slider.setEnabled(processor.parameters.getRawParameterValue("unmask.track")->load()<0.5f);
    face.repaint();
}
