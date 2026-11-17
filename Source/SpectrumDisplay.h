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
    static int fps(int setting) noexcept {return std::array<int,6>{30,15,8,4,2,1}[size_t(std::clamp(setting,0,5))];}
    SpectrumDisplay(){reset();}
    void reset() noexcept {for(size_t t=0;t<5;++t)shown[t].fill(t<3?-120.f:0.f);clearPending();for(auto& values:paired)values.fill(-120.f);nextFrame=lastFrame=0;pendingData=false;ready=false;mode=-1;}
    void push(const RelationAnalyzer& a,uint64_t samples,double nowSeconds,int setting) noexcept {
        pushAtRate(a,samples,nowSeconds,fps(setting));
    }
    void pushAtRate(const RelationAnalyzer& a,uint64_t samples,double nowSeconds,double refreshHz) noexcept {
        if(!a.ready()){reset();return;}
        const double setting=std::isfinite(refreshHz)?std::clamp(refreshHz,1.,30.):8.;
        if(setting!=mode){mode=setting;ready=false;pendingData=false;clearPending();}
        if(samples>0){
            // For each bin, prefer a shared frame with the strongest kick.
            // The three numbers come from that same frame, even during peak hold.
            for (size_t b = 0; b < RelationAnalyzer::bins; ++b) {
                const bool shared = std::max(a.maskPre[b], a.maskPost[b]) >= .12f;
                const bool hadShared = std::max(pending[3][b], pending[4][b]) >= .12f;
                if (!pendingData || (shared && !hadShared) ||
                    (shared == hadShared && a.kick[b] > pendingPaired[2][b])) {
                    pendingPaired[0][b] = a.pre[b];
                    pendingPaired[1][b] = a.post[b];
                    pendingPaired[2][b] = a.kick[b];
                }
                pending[0][b] = std::max(pending[0][b], a.pre[b]);
                pending[1][b] = std::max(pending[1][b], a.post[b]);
                pending[2][b] = std::max(pending[2][b], a.kick[b]);
                pending[3][b] = std::max(pending[3][b], a.maskPre[b]);
                pending[4][b] = std::max(pending[4][b], a.maskPost[b]);
            }
            pendingData=true;
        }
        const double period=1.0/setting;
        if(pendingData&&(!ready||nowSeconds+1e-8>=nextFrame)){
            const bool first=!ready;
            if(first)shown=pending;
            else {
                const double dt=std::clamp(nowSeconds-lastFrame,0.0,1.0);
                const float attack=float(1-std::exp(-dt/.095));
                const float release=float(1-std::exp(-dt/.32));
                for(size_t stream=0;stream<3;++stream)
                    for(size_t bin=0;bin<RelationAnalyzer::bins;++bin){
                        auto& level=shown[stream][bin];
                        const float delta=pending[stream][bin]-level;
                        level+=(delta>=0?attack:release)*delta;
                    }
                shown[3]=pending[3];shown[4]=pending[4];
            }
            lastFrame=nowSeconds;
            paired=pendingPaired;clearPending();pendingData=false;
            nextFrame=first?nowSeconds+period:nextFrame+period*(std::floor((nowSeconds-nextFrame+1e-8)/period)+1);
            ready=true;++revision;
        }
    }

private:
    void clearPending() noexcept {for(size_t t=0;t<5;++t)pending[t].fill(t<3?-120.f:0.f);}
    std::array<Spectrum,3> pendingPaired{};
    std::array<Spectrum,5> pending{};double nextFrame=0,lastFrame=0;bool pendingData=false;double mode=-1;
};
}
