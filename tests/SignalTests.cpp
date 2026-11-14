#include "Dsp.h"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

using bassdeamon::Engine;
using bassdeamon::Settings;
constexpr double pi = bassdeamon::pi;
#include "SignalFixtures.h"
Signal render(const Signal& in,int sr,int block,const std::string& mode,bool variable=false) {
    Engine e;e.prepare(sr);Settings s;
    s.glue=s.phase=s.unmask=false;
    if(mode=="glue")s.glue=true;
    if(mode=="glue_zero"){s.glue=true;s.glueAmount=0;}
    if(mode=="unmask_zero"){s.unmask=true;s.unmaskAmount=0;}
    if(mode=="phase_neutral")s.phase=true;
    if(mode=="unmask"||mode=="no_kick")s.unmask=true;
    if(mode=="phase"){s.phase=true;s.delayMs=2.125;s.rotate=true;s.invert=true;}
    if(mode=="all"||mode=="stress"||mode=="active_to_off"){
        s.glue=s.phase=s.unmask=true;s.delayMs=1.25;s.rotate=true;
        if(mode=="stress")for(auto& g:s.agc)g.enabled=true;
    }
    if(mode=="delta") {s.unmask=true;s.delta=true;}
    Signal out=in;
    int index=0;
    while(index<static_cast<int>(in.l.size())) {
        int n=std::min(variable?1+(index*13+7)%511:block,static_cast<int>(in.l.size())-index);
        // Identical sample boundary is required for transition comparisons.
        if(mode=="active_to_off"&&index<sr)n=std::min(n,sr-index);
        if(mode=="active_to_off"&&index>=sr){s.glue=s.phase=s.unmask=s.delta=false;s.outputDb=0;for(auto& g:s.agc)g.enabled=false;}
        if(mode=="stress") {
            const bool on=(index/(sr/4))%2;
            s.unmaskWidth=on?1:0;s.unmaskFrequency=on?250:30;s.unmaskTrack=false;
            s.glueAmount=on?1:0;s.unmaskAmount=on?1:0;
            s.unmaskAttackMs=on?1:50;s.unmaskReleaseMs=on?30:500;
        }
        float* output[]{out.l.data()+index,out.r.data()+index};
        const float* side[]{in.k.data()+index};
        e.process(output,2,mode=="no_kick"?nullptr:side,mode=="no_kick"?0:1,n,s,120);
        index+=n;
    }
    return out;
}
bool equalBits(float a,float b){return std::memcmp(&a,&b,sizeof(a))==0;}
int main(int argc,char** argv) {
    const std::filesystem::path root=argc>1?argv[1]:"signal-results";std::filesystem::create_directories(root);
    std::ofstream csv(root/"signals.csv");csv<<"test,signal,sample_rate,buffer,samples,nonfinite,max_error,rms_error,bit_differences,peak,post_silence_peak\n";
    csv.precision(12);int failures=0,cases=0;
    const auto record=[&](const std::string& test,const std::string& name,int sr,int block,const Signal& ref,const Signal& out,bool identity){
        double max=0,sum=0,peak=0,tail=0;size_t nonfinite=0,bits=0;
        for(size_t n=0;n<out.l.size();++n)for(int c=0;c<2;++c) {
            const float x=c?ref.r[n]:ref.l[n],y=c?out.r[n]:out.l[n];
            nonfinite+=!std::isfinite(y);bits+=!equalBits(x,y);
            const double d=static_cast<double>(y)-x;max=std::max(max,std::abs(d));sum+=d*d;peak=std::max(peak,std::abs(static_cast<double>(y)));
            if(n>=static_cast<size_t>(3*sr))tail=std::max(tail,std::abs(static_cast<double>(y)));
        }
        csv<<test<<','<<name<<','<<sr<<','<<block<<','<<out.l.size()*2<<','<<nonfinite<<','<<max<<','<<std::sqrt(sum/(out.l.size()*2))<<','<<bits<<','<<peak<<','<<tail<<'\n';
        failures+=nonfinite!=0||(identity&&max!=0);++cases;
    };
    const std::vector<std::string> names{"impulses","sweep","multitone","white","lowpass_noise","bass_saw","kick_bass","quiet","subnormal","signed_zero","near_fullscale","dc","antiphase","independent"};
    for(int sr:{44100,48000,96000})for(const auto& name:names){
        const auto in=makeSignal(name,sr,sr/3);
        for(int block:{1,17,64,127,512,2048})record("modules_off",name,sr,block,in,render(in,sr,block,"off"),true);
    }
    for(int sr:{44100,48000,96000})for(const auto& name:names){
        auto in=makeSignal(name,sr,4*sr);
        // Intentional abrupt stop stresses filter tails; source discontinuity is not counted as a plugin click.
        for(size_t n=2*sr;n<in.l.size();++n)in.l[n]=in.r[n]=in.k[n]=0;
        for(const std::string mode:{"glue","unmask","phase","all","stress","no_kick","active_to_off","glue_zero","unmask_zero","phase_neutral"}){
            const auto out=render(in,sr,127,mode);
            record(mode,name,sr,127,in,out,mode=="no_kick"||mode=="glue_zero"||mode=="unmask_zero"||mode=="phase_neutral");
            if(sr==48000&&(name=="multitone"||name=="sweep"||name=="quiet"||name=="antiphase"||name=="white")){
                std::ofstream f(root/(name+"_"+mode+".f32"),std::ios::binary);
                for(size_t n=0;n<out.l.size();++n){float row[]{in.l[n],in.r[n],out.l[n],out.r[n],in.k[n]};f.write(reinterpret_cast<char*>(row),sizeof(row));}
            }
        }
        const auto fixed=render(in,sr,64,"all");const auto varied=render(in,sr,64,"all",true);
        record("block_invariance",name,sr,0,fixed,varied,true);
    }
    for(int sr:{44100,48000,96000}){
        auto in=makeSignal("independent",sr,sr*2);std::fill(in.r.begin(),in.r.end(),0);
        auto out=render(in,sr,127,"all");
        double right=0;for(float x:out.r)right=std::max(right,std::abs(static_cast<double>(x)));
        failures+=right!=0;std::cout<<"silent_right_peak "<<sr<<' '<<right<<'\n';
        in=makeSignal("white",sr,sr*2);std::fill(in.l.begin(),in.l.end(),0);std::fill(in.r.begin(),in.r.end(),0);
        out=render(in,sr,127,"delta");record("sidechain_only_delta","silence",sr,127,in,out,true);
    }
    std::cout<<"cases "<<cases<<" failures "<<failures<<'\n';
    return failures?1:0;
}
