#pragma once
#include "Analyzer.h"
namespace bassdeamon {
// UI-only frame collector. Analysis continues at its normal cadence. Peak
// envelopes and events are captured between visible frames, then replaced
// entirely: slower display must never retain an old red marker indefinitely.
class SpectrumDisplay {
public:
    using Spectrum=RelationAnalyzer::Spectrum;
    std::array<Spectrum,5> shown{};
    bool ready=false;uint64_t revision=0;
    static int fps(int setting) noexcept {return std::array<int,4>{30,15,8,4}[size_t(std::clamp(setting,0,3))];}
    SpectrumDisplay(){reset();}
    void reset() noexcept {for(size_t t=0;t<5;++t)shown[t].fill(t<3?-120.f:0.f);clearPending();elapsed=0;ready=false;mode=-1;}
    void push(const RelationAnalyzer& a,uint64_t samples,int setting) noexcept {
        if(!a.ready()){reset();return;}
        if(samples==0)return;
        if(setting!=mode){mode=setting;elapsed=0;ready=false;clearPending();}
        const std::array<const Spectrum*,5> input{&a.pre,&a.post,&a.kick,&a.maskPre,&a.maskPost};
        for(size_t t=0;t<5;++t)for(size_t b=0;b<RelationAnalyzer::bins;++b)pending[t][b]=std::max(pending[t][b],(*input[t])[b]);
        elapsed+=double(samples)/a.sampleRate();const double period=1.0/fps(setting);
        if(!ready||elapsed+1e-10>=period){
            const bool first=!ready;shown=pending;clearPending();
            elapsed=first?0:std::max(0.0,elapsed-period*std::floor((elapsed+1e-10)/period));
            ready=true;++revision;
        }
    }
private:
    void clearPending() noexcept {for(size_t t=0;t<5;++t)pending[t].fill(t<3?-120.f:0.f);}
    std::array<Spectrum,5> pending{};double elapsed=0;int mode=-1;
};
}
