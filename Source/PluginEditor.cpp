#include "PluginEditor.h"
#include "BassDeamonArt.h"

bool console::Skin::setRaw(bool useRaw) {
    if (!knob.isValid())
        knob=juce::ImageCache::getFromMemory(BassDeamonArt::RawKnob_png,BassDeamonArt::RawKnob_pngSize);
    if (raw==useRaw) return false;
    raw=useRaw;
    if (raw && !steel.isValid()) {
        steel=juce::ImageCache::getFromMemory(BassDeamonArt::RawSteel_png,BassDeamonArt::RawSteel_pngSize);
    }
    return true;
}

namespace {
const juce::Colour eqTrace(0xffffde78);
const juce::Colour unmaskTrace(0xff69f3d2);

enum class Stage { kickEq, glue, phase, softTube, duck, unmask };
constexpr int advancedButtonWidth=112, advancedButtonHeight=27;
constexpr int panelLeft=24, panelStride=389, panelWidth=374;
constexpr int moduleFooterY=733;
constexpr int timingAttackY=635, timingReleaseY=683, timingFieldHeight=26;
constexpr std::array stageNames{"KICK EQ","GLUE","PHASE","SATURATION","DUCK","UNMASK"};

void stageTitle(juce::Graphics& g, Stage stage, juce::Rectangle<float> area, float fontSize, bool enabled=true) {
    const auto index=static_cast<size_t>(stage);
    const float badgeHeight=fontSize+3;
    const float badgeWidth=fontSize+8;
    const juce::Rectangle<float> badge(area.getX(),area.getCentreY()-badgeHeight*.5f,badgeWidth,badgeHeight);
    const auto accent=enabled?(stage==Stage::kickEq?eqTrace:stage==Stage::unmask?unmaskTrace:console::amber):console::muted;
    g.setColour(console::ink);
    g.fillRoundedRectangle(badge,3);
    g.setColour(accent.withAlpha(.55f));
    g.drawRoundedRectangle(badge,3,1);
    console::text(g,"0"+juce::String(int(index)+1),badge,fontSize*.65f,accent,juce::Justification::centred);
    console::text(g,stageNames[index],area.withTrimmedLeft(badgeWidth+8),fontSize,enabled?console::cream:console::muted);
}

void paintReductionMeter(juce::Graphics& g,float x,float y,float reduction,int fullScale,bool raw) {
    juce::Graphics::ScopedSaveState position(g);
    const float width=raw?224.f:187.f;
    const juce::Rectangle<float> meter(x-width*.5f,y,width,68.f);
    if (raw) {
        const auto bezel=meter.expanded(4);
        g.setColour(juce::Colour(0xff110b08));
        g.fillRoundedRectangle(bezel,6);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff999d9c),bezel.getX(),bezel.getY(),
            juce::Colour(0xff343a3d),bezel.getRight(),bezel.getBottom(),false));
        g.drawRoundedRectangle(bezel.reduced(1),5,2);

    }
    g.setColour(console::ink);
    g.fillRoundedRectangle(meter,4);
    const auto dial=meter.reduced(2);
    const auto dialTop=juce::Colour(raw?0xffffd17b:0xffedcb88);
    const auto dialBottom=juce::Colour(raw?0xffdb963e:0xffb8874d);
    g.setGradientFill(juce::ColourGradient(dialTop,0,dial.getY(),dialBottom,0,dial.getBottom(),false));
    g.fillRoundedRectangle(dial,3);
    { juce::Graphics::ScopedSaveState meterClip(g);
    g.reduceClipRegion(dial.toNearestInt());
    const auto mark=[fullScale,x,y,width](float reduction) {
        const float normal=1.f-2.f*std::sqrt(std::clamp(reduction/float(fullScale),0.f,1.f));
        return juce::Point<float>(x+normal*width*.365f,y+30+normal*normal*8);
    };
    juce::Path scale;
    scale.startNewSubPath(mark(float(fullScale)));
    for (int i=fullScale-1;i>=0;--i) scale.lineTo(mark(float(i)));
    g.setColour(console::ink.withAlpha(.62f));
    g.strokePath(scale,juce::PathStrokeType(.8f));
    for (int tickDb : {fullScale,fullScale == 24 ? 12 : 10,6,3,0}) {
        const auto tick=mark(float(tickDb));
        g.drawLine(tick.x,tick.y,tick.x,tick.y+4,1);
        console::text(g,juce::String(-tickDb),{tick.x-12,tick.y-11,24,10},9,console::ink,
                      juce::Justification::centred);
    }
    if (raw) console::text(g,"GAIN REDUCTION",{meter.getX(),y+4,width,14},12,console::ink,juce::Justification::centred);
    const juce::Point<float> pivot(meter.getCentreX(),dial.getBottom()-8.f);
    const auto tip=mark(reduction);
    const auto direction=(tip-pivot)/tip.getDistanceFrom(pivot);
    const juce::Point<float> shoulder(-direction.y,direction.x);
    juce::Path needle;
    needle.addTriangle(tip.x,tip.y,
                       pivot.x+shoulder.x*1.5f,pivot.y+shoulder.y*1.5f,
                       pivot.x-shoulder.x*1.5f,pivot.y-shoulder.y*1.5f);
    g.setColour(console::ink.withAlpha(.24f));
    g.fillPath(needle,juce::AffineTransform::translation(1.f,1.f));
    g.setColour(juce::Colour(0xff81382a));
    g.fillPath(needle);
    g.setColour(console::ink.withAlpha(.9f));
    g.fillEllipse(pivot.x-2.5f,pivot.y-2.5f,5.f,5.f);
    if (!raw) console::text(g,"GAIN REDUCTION",{meter.getX()+8,y+55,104,18},12,console::ink);
    const float readoutY=y+(raw?46.f:55.f);
    const float displayedDb=reduction>=.05f?-reduction:0.f;
    console::text(g,juce::String(displayedDb,1)+" dB",{meter.getRight()-70,readoutY,61,18},12,console::ink,
                  juce::Justification::centredRight);
    }
    if(raw) {
        for(float sx:{meter.getX()+1,meter.getRight()-1})
            for(float sy:{meter.getY()+1,meter.getBottom()-1}) {
                g.setColour(juce::Colour(0xff161b1c));g.fillEllipse(sx-4,sy-4,8,8);
                g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd6d4c8),sx-2,sy-3,
                    juce::Colour(0xff4c5254),sx+2,sy+3,false));g.fillEllipse(sx-2.5f,sy-2.5f,5,5);
                g.setColour(juce::Colour(0xff202423));g.drawLine(sx-1.3f,sy-1,sx+1.3f,sy+1,1);
            }
    }
}

void paintSignalPath(juce::Graphics& g, const BassDeamonProcessor& processor) {
    constexpr std::array stageSwitches{"kickEq.on","glue.on","phase.on","tube.on","duck.on","unmask.on"};
    const bool powered=!processor.displayBypassed();
    g.setColour(juce::Colour(0xff1a1916));
    if(!processor.usesRawTheme()) g.fillRoundedRectangle(24,102,1152,24,4);
    console::text(g,"BASS SIGNAL PATH",{44,102,159,24},14,console::muted);
    constexpr std::array<float,6> positions{206,365,508,650,824,1004};
    for (size_t index=0;index<positions.size();++index) {
        const float x=positions[index];
        const bool enabled=powered && processor.parameters.getRawParameterValue(stageSwitches[index])->load()>.5f;
        stageTitle(g,static_cast<Stage>(index),{x,102,173,24},16,enabled);
        if (index+1==positions.size()) break;
        const float end=positions[index+1]-16;
        g.setColour(console::copper.brighter());
        g.drawLine(end-22,114,end,114,1.2f);
        g.drawLine(end-4,110,end,114,1.2f);
        g.drawLine(end-4,118,end,114,1.2f);
    }
}
}

BassDeamonEditor::Knob::Knob(State& state,const char* id,const char* name,const char* unit,bool percent) {
    setComponentID(id);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,98,25);
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
    label.setColour(juce::Label::textColourId,console::cream);
    addAndMakeVisible(slider);addAndMakeVisible(label);
}
void BassDeamonEditor::Knob::resized() {slider.setBounds(label.isVisible()?getLocalBounds().withTrimmedBottom(25):getLocalBounds().withHeight(24));label.setBounds(0,getHeight()-24,getWidth(),22);}

BassDeamonEditor::BassDeamonEditor(BassDeamonProcessor& p):AudioProcessorEditor(p),processor(p) {
    setLookAndFeel(&skin); addAndMakeVisible(face);face.setOpaque(true);createTexture();
    presets.addItemList({"INIT / CLEAN LOW END","AFRO / ROUND & STEADY","PUNCH / MAKE ROOM","SUB / GENTLE CONTROL"},1);
    presets.setSelectedId(1,juce::dontSendNotification);presets.setBounds(628,33,310,38);face.addAndMakeVisible(presets);
    presets.setTooltip("Starting points, not automatic mix decisions. PHASE remains off until you choose to enable it.");
    presets.onChange=[this]{processor.loadStartingPoint(presets.getSelectedId()-1);};
    aButton.setBounds(958,33,44,38);bButton.setBounds(1010,33,44,38);
    for(auto* b:{&aButton,&bButton}) {face.addAndMakeVisible(*b);b->setTooltip("Compare two complete settings at their actual output levels. Auto Gain can help level matching.");}
    aButton.onClick=[this]{processor.selectAB(0);};bButton.onClick=[this]{processor.selectAB(1);};
    undoButton.setBounds(402,75,88,24);redoButton.setBounds(498,75,88,24);
    for(auto* b:{&undoButton,&redoButton})face.addAndMakeVisible(*b);
    undoButton.setTooltip("Undo the last parameter edit or starting point. History lasts until session recall or A/B switch.");
    redoButton.setTooltip("Redo the last undone edit. A new edit replaces the redo branch.");
    presets.setTextWhenNothingSelected("CUSTOM");
    undoButton.onClick=[this]{processor.undoEdit();presets.setSelectedId(0,juce::dontSendNotification);timerCallback();};
    redoButton.onClick=[this]{processor.redoEdit();presets.setSelectedId(0,juce::dontSendNotification);timerCallback();};
    cpuLabel.setBounds(628,75,430,24);cpuLabel.setFont(console::font(13));face.addAndMakeVisible(cpuLabel);
    themeSelector.addItemList({"CLASSIC","RAW"},1);
    themeSelector.setSelectedId(processor.usesRawTheme()?2:1,juce::dontSendNotification);
    themeSelector.setBounds(1060,77,110,22);
    themeSelector.setTooltip("Interface theme. Saved with this instance. Audio settings stay the same.");
    themeSelector.onChange=[this] {
        processor.setRawTheme(themeSelector.getSelectedId()==2);
        syncTheme();
    };
    face.addAndMakeVisible(themeSelector);
    syncTheme();
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
    addKnob("glue.amount","AMOUNT","",true,{57,545,166,165},"Bass-led adaptive compression. Kick and host tempo only provide timing context.");
    addSwitch("glue.lock","LOCK ADAPTATION",{249,670,122,30},"Freeze threshold, ratio, attack and release. The compressor envelope keeps moving.");
    addKnob("phase.delay","DELAY"," ms",false,{438,455,154,193},"Intentional causal delay on the bass. No advance and no host latency compensation of this musical offset.");
    rotationKnob=&addKnob("phase.frequency","ROTATION","",false,{609,455,154,193},
        "All-pass rotation at REF. The available angle range depends on the reference and the filter limits.");
    const auto phaseRate=[this]{return processor.getSampleRate()>0?processor.getSampleRate():48000.;};
    rotationKnob->slider.textFromValueFunction=[this,phaseRate](double corner) {
        return juce::String(bassdeamon::allPassDegrees(processor.phaseReferenceHz.load(),corner,phaseRate()),1)+juce::String::charToString(0x00b0);
    };
    rotationKnob->slider.valueFromTextFunction=[this,phaseRate](const juce::String& text) {
        return bassdeamon::cornerForAllPassDegrees(-std::abs(text.getDoubleValue()),processor.phaseReferenceHz.load(),phaseRate());
    };
    rotationKnob->slider.updateText();
    addSwitch("phase.invert","POLARITY -",{438,654,150,28},"Invert both bass channels together.");
    addSwitch("phase.rotate","LEGACY AP",{610,654,150,28},"Enable the manual all-pass stage.");
    learn.setBounds(438,688,150,28);face.addAndMakeVisible(learn);learn.onClick=[this]{processor.requestLearn();};
    learn.setTooltip("Analyse four seconds. A reliable result sets delay and polarity and enables PHASE. Uncertain results keep the current settings.");
    depthKnob=&addKnob("unmask.amount","DEPTH","",true,{},"Maximum spectral unmasking amount. The kick is never summed into the bass.");
    attenuationKnob=&addKnob("unmask.attenuation","ATTENUATION"," dB",false,{830,448,170,184},"Maximum dynamic attenuation at the focus, 0-24 dB. Actual reduction follows shared signal activity. Before Auto Gain: turn Auto Gain off to measure the requested cut.");
    attenuationKnob->slider.textFromValueFunction=[](double value){return (value>0?juce::String("-"):juce::String{})+juce::String(value,1)+" dB";};
    attenuationKnob->slider.valueFromTextFunction=[](const juce::String& text){return std::abs(text.getDoubleValue());};attenuationKnob->slider.updateText();
    frequencyKnob=&addKnob("unmask.frequency","MANUAL FOCUS"," Hz",false,{},"Manual focus frequency, available when AUTO FOCUS is off.");
    unmaskWidthKnob=&addKnob("unmask.width","WIDTH","",true,{},"Width of the focus region and dynamic EQ bands.");
    unmaskAttackKnob=&addKnob("unmask.attack","ATTACK"," ms",false,{},"Dynamic cut attack, 0 to 50 ms.");
    unmaskReleaseKnob=&addKnob("unmask.release","RELEASE"," ms",false,{},"Dynamic cut recovery, 30 to 500 ms.");
    addSwitch("unmask.controlDb","dB CONTROL",{},"On: calibrated attenuation knob in dB. Off: previous DEPTH percentage algorithm, retained for existing sessions.");
    addSwitch("unmask.track","AUTO FOCUS",{},"Follow the strongest low-end overlap at the UNMASK input, after Duck. A learned kick frequency gives nearby bands priority.");
    addSwitch("unmask.hold","HOLD FOCUS",{},"Hold the current auto focus frequency. The dynamic gain reduction remains active.");
    addSwitch("unmask.delta","DELTA",{},"Audition the bass removed by UNMASK, before its Auto Gain and output trim.");
    output.setSliderStyle(juce::Slider::LinearHorizontal);output.setTextBoxStyle(juce::Slider::TextBoxRight,false,78,24);
    output.setTextValueSuffix(" dB");output.setNumDecimalPlacesToDisplay(1);output.setScrollWheelEnabled(false);
    output.setBounds(915,808,169,28);face.addAndMakeVisible(output);
    outputAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"output.trim",output);
    output.textFromValueFunction=[](double value){return juce::String(value,1);};
    output.valueFromTextFunction=[](const juce::String& value){return value.getDoubleValue();}; output.updateText();
    output.setTooltip("Final trim. No limiter is applied; red peak indication means you must reduce the level.");
    for(size_t i=0;i<bass.size();++i){bass[i]=processor.spectrumBass[i].load();kick[i]=processor.spectrumKick[i].load();cut[i]=processor.spectrumCut[i].load();}
    // Keep existing modules together; add scope above and tube below.
    for(auto* child:face.getChildren())if(child->getY()>=340)child->setTopLeftPosition(child->getX(),child->getY()+(child->getY()>=798?320:160));
    addSwitch("tube.on","OFF",{221,942,55,17},"Enable parallel Soft Tube. Off by default; the settled off state leaves the old chain unchanged.");
    addSwitch("tube.agc","AUTO GAIN",{52,1016,120,28},"Slow stereo-linked compensation, max +/-3 dB, shared positive-gain budget. Disabled during tube audition.");
    addSwitch("tube.agcLock","GAIN LOCK",{52,1054,120,28},"Freeze the learned Soft Tube compensation.");
    addKnob("tube.drive","DRIVE"," dB",false,{203,962,135,140},"Soft asymmetric waveshaping with selectable oversampling.");
    addKnob("tube.amount","AMOUNT","",true,{365,962,135,140},"How much nonlinear residual is added in parallel. The added branch may retain fundamental energy.");
    addKnob("tube.source","SOURCE LP"," Hz",false,{527,962,135,140},"Select the low-frequency source, 60-500 Hz. This does not track individual notes.");
    addKnob("tube.colour","COLOUR","",true,{689,962,135,140},"Smoothly varies asymmetry and the added-branch tone filter.");
    addSwitch("tube.audition","DELTA",{872,1005,145,32},"Send the added residual through UNMASK/output, with tube AGC disabled. It may retain fundamental energy.");
    scopeChannel.addItemList({"STEREO","MONO"},1);
    scopeChannel.setSelectedId(1,juce::dontSendNotification);
    scopeChannel.setBounds(779,336,114,26);
    scopeChannel.setTooltip("Display the separate left/right sums or the mono average. This is a display-only control.");
    scopeChannel.onChange=[this] {
        if (scopeChannel.getSelectedId()==1) {
            phaseScopeButton.setToggleState(false,juce::dontSendNotification);
            refreshDisplayPage();
        }
        face.repaint();
    };
    face.addAndMakeVisible(scopeChannel);
    scopeCompareButton.setBounds(1002,336,145,26);face.addAndMakeVisible(scopeCompareButton);
    scopeCompareButton.setTooltip("Overlay kick + bass before processing. The green post-processing sum stays visible.");
    scopeCompareButton.onClick=[this]{compareScopePre=!compareScopePre;scopeCompareButton.setToggleState(compareScopePre,juce::dontSendNotification);face.repaint();};
    addSwitch("view.hold","HOLD",{904,336,88,26},"Freeze the last complete quarter-note capture; audio is unaffected.");
    spectrumPre.setBounds(72,136,66,26);spectrumPost.setBounds(145,136,72,26);
    face.addAndMakeVisible(spectrumPre);
    face.addAndMakeVisible(spectrumPost);
    spectrumPre.setTooltip("Show the input spectrum of bass and sidechain kick.");
    spectrumPost.setTooltip("Show the processed bass, sidechain kick and remaining overlaps.");
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
    spectrumHold.setBounds(374,136,75,26);
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
    analyzerRefresh.setTextBoxStyle(juce::Slider::TextBoxLeft,false,90,26);
    analyzerRefresh.setRange(1,60,.1);
    analyzerRefresh.setTextValueSuffix(" fps");
    analyzerRefresh.valueFromTextFunction=[](const juce::String& text) {
        return text.replaceCharacter(',', '.').getDoubleValue();
    };
    analyzerRefresh.setScrollWheelEnabled(false);
    syncedRefreshHz = processor.analyzerRefreshHz();
    analyzerRefresh.setValue(syncedRefreshHz,juce::dontSendNotification);
    analyzerRefresh.setBounds(275,136,90,26);
    analyzerRefresh.setTooltip("Drag or type a refresh rate from 1 to 60 fps.");
    analyzerRefresh.onValueChange=[this] {
        processor.setAnalyzerRefreshHz(analyzerRefresh.getValue());
        syncedRefreshHz = processor.analyzerRefreshHz();
    };
    face.addAndMakeVisible(analyzerRefresh);
    for(auto* slider:{&spectrumMin,&spectrumMax}){
        slider->setSliderStyle(juce::Slider::LinearBar);
        slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,118,26);
        slider->setTextValueSuffix(" Hz");slider->setNumDecimalPlacesToDisplay(1);slider->setScrollWheelEnabled(false);
        slider->setTooltip("Drag or type a frequency in Hz. Set FROM below TO; a minimum 20 Hz window is displayed. Display only.");
        face.addAndMakeVisible(*slider);
    }
    spectrumMin.setBounds(850,136,118,26);spectrumMax.setBounds(1024,136,118,26);
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
    focusMin.setTooltip("Lower frequency limit for Auto Focus.");
    focusMax.setTooltip("Upper frequency limit for Auto Focus.");
    focusMinAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"unmask.focusMinHz",focusMin);
    focusMaxAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"unmask.focusMaxHz",focusMax);
    focusMin.setNumDecimalPlacesToDisplay(1);
    focusMax.setNumDecimalPlacesToDisplay(1);
    addChoice("unmask.stereoMode",{"STEREO","MID","SIDE"},{});
    choices.back()->box.setTooltip("UNMASK target. MID processes the common signal; SIDE processes the difference. SIDE has no effect on mono bass.");
    addChoice("glue.timing",{"AUTO","MS","SYNC"},{249,623,122,26});
    addChoice("unmask.timing",{"MS","SYNC"},{});
    const juce::StringArray notes{"1/64","1/32","1/16","1/8","1/4","1/2","1/1","1/8 T","1/8 D"};
    addChoice("glue.releaseNote",notes,{249,759,122,32});
    addChoice("unmask.releaseNote",notes,{});
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
        if (id=="tube.on") {
            auto& button=sw->button;
            button.onStateChange=[&button] {
                button.setButtonText(button.getToggleState()?"ON":"OFF");
            };
            button.onStateChange();
        }
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
    displaySampleRate=processor.getSampleRate()>0?processor.getSampleRate():48000;
    analyzer.prepare(displaySampleRate);
    lastAnalysisTime=juce::Time::getMillisecondCounterHiRes();
    bassdeamon::AnalysisFrame oldFrame;const auto pending=processor.analysisQueue.pending();
    for(uint32_t i=0;i<pending&&processor.analysisQueue.pop(oldFrame);++i){}
    processor.analysisQueue.enabled.store(true,std::memory_order_release);
    glueGr=processor.glueReduction.load();
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
    for (auto* child : face.getChildren())
        if (child->getY() >= 1134) child->setTopLeftPosition(child->getX(),child->getY()-40);
    glueAttackSlider.setTopLeftPosition(249,876);
    glueReleaseSlider.setTopLeftPosition(249,955);
    for (auto& control : choices)
        if (control->box.getComponentID()=="glue.releaseNote") control->box.setTopLeftPosition(249,955);
        else if (control->box.getComponentID()=="glue.timing") control->box.setTopLeftPosition(249,818);
    phaseReference.setSliderStyle(juce::Slider::LinearBar);
    phaseReference.setRange(30,130,.1);
    phaseReference.setTextValueSuffix(" Hz");
    phaseReference.setTextBoxStyle(juce::Slider::TextBoxLeft,false,130,21);
    phaseReference.setNumDecimalPlacesToDisplay(1);
    phaseReference.setScrollWheelEnabled(false);
    phaseReference.setBounds(623,793,130,21);
    phaseReference.setValue(processor.phaseReferenceHz.load(),juce::dontSendNotification);
    phaseReference.setTooltip("Reference frequency for the degree dial and the mono sum comparison.");
    phaseReference.onValueChange=[this] {
        processor.phaseReferenceAutomatic.store(false);
        processor.phaseReferenceHz.store(float(phaseReference.getValue()));
        processor.phaseProcessingReferenceHz.store(float(phaseReference.getValue()));
        rotationKnob->slider.updateText();
    };
    face.addAndMakeVisible(phaseReference);
    phaseAutoReference.setToggleState(processor.phaseReferenceAutomatic.load(),juce::dontSendNotification);
    phaseAutoReference.onClick=[this]{
        processor.phaseReferenceAutomatic.store(phaseAutoReference.getToggleState());
        if(phaseAutoReference.getToggleState())processor.acceptKickReference(processor.detectedKickHz.load());
        phaseAngle.updateText();
    };
    phaseAutoReference.setTooltip("Use the stable kick frequency after analysis for Scope. Does not retune the Phase processor.");
    face.addAndMakeVisible(phaseAutoReference);
    phaseScopeButton.setBounds(620,468,140,26);
    phaseScopeButton.setClickingTogglesState(true);
    phaseScopeButton.setTooltip("Show mono kick + bass at the Phase input and output. The dB comparison matches bass energy in the selected band.");
    phaseScopeButton.onClick=[this] {
        processor.phaseScopeEnabled.store(phaseScopeButton.getToggleState());
        face.repaint();
    };
    face.addAndMakeVisible(phaseScopeButton);
    addSwitch("duck.on","OFF",{160,1289,62,27},"Duck the full bass signal from the existing kick sidechain.");
    addKnob("duck.amount","AMOUNT"," dB",false,{},"Maximum Duck attenuation.");
    addKnob("duck.attack","ATTACK"," ms",false,{},"Duck attack, 0 to 20 ms.");
    addKnob("duck.release","RELEASE"," ms",false,{},"Duck release in milliseconds.");
    addSwitch("duck.multiband","LOW-BAND",{},"Switch between full-band and low-band ducking.");
    addKnob("duck.lowHz","FREQUENCY"," Hz",false,{},"Low-band cutoff. Zero recalls the legacy multiband sound.");
    addChoice("duck.timing",{"MS","SYNC"},{});
    addChoice("duck.releaseNote",notes,{});
    addChoice("tube.oversampling",{"2X","4X"},{});
    output.setTopLeftPosition(output.getX(),output.getY()+64);
    addSwitch("subMono.on","SUB MONO",{690,1348,105,27},"Reduce stereo Side below the selected frequency, before Output Trim.");
    subMonoHz.setSliderStyle(juce::Slider::LinearBar);
    subMonoHz.setTextBoxStyle(juce::Slider::TextBoxRight,false,84,25);
    subMonoHz.setTextValueSuffix(" Hz");
    subMonoHz.setNumDecimalPlacesToDisplay(0);
    subMonoHz.setScrollWheelEnabled(false);
    subMonoHz.setBounds(800,1348,89,27);
    face.addAndMakeVisible(subMonoHz);
    subMonoAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"subMono.hz",subMonoHz);

    juce::Image icon(juce::Image::ARGB,54,54,true);
    juce::Graphics iconGraphics(icon);
    console::demon(iconGraphics,{0,0,54,54});
    juce::DrawableImage demonIcon;
    demonIcon.setImage(icon);
    autoMixButton.setButtonText("AUTO MIX");
    autoMixButton.setImages(&demonIcon);
    autoMixButton.setBounds(1004,19,66,58);
    aButton.setTopLeftPosition(900,33);
    bButton.setTopLeftPosition(950,33);
    presets.setBounds(628,33,260,38);
    face.addAndMakeVisible(autoMixButton);
    autoMixButton.onClick=[this] {
        if(processor.autoMixState()==BassDeamonProcessor::AutoMixState::idle){balanceTrim.setValue(processor.parameters.getRawParameterValue("balance.trim")->load(),juce::dontSendNotification);balanceInitialized=false;}
        autoPanel.setVisible(true);
        autoPanel.toFront(false);
        refreshAutoMix();
    };
    face.addChildComponent(autoPanel);
    autoPanel.setBounds(228,125,744,620);
    autoTitle.setText("AUTO MIX",juce::dontSendNotification);
    autoTitle.setFont(console::font(25,true));
    autoTitle.setColour(juce::Label::textColourId,console::cream);
    autoStatus.setColour(juce::Label::textColourId,console::amber);
    autoReport.setColour(juce::TextEditor::backgroundColourId,console::ink.darker());
    autoReport.setColour(juce::TextEditor::textColourId,console::cream);
    autoReport.setColour(juce::TextEditor::outlineColourId,console::copper);
    autoTitle.setBounds(28,19,260,42);
    autoBars.addItemList({"4 BARS","8 BARS","16 BARS"},1);
    autoBars.setSelectedId(2,juce::dontSendNotification);
    autoBars.setBounds(29,70,120,34);
    autoScan.setBounds(160,70,120,34);
    autoStatus.setBounds(293,70,420,34);
    autoReport.setMultiLine(true);
    autoReport.setReadOnly(true);
    autoReport.setScrollbarsShown(true);
    autoReport.setCaretVisible(false);
    autoReport.setFont(console::font(17));
    autoReport.setBounds(28,115,688,300);
    autoDry.setBounds(28,466,90,36);
    autoWet.setBounds(125,466,133,36);
    autoMatch.setBounds(265,466,165,36);
    autoApply.setBounds(503,466,102,36);
    autoCancel.setBounds(612,466,104,36);
    for (auto* component : std::array<juce::Component*,10>{
            &autoTitle,&autoBars,&autoScan,&autoStatus,&autoReport,&autoDry,&autoWet,&autoMatch,&autoApply,&autoCancel})
        autoPanel.addAndMakeVisible(*component);
    balanceCaption.setText("BALANCE / optional bass trim",juce::dontSendNotification);
    balanceCaption.setBounds(28,420,420,22);autoPanel.addAndMakeVisible(balanceCaption);
    balanceTrim.setRange(-12,12,.1);balanceTrim.setSliderStyle(juce::Slider::LinearBar);
    balanceTrim.setTextBoxStyle(juce::Slider::TextBoxLeft,false,100,26);balanceTrim.setTextValueSuffix(" dB");
    balanceTrim.setBounds(28,446,100,28);autoPanel.addAndMakeVisible(balanceTrim);
    balanceListen.setBounds(140,446,123,28);balanceListen.setClickingTogglesState(true);
    balanceApply.setBounds(270,446,123,28);balanceSave.setBounds(402,446,140,28);balanceReset.setBounds(550,446,165,28);
    for(auto* b:{&balanceListen,&balanceApply,&balanceSave,&balanceReset})autoPanel.addAndMakeVisible(*b);
    const std::array<const char*,3> ids{"balance.lowHz","balance.splitHz","balance.highHz"};
    const std::array<juce::Slider*,3> sliders{&balanceLow,&balanceSplit,&balanceHigh};
    for(size_t i=0;i<3;++i){auto* slider=sliders[i];slider->setSliderStyle(juce::Slider::LinearBar);slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,100,24);slider->setTextValueSuffix(" Hz");slider->setBounds(28+int(i)*115,486,105,26);autoPanel.addAndMakeVisible(*slider);balanceBandAttachments[i]=std::make_unique<State::SliderAttachment>(processor.parameters,ids[i],*slider);slider->setTooltip(i==0?"Lower measurement edge":i==1?"Sub/body crossover":"Upper measurement edge");}
    balanceBandLabel.setText("BAND EDGES: LOW / SPLIT / HIGH",juce::dontSendNotification);
    balanceBandLabel.setBounds(382,486,330,26);autoPanel.addAndMakeVisible(balanceBandLabel);
    balanceTrim.onValueChange=[this]{if(processor.balancePreviewActive.load())processor.balancePreviewDb.store(float(balanceTrim.getValue()));};
    balanceListen.onClick=[this]{
        processor.auditionAutoMix(0,false);autoSide=0;
        processor.balancePreviewDb.store(float(balanceTrim.getValue()));processor.balancePreviewActive.store(balanceListen.getToggleState());
    };
    balanceApply.onClick=[this]{processor.applyBalanceTrim(float(balanceTrim.getValue()));balanceListen.setToggleState(false,juce::dontSendNotification);};
    balanceReset.onClick=[this]{balanceTrim.setValue(0);processor.applyBalanceTrim(0);balanceListen.setToggleState(false,juce::dontSendNotification);};
    balanceSave.setTooltip("Analyze your preferred balance, then save the measured target. Temporary auditions are excluded.");
    balanceSave.onClick=[this]{processor.saveBalanceReference();balanceInitialized=false;autoResultShown=false;refreshAutoMix();};
    autoDry.setTopLeftPosition(28,560);autoWet.setTopLeftPosition(125,560);autoMatch.setTopLeftPosition(265,560);autoApply.setTopLeftPosition(503,560);autoCancel.setTopLeftPosition(612,560);
    autoMatch.setClickingTogglesState(true);
    autoMatch.setToggleState(true,juce::dontSendNotification);
    autoScan.onClick=[this] {
        const int bars=autoBars.getSelectedId()==1?4:autoBars.getSelectedId()==3?16:8;
        autoResultShown=false;balanceInitialized=false;
        autoSide=0;
        autoReport.clear();
        processor.startAutoMix(bars);
        refreshAutoMix();
    };
    autoDry.onClick=[this]{processor.balancePreviewActive.store(false);balanceListen.setToggleState(false,juce::dontSendNotification);autoSide=1;refreshAutoMix();};
    autoWet.onClick=[this]{processor.balancePreviewActive.store(false);balanceListen.setToggleState(false,juce::dontSendNotification);autoSide=2;refreshAutoMix();};
    autoMatch.onClick=[this]{refreshAutoMix();};
    autoApply.onClick=[this]{processor.applyAutoMix();autoPanel.setVisible(false);};
    autoCancel.onClick=[this]{processor.cancelAutoMix();autoPanel.setVisible(false);};
    layoutMain();
    layoutUnmask();
    refreshDisplayPage();
    setResizable(true,true);
    setResizeLimits(840,609,1560,1131);
    if(auto* c=getConstrainer()) c->setFixedAspectRatio(1200.0/870.0);
    setSize(1080,783);
    refreshVisuals(juce::Time::getMillisecondCounterHiRes());
    startTimerHz(60);
}
BassDeamonEditor::~BassDeamonEditor() {
    stopTimer();
    processor.analysisQueue.enabled.store(false,std::memory_order_release);
    stopAnalysis.store(true,std::memory_order_release);
    if (analysisThread.joinable()) analysisThread.join();
    processor.phaseScopeEnabled.store(false);
    processor.cancelAutoMix();
    setLookAndFeel(nullptr);
}
void BassDeamonEditor::resized(){face.setBounds(0,0,1200,870);face.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth())/1200.0f));}
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
juce::Component* BassDeamonEditor::control(const char* id) {
    for (auto* child : face.getChildren())
        if (child->getComponentID() == id) return child;
    jassertfalse;
    return nullptr;
}

void BassDeamonEditor::layoutMain() {
    const auto place=[&](const char* id, juce::Rectangle<int> bounds) {
        auto* item=control(id);
        item->setBounds(bounds);
        return item;
    };
    presets.setBounds(500,19,218,34);
    undoButton.setBounds(730,19,65,34);redoButton.setBounds(800,19,65,34);
    aButton.setBounds(879,19,40,34);bButton.setBounds(924,19,40,34);
    autoMixButton.setBounds(980,12,107,72);
    place("global.on",{1091,22,83,33});
    themeSelector.setBounds(76,69,103,24);
    cpuLabel.setBounds(715,66,255,28);
    for (auto* button : {&analyzerTab,&scopeTab,&phaseAdvanced,&duckAdvanced})
        face.addAndMakeVisible(*button);
    analyzerTab.setBounds(45,136,107,26);scopeTab.setBounds(158,136,93,26);
    analyzerTab.onClick=[this]{showScope=false;showPhaseAdvanced=false;refreshDisplayPage();face.repaint();};
    scopeTab.onClick=[this]{showScope=true;showPhaseAdvanced=false;refreshDisplayPage();face.repaint();};
    spectrumPre.setBounds(282,136,53,26);spectrumPost.setBounds(341,136,59,26);
    analyzerRefresh.setBounds(460,136,80,26);spectrumHold.setBounds(548,136,66,26);
    scopeChannel.setBounds(768,182,114,26);
    place("view.hold",{895,182,89,26});scopeCompareButton.setBounds(996,182,145,26);

    place("kickEq.on",{172,461,54,25});
    kickEqLearn.setBounds(230,461,73,25);kickEqFrequency.setBounds(310,461,85,25);
    kickEqCut.setBounds(402,461,84,25);dryCompare.setBounds(498,461,57,25);
    matchCompare.setBounds(562,461,96,25);
    place("phase.on",{812,461,53,25});
    phaseAngle.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    phaseAngle.setTextBoxStyle(juce::Slider::TextBoxRight,false,64,24);
    phaseAngle.setNumDecimalPlacesToDisplay(1);
    phaseAngle.setDoubleClickReturnValue(true,0);
    phaseAngle.setScrollWheelEnabled(false);
    phaseAngle.setBounds(865,455,114,40);
    phaseAngle.setTooltip("Phase rotation at the reference frequency. Double-click to reset to zero. Phase must be enabled.");
    face.addAndMakeVisible(phaseAngle);
    phaseAngleAttachment=std::make_unique<State::SliderAttachment>(processor.parameters,"phase.angle",phaseAngle);
    const auto phaseRate=[this]{return processor.getSampleRate()>0?processor.getSampleRate():48000.;};
    phaseAngle.textFromValueFunction=[this,phaseRate](double value){
        return juce::String(bassdeamon::phaseDegreesAtReference(value,processor.phaseProcessingReferenceHz.load(),processor.phaseReferenceHz.load(),phaseRate()),1)+juce::String::charToString(0x00b0);
    };
    phaseAngle.valueFromTextFunction=[this,phaseRate](const juce::String& value){return bassdeamon::phaseDegreesAtReference(value.getDoubleValue(),processor.phaseReferenceHz.load(),processor.phaseProcessingReferenceHz.load(),phaseRate());};
    phaseAngle.updateText();
    phaseScopeButton.setBounds(985,461,72,25);
    phaseAdvanced.setComponentID("view-mode");
    phaseAdvanced.setBounds(1063,461,advancedButtonWidth,advancedButtonHeight);
    phaseAdvanced.onClick=[this]{showPhaseAdvanced=!showPhaseAdvanced;refreshDisplayPage();face.repaint();};
    const auto compare=phaseScopeButton.onClick;
    phaseScopeButton.onClick=[this,compare]{
        if (phaseScopeButton.getToggleState()) {
            showScope=true;showPhaseAdvanced=false;
            scopeChannel.setSelectedId(2,juce::dontSendNotification);
        }
        compare();
        refreshDisplayPage();
    };
    phaseControls={place("phase.delay",{193,190,138,145}),
        place("phase.frequency",{365,190,138,145}),
        place("phase.invert",{546,219,126,26}),place("phase.rotate",{546,253,126,26}),
        place("phase.agc",{699,219,126,26}),place("phase.agcLock",{699,253,126,26}),
        &phaseReference,&learn};
    phaseReference.setBounds(855,219,133,26);phaseAutoReference.setBounds(1000,219,144,26);learn.setBounds(855,255,175,29);

    place("glue.on",{313,511,58,25});place("glue.timing",{190,511,110,25});
    place("glue.amount",{93,620,144,112});
    place("glue.agc",{44,moduleFooterY,87,advancedButtonHeight});
    place("glue.agcLock",{139,moduleFooterY,87,advancedButtonHeight});
    place("glue.lock",{234,moduleFooterY,145,advancedButtonHeight});
    glueAttackSlider.setBounds(266,timingAttackY,100,26);glueReleaseSlider.setBounds(266,timingReleaseY,100,26);
    place("glue.releaseNote",glueReleaseSlider.getBounds());

    place("unmask.on",{1091,511,58,25});
    place("duck.on",{702,511,58,25});duckAdvanced.setBounds(652,moduleFooterY,advancedButtonWidth,advancedButtonHeight);
    duckAdvanced.setComponentID("view-mode");
    duckAdvanced.onClick=[this]{showDuckAdvanced=!showDuckAdvanced;refreshDisplayPage();face.repaint();};
    place("duck.amount",{482,620,144,112});
    place("duck.multiband",{419,733,192,27});
    const auto duckField=[&](const char* id,juce::Rectangle<int> bounds) {
        auto* knob=static_cast<Knob*>(place(id,bounds));
        knob->label.setVisible(false);
        knob->slider.setSliderStyle(juce::Slider::LinearBar);
        knob->slider.setTextBoxStyle(juce::Slider::TextBoxLeft,false,bounds.getWidth(),24);
        knob->resized();
    };
    duckField("duck.attack",{655,timingAttackY,100,26});duckField("duck.release",{655,timingReleaseY,100,26});
    place("duck.releaseNote",{655,timingReleaseY,100,26});place("duck.timing",{490,timingReleaseY,128,25});
    duckField("duck.lowHz",{490,timingAttackY,128,25});

    place("tube.on",{175,786,58,25});place("tube.audition",{241,786,65,25});
    const auto tubeField=[&](const char* id,int x,int width) {
        auto* knob=static_cast<Knob*>(place(id,{x,786,width,25}));
        knob->label.setVisible(false);
        knob->slider.setSliderStyle(juce::Slider::LinearBar);
        knob->slider.setTextBoxStyle(juce::Slider::TextBoxLeft,false,width,24);
        knob->resized();
    };
    tubeField("tube.drive",357,60);tubeField("tube.amount",466,60);
    tubeField("tube.source",611,75);tubeField("tube.colour",750,61);
    place("tube.oversampling",{907,786,61,25});
    place("tube.agc",{978,786,83,25});place("tube.agcLock",{1070,786,86,25});

    place("subMono.on",{690,830,105,26});subMonoHz.setBounds(800,830,89,26);
    output.setBounds(936,830,148,26);
}

void BassDeamonEditor::refreshDisplayPage() {
    const bool analyzerPage=!showScope && !showPhaseAdvanced;
    const bool scopePage=showScope && !showPhaseAdvanced;
    analyzerTab.setToggleState(analyzerPage,juce::dontSendNotification);
    scopeTab.setToggleState(scopePage,juce::dontSendNotification);
    processor.phaseScopeEnabled.store(scopePage && scopeChannel.getSelectedId()==2 && phaseScopeButton.getToggleState());
    phaseAdvanced.setToggleState(showPhaseAdvanced,juce::dontSendNotification);
    for (auto* item : std::array<juce::Component*,6>{&spectrumPre,&spectrumPost,&spectrumHold,&analyzerRefresh,&spectrumMin,&spectrumMax})
        item->setVisible(analyzerPage);
    for (auto* item : std::array<juce::Component*,3>{&scopeChannel,&scopeCompareButton,control("view.hold")})
        item->setVisible(scopePage);
    for (auto* item : phaseControls) item->setVisible(showPhaseAdvanced);
    phaseAutoReference.setVisible(showPhaseAdvanced);
    duckAdvanced.setToggleState(showDuckAdvanced,juce::dontSendNotification);
    control("duck.amount")->setVisible(!showDuckAdvanced);
    control("duck.lowHz")->setVisible(showDuckAdvanced);

    control("duck.timing")->setVisible(showDuckAdvanced);
    const bool sync=processor.parameters.getRawParameterValue("duck.timing")->load()>.5f;
    control("duck.release")->setVisible(!sync);
    control("duck.releaseNote")->setVisible(sync);
}

void BassDeamonEditor::layoutUnmask() {
    const auto position=[&](const char* id, juce::Rectangle<int> bounds) {
        auto* child=control(id);
        child->setBounds(bounds);
        return child;
    };
    unmaskAdvanced.setComponentID("view-mode");
    unmaskAdvanced.setBounds(1041,moduleFooterY,advancedButtonWidth,advancedButtonHeight);
    unmaskAdvanced.setTooltip("Show UNMASK timing, focus limits and gain settings.");
    unmaskAdvanced.setClickingTogglesState(true);
    unmaskAdvanced.onClick=[this] {
        showUnmaskAdvanced=unmaskAdvanced.getToggleState();
        unmaskAdvanced.setButtonText(showUnmaskAdvanced?"Advanced view":"Main view");
        refreshUnmaskControls();
        face.repaint();
    };
    face.addAndMakeVisible(unmaskAdvanced);

    attenuationKnob->setBounds(847,620,144,112);
    depthKnob->setBounds(attenuationKnob->getBounds());
    unmaskWidthKnob->setBounds(994,620,144,112);
    unmaskChannel=position("unmask.stereoMode",{938,583,102,25});
    unmaskAutoFocus=position("unmask.track",{821,733,102,27});
    frequencyKnob->slider.setSliderStyle(juce::Slider::LinearBar);
    frequencyKnob->slider.setTextBoxStyle(juce::Slider::TextBoxLeft,false,122,24);
    frequencyKnob->label.setVisible(false);
    frequencyKnob->setBounds(955,510,122,24);
    frequencyKnob->slider.setTooltip("Manual focus frequency in Hz. Available when Auto Focus is off.");

    unmaskDelta=position("unmask.delta",{929,733,102,27});
    unmaskAdvancedControls={
        position("unmask.agc",{821,543,104,25}),
        position("unmask.agcLock",{936,543,104,25}),
        position("unmask.timing",{821,583,102,25}),
        position("unmask.controlDb",{1047,583,104,25})
    };
    unmaskAttackKnob->setBounds(1044,timingAttackY,100,timingFieldHeight);
    unmaskReleaseKnob->setBounds(1044,timingReleaseY,100,timingFieldHeight);
    for (auto* field : {unmaskAttackKnob,unmaskReleaseKnob}) {
        field->label.setVisible(false);
        field->slider.setSliderStyle(juce::Slider::LinearBar);
        field->slider.setTextBoxStyle(juce::Slider::TextBoxLeft,false,100,24);
        field->resized();
    }
    unmaskReleaseNote=position("unmask.releaseNote",{1044,timingReleaseY,100,timingFieldHeight});
    unmaskHoldFocus=position("unmask.hold",{1047,543,104,25});
    focusMin.setBounds(821,733,87,25);
    focusMax.setBounds(917,733,87,25);
    refreshUnmaskControls();
}

void BassDeamonEditor::refreshUnmaskControls() {
    const auto active=[&](const char* id) {
        return processor.parameters.getRawParameterValue(id)->load()>.5f;
    };
    const bool advanced=showUnmaskAdvanced;
    const bool autoFocus=active("unmask.track");
    const bool direct=active("unmask.controlDb");
    const bool sync=active("unmask.timing");
    attenuationKnob->setVisible(!advanced && direct);
    depthKnob->setVisible(!advanced && !direct);
    unmaskWidthKnob->setVisible(!advanced);
    frequencyKnob->setVisible(!advanced && !autoFocus);
    unmaskAutoFocus->setVisible(!advanced);
    unmaskChannel->setVisible(advanced);
    unmaskDelta->setTopLeftPosition(advanced?821:929,advanced?650:moduleFooterY);
    for (auto* control:unmaskAdvancedControls) control->setVisible(advanced);
    unmaskAttackKnob->setVisible(advanced);
    unmaskReleaseKnob->setVisible(advanced && !sync);
    unmaskReleaseNote->setVisible(advanced && sync);
    focusMin.setVisible(advanced && autoFocus);
    focusMax.setVisible(advanced && autoFocus);
    unmaskHoldFocus->setVisible(advanced && autoFocus);
}
void BassDeamonEditor::createTexture(){
    grain=juce::Image(juce::Image::ARGB,1200,870,true);juce::Random random(0xBADB00B);
    {
        juce::Image::BitmapData data(grain,juce::Image::BitmapData::writeOnly);
        for(int y=0;y<870;++y)for(int x=0;x<1200;++x){
            const int n=random.nextInt(9);
            data.setPixelColour(x,y,juce::Colour::fromRGBA(230,216,180,static_cast<juce::uint8>(n)));
        }
    }

}
void BassDeamonEditor::rebuildConsoleBackground() {
    consoleBackground=juce::Image(juce::Image::RGB,1200,870,true);
    juce::Graphics g(consoleBackground);
    g.fillAll(console::ink);
    skin.paintPanel(g,{5,5,1190,860});
    g.drawImageAt(grain,0,0);
    static const auto wordmark=juce::ImageCache::getFromMemory(BassDeamonArt::BassDeamonWordmark_png,
                                                              BassDeamonArt::BassDeamonWordmark_pngSize);
    g.drawImageWithin(wordmark,45,18,265,39,juce::RectanglePlacement::centred);
    console::text(g,"Engineered by Guy_BioRg",{319,20,170,33},15,console::amber);
    console::text(g,"SKIN",{35,69,40,25},13,console::muted);
    skin.paintPanel(g,{24,128,1152,320},!skin.isRaw());
    skin.paintPanel(g,{24,453,660,45});
    skin.paintPanel(g,{691,453,485,45});
    for (int i=0;i<3;++i) skin.paintPanel(g,{float(panelLeft+panelStride*i),503,float(panelWidth),267});
    skin.paintPanel(g,{24,776,1152,44});
    skin.paintPanel(g,{24,824,1152,38},!skin.isRaw());
}

void BassDeamonEditor::paintConsole(juce::Graphics& g) {
    if (!consoleBackground.isValid()) rebuildConsoleBackground();
    g.drawImageAt(consoleBackground,0,0);
    const auto meter=[&](const char* label,float db,float x) {
        console::text(g,label,{x,69,68,24},12,console::cream);
        const float level=std::clamp((db+60.f)/60.f,0.f,1.f);
        for(int segment=0;segment<16;++segment) {
            g.setColour(segment+1<=level*16?console::amber:console::ink);
            g.fillRoundedRectangle(x+68+segment*5.6f,76,4.5f,10,1);
        }
        console::text(g,juce::String(std::max(db,-99.f),1)+" dBFS",{x+163,69,78,24},11,console::cream);
    };
    meter("IN RMS",inputDb,193);meter("OUT RMS",outputDb,449);

    const auto autoMixFace=autoMixButton.getBounds().toFloat().reduced(1);
    g.setColour(juce::Colours::black.withAlpha(.28f));
    g.fillRoundedRectangle(autoMixFace.translated(0,2),5);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff37332f),0,autoMixFace.getY(),
                                           juce::Colour(0xff24211e),0,autoMixFace.getBottom(),false));
    g.fillRoundedRectangle(autoMixFace,5);
    g.setColour(console::amber.withAlpha(.55f));
    g.drawRoundedRectangle(autoMixFace,5,1.f);
    paintSignalPath(g,processor);
    if (g.clipRegionIntersects({24,128,1152,320})) {
        if (visualBypassed) {
            console::text(g,"BYPASSED",{70,200,1060,154},20,console::muted,juce::Justification::centred);
        } else if (showPhaseAdvanced) {
            console::text(g,"PHASE / ADVANCED",{52,177,175,24},14,console::cream);
            console::text(g,processor.phaseReferenceAutomatic.load()?(processor.detectedKickHz.load()>0?"KICK REFERENCE":"AUTO / WAITING"):"MANUAL REFERENCE",{855,193,280,24},12,console::muted);
            const int state=processor.learnStatus.load();
            const auto status=state==1?"LEARNING "+juce::String(int(processor.learnProgress.load()*100))+"%"
                :state==5?"APPLIED / r "+juce::String(processor.learnCorrelation.load(),2)
                :state==7?juce::String("PRESS PLAY"):state==6?juce::String("NO KICK SIDECHAIN")
                :state>=2&&state<=4?juce::String("NO RELIABLE MATCH"):juce::String("READY");
            console::text(g,status,{855,290,228,20},13,console::amber);
        } else if (showScope) {
            juce::Graphics::ScopedSaveState page(g);
            g.addTransform(juce::AffineTransform::translation(0,-155));
            paintWave(g);
        } else {
            paintSpectrum(g);
        }
    }

    stageTitle(g,Stage::kickEq,{38,457,130,33},18);
    stageTitle(g,Stage::phase,{706,457,130,33},18);
    const int eq=processor.kickEqStatus.load();
    if (eq==1 || eq==2 || eq==4 || eq==5) {
        const auto status=eq==1?"LEARNING "+juce::String(int(processor.kickEqProgress.load()*100))+"%"
            :eq==2?juce::String("NO RELIABLE PEAK"):eq==4?juce::String("NO KICK SIDECHAIN"):juce::String("PRESS PLAY");
        console::text(g,status,{382,429,210,17},12,console::amber);
    }
    constexpr std::array stages{Stage::glue,Stage::duck,Stage::unmask};
    for (size_t index=0;index<stages.size();++index) {
        const float x=float(panelLeft)+float(index)*panelStride;
        stageTitle(g,stages[index],{x+18,509,152,27},20);
    }
    const bool raw=skin.isRaw();
    if(raw) {
        g.setColour(juce::Colour(0xffa5a59c).withAlpha(.65f));
        g.drawLine(244,626,244,721,.8f);g.drawLine(633,626,633,721,.8f);
    }
    paintReductionMeter(g,panelLeft+panelWidth*.5f,542,glueGr,20,raw);
    paintReductionMeter(g,panelLeft+panelStride+panelWidth*.5f,542,processor.duckReduction.load(),18,raw);
    if (!showUnmaskAdvanced) paintReductionMeter(g,panelLeft+2*panelStride+panelWidth*.5f,542,processor.unmaskReduction.load(),24,raw);
    console::text(g,"ATTACK",{266,timingAttackY-19,100,19},12,console::muted);
    console::text(g,"RELEASE",{266,timingReleaseY-19,100,19},12,console::muted);
    const int timing=int(processor.parameters.getRawParameterValue("glue.timing")->load());
    if (timing==0) {
        console::text(g,juce::String(processor.glueAttack.load(),1)+" ms",{266,timingAttackY,100,26},15,console::amber);
        console::text(g,juce::String(processor.glueRelease.load(),1)+" ms",{266,timingReleaseY,100,26},15,console::amber);
    }
    if (showUnmaskAdvanced) {
        console::text(g,"TIMING",{821,568,102,15},11,console::muted);
        console::text(g,"CHANNEL",{938,568,102,15},11,console::muted);
        console::text(g,"ATTACK",{1044,timingAttackY-19,100,19},12,console::muted);
        console::text(g,"RELEASE",{1044,timingReleaseY-19,100,19},12,console::muted);
        if (processor.parameters.getRawParameterValue("unmask.track")->load()>.5f) {
            console::text(g,"FOCUS MIN",{821,715,87,16},11,console::muted);
            console::text(g,"FOCUS MAX",{917,715,87,16},11,console::muted);
        }
    } else if (processor.parameters.getRawParameterValue("unmask.track")->load()>.5f) {
        console::text(g,"FOCUS "+juce::String(focus,1)+" Hz",{955,512,125,22},12,console::muted);
    }
    console::text(g,"ATTACK",{655,timingAttackY-19,100,19},12,console::muted);
    console::text(g,"RELEASE",{655,timingReleaseY-19,100,19},12,console::muted);
    if (showDuckAdvanced) {
        console::text(g,"LOW-BAND FREQUENCY",{490,timingAttackY-19,145,19},12,console::muted);
        console::text(g,"RELEASE MODE",{490,timingReleaseY-19,128,19},12,console::muted);
    }


    stageTitle(g,Stage::softTube,{39,782,137,31},17);
    for (const auto& caption : std::array<std::pair<const char*,float>,5>{{
        {"DRIVE",313},{"MIX",431},{"SOURCE LP",534},{"COLOUR",690},{"OVERSAMPLING",816}}})
        console::text(g,caption.first,{caption.second,786,93,25},11,console::muted);
    g.setColour(sidePeak>.0003f?console::amber:console::muted.darker());
    g.fillEllipse(47,839,8,8);
    console::text(g,sidePeak>.0003f?"KICK SC ACTIVE":"KICK SC IDLE",{64,831,150,25},13,console::cream);
    console::text(g,"PEAK "+juce::String(std::max(peak,-99.f),1)+" dBFS",{255,830,178,25},13,
                  clipFrames>0?console::orange:console::cream);
    console::text(g,"TRIM",{897,830,45,25},11,console::muted);
    console::text(g,"v"+juce::String(JucePlugin_VersionString),{1092,830,71,25},12,console::muted);
}
bool BassDeamonEditor::spectrumIsPost() const {
    return processor.spectrumView()!=0;
}
std::array<double,2> BassDeamonEditor::spectrumLimits() const {
    const double ceiling=std::min(20000.0,displaySampleRate*.5);
    const double minimum=juce::jlimit(0.0,ceiling-20.0,double(processor.parameters.getRawParameterValue("view.minHz")->load()));
    const double maximum=juce::jlimit(minimum+20.0,ceiling,double(processor.parameters.getRawParameterValue("view.maxHz")->load()));
    return {minimum,maximum};
}
void BassDeamonEditor::paintSpectrum(juce::Graphics& g) {
    const bool post = spectrumIsPost();
    const juce::Colour bassBlue(skin.isRaw()?0xff08d8f0:0xff8faef1);
    g.setColour(juce::Colour(skin.isRaw()?0xff06161c:0xff181a18));
    g.fillRoundedRectangle(72,166,1070,22,3);
    const auto legend = [&](const juce::String& label, float x, juce::Colour colour, bool dashed=false) {
        g.setColour(colour);
        if (dashed) {
            g.drawLine(x,177,x+9,177,2);
            g.drawLine(x+14,177,x+23,177,2);
        } else {
            g.drawLine(x,177,x+23,177,2);
        }
        console::text(g,label,{x+32,166,140,22},15,colour);
    };
    legend(post?"BASS POST":"BASS PRE",88,bassBlue);
    legend("KICK",340,juce::Colour(skin.isRaw()?0xffffae12:0xffd59666));
    legend("KICK EQ",600,eqTrace,true);
    if (post) legend("UNMASK",860,unmaskTrace);
    console::text(g,"RATE",{408,136,48,26},13,console::muted);
    console::text(g,"FROM",{799,136,48,26},13,console::muted);
    console::text(g,"TO",{995,136,25,26},13,console::muted);
    const int clashes = paintSpectrumPlot(g,{72,190,1070,160},post);
    const juce::String status = !spectrumDisplay.ready ? "COLLECTING AUDIO"
        : !post ? "COLLISIONS SHOWN IN POST"
        : !spectrumDisplay.residualReady ? "WAITING FOR PROCESSED AUDIO"
        : clashes > 0 ? juce::String(clashes) + (clashes==1?" RESIDUAL BAND":" RESIDUAL BANDS") + " / 30-250 Hz"
        : "NO SIGNIFICANT RESIDUAL";
    const auto mode = visualUnmaskResponse.mode;
    const juce::String component = mode == bassdeamon::UnmaskStereoMode::mid ? " MID"
        : mode == bassdeamon::UnmaskStereoMode::side ? " SIDE" : "";
    const juce::String comparison = processor.compareDry.load() ? "  /  DRY AUDITION" : "";
    console::text(g,status+comparison,{72,433,430,14},13,console::muted);
    if (post) console::text(g,"UNMASK"+component+" PEAK  "+juce::String(visualUnmaskGr,1)+" dB",
                            {838,433,304,14},13,unmaskTrace,juce::Justification::centredRight);
}

int BassDeamonEditor::paintSpectrumPlot(juce::Graphics& g,juce::Rectangle<float> plot,bool post) {
    const auto limits=spectrumLimits();const double minimum=limits[0],maximum=limits[1];
    // Log-like bass zoom preserves a usable zero-Hz endpoint.
    const double start = std::log1p(minimum/20);
    const double span = std::log1p(maximum/20)-start;
    const auto fx=[&](double hz){return plot.getX()+float((std::log1p(hz/20)-start)/span)*plot.getWidth();};
    const auto fy=[&](float level){return plot.getBottom()-juce::jlimit(0.f,1.f,(level+96)/96)*plot.getHeight();};
    g.setColour(juce::Colour(skin.isRaw()?0xff03141d:0xff11151a));
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
    const double binWidth=displaySampleRate/bassdeamon::RelationAnalyzer::fftSize;
    using Analyzer=bassdeamon::RelationAnalyzer;
    const auto& paired=spectrumDisplay.paired;
    const auto levelsAt=[&](size_t bin) {
        if (bin<Analyzer::readoutBins) return spectrumDisplay.eventLevels[bin];
        return Analyzer::LevelTriplet{paired[0][bin],paired[1][bin],paired[2][bin]};
    };
    int activeRegions=0;
    Analyzer::CollisionBand strongest;
    float strongestRank=-121;
    {
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(plot.toNearestInt());
        for(int trace=0;trace<2;++trace){
            const auto& values=spectrumDisplay.ready
                ? spectrumDisplay.trace[trace==0?2:(post?1:0)]
                : (trace==0?visualKick:selected);
            const auto colour=trace==0?juce::Colour(skin.isRaw()?0xffffae12:0xffd59666):bassBlue;
            juce::Path curve;
            const size_t first=size_t(std::max(0.,std::floor(minimum/binWidth)));
            const size_t last=std::min(values.size()-1,size_t(std::ceil(maximum/binWidth)));
            curve.startNewSubPath(fx(first*binWidth),fy(values[first]));
            // Retain the strongest bin per screen column, keeping the draw
            // cost bounded when the view is zoomed to the whole spectrum.
            for (size_t b=first+1;b<=last;) {
                const int column=int(fx(b*binWidth));
                size_t peak=b++;
                while (b<=last && int(fx(b*binWidth))==column) {
                    if (values[b]>values[peak]) peak=b;
                    ++b;
                }
                curve.lineTo(fx(peak*binWidth),fy(values[peak]));
            }
            if (last>first) curve.lineTo(fx(last*binWidth),fy(values[last]));
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
        if (post && spectrumDisplay.residualReady) {
            Analyzer::visitCollisionBands(spectrumDisplay.maskPost,paired[1],displaySampleRate,minimum,maximum,
                [&](const Analyzer::CollisionBand& region) {
                    ++activeRegions;
                    const float x=fx(region.hz), y=fy(paired[1][region.peak]);
                    const auto accent=region.strength>=.65f ? red : console::amber;
                    g.setColour(accent.withAlpha(.30f));
                    g.drawVerticalLine(juce::roundToInt(x),y,plot.getBottom());
                    g.setColour(accent);
                    g.fillEllipse(x-3,y-3,6,6);
                    const auto levels=levelsAt(region.peak);
                    const float rank=std::min(levels[1],levels[2])+10*region.strength;
                    if (activeRegions==1 || rank>strongestRank) {
                        strongest=region;
                        strongestRank=rank;
                    }
                });
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
        console::text(g,"dB",{plot.getRight()+2,173,25,15},11,console::muted,juce::Justification::centred);
        console::text(g,"dBFS",{27,173,42,15},11,console::muted,juce::Justification::centredRight);
        for (int level : {0,-4,-12,-24}) {
            const float y=gainY(level);
            g.setColour(console::cream.withAlpha(.35f));
            g.drawHorizontalLine(juce::roundToInt(y),plot.getRight()-4,plot.getRight()+3);
            console::text(g,juce::String(level),{plot.getRight()+2,y-7,25,14},11,console::muted,juce::Justification::centred);
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
                        return visualUnmaskResponse.gainDb(hz,displaySampleRate);
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
            const double halfRateAngle=bassdeamon::pi/displaySampleRate;
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
    if (activeRegions>0) {
        const auto formatDb=[](float value){
            return (value>=0?juce::String("+"):juce::String{})+juce::String(value,1)+" dB";
        };
        // Keep readouts below the plot, leaving the actual peaks visible.
        const auto [pre, after, kickDb]=levelsAt(strongest.peak);
        const auto relation=after>-110 && kickDb>-110 ? formatDb(after-kickDb) : juce::String("--");
        const auto delta=pre>-110 && after>-110 ? formatDb(after-pre) : juce::String("--");
        const float boxX=plot.getX();
        const float width=plot.getWidth();
        const auto accent=(strongest.strength>=.65f?red:console::amber).withAlpha(.45f+.55f*strongest.strength);
        juce::Rectangle<float> label(boxX,374,width,50);
        g.setColour(console::muted.withAlpha(.25f));g.drawHorizontalLine(374,label.getX(),label.getRight());
        console::text(g,"RESIDUAL COLLISION",label.withWidth(190).withHeight(17),12,console::muted);
        console::text(g,juce::String(strongest.hz,1)+" Hz",label.withWidth(190).withTrimmedTop(17),skin.isRaw()?24.f:21.f,accent);
        console::text(g,"BASS POST - KICK",{boxX+width*.33f,374,185,17},12,console::muted);
        console::text(g,relation,{boxX+width*.33f,391,185,29},skin.isRaw()?22.f:20.f,console::cream);
        console::text(g,"POST - PRE",{boxX+width*.66f,374,185,17},12,console::muted);
        console::text(g,delta,{boxX+width*.66f,391,185,29},skin.isRaw()?22.f:20.f,console::cream);
    }
    return activeRegions;
}
void BassDeamonEditor::paintWave(juce::Graphics& g){
    console::text(g,"OSCILLOSCOPE / KICK + BASS",{47,337,300,20},13,console::cream);
    const juce::Colour postColour(0xff95f4ab);
    const juce::Colour preColour(0xffe9ba78);
    const bool phaseView=scopeChannel.getSelectedId()==2 && phaseScopeButton.getToggleState() &&
        processor.parameters.getRawParameterValue("phase.on")->load()>.5f;
    const bool phaseMeasured=phaseView && visibleScope.phaseMeasured
        && std::abs(visibleScope.measuredPhaseHz-processor.phaseReferenceHz.load())<.05
        && processor.parameters.getRawParameterValue("phase.on")->load()>.5f
        && processor.parameters.getRawParameterValue("global.on")->load()>.5f;
    console::text(g,phaseView?"ON":"POST",{373,337,68,20},13,postColour);
    if (phaseView || compareScopePre) console::text(g,phaseView?"OFF":"PRE",{441,337,65,20},13,preColour);

    using Scope=bassdeamon::BeatScope;
    const bool mono=scopeChannel.getSelectedId()==2;
    const juce::Rectangle<float> bezel(42,362,1116,216);
    const auto glass=bezel.reduced(5,4);
    const auto plot=glass.reduced(10,3).withTrimmedLeft(22);
    g.setColour(juce::Colours::black.withAlpha(.7f));
    g.fillRoundedRectangle(bezel.translated(0,2),8);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff665848),0,bezel.getY(),
        juce::Colour(0xff29251e),0,bezel.getBottom(),false));
    g.fillRoundedRectangle(bezel,8);
    g.setColour(juce::Colour(0xff141610));
    g.fillRoundedRectangle(bezel.reduced(2),7);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff14251b),0,glass.getY(),
        juce::Colour(0xff080f0b),0,glass.getBottom(),false));
    g.fillRoundedRectangle(glass,5);
    g.setColour(postColour.withAlpha(.18f));
    g.drawRoundedRectangle(glass,5,.8f);

    float maximum=.05f;
    const auto measure=[&](Scope::Stream stream) {
        for (const auto* values : {&visibleScope.high[stream],&visibleScope.low[stream]})
            for (float v : *values) maximum=std::max(maximum,std::abs(v));
    };
    if (phaseView) { measure(Scope::phaseOff); measure(Scope::phaseOn); }
    else measure(mono?Scope::postMono:Scope::postLeft);
    if (!phaseView && !mono) measure(Scope::postRight);
    if (!phaseView && compareScopePre) {
        measure(mono?Scope::preMono:Scope::preLeft);
        if (!mono) measure(Scope::preRight);
    }
    const auto lane=[&](juce::Rectangle<float> area,const char* label,Scope::Stream pre,Scope::Stream post) {
        const float centre=area.getCentreY();
        console::text(g,label,{55,centre-8,19,16},11,postColour.withAlpha(.8f));
        for (int i=0;i<=8;++i) {
            const float x=area.getX()+float(i)*area.getWidth()/8;
            g.setColour(postColour.withAlpha(i==4?.24f:i%2==0?.16f:.075f));
            g.drawVerticalLine(juce::roundToInt(x),area.getY(),area.getBottom());
        }
        for (int i=0;i<=4;++i) {
            const float y=area.getY()+float(i)*area.getHeight()/4;
            g.setColour(postColour.withAlpha(i==2?.26f:.09f));
            g.drawHorizontalLine(juce::roundToInt(y),area.getX(),area.getRight());
        }
        const auto trace=[&](Scope::Stream stream,juce::Colour colour) {
            if (visibleScope.completed==0) return;
            const float scale=area.getHeight()*.43f/maximum;
            juce::Path line, peaks;
            for (size_t i=0;i<Scope::size;++i) {
                const float x=area.getX()+float(i)*area.getWidth()/float(Scope::size-1);
                const float low=centre-visibleScope.low[stream][i]*scale;
                const float high=centre-visibleScope.high[stream][i]*scale;
                const float middle=(low+high)*.5f;
                if (i==0) line.startNewSubPath(x,middle); else line.lineTo(x,middle);
                if (low-high>.5f) {peaks.startNewSubPath(x,low);peaks.lineTo(x,high);}
            }
            g.setColour(colour.withMultipliedAlpha(.18f));
            g.strokePath(line,juce::PathStrokeType(4.f));
            g.setColour(colour.withMultipliedAlpha(.7f));
            g.strokePath(peaks,juce::PathStrokeType(1.f));
            g.setColour(colour);
            g.strokePath(line,juce::PathStrokeType(1.3f));
        };
        if (phaseView && !phaseMeasured) {
            trace(Scope::postMono,postColour.withAlpha(.65f));
            return;
        }
        if (phaseView || compareScopePre) trace(pre,preColour.withAlpha(.72f));
        trace(post,postColour);
    };
    if (phaseView) {
        lane(plot,"M",Scope::phaseOff,Scope::phaseOn);
    } else if (mono) {
        lane(plot,"M",Scope::preMono,Scope::postMono);
    } else {
        const float laneHeight=(plot.getHeight()-4)*.5f;
        lane(plot.withHeight(laneHeight),"L",Scope::preLeft,Scope::postLeft);
        lane(plot.withTrimmedTop(laneHeight+4),"R",Scope::preRight,Scope::postRight);
    }
    if (phaseView && !phaseMeasured)
        console::text(g,"PLAY KICK + BASS TO MEASURE PHASE A/B",plot,12,postColour.withAlpha(.6f),juce::Justification::centred);
    else if (visibleScope.completed==0)
        console::text(g,"WAITING FOR SIGNAL",plot,12,postColour.withAlpha(.55f),juce::Justification::centred);
    if (phaseView && phaseMeasured) {
        const double delta=visibleScope.phaseDelta;
        console::text(g,"MATCHED SUM  "+juce::String(delta>=0?"+":"")+juce::String(delta,1)+" dB @ "+
            juce::String(processor.phaseReferenceHz.load(),1)+" Hz",{285,581,222,14},11,postColour);
    }
    const bool host=processor.tempoFromHost.load();
    console::text(g,"1/4 NOTE / "+juce::String(visibleScope.milliseconds,1)+" ms / "+(host?"PROJECT ":"FALLBACK ")+
        juce::String(processor.tempo.load(),1)+" BPM",{48,581,230,14},11,console::cream);
    const auto& sub=visibleScope.subMonoCheck;
    if (!phaseView) console::text(g,"SUB FOLD LOSS  "+(sub.active?juce::String(sub.foldLossDb,1)+" dB":juce::String("--")),
                  {280,581,225,14},11,console::cream,juce::Justification::centredRight);
    const auto& check=visibleScope.monoCheck;
    juce::String monoStatus="BASS OUT MONO CHECK";
    const char* severity="NO SIGNAL";
    auto accent=console::muted;
    if (check.active) {
        const auto correlation=check.correlationValid?juce::String(check.correlation,2):juce::String("--");
        const auto loss=check.foldLossDb<=-79.99?juce::String("< -80"):juce::String(check.foldLossDb,1);
        monoStatus+=" / L/R CORR "+correlation+" / FOLD LOSS "+loss+" dB";
        const auto lossSeverity=bassdeamon::monoLossSeverity(check);
        if (lossSeverity==bassdeamon::MonoLossSeverity::high) {
            severity="HIGH LOSS";
            accent=juce::Colour(0xffff8276);
        } else if (lossSeverity==bassdeamon::MonoLossSeverity::caution) {
            severity="CAUTION";
            accent=console::amber;
        } else {
            severity="LOW LOSS";
            accent=postColour;
        }
    }
    console::text(g,monoStatus,{518,581,525,14},11,console::cream,juce::Justification::centredRight);
    const juce::Rectangle<float> badge(1052,580,100,16);
    g.setColour(accent.withAlpha(.13f));
    g.fillRoundedRectangle(badge,3);
    g.setColour(accent.withAlpha(.65f));
    g.drawRoundedRectangle(badge,3,1);
    console::text(g,severity,badge,11,accent,juce::Justification::centred);
}
void BassDeamonEditor::syncTheme() {
    const bool raw=processor.usesRawTheme();
    if (skin.setRaw(raw)) {
        themeSelector.setSelectedId(raw?2:1,juce::dontSendNotification);
        rebuildConsoleBackground();
        face.repaint();
        for (auto* child : face.getChildren()) child->repaint();
    }
}
void BassDeamonEditor::timerCallback() {
    if (isTimerRunning() && !analysisThread.joinable()) {
        analysisThread=std::thread([this]{
            while (!stopAnalysis.load(std::memory_order_acquire)) {
                const auto then=juce::Time::getMillisecondCounterHiRes();
                analyzeFrames(then);
                publishAnalysis();
                const auto elapsed=juce::Time::getMillisecondCounterHiRes()-then;
                const double interval=1000./std::clamp(processor.analyzerRefreshHz(),30.,60.);
                juce::Thread::sleep(std::max(1,int(interval-elapsed)));
            }
        });
    }
    refreshVisuals(juce::Time::getMillisecondCounterHiRes());
}

void BassDeamonEditor::analyzeFrames(double now) {
    const auto started=juce::Time::getMillisecondCounterHiRes();
    const bool bypassed=processor.displayBypassed();
    const double rate=processor.getSampleRate()>0?processor.getSampleRate():48000;
    if (bypassed || analysisWasBypassed) {
        bassdeamon::AnalysisFrame frame;
        for (uint32_t n=processor.analysisQueue.pending();n>0 && processor.analysisQueue.pop(frame);--n) {}
        if (bypassed!=analysisWasBypassed) {
            analyzer.prepare(rate);
            lastAnalysisTime=now;
            ++analysisRevision;
        }
        analysisWasBypassed=bypassed;
        return;
    }
    if (rate!=analyzer.sampleRate()) {
        analyzer.prepare(rate);
        ++analysisRevision;
    }
    const auto beforeSequence=analyzer.streamSequence();
    analyzer.scope.setPhaseFrequency(processor.phaseReferenceHz.load());
    analyzer.scope.setSubFrequency(processor.parameters.getRawParameterValue("subMono.hz")->load());
    analyzer.consume(processor.analysisQueue,processor.tempo.load(),processor.parameters.getRawParameterValue("view.hold")->load()>.5f);
    if (analyzer.streamSequence()!=beforeSequence) {
        lastAnalysisTime=now;
        staleAnalysis=false;
        ++analysisRevision;
    } else if (!staleAnalysis && now-lastAnalysisTime>500) {
        analyzer.prepare(rate);
        staleAnalysis=true;
        ++analysisRevision;
    }
    workerAnalysisMilliseconds=.85*workerAnalysisMilliseconds+
        .15*(juce::Time::getMillisecondCounterHiRes()-started);
}

void BassDeamonEditor::publishAnalysis() {
    const std::lock_guard<std::mutex> lock(publicationMutex);
    publication.milliseconds=workerAnalysisMilliseconds;
    if (publication.revision==analysisRevision) return;
    publication.revision=analysisRevision;
    publication.samples=analyzer.consumedFrames;
    publication.rate=analyzer.sampleRate();
    publication.bypassed=analysisWasBypassed;
    analyzer.copyFrame(publication.frame);
    publication.scope=analyzer.scope;
}

void BassDeamonEditor::clearVisualAnalysis() {
    receivedFrame.hasSpectrum=false;
    spectrumDisplay.reset();
    visualPre.fill(-120);visualPost.fill(-120);visualKick.fill(-120);
    visualUnmaskResponse={};pendingUnmaskResponse={};
    unmaskPathDirty=true;
    visibleScope={};
}

void BassDeamonEditor::refreshVisuals(double clockMilliseconds) {
    syncTheme();
    const bool bypassed=processor.displayBypassed();
    const bool bypassChanged=visualBypassed!=bypassed;
    if (bypassChanged) {
        visualBypassed=bypassed;
        clearVisualAnalysis();
        processor.consumeUnmaskResponse();
    }
    const double currentRate=processor.analyzerRefreshHz();
    if (currentRate!=syncedRefreshHz) {
        syncedRefreshHz=currentRate;
        analyzerRefresh.setValue(currentRate,juce::dontSendNotification);
    }
    // Only the worker runs the FFT. The message thread holds the lock only
    // long enough to copy a finished measurement and the current scope.
    if (!analysisThread.joinable()) {
        analyzeFrames(clockMilliseconds);
        publishAnalysis();
    }
    bool received=false,scopeUpdated=false;
    {
        std::unique_lock<std::mutex> lock(publicationMutex,std::try_to_lock);
        if (lock.owns_lock()) {
            analyzerMilliseconds=bypassed?0:publication.milliseconds;
            if (!bypassed && !publication.bypassed && !bypassChanged && publication.revision!=displayedAnalysisRevision) {
                if (displaySampleRate!=publication.rate) {
                    displaySampleRate=publication.rate;
                    clearVisualAnalysis();
                }
                displayedAnalysisRevision=publication.revision;
                received=publication.samples!=displayedSamples;
                displayedSamples=publication.samples;
                receivedFrame=publication.frame;
                visibleScope=publication.scope;
                scopeUpdated=true;
            }
        }
    }
    const auto revision=spectrumDisplay.revision;
    const bool wasReady=spectrumDisplay.ready;
    const auto response=processor.consumeUnmaskResponse();
    if (!bypassed && !bypassChanged && !spectrumHold.getToggleState()) {
        if (response.activity>=pendingUnmaskResponse.activity ||
            (response.centreHz>0 && std::abs(response.centreHz-pendingUnmaskResponse.centreHz)>response.centreHz*.05))
            pendingUnmaskResponse=response;
        spectrumDisplay.pushAtRate(receivedFrame,received?1:0,clockMilliseconds*.001,currentRate);
    }
    const bool newFrame=revision!=spectrumDisplay.revision || (wasReady&&!spectrumDisplay.ready);
    if (newFrame) {
        visualPre=spectrumDisplay.shown[0];visualPost=spectrumDisplay.shown[1];visualKick=spectrumDisplay.shown[2];
        visualUnmaskResponse=spectrumDisplay.ready?pendingUnmaskResponse:bassdeamon::UnmaskResponse{};
        pendingUnmaskResponse={};
        unmaskPathDirty=true;
    }
    const bool tracesChanged=!bypassed && !showScope && !showPhaseAdvanced &&
        !spectrumHold.getToggleState() && spectrumDisplay.advanceTrace(clockMilliseconds*.001);
    if (bypassChanged || (showScope?scopeUpdated:showPhaseAdvanced || newFrame || tracesChanged))
        face.repaint(24,128,1152,320);
    if (!bypassChanged && lastMetersPoll>=0 && clockMilliseconds>=lastMetersPoll &&
        clockMilliseconds-lastMetersPoll<30) return;
    lastMetersPoll=clockMilliseconds;
    processor.refreshHistory();undoButton.setEnabled(processor.canUndo());redoButton.setEnabled(processor.canRedo());
    cpuLabel.setText("DSP CPU "+juce::String(processor.cpuLoad.getLoadAsPercentage(),1)+"%  |  ANALYZER "+juce::String(analyzerMilliseconds,1)+" ms  |  OVR "+juce::String(processor.cpuLoad.getXRunCount()),juce::dontSendNotification);
    const int timing=int(processor.parameters.getRawParameterValue("glue.timing")->load());
    refreshUnmaskControls();
    spectrumPre.setToggleState(!spectrumIsPost(),juce::dontSendNotification);
    spectrumPost.setToggleState(processor.spectrumView()==1,juce::dontSendNotification);
    const bool powered=!bypassed;
    const bool canCompare=powered && processor.parameters.getRawParameterValue("unmask.delta")->load()<.5f &&
        !(processor.parameters.getRawParameterValue("tube.on")->load()>.5f &&
          processor.parameters.getRawParameterValue("tube.audition")->load()>.5f);
    kickEqLearn.setEnabled(powered && processor.kickEqStatus.load()!=1);
    dryCompare.setEnabled(canCompare);matchCompare.setEnabled(canCompare);
    dryCompare.setToggleState(processor.compareDry.load(),juce::dontSendNotification);
    matchCompare.setToggleState(processor.compareMatch.load(),juce::dontSendNotification);
    refreshDisplayPage();
    glueAttackSlider.setVisible(timing!=0);glueReleaseSlider.setVisible(timing==1);
    for(auto& c:choices){const auto id=c->box.getComponentID();
        if(id=="glue.releaseNote")c->box.setVisible(timing==2);
    }
    for(size_t i=0;i<bass.size();++i){bass[i]=0.7f*bass[i]+0.3f*processor.spectrumBass[i].load();kick[i]=0.7f*kick[i]+0.3f*processor.spectrumKick[i].load();cut[i]=processor.spectrumCut[i].load();}
    for(size_t i=0;i<3;++i)compensation[i]=processor.agcMeters[i].load();
    glueGr=0.65f*glueGr+0.35f*processor.glueReduction.load();focus=processor.focusHz.load();q=processor.unmaskQ.load();
    peak=static_cast<float>(bassdeamon::db(processor.outputPeak.load()));sidePeak=processor.kickPeak.load();inputDb=processor.inputRmsDb.load();outputDb=processor.outputRmsDb.load();
    if(peak>=0)clipFrames=90;else clipFrames=std::max(0,clipFrames-1);
    for(auto& sw:switches)if(sw->button.getComponentID()=="tube.audition")sw->button.setEnabled(processor.parameters.getRawParameterValue("global.on")->load()>.5f);
    learn.setEnabled(processor.learnStatus.load()!=1&&processor.parameters.getRawParameterValue("global.on")->load()>.5f);
    aButton.setToggleState(processor.selectedAB()==0,juce::dontSendNotification);bButton.setToggleState(processor.selectedAB()==1,juce::dontSendNotification);
    const double reference=processor.phaseReferenceHz.load();
    if (std::abs(phaseReference.getValue()-reference)>.051) {
        phaseReference.setValue(reference,juce::dontSendNotification);
        rotationKnob->slider.updateText();phaseAngle.updateText();
    }
    phaseAutoReference.setToggleState(processor.phaseReferenceAutomatic.load(),juce::dontSendNotification);
    if (bypassed) {
        inputDb=outputDb=peak=-120;
        glueGr=sidePeak=0;
    }
    if (autoPanel.isVisible()) refreshAutoMix();
    face.repaint(24,60,1152,66);
    face.repaint(24,453,1152,409);
}


void BassDeamonEditor::refreshAutoMix() {
    using Status=BassDeamonProcessor::AutoMixState;
    const auto status=processor.autoMixState();
    const auto& result=processor.autoMixResult();
    const bool ready=status==Status::ready;
    const bool usable=ready && result.hasBass && result.hasKick;
    const bool audition=usable && !result.changes.empty();
    autoBars.setEnabled(status==Status::idle);
    autoScan.setEnabled(status==Status::idle);
    autoCancel.setButtonText(ready && !audition ? "CLOSE" : "CANCEL");
    autoDry.setEnabled(audition);
    autoWet.setEnabled(audition);
    autoApply.setEnabled(usable && !result.changes.empty());
    autoMatch.setEnabled(audition);
    const bool match=autoMatch.getToggleState();
    if (audition && autoSide == 0) autoSide=2;
    if (audition && !processor.balancePreviewActive.load()) processor.auditionAutoMix(autoSide, match);
    autoDry.setToggleState(audition && autoSide==1,juce::dontSendNotification);
    autoWet.setToggleState(audition && autoSide==2,juce::dontSendNotification);
    const auto message=status==Status::armed?"PRESS PLAY":status==Status::recording?
        juce::String("CAPTURING  ")+juce::String(int(processor.autoMixProgress.load()*100))+"%":
        status==Status::captured||status==Status::analyzing?"ANALYZING":
        usable?(result.changes.empty()?"NO CHANGES PROPOSED":"REVIEW PROPOSAL"):
        ready?"CHECK INPUTS":"SELECT LENGTH AND ANALYZE";
    autoStatus.setText(message,juce::dontSendNotification);
    const bool dryTrim=result.dryGainDb < -.05;
    const double trim=dryTrim?result.dryGainDb:result.wetGainDb;
    autoMatch.setButtonText(usable && match && trim < -.05 ?
        juce::String(dryTrim?"A  ":"B  ")+juce::String(trim,1)+" dB / MATCH":"LEVEL MATCH");
    for(auto* button:{&balanceListen,&balanceApply,&balanceSave})button->setEnabled(usable && result.balanceValid);
    balanceReset.setEnabled(true);
    balanceListen.setToggleState(processor.balancePreviewActive.load(),juce::dontSendNotification);
    bool target=processor.balanceTargetValid.load();
    for(size_t i=0;i<3;++i)if(std::abs(result.balanceEdges[i]-processor.balanceTargetEdges[i].load())>.5)target=false;
    for(auto* slider:{&balanceLow,&balanceSplit,&balanceHigh})slider->setEnabled(status==Status::idle);
    if(usable && result.balanceValid && !balanceInitialized) {
        const double current=result.balanceBaseTrimDb;
        const double delta=target?std::clamp(.5*((processor.balanceTargetSub.load()-result.balanceSubDb)+(processor.balanceTargetBody.load()-result.balanceBodyDb)),-3.,3.):0;
        balanceTrim.setValue(std::clamp(current+delta,-12.,12.),juce::dontSendNotification);balanceInitialized=true;
    }
    balanceCaption.setText(processor.balancePreviewActive.load()?"BALANCE TRIM AUDITION / kick unchanged":target?"BALANCE / suggestion relative to saved target":"BALANCE / no target: manual listening trial",juce::dontSendNotification);
    if (!ready || autoResultShown) return;
    autoResultShown=true;
    juce::String report;
    if (result.analysisFailed) {
        autoReport.setText("Analysis could not finish. Close Auto Mix and try again.",false);
        return;
    }
    if (result.unsupportedLength) {
        autoReport.setText("Capture too long for this tempo, meter or sample rate. Choose fewer bars.",false);
        return;
    }
    if (!result.hasBass) report+="No usable bass captured.\n";
    if (!result.hasKick) report+="No usable kick sidechain captured.\n";
    if (usable) {
        if(result.balanceValid) {
            report+="BALANCE CAPTURED BASS OUT / KICK\n";
            report+=juce::String(result.balanceEdges[0],0)+"-"+juce::String(result.balanceEdges[1],0)+" Hz  "+juce::String(result.balanceSubDb,1)+" dB / "+juce::String(result.balanceEdges[1],0)+"-"+juce::String(result.balanceEdges[2],0)+" Hz  "+juce::String(result.balanceBodyDb,1)+" dB\n";
            report+="Virtual K+B peak  "+juce::String(result.sumPeakDb,1)+" dBFS\n";
            report+=target?"Trim suggestion uses your saved target.\n":"No universal target. Save a preferred balance to obtain advice.\n";
        }
        for (const auto& change : result.changes) {
            if (auto* parameter=processor.parameters.getParameter(change.parameter))
                report+=parameter->getName(64)+"   "+parameter->getText(parameter->convertTo0to1(change.value),48)+"\n";
        }
        if (!result.reliableKick) report+="Kick EQ kept / no stable fundamental\n";
        if (!result.duckTimingAvailable) report+="Duck kept / insufficient kick and bass overlap\n";
        if (!result.reliablePhase) report+="Phase kept / no repeatable alignment\n";
        if (result.overlapValid)
            report+="Overlap  "+juce::String(result.overlapBefore,1)+"% to "+juce::String(result.overlapAfter,1)+"%\n";
    }
    const auto monoLine=[&](const juce::String& label,const bassdeamon::StereoPhaseBandReport& band) {
        const auto severity=bassdeamon::monoLossSeverity(band.worst);
        if (!band.worst.active) return label+"  NO SIGNAL\n";
        const char* level=severity==bassdeamon::MonoLossSeverity::high?"HIGH LOSS":
            severity==bassdeamon::MonoLossSeverity::caution?"CAUTION":"LOW LOSS";
        const bool cancellation=band.worst.correlationValid && band.worst.correlation<0;
        return label+"  "+level+" / "+juce::String(band.worst.foldLossDb,1)+" dB / BEAT "+
            juce::String(int(band.worstBeat)+1)+" / "+juce::String(int(band.affectedBeats))+" FLAGGED"+
            (cancellation?" / L/R CANCELLATION":"")+"\n";
    };
    report+="\nBASS INPUT MONO CHECK\n";
    report+=monoLine("FULL BAND",result.mono.fullBand);
    report+=monoLine("SUB < "+juce::String(result.mono.subFrequencyHz,0)+" Hz",result.mono.sub);
    autoReport.setText(report,false);
    autoReport.setCaretPosition(0);
}
