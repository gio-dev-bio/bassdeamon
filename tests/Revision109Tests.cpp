#include "PluginEditor.h"
#include <stdexcept>
#include <cstring>
#include <iostream>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);} }
void revision109Tests(){
    BassDeamonProcessor a,b;
    const auto set=[](BassDeamonProcessor& p,const char* id,float v){auto* x=p.parameters.getParameter(id);x->setValueNotifyingHost(x->convertTo0to1(v));};
    for(auto* p:{&a,&b}){set(*p,"glue.on",0);set(*p,"unmask.on",0);set(*p,"phase.on",1);set(*p,"phase.angle",73);p->prepareToPlay(48000,128);}
    a.acceptKickReference(48);
    check(a.parameters.getRawParameterValue("unmask.kickReferenceHz")->load()==0,"Scope acquisition changed live Auto Focus settings before Apply");
    check(a.phaseReferenceHz.load()==48 && a.phaseProcessingReferenceHz.load()==60,"Auto reference retuned physical Phase reference");
    a.phaseReferenceAutomatic.store(false);a.acceptKickReference(92);
    check(a.phaseReferenceHz.load()==48,"Automatic learn overrode manual reference");
    a.phaseReferenceAutomatic.store(true);a.acceptKickReference(0);check(a.phaseReferenceHz.load()==48,"Invalid kick root replaced reference");
    juce::AudioBuffer<float> x(2,128),y(2,128);juce::MidiBuffer midi;
    for(int block=0;block<80;++block){for(int c=0;c<2;++c)for(int n=0;n<128;++n)x.setSample(c,n,float(.2*std::sin(2*bassdeamon::pi*73*(block*128+n)/48000)));y.makeCopyOf(x);a.processBlock(x,midi);b.processBlock(y,midi);for(int c=0;c<2;++c)check(std::memcmp(x.getReadPointer(c),y.getReadPointer(c),128*sizeof(float))==0,"Scope root acquisition changed output audio");}
    for(double hz:{35.,48.,92.,125.})for(double degrees:{-175.,-73.,-10.,10.,73.,175.}){
        const double display=bassdeamon::phaseDegreesAtReference(degrees,60,hz,48000);
        check(std::abs(bassdeamon::phaseDegreesAtReference(display,hz,60,48000)-degrees)<1e-6,"Degree coordinate conversion is not reversible");
    }
    a.applyBalanceTrim(-2.5f);check(a.parameters.getRawParameterValue("balance.trim")->load()==-2.5f,"Balance apply failed");
    check(a.undoEdit()&&a.parameters.getRawParameterValue("balance.trim")->load()==0,"Balance trim is not undoable");
    a.applyBalanceTrim(-1.5f);juce::MemoryBlock state;a.getStateInformation(state);b.setStateInformation(state.getData(),int(state.getSize()));
    check(b.phaseReferenceHz.load()==48 && b.phaseProcessingReferenceHz.load()==60,"Scope/physical references did not survive state recall");
    check(b.parameters.getRawParameterValue("balance.trim")->load()==-1.5f,"Balance trim did not survive state recall");
    std::cout<<"PASS Phase automatic root audio null / manual override / invalid root / angle coordinates / Balance apply Undo recall\n";
}
