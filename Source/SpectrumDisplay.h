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
    std::array<Spectrum,3> paired{};
    bool ready=false;uint64_t revision=0;
    static int fps(int setting) noexcept {return std::array<int,4>{30,15,8,4}[size_t(std::clamp(setting,0,3))];}
    SpectrumDisplay(){reset();}
    void reset() noexcept {for(size_t t=0;t<5;++t)shown[t].fill(t<3?-120.f:0.f);clearPending();for(auto& values:paired)values.fill(-120.f);nextFrame=0;pendingData=false;ready=false;mode=-1;}
    void push(const RelationAnalyzer& a,uint64_t samples,double nowSeconds,int setting) noexcept {
        if(!a.ready()){reset();return;}
        if(setting!=mode){mode=setting;ready=false;pendingData=false;clearPending();}
        if(samples>0){
            const std::array<const Spectrum*,5> input{&a.pre,&a.post,&a.kick,&a.maskPre,&a.maskPost};
            // Keep all three levels from one analysis frame for each bin.
            // Peak-held display curves may peak at different times.
            for (size_t b = 0; b < RelationAnalyzer::bins; ++b) {
                const float score = std::max(a.maskPre[b], a.maskPost[b]);
                if (!pendingData || score >= std::max(pending[3][b], pending[4][b])) {
                    pendingPaired[0][b] = a.pre[b];
                    pendingPaired[1][b] = a.post[b];
                    pendingPaired[2][b] = a.kick[b];
                }
            }
            for(size_t t=0;t<5;++t)for(size_t b=0;b<RelationAnalyzer::bins;++b)pending[t][b]=std::max(pending[t][b],(*input[t])[b]);
            pendingData=true;
        }
        const double period=1.0/fps(setting);
        if(pendingData&&(!ready||nowSeconds+1e-8>=nextFrame)){
            const bool first=!ready;shown=pending;paired=pendingPaired;clearPending();pendingData=false;
            nextFrame=first?nowSeconds+period:nextFrame+period*(std::floor((nowSeconds-nextFrame+1e-8)/period)+1);
            ready=true;++revision;
        }
    }

private:
    void clearPending() noexcept {for(size_t t=0;t<5;++t)pending[t].fill(t<3?-120.f:0.f);}
    std::array<Spectrum,3> pendingPaired{};
    std::array<Spectrum,5> pending{};double nextFrame=0;bool pendingData=false;int mode=-1;
};
}
