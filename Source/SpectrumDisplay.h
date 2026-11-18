#pragma once
#include "Analyzer.h"
namespace bassdeamon {
// Peak traces may blend across updates. Collision maps and levels always
// come from one measured frame, so separate notes cannot add to the count.
class SpectrumDisplay {
public:
    using Spectrum=RelationAnalyzer::Spectrum;
    std::array<Spectrum,5> shown{};
    std::array<Spectrum,3> paired{};
    RelationAnalyzer::CollisionLevels eventLevels{};
    bool ready=false;uint64_t revision=0;
    static int fps(int setting) noexcept {return std::array<int,6>{30,15,8,4,2,1}[size_t(std::clamp(setting,0,5))];}
    SpectrumDisplay(){reset();}
    void reset() noexcept {for(size_t t=0;t<5;++t)shown[t].fill(t<3?-120.f:0.f);clearPending();for(auto& values:paired)values.fill(-120.f);nextFrame=lastFrame=0;pendingData=false;ready=false;mode=-1;}
    void push(const RelationAnalyzer& a,uint64_t samples,double nowSeconds,int setting) noexcept {
        pushAtRate(a,samples,nowSeconds,fps(setting));
    }
    template<class Frame>
    void pushAtRate(const Frame& a,uint64_t samples,double nowSeconds,double refreshHz) noexcept {
        if(!a.ready()){reset();return;}
        const double setting=std::isfinite(refreshHz)?std::clamp(refreshHz,1.,60.):30.;
        if(setting!=mode){mode=setting;ready=false;pendingData=false;clearPending();}
        if(samples>0){
            float confidence=0;
            const double binHz=a.sampleRate()/RelationAnalyzer::fftSize;
            const size_t start=size_t(std::ceil(RelationAnalyzer::collisionMinHz/binHz));
            const size_t end=size_t(std::floor(RelationAnalyzer::collisionMaxHz/binHz));
            for (size_t b=0;b<RelationAnalyzer::bins;++b) {
                if (b>=start && b<=end) confidence=std::max(confidence,std::max(a.maskPre[b],a.maskPost[b]));
                pending[0][b]=std::max(pending[0][b],a.pre[b]);
                pending[1][b]=std::max(pending[1][b],a.post[b]);
                pending[2][b]=std::max(pending[2][b],a.kick[b]);
            }
            // Preserve a short kick between low-rate updates. Among detected
            // events the latest frame wins, regardless of how many bands it has.
            const bool hasCollision=confidence>=RelationAnalyzer::collisionThreshold;
            if (!pendingData || hasCollision || !pendingCollision) {
                pendingEventLevels=a.eventLevels;
                pendingPaired[0]=a.pre;pendingPaired[1]=a.post;pendingPaired[2]=a.kick;
                pending[3]=a.maskPre;pending[4]=a.maskPost;
                pendingCollision=hasCollision;
            }
            pendingData=true;
        }
        const double period=1.0/setting;
        if(pendingData&&(!ready||nowSeconds+1e-8>=nextFrame)){
            const bool first=!ready;
            if(first)shown=pending;
            else {
                const double dt=std::clamp(nowSeconds-lastFrame,0.0,1.0);
                const float attack=float(1-std::exp(-dt/.055));
                const float release=float(1-std::exp(-dt/.21));
                for(size_t stream=0;stream<3;++stream)
                    for(size_t bin=0;bin<RelationAnalyzer::bins;++bin){
                        auto& level=shown[stream][bin];
                        const float delta=pending[stream][bin]-level;
                        level+=(delta>=0?attack:release)*delta;
                    }
                shown[3]=pending[3];shown[4]=pending[4];
            }
            lastFrame=nowSeconds;
            paired=pendingPaired;eventLevels=pendingEventLevels;clearPending();pendingData=false;
            nextFrame=first?nowSeconds+period:nextFrame+period*(std::floor((nowSeconds-nextFrame+1e-8)/period)+1);
            ready=true;++revision;
        }
    }

private:
    void clearPending() noexcept {pendingCollision=false;for(size_t t=0;t<5;++t)pending[t].fill(t<3?-120.f:0.f);}
    std::array<Spectrum,3> pendingPaired{};
    RelationAnalyzer::CollisionLevels pendingEventLevels{};
    std::array<Spectrum,5> pending{};double nextFrame=0,lastFrame=0;bool pendingData=false,pendingCollision=false;double mode=-1;
};
}
