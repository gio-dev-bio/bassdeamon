#include "PluginProcessor.h"
#include "SignalFixtures.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <stdexcept>

namespace {
void parameter(BassDeamonProcessor& p,const char* id,float value){auto* a=p.parameters.getParameter(id);a->setValueNotifyingHost(a->convertTo0to1(value));}
}
int pluginSignalTests(const std::string& path) {
    const std::filesystem::path root(path);std::filesystem::create_directories(root);
    std::ofstream csv(root/"plugin_bypass.csv");
    csv<<"bypass,signal,sample_rate,channels,buffer,samples,bit_differences,max_error,late_max_error\n";csv.precision(12);
    int failures=0;
    for(int sr:{44100,48000,96000})for(int channels:{1,2})for(int block:{1,17,64,127,512,2048}) {
        BassDeamonProcessor p;auto layout=p.getBusesLayout();
        layout.inputBuses.set(0,channels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());layout.outputBuses.set(0,layout.inputBuses[0]);
        layout.inputBuses.set(1,juce::AudioChannelSet::mono());
        if(!p.setBusesLayout(layout))throw std::runtime_error("Signal test layout rejected");
        parameter(p,"phase.on",1);parameter(p,"phase.delay",4.25f);parameter(p,"phase.invert",1);parameter(p,"phase.rotate",1);
        parameter(p,"output.trim",6);parameter(p,"glue.agc",1);parameter(p,"phase.agc",1);parameter(p,"unmask.agc",1);
        p.prepareToPlay(sr,block);
        juce::AudioBuffer<float> buffer(channels+1,block);juce::MidiBuffer midi;
        for(const std::string name:{"impulses","sweep","multitone","white","lowpass_noise","bass_saw","kick_bass","quiet","subnormal","signed_zero","near_fullscale","dc","antiphase","independent"}) {
            auto in=makeSignal(name,sr,block*3);size_t bits=0;double max=0;
            for(int b=0;b<3;++b){
                for(int n=0;n<block;++n){buffer.setSample(0,n,in.l[b*block+n]);if(channels==2)buffer.setSample(1,n,in.r[b*block+n]);buffer.setSample(channels,n,in.k[b*block+n]);}
                p.processBlockBypassed(buffer,midi);
                for(int c=0;c<channels;++c)for(int n=0;n<block;++n){const float x=c?in.r[b*block+n]:in.l[b*block+n],y=buffer.getSample(c,n);bits+=std::memcmp(&x,&y,4)!=0;max=std::max(max,std::abs(static_cast<double>(x)-y));}
            }
            csv<<"host,"<<name<<','<<sr<<','<<channels<<','<<block<<','<<block*3*channels<<','<<bits<<','<<max<<",0\n";
            if(bits||max||p.getLatencySamples()!=0)++failures;
        }
    }
    // Internal GUI ON switch: distinguish a smoothed transition from a hard host bypass.
    for(const std::string name:{"multitone","quiet","subnormal","signed_zero"})for(bool startOff:{true,false}) {
        constexpr int sr=48000,block=128,count=sr*2;
        BassDeamonProcessor p;parameter(p,"phase.on",1);parameter(p,"phase.delay",4.25f);parameter(p,"phase.invert",1);parameter(p,"phase.rotate",1);
        parameter(p,"output.trim",6);parameter(p,"glue.agc",1);parameter(p,"phase.agc",1);parameter(p,"unmask.agc",1);
        parameter(p,"global.on",startOff?0:1);p.prepareToPlay(sr,block);
        auto in=makeSignal(name,sr,count+sr);juce::AudioBuffer<float> buffer(2,block);juce::MidiBuffer midi;
        size_t bits=0;double max=0,late=0;std::ofstream raw;
        if(!startOff&&name=="multitone")raw.open(root/"global_transition.f32",std::ios::binary);
        for(int index=0;index<count;index+=block){
            if(!startOff&&index>=sr)parameter(p,"global.on",0);
            for(int n=0;n<block;++n){buffer.setSample(0,n,in.l[index+n]);buffer.setSample(1,n,in.r[index+n]);}
            p.processBlock(buffer,midi);
            const bool bypass=startOff||index>=sr;
            for(int n=0;n<block;++n){
                for(int c=0;c<2;++c){const float x=c?in.r[index+n]:in.l[index+n],y=buffer.getSample(c,n);if(bypass){const double d=std::abs(static_cast<double>(x)-y);max=std::max(max,d);bits+=std::memcmp(&x,&y,4)!=0;if(index+n>=(startOff?0:sr)+sr/2)late=std::max(late,d);}}
                if(raw){float row[]{in.l[index+n],buffer.getSample(0,n)};raw.write(reinterpret_cast<char*>(row),sizeof(row));}
            }
        }
        csv<<(startOff?"global_initial":"global_transition")<<','<<name<<",48000,2,128,"<<(startOff?count:count-sr)*2<<','<<bits<<','<<max<<','<<late<<'\n';
        // The new finite global ramp must reach exact dry after 20 ms, including IEEE edge values.
        if(startOff&&bits)++failures;
        if(late!=0||!std::isfinite(max))++failures;
    }
    // Deliberately switch at a nonzero sample to expose possible hard-bypass clicks.
    for(bool host:{true,false}) {
        constexpr int sr=48000,block=128,count=sr*2,edge=384*128;
        BassDeamonProcessor p;parameter(p,"glue.on",0);parameter(p,"unmask.on",0);
        parameter(p,"phase.on",1);parameter(p,"phase.invert",1);p.prepareToPlay(sr,block);
        auto in=makeSignal("tone80",sr,count);juce::AudioBuffer<float> buffer(2,block);juce::MidiBuffer midi;
        std::ofstream raw(root/(host?"host_switch_80.f32":"global_switch_80.f32"),std::ios::binary);
        for(int index=0;index<count;index+=block){
            for(int n=0;n<block;++n){buffer.setSample(0,n,in.l[index+n]);buffer.setSample(1,n,in.r[index+n]);}
            if(host&&index>=edge)p.processBlockBypassed(buffer,midi);
            else{if(!host&&index>=edge)parameter(p,"global.on",0);p.processBlock(buffer,midi);}
            for(int n=0;n<block;++n){float row[]{in.l[index+n],buffer.getSample(0,n)};raw.write(reinterpret_cast<char*>(row),sizeof(row));}
        }
    }
    return failures;
}
