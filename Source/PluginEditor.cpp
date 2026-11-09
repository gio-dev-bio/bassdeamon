#include "PluginEditor.h"
#include <complex>

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
    learn.setTooltip("Analyse four seconds. Apply only a strong, distinct match stable across both halves. Weak, ambiguous or moving matches keep your settings. Does not enable PHASE. Correlation is not a sound-quality score.");
    addKnob("unmask.amount","DEPTH","",true,{830,448,170,184},"Maximum spectral unmasking amount. The kick is never summed into the bass.");
    frequencyKnob=&addKnob("unmask.frequency","MANUAL FOCUS"," Hz",false,{1026,460,118,120},"Manual focus frequency, available when AUTO FOCUS is off.");
    addKnob("unmask.width","WIDTH","",true,{1026,580,118,120},"Width of the focus region and dynamic EQ bands.");
    unmaskAttackKnob=&addKnob("unmask.attack","ATTACK"," ms",false,{824,670,99,95},"Dynamic cut attack, 1 to 50 ms.");
    unmaskReleaseKnob=&addKnob("unmask.release","RELEASE"," ms",false,{926,670,99,95},"Dynamic cut recovery, 30 to 500 ms.");
    ceiling.addItemList({"BUDGET 3 dB","BUDGET 6 dB","BUDGET 9 dB","BUDGET 12 dB"},1);ceiling.setBounds(832,640,110,25);
    face.addAndMakeVisible(ceiling);ceilingAttachment=std::make_unique<State::ComboBoxAttachment>(processor.parameters,"unmask.maxCut",ceiling);
    ceiling.setTooltip("Budget for the sum of all commanded band cuts, before Auto Gain. Not a brickwall output limiter.");
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
        button->setTooltip("Select one bass spectrum: PRE before processing or POST actual output. Kick remains the reference. Display only.");
    }
    const auto selectSpectrum=[this](float mode){
        auto* parameter=processor.parameters.getParameter("view.spectrum");
        parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(mode));parameter->endChangeGesture();
        timerCallback();
    };
    spectrumPre.onClick=[selectSpectrum]{selectSpectrum(0);};spectrumPost.onClick=[selectSpectrum]{selectSpectrum(1);};
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
    bassdeamon::AnalysisFrame oldFrame;while(processor.analysisQueue.pop(oldFrame)){}
    processor.analysisQueue.enabled.store(true,std::memory_order_release);
    vu=processor.glueReduction.load();
    setResizable(true,true);setResizeLimits(780,767,1560,1534);
    if(auto* c=getConstrainer())c->setFixedAspectRatio(1200.0/1180.0);
    setSize(900,885);timerCallback();startTimerHz(30);
}
BassDeamonEditor::~BassDeamonEditor(){stopTimer();processor.analysisQueue.enabled.store(false,std::memory_order_release);setLookAndFeel(nullptr);}
void BassDeamonEditor::resized(){face.setBounds(0,0,1200,1180);face.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth())/1200.0f));}
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
    grain=juce::Image(juce::Image::ARGB,1200,1180,true);juce::Random random(0xBADB00B);
    juce::Image::BitmapData data(grain,juce::Image::BitmapData::writeOnly);
    for(int y=0;y<1180;++y)for(int x=0;x<1200;++x){const int n=random.nextInt(9);data.setPixelColour(x,y,juce::Colour::fromRGBA(230,216,180,static_cast<juce::uint8>(n)));}
}
void BassDeamonEditor::paintConsole(juce::Graphics& g){
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff302e2a),0,0,console::ink,1200,1180,false));g.fillAll();
    console::panel(g,{5,5,1190,1170});
    g.setColour(console::copper.withAlpha(0.35f));g.drawRoundedRectangle(juce::Rectangle<float>(9,9,1182,1162),10,2);
    juce::Random scratches(1829);
    for(int i=0;i<160;++i){const float x=static_cast<float>(scratches.nextInt(1190)+5),y=i%2==0?static_cast<float>(scratches.nextInt(10)+5):static_cast<float>(scratches.nextInt(10)+1163);g.setColour(console::copper.withAlpha(0.4f));g.drawLine(x,y,x+static_cast<float>(scratches.nextInt(12)+2),y+1);}
    console::demon(g,{30,22,80,86});
    g.setColour(console::cream);g.setFont(console::font(49,true));g.drawText("BASSDEAMON",124,19,470,61,juce::Justification::centredLeft);
    console::text(g,"By Guy_BioRg",{128,80,240,22},16,console::copper.brighter());
    console::text(g,"KICK / BASS RELATION PROCESSOR",{628,82,465,24},15,console::muted);
    console::panel(g,{24,128,820,188},true);paintSpectrum(g);
    console::panel(g,{862,128,314,188});paintVU(g);
    console::panel(g,{24,328,1152,144},true);paintWave(g);
    {juce::Graphics::ScopedSaveState moduleScope(g);g.addTransform(juce::AffineTransform::translation(0,160));
    const char* titles[]{"01  GLUE","02  PHASE","03  UNMASK"};
    const char* descriptions[]{"ADAPTIVE BASS COMPRESSION","LEARN + LOCK / MANUAL PHASE","DYNAMIC SPECTRAL SPACE"};
    for(int i=0;i<3;++i){const float x=24+static_cast<float>(i)*392;
        console::panel(g,{x,340,368,442});console::text(g,titles[i],{x+22,351,255,35},28);
        console::text(g,descriptions[i],{x+22,383,320,17},12,console::muted);
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
    const juce::String status=state==1?"LEARNING "+juce::String(int(processor.learnProgress.load()*100))+"%":state==2?"NO RELIABLE MATCH":state==3?"AMBIGUOUS MATCH":state==4?"UNSTABLE MATCH":state==5?(phaseOn?"APPLIED / r ":"READY / PHASE OFF / r ")+juce::String(match,2):"READY";
    console::text(g,status,{604,707,170,28},12,state>=2&&state<=4?console::orange:console::amber);
    const juce::String hint=state==1?"Keep bass + kick playing for 4 seconds":state==5&&!phaseOn?"Enable PHASE to hear the learned settings":state>=2&&state<=4?"Settings kept: use manual delay / polarity":"Learn sets delay + polarity; all-pass stays manual";
    console::text(g,hint,{438,746,325,22},12,console::cream);
    console::text(g,"BAND GR "+juce::String(processor.unmaskReduction.load(),2)+" dB",{832,439,185,18},12,console::cream);
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
    console::text(g,"v0.2.5",{1090,808,66,26},13,console::muted,juce::Justification::centredRight);
    }
    g.drawImageAt(grain,0,0);
}
bool BassDeamonEditor::spectrumIsPost() const {
    return processor.parameters.getRawParameterValue("view.spectrum")->load()>.5f;
}
std::array<double,2> BassDeamonEditor::spectrumLimits() const {
    const double ceiling=std::min(20000.0,analyzer.sampleRate()*.5);
    const double minimum=juce::jlimit(0.0,ceiling-20.0,double(processor.parameters.getRawParameterValue("view.minHz")->load()));
    const double maximum=juce::jlimit(minimum+20.0,ceiling,double(processor.parameters.getRawParameterValue("view.maxHz")->load()));
    return {minimum,maximum};
}
void BassDeamonEditor::paintSpectrum(juce::Graphics& g){
    const juce::Rectangle<float> plot(72,190,742,88);
    const auto limits=spectrumLimits();const double minimum=limits[0],maximum=limits[1];
    const auto fx=[&](double hz){return plot.getX()+float((hz-minimum)/(maximum-minimum))*plot.getWidth();};
    const auto fy=[&](float db){return plot.getBottom()-juce::jlimit(0.f,1.f,(db+72)/72)*plot.getHeight();};
    for(int level:{-72,-48,-24,0}){const float y=fy(float(level));g.setColour(console::cream.withAlpha(.15f));g.drawHorizontalLine(juce::roundToInt(y),plot.getX(),plot.getRight());
        console::text(g,juce::String(level),{34,y-7,33,14},11,console::cream,juce::Justification::centredRight);}
    for(int i=0;i<=10;++i){const double hz=minimum+(maximum-minimum)*i/10;const float x=fx(hz);g.setColour(console::cream.withAlpha(i%2==0?.13f:.06f));g.drawVerticalLine(juce::roundToInt(x),plot.getY(),plot.getBottom());
        if(i%2==0)console::text(g,juce::String(hz,hz<100?1:0),{x-27,282,54,16},12,console::cream,juce::Justification::centred);}
    const juce::Colour postColour(0xff56e6c0);
    const juce::Colour collisionColour(0xffff4545);
    console::text(g,"KICK",{235,140,58,20},13,console::amber);
    console::text(g,"COLLISION",{318,140,118,20},13,collisionColour);
    console::text(g,"FROM",{526,166,130,15},11,console::muted);
    console::text(g,"TO",{682,166,132,15},11,console::muted);
    const bool post=spectrumIsPost();const auto& selected=post?visualPost:visualPre;
    const double binWidth=analyzer.sampleRate()/bassdeamon::RelationAnalyzer::fftSize;
    {
        juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(plot.toNearestInt());
        for(size_t b=1;b<selected.size();++b){const double hz=double(b)*binWidth;
            if(hz+binWidth*.5<minimum)continue;if(hz-binWidth*.5>maximum)break;
            const float overlap=bassdeamon::RelationAnalyzer::mask(selected[b],visualKick[b]);
            if(overlap<.08f)continue;
            const float left=std::max(plot.getX(),fx(hz-binWidth*.5));
            const float right=std::min(plot.getRight(),fx(hz+binWidth*.5));
            // Same potential-overlap criterion in both views, derived from the
            // selected bass and kick levels; never display commanded EQ cuts.
            g.setColour(collisionColour.withAlpha(.08f+.20f*overlap));
            g.fillRect(left,plot.getY(),std::max(.5f,right-left),plot.getHeight());
            const float top=fy(std::min(selected[b],visualKick[b]));
            g.setColour(collisionColour.withAlpha(.25f+.45f*overlap));
            g.fillRect(left,top,std::max(.5f,right-left),plot.getBottom()-top);
        }
        for(int trace=0;trace<2;++trace){const auto& values=trace==0?selected:visualKick;
            juce::Path line;bool started=false;
            for(size_t b=0;b<values.size();++b){const double hz=double(b)*binWidth;
                if(hz+binWidth<minimum)continue;
                if(!started){line.startNewSubPath(fx(hz),fy(values[b]));started=true;}
                else line.lineTo(fx(hz),fy(values[b]));
                if(hz>maximum)break;
            }
            if(trace==0&&started){auto fill=line;fill.lineTo(plot.getRight(),plot.getBottom());fill.lineTo(plot.getX(),plot.getBottom());fill.closeSubPath();g.setColour((post?postColour:console::cream).withAlpha(.08f));g.fillPath(fill);}
            g.setColour(trace==0?(post?postColour:console::cream):console::amber);
            g.strokePath(line,juce::PathStrokeType(2.f));
        }
    }
    console::text(g,(post?"POST output":"PRE input")+juce::String(" + KICK / RED: potential overlap / ")+juce::String(minimum,1)+" - "+juce::String(maximum,1)+" Hz / dBFS",{72,301,740,12},11,console::cream);
}
void BassDeamonEditor::paintWave(juce::Graphics& g){
    console::text(g,showSum?"SCOPE / KICK + BASS PRE & POST":"SCOPE / SUM K+B OFF",{47,337,650,20},13,console::cream);
    if(!showSum){
        console::text(g,"SUM GRAPH HIDDEN / AUDIO UNCHANGED",{48,390,1104,32},16,console::muted,juce::Justification::centred);
        return;
    }
    const juce::Rectangle<float> plot(48,374,1104,75);
    float maximum=.05f;for(const auto& w:analyzer.scope.high)for(float v:w)maximum=std::max(maximum,std::abs(v));
    for(const auto& w:analyzer.scope.low)for(float v:w)maximum=std::max(maximum,std::abs(v));
    if(processor.parameters.getRawParameterValue("view.hold")->load()<.5f)
        scopeScale=std::max(maximum,scopeScale*.98f);
    for(int i=0;i<=4;++i){const float x=plot.getX()+i*plot.getWidth()/4;g.setColour(console::cream.withAlpha(.15f));g.drawVerticalLine(int(x),plot.getY(),plot.getBottom());}
    g.setColour(console::cream.withAlpha(.2f));g.drawHorizontalLine(int(plot.getCentreY()),plot.getX(),plot.getRight());
    for(int trace=0;trace<2;++trace){const size_t stream=size_t(trace+3);
        g.setColour(trace==0?console::cream.withAlpha(.65f):trace==1?juce::Colour(0xff56e6c0):console::amber);
        for(size_t i=0;i<bassdeamon::BeatScope::size;++i){const float x=plot.getX()+float(i)*plot.getWidth()/float(bassdeamon::BeatScope::size-1);
            const float y1=plot.getCentreY()-analyzer.scope.low[stream][i]/scopeScale*plot.getHeight()*.48f;
            const float y2=plot.getCentreY()-analyzer.scope.high[stream][i]/scopeScale*plot.getHeight()*.48f;
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
void BassDeamonEditor::timerCallback(){
    const double rate=processor.getSampleRate()>0?processor.getSampleRate():48000;
    if(rate!=analyzer.sampleRate())analyzer.prepare(rate);
    analyzer.consume(processor.analysisQueue,processor.tempo.load(),processor.parameters.getRawParameterValue("view.hold")->load()>.5f);
    for(size_t i=0;i<visualPre.size();++i)for(int t=0;t<3;++t){auto& v=t==0?visualPre[i]:t==1?visualPost[i]:visualKick[i];const float target=t==0?analyzer.pre[i]:t==1?analyzer.post[i]:analyzer.kick[i];v+=(target-v)*(target>v?.35f:.10f);}
    const int timing=int(processor.parameters.getRawParameterValue("glue.timing")->load());
    const bool sync=processor.parameters.getRawParameterValue("unmask.timing")->load()>.5f;
    spectrumPre.setToggleState(!spectrumIsPost(),juce::dontSendNotification);
    spectrumPost.setToggleState(spectrumIsPost(),juce::dontSendNotification);
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
