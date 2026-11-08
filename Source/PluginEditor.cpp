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
    sumButton.setTooltip("Preview kick plus bass before/after in the scope. Display only: kick is never mixed into the plugin output.");
    sumButton.onClick=[this]{showSum=!showSum;sumButton.setToggleState(showSum,juce::dontSendNotification);face.repaint();};
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
    auto s=std::make_unique<Switch>();s->button.setButtonText(name);s->button.setBounds(bounds);s->button.setTooltip(tip);
    face.addAndMakeVisible(s->button);s->attachment=std::make_unique<State::ButtonAttachment>(processor.parameters,id,s->button);switches.push_back(std::move(s));
}
BassDeamonEditor::Knob& BassDeamonEditor::addKnob(const char* id,const char* name,const char* unit,bool pct,juce::Rectangle<int> r,const char* tip){
    auto k=std::make_unique<Knob>(processor.parameters,id,name,unit,pct);k->setBounds(r);k->slider.setTooltip(tip);face.addAndMakeVisible(*k);
    auto& ref=*k;knobs.push_back(std::move(k));return ref;
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
    }
    console::panel(g,{24,960,1152,144});
    console::text(g,"04  SOFT TUBE",{52,944,300,14},13,console::copper);
    console::text(g,"4x / PARALLEL LOW-BAND",{870,1051,275,20},13,console::muted);
    console::text(g,"AGC "+juce::String(processor.tubeGainDb.load(),1)+" dB",{1035,1007,125,24},14,console::amber);
    console::panel(g,{24,1118,1152,46},true);
    {juce::Graphics::ScopedSaveState footerScope(g);g.addTransform(juce::AffineTransform::translation(0,320));
    g.setColour(sidePeak>0.0003f?console::amber:console::muted.darker());g.fillEllipse(45,816,9,9);
    console::text(g,sidePeak>0.0003f?"KICK SC ACTIVE":"KICK SC IDLE",{63,808,144,26},13,console::cream);
    console::text(g,"IN "+juce::String(std::max(inputDb,-99.0f),1)+"  /  OUT "+juce::String(std::max(outputDb,-99.0f),1)+" dBFS RMS",{221,808,315,26},13,console::muted);
    console::text(g,"PEAK "+juce::String(std::max(peak,-99.0f),1)+" dBFS",{548,808,180,26},14,clipFrames>0?console::orange:console::cream);
    console::text(g,"TRIM",{762,808,60,26},13,console::muted);
    console::text(g,"v0.2.3",{1090,808,66,26},13,console::muted,juce::Justification::centredRight);
    }
    g.drawImageAt(grain,0,0);
}
void BassDeamonEditor::paintSpectrum(juce::Graphics& g){
    for(int view=0;view<2;++view){
        const juce::Rectangle<float> plot(58.f+view*393.f,164,355,115);
        const auto fx=[&](double hz){return plot.getX()+float(std::log(hz/25)/std::log(5000.0/25))*plot.getWidth();};
        const auto fy=[&](float level){return plot.getBottom()-juce::jlimit(0.f,1.f,(level+90)/90)*plot.getHeight();};
        g.setColour(console::cream.withAlpha(.08f));
        for(int level:{-90,-60,-30,0}){const float y=fy(float(level));g.drawHorizontalLine(juce::roundToInt(y),plot.getX(),plot.getRight());}
        for(double hz:{30.,100.,1000.,5000.}){const float x=fx(hz);g.drawVerticalLine(juce::roundToInt(x),plot.getY(),plot.getBottom());
            console::text(g,hz>=1000?juce::String(hz/1000,0)+"k":juce::String(hz,0),{x-16,285,34,16},11,console::muted,juce::Justification::centred);}
        const auto& bassValues=view==0?analyzer.pre:analyzer.post;
        const auto& masks=view==0?analyzer.maskPre:analyzer.maskPost;
        for(int trace=0;trace<2;++trace){const auto& values=trace==0?analyzer.kick:bassValues;
            juce::Path path;bool first=true;
            for(size_t i=1;i<values.size();++i){const double hz=double(i)*analyzer.sampleRate()/bassdeamon::RelationAnalyzer::fftSize;
                if(hz<25||hz>5000)continue;const float x=fx(hz),y=fy(values[i]);if(first){path.startNewSubPath(x,y);first=false;}else path.lineTo(x,y);}
            g.setColour(trace==0?console::amber:console::cream);g.strokePath(path,juce::PathStrokeType(trace==0?1.2f:1.7f));
        }
        for(size_t i=2;i<bassValues.size();++i){const double hz=double(i)*analyzer.sampleRate()/bassdeamon::RelationAnalyzer::fftSize;
            const double previous=double(i-1)*analyzer.sampleRate()/bassdeamon::RelationAnalyzer::fftSize;
            if(previous<25||hz>5000||masks[i]<=.08f)continue;
            g.setColour(juce::Colour(0xffef5350).withAlpha(.3f+.7f*masks[i]));
            g.drawLine(fx(previous),fy(bassValues[i-1]),fx(hz),fy(bassValues[i]),2.8f);
        }
        console::text(g,view==0?"PRE / INPUT":"POST / OUTPUT",{plot.getX(),138,190,18},13,console::muted);
    }
    console::text(g,"RED: POTENTIAL MASKING / SAME SCALE / 25 Hz - 5 kHz",{135,302,650,12},10,console::orange);
}
void BassDeamonEditor::paintWave(juce::Graphics& g){
    console::text(g,showSum?"SCOPE L / KICK + BASS PRE & POST":"SCOPE L / BASS PRE & POST + KICK",{47,337,650,20},13,console::muted);
    const juce::Rectangle<float> plot(48,374,1104,75);
    float maximum=.05f;
    for(size_t i=0;i<analyzer.wavePre.size();++i)maximum=std::max(maximum,std::max({std::abs(analyzer.sumPre[i]),std::abs(analyzer.sumPost[i]),std::abs(analyzer.waveKick[i])}));
    g.setColour(console::cream.withAlpha(.1f));g.drawHorizontalLine(juce::roundToInt(plot.getCentreY()),plot.getX(),plot.getRight());
    for(int trace=0;trace<(showSum?2:3);++trace){
        const auto& values=showSum?(trace==0?analyzer.sumPre:analyzer.sumPost):(trace==0?analyzer.wavePre:trace==1?analyzer.wavePost:analyzer.waveKick);
        juce::Path path;
        for(size_t i=0;i<values.size();++i){const float x=plot.getX()+float(i)*plot.getWidth()/float(values.size()-1),y=plot.getCentreY()-values[i]/maximum*plot.getHeight()*.48f;
            if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
        g.setColour(trace==0?console::cream.withAlpha(.65f):trace==1?juce::Colour(0xff56d6b4):console::amber);g.strokePath(path,juce::PathStrokeType(1.2f));
    }
    console::text(g,"PRE: GREY   POST: GREEN   KICK: AMBER / "+juce::String(1000.0*bassdeamon::RelationAnalyzer::fftSize/analyzer.sampleRate(),1)+" ms / visual sum only",{48,451,1080,14},10,console::muted);
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
    analyzer.consume(processor.analysisQueue);
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
