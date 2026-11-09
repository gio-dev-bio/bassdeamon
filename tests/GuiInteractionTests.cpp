#include "PluginEditor.h"
#include <iostream>
#include <random>
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
    BassDeamonProcessor p;auto layout=p.getBusesLayout();
    layout.inputBuses.set(0,juce::AudioChannelSet::mono());layout.outputBuses.set(0,juce::AudioChannelSet::mono());layout.inputBuses.set(1,juce::AudioChannelSet::mono());
    ensure(p.setBusesLayout(layout),"GUI test layout");p.prepareToPlay(48000,256);
    TestPlayHead host;p.setPlayHead(&host);
    const auto set=[&](const char* id,float value){auto* param=p.parameters.getParameter(id);param->setValueNotifyingHost(param->convertTo0to1(value));};
    const auto get=[&](const char* id){return p.parameters.getRawParameterValue(id)->load();};
    std::unique_ptr<BassDeamonEditor> editor(static_cast<BassDeamonEditor*>(p.createEditor()));
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
        const auto image=editor->face.createComponentSnapshot({72,190,742,88});int red=0;
        for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x){const auto c=image.getPixelAt(x,y);
            if(c.getRed()>c.getGreen()*1.6&&c.getRed()>c.getBlue()*1.6&&c.getRed()>50)++red;
        }
        return red;
    };
    // Select via real buttons, then inject deterministic analysis levels to
    // exercise the renderer independently of its smoothing/FFT settling time.
    editor->spectrumPre.onClick();
    editor->visualPre[overlapBin]=editor->visualKick[overlapBin]=-12;
    ensure(!editor->spectrumIsPost()&&editor->spectrumPre.getToggleState(),"PRE selector did not switch view");
    ensure(redInSpectrum()>20,"PRE collision bands missing");
    editor->spectrumPost.onClick();editor->visualPost[overlapBin]=editor->visualKick[overlapBin]=-12;
    ensure(editor->spectrumIsPost()&&editor->spectrumPost.getToggleState(),"POST selector did not switch view");
    ensure(redInSpectrum()>20,"POST collision bands missing");
    editor->visualPost.fill(-120);
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
    set("unmask.delta",1);clickAdded();
    ensure(get("tube.audition")==1,"GUI click did not set Tube audition");
    ensure(get("tube.on")==1&&get("unmask.delta")==0,"Tube audition did not enable generator and clear DELTA");
    set("glue.on",0);set("unmask.on",0);set("tube.drive",18);set("tube.amount",1);
    juce::AudioBuffer<float> buffer(2,256);juce::MidiBuffer midi;double output=0;
    for(int b=0;b<200;++b){host.time=b*256/48000.;for(int n=0;n<256;++n){buffer.setSample(0,n,float(.6*std::sin(2*bassdeamon::pi*75*(b*256+n)/48000.)));buffer.setSample(1,n,0);}
        p.processBlock(buffer,midi);if(b>100)output+=buffer.getRMSLevel(0,0,256);editor->timerCallback();}
    ensure(output>1,"GUI audition path has no added audio");ensure(p.tubeAddedDb.load()>-50,"GUI added signal meter missing");
    const auto greenInScope=[&]{
        const auto image=editor->face.createComponentSnapshot({48,374,1104,75});
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
    set("view.spectrum",0);juce::MemoryBlock viewState;p.getStateInformation(viewState);
    set("view.spectrum",1);set("view.maxHz",1000);
    p.setStateInformation(viewState.getData(),int(viewState.getSize()));
    ensure(get("view.spectrum")==0&&std::abs(get("view.maxHz")-437.25)<.01,"Spectrum view state recall failed");
    set("glue.timing",0);set("unmask.delta",0);
    ensure(editor->face.getComponentAt(editor->learn.getBounds().getCentre())==&editor->learn,"Learn hit target covered");
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
    // Schema-3 sessions restore the original timing modes on an existing instance.
    auto old=p.parameters.copyState();old.setProperty("schemaVersion",3,nullptr);
    const juce::StringArray newIds{"glue.timing","glue.attackMs","glue.releaseMs","glue.attackNote","glue.releaseNote","unmask.timing","unmask.attackNote","unmask.releaseNote","view.range","view.hold","view.spectrum","view.minHz","view.maxHz"};
    for(int i=old.getNumChildren()-1;i>=0;--i)if(newIds.contains(old.getChild(i).getProperty("id").toString()))old.removeChild(i,nullptr);
    set("glue.timing",2);set("unmask.timing",1);juce::MemoryBlock bytes;auto xml=old.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,bytes);
    p.setStateInformation(bytes.getData(),int(bytes.getSize()));ensure(get("glue.timing")==0&&get("unmask.timing")==0,"Old session did not reset new timing modes");
    std::cout<<"PASS real GUI attachments / Tube audition audio / host timing / Learn across short cycles / legacy timing\n";
    return 0;
}
