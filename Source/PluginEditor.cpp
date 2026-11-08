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
    addKnob("unmask.attack","ATTACK"," ms",false,{824,670,99,95},"Dynamic cut attack, 1 to 50 ms.");
    addKnob("unmask.release","RELEASE"," ms",false,{926,670,99,95},"Dynamic cut recovery, 30 to 500 ms.");
    ceiling.addItemList({"MAX 3 dB","MAX 6 dB","MAX 9 dB","MAX 12 dB"},1);ceiling.setBounds(832,640,110,25);
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
    vu=processor.glueReduction.load();
    setResizable(true,true);setResizeLimits(900,645,1560,1118);
    if(auto* c=getConstrainer())c->setFixedAspectRatio(1200.0/860.0);
    setSize(1080,774);timerCallback();startTimerHz(30);
}
BassDeamonEditor::~BassDeamonEditor(){stopTimer();setLookAndFeel(nullptr);}
void BassDeamonEditor::resized(){face.setBounds(0,0,1200,860);face.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth())/1200.0f));}
void BassDeamonEditor::addSwitch(const char* id,const char* name,juce::Rectangle<int> bounds,const char* tip){
    auto s=std::make_unique<Switch>();s->button.setButtonText(name);s->button.setBounds(bounds);s->button.setTooltip(tip);
    face.addAndMakeVisible(s->button);s->attachment=std::make_unique<State::ButtonAttachment>(processor.parameters,id,s->button);switches.push_back(std::move(s));
}
BassDeamonEditor::Knob& BassDeamonEditor::addKnob(const char* id,const char* name,const char* unit,bool pct,juce::Rectangle<int> r,const char* tip){
    auto k=std::make_unique<Knob>(processor.parameters,id,name,unit,pct);k->setBounds(r);k->slider.setTooltip(tip);face.addAndMakeVisible(*k);
    auto& ref=*k;knobs.push_back(std::move(k));return ref;
}
void BassDeamonEditor::createTexture(){
    grain=juce::Image(juce::Image::ARGB,1200,860,true);juce::Random random(0xBADB00B);
    juce::Image::BitmapData data(grain,juce::Image::BitmapData::writeOnly);
    for(int y=0;y<860;++y)for(int x=0;x<1200;++x){const int n=random.nextInt(9);data.setPixelColour(x,y,juce::Colour::fromRGBA(230,216,180,static_cast<juce::uint8>(n)));}
}
void BassDeamonEditor::paintConsole(juce::Graphics& g){
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff302e2a),0,0,console::ink,1200,860,false));g.fillAll();
    console::panel(g,{5,5,1190,850});
    g.setColour(console::copper.withAlpha(0.35f));g.drawRoundedRectangle(juce::Rectangle<float>(9,9,1182,842),10,2);
    juce::Random scratches(1829);
    for(int i=0;i<160;++i){const float x=static_cast<float>(scratches.nextInt(1190)+5),y=i%2==0?static_cast<float>(scratches.nextInt(10)+5):static_cast<float>(scratches.nextInt(10)+843);g.setColour(console::copper.withAlpha(0.4f));g.drawLine(x,y,x+static_cast<float>(scratches.nextInt(12)+2),y+1);}
    console::demon(g,{30,22,80,86});
    g.setColour(console::cream);g.setFont(console::font(49,true));g.drawText("BASSDEAMON",124,19,470,61,juce::Justification::centredLeft);
    console::text(g,"By Guy_BioRg",{128,80,240,22},16,console::copper.brighter());
    console::text(g,"KICK / BASS RELATION PROCESSOR",{628,82,465,24},15,console::muted);
    console::panel(g,{24,128,820,188},true);paintSpectrum(g);
    console::panel(g,{862,128,314,188});paintVU(g);
    const char* titles[]{"01  GLUE","02  PHASE","03  UNMASK"};
    const char* descriptions[]{"ADAPTIVE BASS COMPRESSION","LEARN + LOCK / MANUAL PHASE","DYNAMIC SPECTRAL SPACE"};
    for(int i=0;i<3;++i){const float x=24+static_cast<float>(i)*392;
        console::panel(g,{x,340,368,442});console::text(g,titles[i],{x+22,351,255,35},28);
        console::text(g,descriptions[i],{x+22,383,320,17},12,console::muted);
        g.setColour(console::copper.withAlpha(0.45f));g.drawLine(x+20,438,x+348,438,1);
        g.setColour(console::ink);g.fillRoundedRectangle(x+250,403,95,28,4);
        console::text(g,juce::String(compensation[static_cast<size_t>(i)],1)+" dB",{x+253,405,89,24},15,console::amber,juce::Justification::centred);
    }
    console::text(g,"AUTO TIMING",{249,463,122,25},14,console::muted);
    const auto readout=[&](const juce::String& title,const juce::String& value,float y){
        console::text(g,title,{249,y,122,20},12,console::muted);g.setColour(console::ink);g.fillRoundedRectangle(249,y+24,122,32,4);
        console::text(g,value,{253,y+27,114,26},18,console::amber,juce::Justification::centred);
    };
    readout("ATTACK",juce::String(processor.glueAttack.load(),1)+" ms",496);
    readout("RELEASE",juce::String(juce::roundToInt(processor.glueRelease.load()))+" ms",575);
    console::text(g,"GLUE GR  "+juce::String(processor.glueReduction.load(),1)+" dB",{46,724,190,24},17,console::amber);
    console::text(g,"BASS-LED / STEREO LINKED",{46,752,294,18},12,console::muted);
    const int state=processor.learnStatus.load();
    const auto match=processor.learnCorrelation.load();
    const juce::String status=state==1?"ANALYSING 4 s...":state==2?"WEAK / NO SIGNAL":state==3?"AMBIGUOUS MATCH":state==4?"MOVING MATCH":state==5?"LOCKED  /  r "+juce::String(match,2):"MANUAL / READY";
    console::text(g,status,{604,707,170,28},13,state>=2&&state<=4?console::orange:console::amber);
    console::text(g,"POLARITY + DELAY ONLY / ALL-PASS IS MANUAL",{438,750,325,18},11.5f,console::muted);
    console::text(g,"FOCUS  "+juce::String(focus,1)+" Hz",{1026,439,118,18},13,console::amber);
    console::panel(g,{24,798,1152,46},true);
    g.setColour(sidePeak>0.0003f?console::amber:console::muted.darker());g.fillEllipse(45,816,9,9);
    console::text(g,sidePeak>0.0003f?"KICK SC ACTIVE":"KICK SC IDLE",{63,808,144,26},13,console::cream);
    console::text(g,"IN "+juce::String(std::max(inputDb,-99.0f),1)+"  /  OUT "+juce::String(std::max(outputDb,-99.0f),1)+" dBFS RMS",{221,808,315,26},13,console::muted);
    console::text(g,"PEAK "+juce::String(std::max(peak,-99.0f),1)+" dBFS",{548,808,180,26},14,clipFrames>0?console::orange:console::cream);
    console::text(g,"TRIM",{762,808,60,26},13,console::muted);
    console::text(g,"v0.2.2",{1090,808,66,26},13,console::muted,juce::Justification::centredRight);
    g.drawImageAt(grain,0,0);
}
void BassDeamonEditor::paintSpectrum(juce::Graphics& g){
    const juce::Rectangle<float> plot(58,162,750,120);
    const auto fx=[&](double hz){return plot.getX()+static_cast<float>(std::log(hz/30)/std::log(500.0/30))*plot.getWidth();};
    const auto fy=[&](float level){return plot.getBottom()-juce::jlimit(0.0f,1.0f,(level+72)/72)*plot.getHeight();};
    g.setColour(console::cream.withAlpha(0.065f));
    for(int i=0;i<5;++i){const float y=plot.getY()+static_cast<float>(i)*plot.getHeight()/4;g.drawHorizontalLine(juce::roundToInt(y),plot.getX(),plot.getRight());}
    for(double hz:{30.0,50.0,100.0,200.0,500.0}){
        const float x=fx(hz);g.drawVerticalLine(juce::roundToInt(x),plot.getY(),plot.getBottom());
        console::text(g,juce::String(hz,0),{x-17,287,38,17},12,console::muted,juce::Justification::centred);
    }
    for(int trace=0;trace<2;++trace){juce::Path p;
        const auto& values=trace==0?kick:bass;
        for(size_t i=0;i<values.size();++i){const float x=fx(bassdeamon::bandFrequencies[i]),y=fy(values[i]);if(i==0)p.startNewSubPath(x,y);else p.lineTo(x,y);}
        const auto colour=trace==0?console::amber:console::cream;
        auto fill=p;fill.lineTo(fx(bassdeamon::bandFrequencies.back()),plot.getBottom());fill.lineTo(plot.getX(),plot.getBottom());fill.closeSubPath();
        g.setColour(colour.withAlpha(0.055f));g.fillPath(fill);g.setColour(colour);g.strokePath(p,juce::PathStrokeType(1.8f));
    }
    // Evaluate the actual cascade response from the current per-band cuts, not a decorative curve.
    juce::Path eq;
    const double sr=processor.getSampleRate()>0?processor.getSampleRate():48000;
    std::array<bassdeamon::Biquad,bassdeamon::bandCount> responseFilters;
    for(size_t i=0;i<cut.size();++i)responseFilters[i].peak(bassdeamon::bandFrequencies[i],q,cut[i],sr);
    for(int j=0;j<=160;++j){const double hz=30*std::pow(500.0/30,static_cast<double>(j)/160);
        double attenuation=0;
        const auto z=std::polar(1.0,-2*bassdeamon::pi*hz/sr);
        for(size_t i=0;i<cut.size();++i){const auto& filter=responseFilters[i];
            attenuation+=bassdeamon::db(std::abs((filter.b0+filter.b1*z+filter.b2*z*z)/(1.0+filter.a1*z+filter.a2*z*z)));
        }
        const float x=fx(hz),y=plot.getY()+6+static_cast<float>(std::clamp(-attenuation/12,0.0,1.0))*104;
        if(j==0)eq.startNewSubPath(x,y);else eq.lineTo(x,y);
    }
    g.setColour(console::orange.withAlpha(0.9f));g.strokePath(eq,juce::PathStrokeType(1.6f));
    g.setColour(console::amber.withAlpha(0.3f));g.drawDashedLine({fx(focus),plot.getY(),fx(focus),plot.getBottom()},std::array<float,2>{3,4}.data(),2);
    console::text(g,"LOW-END / BAND ENERGY",{58,138,260,18},13,console::muted);
    console::text(g,"KICK",{566,138,48,18},13,console::amber);
    console::text(g,"BASS",{630,138,48,18},13,console::cream);
    console::text(g,"EQ CUT 0-12 dB",{694,138,126,18},12,console::orange);
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
    for(size_t i=0;i<bass.size();++i){bass[i]=0.7f*bass[i]+0.3f*processor.spectrumBass[i].load();kick[i]=0.7f*kick[i]+0.3f*processor.spectrumKick[i].load();cut[i]=processor.spectrumCut[i].load();}
    for(size_t i=0;i<3;++i)compensation[i]=processor.agcMeters[i].load();
    vu=0.65f*vu+0.35f*processor.glueReduction.load();focus=processor.focusHz.load();q=processor.unmaskQ.load();
    peak=static_cast<float>(bassdeamon::db(processor.outputPeak.load()));sidePeak=processor.kickPeak.load();inputDb=processor.inputRmsDb.load();outputDb=processor.outputRmsDb.load();
    if(peak>=0)clipFrames=90;else clipFrames=std::max(0,clipFrames-1);
    learn.setEnabled(processor.learnStatus.load()!=1&&processor.parameters.getRawParameterValue("global.on")->load()>.5f);
    aButton.setToggleState(processor.selectedAB()==0,juce::dontSendNotification);bButton.setToggleState(processor.selectedAB()==1,juce::dontSendNotification);
    frequencyKnob->slider.setEnabled(processor.parameters.getRawParameterValue("unmask.track")->load()<0.5f);
    face.repaint();
}
