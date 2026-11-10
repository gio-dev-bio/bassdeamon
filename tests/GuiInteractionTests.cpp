#include "PluginEditor.h"
#include <iostream>
#include <random>
#include <cstring>
#include <stdexcept>

namespace {
void ensure(bool v,const char* m){if(!v)throw std::runtime_error(m);}
class TestPlayHead final:public juce::AudioPlayHead {
public:
    double time=0,bpm=120;bool playing=true;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo info;info.setBpm(bpm);info.setTimeInSeconds(time);info.setPpqPosition(time*bpm/60);info.setIsPlaying(playing);return info;
    }
};
}
int interactionTests(){
    // Changing the display must leave every output sample and every audio
    // parameter untouched, including adaptive processors and Auto Gain.
    {
        BassDeamonProcessor a,b;
        const auto configure=[](BassDeamonProcessor& processor){
            auto l=processor.getBusesLayout();l.inputBuses.set(1,juce::AudioChannelSet::mono());ensure(processor.setBusesLayout(l),"Null test layout");
            for(const char* id:{"glue.on","phase.on","unmask.on","tube.on","glue.agc","phase.agc","unmask.agc"})processor.parameters.getParameter(id)->setValueNotifyingHost(1);
            auto* delay=processor.parameters.getParameter("phase.delay");delay->setValueNotifyingHost(delay->convertTo0to1(1.375f));processor.prepareToPlay(48000,256);
        };configure(a);configure(b);
        std::unique_ptr<BassDeamonEditor> gui(static_cast<BassDeamonEditor*>(a.createEditor()));
        const auto before=a.parameters.copyState().toXmlString();
        juce::AudioBuffer<float> x(3,256),y(3,256);juce::MidiBuffer midi;
        for(int block=0;block<360;++block){
            if(block%2)gui->spectrumPre.onClick();else gui->spectrumPost.onClick();
            for(int n=0;n<256;++n){const double t=(block*256+n)/48000.;const float bass=float(.3*std::sin(2*bassdeamon::pi*55*t)+.08*std::sin(2*bassdeamon::pi*110*t));
                x.setSample(0,n,bass);x.setSample(1,n,bass*.85f);x.setSample(2,n,float(.5*std::exp(-std::fmod(t,.5)*25)*std::sin(2*bassdeamon::pi*57*t)));}
            y.makeCopyOf(x);a.processBlock(x,midi);b.processBlock(y,midi);
            for(int c=0;c<2;++c)ensure(std::memcmp(x.getReadPointer(c),y.getReadPointer(c),256*sizeof(float))==0,"PRE/POST click changes actual audio");
            if(block%6==0)gui->timerCallback();
        }
        ensure(a.parameters.copyState().toXmlString()==before,"PRE/POST modified host parameters");
        a.setSpectrumView(0);juce::MemoryBlock state;a.getStateInformation(state);b.setStateInformation(state.getData(),int(state.getSize()));
        ensure(b.spectrumView()==0,"Local visual view not recalled");
        std::cout<<"PASS PRE/POST null test: 184320 identical audio samples across 360 real button clicks, with active processors and AGC; host parameters unchanged\n";
    }
    BassDeamonProcessor p;auto layout=p.getBusesLayout();
    layout.inputBuses.set(0,juce::AudioChannelSet::mono());layout.outputBuses.set(0,juce::AudioChannelSet::mono());layout.inputBuses.set(1,juce::AudioChannelSet::mono());
    ensure(p.setBusesLayout(layout),"GUI test layout");p.prepareToPlay(48000,256);
    TestPlayHead host;p.setPlayHead(&host);
    const auto set=[&](const char* id,float value){auto* param=p.parameters.getParameter(id);param->setValueNotifyingHost(param->convertTo0to1(value));};
    const auto get=[&](const char* id){return p.parameters.getRawParameterValue(id)->load();};
    std::unique_ptr<BassDeamonEditor> editor(static_cast<BassDeamonEditor*>(p.createEditor()));
    // Real JUCE gestures and APVTS undo, including intermediate drag values.
    p.refreshHistory();p.undoManager.clearUndoHistory();
    const float originalDrive=get("tube.drive");
    juce::ParameterAttachment drive(*p.parameters.getParameter("tube.drive"),[](float){},&p.undoManager);
    drive.beginGesture();
    for(float value:{3.f,8.f,13.f}){drive.setValueAsPartOfGesture(value);p.refreshHistory();}
    drive.endGesture();p.refreshHistory();editor->timerCallback();
    ensure(editor->undoButton.isEnabled(),"Undo button disabled after gesture");
    editor->undoButton.onClick();ensure(std::abs(get("tube.drive")-originalDrive)<.001,"Undo did not group entire knob gesture");
    ensure(editor->redoButton.isEnabled(),"Redo unavailable after undo");
    editor->redoButton.onClick();ensure(std::abs(get("tube.drive")-13)<.001,"Redo did not restore knob value");
    editor->undoButton.onClick();drive.setValueAsCompleteGesture(5);p.refreshHistory();
    ensure(!p.canRedo(),"New edit failed to discard redo branch");
    drive.setValueAsCompleteGesture(7);drive.setValueAsCompleteGesture(9);
    ensure(p.undoEdit()&&std::abs(get("tube.drive")-7)<.001,"Rapid consecutive gestures were merged");
    ensure(p.undoEdit()&&std::abs(get("tube.drive")-5)<.001,"Rapid gesture undo lost previous value");
    const auto beforePreset=p.parameters.copyState().toXmlString();
    p.loadStartingPoint(3);const auto afterPreset=p.parameters.copyState().toXmlString();
    ensure(beforePreset!=afterPreset&&p.undoEdit(),"Starting point not undoable");
    ensure(p.parameters.copyState().toXmlString()==beforePreset,"Undo did not restore complete preset state");
    ensure(p.redoEdit()&&p.parameters.copyState().toXmlString()==afterPreset,"Preset redo incomplete");
    p.loadStartingPoint(0);p.refreshHistory();p.undoManager.clearUndoHistory();
    juce::MemoryBlock cleanState;p.getStateInformation(cleanState);
    drive.setValueAsCompleteGesture(12);p.refreshHistory();ensure(p.canUndo(),"History lost edit");
    p.setStateInformation(cleanState.getData(),int(cleanState.getSize()));p.refreshHistory();
    ensure(!p.canUndo()&&!p.canRedo(),"State recall retained stale undo history");
    juce::AudioProcessLoadMeasurer calibration;calibration.reset(48000,480);
    for(int n=0;n<100;++n)calibration.registerRenderTime(5,480);
    ensure(std::abs(calibration.getLoadAsPercentage()-50)<.01,"CPU meter denominator/calibration wrong");
    calibration.registerRenderTime(20,480);ensure(calibration.getXRunCount()==1,"CPU over-budget count missing");
    calibration.reset();ensure(calibration.getLoadAsPercentage()==0&&calibration.getXRunCount()==0,"CPU reset failed");
    ensure(editor->attenuationKnob->isVisible()&&!editor->depthKnob->isVisible(),"dB knob not default");
    editor->attenuationKnob->slider.setValue(11.7,juce::sendNotificationSync);
    ensure(std::abs(get("unmask.attenuation")-11.7f)<.001,"Typed attenuation does not reach DSP parameter");
    ensure(editor->attenuationKnob->slider.getTextFromValue(11.7).contains("-11.7"),"Attenuation readout missing minus sign");
    ensure(std::abs(editor->attenuationKnob->slider.getValueFromText("-9.3 dB")-9.3)<.001,"Negative dB typing parsed incorrectly");
    ensure(p.undoEdit()&&std::abs(get("unmask.attenuation")-6)<.001,"Attenuation undo failed");
    ensure(p.redoEdit()&&std::abs(get("unmask.attenuation")-11.7f)<.001,"Attenuation redo failed");
    ensure(editor->showSum,"SUM must be default");ensure(get("view.range")==0,"Spectrum must default 0-1000");
    ensure(editor->face.getComponentAt(editor->sumButton.getBounds().getCentre())==&editor->sumButton,"SUM hit target covered");
    const auto stateBeforeSum=p.parameters.copyState().toXmlString();
    editor->sumButton.onClick();
    ensure(!editor->showSum&&!editor->sumButton.getToggleState(),"SUM OFF must hide the sum graph");
    editor->sumButton.onClick();
    ensure(editor->showSum&&editor->sumButton.getToggleState(),"SUM ON must restore the sum graph");
    ensure(p.parameters.copyState().toXmlString()==stateBeforeSum,"SUM changed audio parameters");
    ensure(editor->spectrumIsPost(),"Spectrum must default to POST");
    for(auto* button:{&editor->spectrumPre,&editor->spectrumPost})
        ensure(editor->face.getComponentAt(button->getBounds().getCentre())==button,"Spectrum selector hit target covered");
    // Arbitrary typed values, not the former fixed frequency presets.
    editor->spectrumMin.setValue(37.5,juce::sendNotificationSync);
    editor->spectrumMax.setValue(437.25,juce::sendNotificationSync);
    const auto limits=editor->spectrumLimits();
    ensure(std::abs(limits[0]-37.5)<.01&&std::abs(limits[1]-437.25)<.01,"Arbitrary spectrum limits not attached");
    editor->visualPre.fill(-120);editor->visualPost.fill(-120);editor->visualKick.fill(-120);
    const size_t overlapBin=size_t(std::round(75.*bassdeamon::RelationAnalyzer::fftSize/48000.));
    editor->visualPre[overlapBin]=editor->visualPost[overlapBin]=editor->visualKick[overlapBin]=-12;
    const auto redInSpectrum=[&]{
        const auto image=editor->face.createComponentSnapshot({72,190,742,160});int red=0;
        for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x){const auto c=image.getPixelAt(x,y);
            if(c.getRed()>c.getGreen()*1.6&&c.getRed()>c.getBlue()*1.6&&c.getRed()>50)++red;
        }
        return red;
    };
    // Select via real buttons, then inject deterministic analysis levels to
    // exercise the renderer independently of its smoothing/FFT settling time.
    editor->spectrumPre.onClick();
    editor->visualPre[overlapBin]=editor->visualKick[overlapBin]=-12;
    editor->analyzer.maskPre.fill(0);editor->analyzer.maskPre[overlapBin]=1;
    ensure(!editor->spectrumIsPost()&&editor->spectrumPre.getToggleState(),"PRE selector did not switch view");
    ensure(redInSpectrum()>20,"PRE collision bands missing");
    editor->spectrumPost.onClick();editor->visualPost[overlapBin]=editor->visualKick[overlapBin]=-12;
    editor->analyzer.maskPost.fill(0);editor->analyzer.maskPost[overlapBin]=1;
    ensure(editor->spectrumIsPost()&&editor->spectrumPost.getToggleState(),"POST selector did not switch view");
    ensure(redInSpectrum()>20,"POST collision bands missing");
    const auto lower=editor->face.createComponentSnapshot({72,240,742,27});int lowerRed=0;
    for(int y=0;y<lower.getHeight();++y)for(int x=0;x<lower.getWidth();++x){const auto colour=lower.getPixelAt(x,y);
        if(colour.getRed()>colour.getGreen()*1.6&&colour.getRed()>colour.getBlue()*1.6&&colour.getRed()>50)++lowerRed;}
    ensure(lowerRed<150,"Collision indicator filled unrelated lower spectrum");
    editor->visualPost.fill(-120);editor->analyzer.maskPost.fill(0);
    ensure(redInSpectrum()==0,"POST collision bands incorrectly use PRE");
    BassDeamonEditor::UserToggle* added=nullptr;
    for(auto& sw:editor->switches)if(sw->button.getButtonText()=="ADDED ONLY")added=&sw->button;
    ensure(added!=nullptr,"Missing ADDED ONLY GUI control");
    ensure(editor->face.getComponentAt(added->getBounds().getCentre())==added,"ADDED ONLY hit target covered");
    // Merely opening the GUI must not make automation or state recall turn
    // Tube on, clear DELTA, or produce a different sound from a closed editor.
    set("tube.audition",1);
    ensure(get("tube.on")==0,"GUI notification silently enabled automated Tube");
    set("tube.audition",0);
    const auto clickAdded=[&]{added->internalClickCallback(juce::ModifierKeys{});};
    set("unmask.delta",1);p.refreshHistory();clickAdded();p.refreshHistory();
    ensure(get("tube.audition")==1,"GUI click did not set Tube audition");
    ensure(get("tube.on")==1&&get("unmask.delta")==0,"Tube audition did not enable generator and clear DELTA");
    ensure(p.undoEdit()&&get("tube.audition")==0&&get("tube.on")==0&&get("unmask.delta")==1,"Compound monitor edit did not undo together");
    ensure(p.redoEdit()&&get("tube.audition")==1&&get("tube.on")==1&&get("unmask.delta")==0,"Compound monitor redo incomplete");
    set("glue.on",0);set("unmask.on",0);set("tube.drive",18);set("tube.amount",1);
    juce::AudioBuffer<float> buffer(2,256);juce::MidiBuffer midi;double output=0;
    // End-to-end: actual processor -> SPSC -> FFT -> PRE/POST -> red pixels.
    // No manually assigned FFT levels/maps in this regression test.
    juce::MemoryBlock audioState;p.getStateInformation(audioState);
    set("tube.on",0);set("tube.audition",0);set("unmask.on",0);set("phase.on",0);set("glue.on",0);
    editor->analyzer.prepare(48000);bassdeamon::AnalysisFrame discarded;while(p.analysisQueue.pop(discarded)){}
    int preHits=0,postHits=0;bool prePainted=false,postPainted=false;
    for(int b=0;b<240;++b){for(int n=0;n<256;++n){const double t=(b*256+n)/48000.,since=std::fmod(t,.5);
        buffer.setSample(0,n,float(.3*std::sin(2*bassdeamon::pi*75*t)+.12*std::sin(2*bassdeamon::pi*150*t)));
        const double envelope=since<.020?std::pow(std::sin(bassdeamon::pi*since/.020),2):0;
        buffer.setSample(1,n,float(.012*envelope*std::sin(2*bassdeamon::pi*75*t)));}
        host.time=b*256/48000.;p.processBlock(buffer,midi);
        if(b%6==0){editor->timerCallback();
            if(editor->analyzer.summary(false).regions){++preHits;editor->spectrumPre.onClick();prePainted|=redInSpectrum()>10;}
            if(editor->analyzer.summary(true).regions){++postHits;editor->spectrumPost.onClick();postPainted|=redInSpectrum()>10;}
            const auto unrelated=size_t(std::round(150.*bassdeamon::RelationAnalyzer::fftSize/48000.));
            ensure(editor->analyzer.maskPre[unrelated]<.12f&&editor->analyzer.maskPost[unrelated]<.12f,"Real GUI marks bass-only harmonic");
        }
    }
    ensure(preHits>0&&postHits>0&&prePainted&&postPainted,"Real short/quiet kick has no visible PRE/POST collisions");
    p.setStateInformation(audioState.getData(),int(audioState.getSize()));editor->timerCallback();
    for(int b=0;b<200;++b){host.time=b*256/48000.;for(int n=0;n<256;++n){buffer.setSample(0,n,float(.6*std::sin(2*bassdeamon::pi*75*(b*256+n)/48000.)));buffer.setSample(1,n,0);}
        p.processBlock(buffer,midi);if(b>100)output+=buffer.getRMSLevel(0,0,256);editor->timerCallback();}
    ensure(std::isfinite(p.cpuLoad.getLoadAsPercentage())&&p.cpuLoad.getLoadAsPercentage()>0,"Real callback CPU not measured");
    ensure(editor->cpuLabel.getText().contains("DSP CPU")&&editor->cpuLabel.getText().contains("ANALYZER"),"CPU display missing");
    ensure(output>1,"GUI audition path has no added audio");ensure(p.tubeAddedDb.load()>-50,"GUI added signal meter missing");
    const auto greenInScope=[&]{
        const auto image=editor->face.createComponentSnapshot({48,446,1104,75});
        int count=0;
        for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x){const auto c=image.getPixelAt(x,y);
            if(c.getGreen()>c.getRed()*1.4&&c.getGreen()>c.getBlue()*1.05)++count;
        }
        return count;
    };
    ensure(greenInScope()>10,"SUM ON did not draw actual POST waveform");
    editor->sumButton.onClick();
    ensure(greenInScope()==0,"SUM OFF left waveform visible");
    editor->sumButton.onClick();
    ensure(greenInScope()>10,"SUM ON did not restore waveform");
    clickAdded();set("tube.on",0);
    // Test real parameter attachments, not a decorative timing display.
    for(auto& choice:editor->choices)if(choice->box.getComponentID()=="glue.timing")choice->box.setSelectedId(3,juce::sendNotificationSync);
    ensure(get("glue.timing")==2,"GLUE SYNC selector attachment");
    set("glue.attackMs",17);set("unmask.timing",1);set("unmask.attack",7);
    set("glue.releaseNote",4);host.bpm=90;buffer.clear();p.processBlock(buffer,midi);editor->timerCallback();
    ensure(std::abs(p.glueRelease.load()-60000.f/90)<.01,"GLUE timing ignores host tempo");
    ensure(std::abs(p.glueAttack.load()-17)<.01&&std::abs(p.unmaskAttack.load()-7)<.01,"SYNC changed manual attack");
    ensure(editor->glueAttackSlider.isVisible()&&editor->unmaskAttackKnob->isVisible(),"SYNC hid millisecond attack controls");
    ensure(!editor->glueReleaseSlider.isVisible()&&!editor->unmaskReleaseKnob->isVisible(),"SYNC retained manual release controls");
    bool glueRelease=false,unmaskRelease=false;
    for(auto& choice:editor->choices){const auto id=choice->box.getComponentID();
        ensure(id!="glue.attackNote"&&id!="unmask.attackNote","Attack note selector still present");
        if(id=="glue.releaseNote")glueRelease=choice->box.isVisible();
        if(id=="unmask.releaseNote")unmaskRelease=choice->box.isVisible();
    }
    ensure(glueRelease&&unmaskRelease,"SYNC release note selectors missing");
    // View selection and exact limits survive save/recall.
    p.setSpectrumView(0);juce::MemoryBlock viewState;p.getStateInformation(viewState);
    p.setSpectrumView(1);set("view.maxHz",1000);
    p.setStateInformation(viewState.getData(),int(viewState.getSize()));
    ensure(p.spectrumView()==0&&std::abs(get("view.maxHz")-437.25)<.01,"Spectrum view state recall failed");
    set("glue.timing",0);set("unmask.delta",0);
    ensure(editor->face.getComponentAt(editor->learn.getBounds().getCentre())==&editor->learn,"Learn hit target covered");
    const float delayBeforeLearn=get("phase.delay"),invertBeforeLearn=get("phase.invert");
    editor->learn.onClick();
    std::mt19937 rng(79);std::uniform_real_distribution<float> random(-.4f,.4f);
    std::vector<float> noise(4*48000+256);for(auto& v:noise)v=random(rng);
    for(int pos=0;pos<4*48000;pos+=256){host.time=std::fmod(pos/48000.,.25); // short Cubase-style cycle
        for(int n=0;n<256;++n){buffer.setSample(0,n,noise[size_t(pos+n)]);buffer.setSample(1,n,pos+n>=96?-noise[size_t(pos+n-96)]:0);}
        p.processBlock(buffer,midi);editor->timerCallback();}
    ensure(p.learnStatus.load()==5,"Learn GUI never completed across short host cycles");
    p.timerCallback();
    ensure(std::abs(get("phase.delay")-2)<.26&&get("phase.invert")==1,"Learn did not apply delay and polarity to host parameters");
    ensure(get("phase.on")==0,"Learn silently enabled PHASE");
    ensure(p.undoEdit()&&std::abs(get("phase.delay")-delayBeforeLearn)<.001&&get("phase.invert")==invertBeforeLearn,"Learn undo incomplete");
    ensure(p.redoEdit()&&std::abs(get("phase.delay")-2)<.26&&get("phase.invert")==1,"Learn redo incomplete");
    // Schema-3 sessions restore the original timing modes on an existing instance.
    auto old=p.parameters.copyState();old.setProperty("schemaVersion",3,nullptr);
    const juce::StringArray newIds{"glue.timing","glue.attackMs","glue.releaseMs","glue.attackNote","glue.releaseNote","unmask.timing","unmask.attackNote","unmask.releaseNote","view.range","view.hold","view.spectrum","view.minHz","view.maxHz"};
    for(int i=old.getNumChildren()-1;i>=0;--i)if(newIds.contains(old.getChild(i).getProperty("id").toString()))old.removeChild(i,nullptr);
    set("glue.timing",2);set("unmask.timing",1);juce::MemoryBlock bytes;auto xml=old.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,bytes);
    p.setStateInformation(bytes.getData(),int(bytes.getSize()));ensure(get("glue.timing")==0&&get("unmask.timing")==0,"Old session did not reset new timing modes");
    ensure(get("unmask.controlDb")==0,"Old session silently switched attenuation algorithms");
    std::cout<<"PASS undo/redo gestures, presets, recall / calibrated CPU / real PRE+POST collision pixels / real GUI attachments / Tube audition audio / host timing / Learn across short cycles / legacy timing\n";
    return 0;
}
