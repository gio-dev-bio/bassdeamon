#pragma once
#include "Analyzer.h"
namespace bassdeamon {
// Peak traces may blend across updates. Collision maps and levels always
// come from one measured frame, so separate notes cannot add to the count.
class SpectrumDisplay {
public:
    using Spectrum=RelationAnalyzer::Spectrum;
    std::array<Spectrum,3> shown{}, paired{}, trace{};
    Spectrum maskPost{};
    RelationAnalyzer::CollisionLevels eventLevels{};
    bool ready=false, residualReady=false;
    uint64_t revision=0;
    static int fps(int setting) noexcept {return std::array<int,6>{30,15,8,4,2,1}[size_t(std::clamp(setting,0,5))];}
    SpectrumDisplay(){reset();}
    void reset() noexcept {
        for (auto& values : shown) values.fill(-120.f);
        maskPost.fill(0);
        clearPending();
        for (auto& values : paired) values.fill(-120.f);
        for (auto& values : trace) values.fill(-120.f);
        nextFrame=0; lastTrace=-1;
        pendingData=false; ready=false; residualReady=false; mode=-1;
    }
    bool advanceTrace(double nowSeconds) noexcept {
        if (!ready) return false;
        if (lastTrace<0 || nowSeconds<lastTrace) {
            for (size_t stream=0;stream<3;++stream) trace[stream]=shown[stream];
            lastTrace=nowSeconds;
            return true;
        }
        const double dt=std::clamp(nowSeconds-lastTrace,0.,.12);
        lastTrace=nowSeconds;
        const float attack=float(1-std::exp(-dt/.045));
        const float release=float(1-std::exp(-dt/.18));
        bool changed=false;
        for (size_t stream=0;stream<3;++stream)
            for (size_t bin=0;bin<RelationAnalyzer::bins;++bin) {
                auto& level=trace[stream][bin];
                const float delta=shown[stream][bin]-level;
                if (delta!=0) {
                    level+=std::abs(delta)<.02f?delta:(delta>0?attack:release)*delta;
                    changed=true;
                }
            }
        return changed;
    }
    void push(const RelationAnalyzer& a,uint64_t samples,double nowSeconds,int setting) noexcept {
        pushAtRate(a,samples,nowSeconds,fps(setting));
    }
    template<class Frame>
    void pushAtRate(const Frame& a,uint64_t samples,double nowSeconds,double refreshHz) noexcept {
        if(!a.ready()){reset();return;}
        const double setting=std::isfinite(refreshHz)?std::clamp(refreshHz,1.,60.):30.;
        if(setting!=mode){mode=setting;ready=false;residualReady=false;lastTrace=-1;pendingData=false;maskPost.fill(0);clearPending();}
        if(samples>0){
            if (!a.residualReady() && residualReady) {
                // Invalidate old measurements even if RATE is holding the traces.
                maskPost.fill(0);
                residualReady=false;
                ++revision;
            }
            float confidence=0;
            const double binHz=a.sampleRate()/RelationAnalyzer::fftSize;
            const size_t start=size_t(std::ceil(RelationAnalyzer::collisionMinHz/binHz));
            const size_t end=size_t(std::floor(RelationAnalyzer::collisionMaxHz/binHz));
            for (size_t b=0;b<RelationAnalyzer::bins;++b) {
                if (b>=start && b<=end) confidence=std::max(confidence,a.maskPost[b]);
                pending[0][b]=std::max(pending[0][b],a.pre[b]);
                pending[1][b]=std::max(pending[1][b],a.post[b]);
                pending[2][b]=std::max(pending[2][b],a.kick[b]);
            }
            // Preserve a short kick between low-rate updates. Among detected
            // events the latest frame wins, regardless of how many bands it has.
            const bool hasCollision=a.residualReady() && confidence>=RelationAnalyzer::collisionThreshold;
            if (!pendingData || hasCollision || !pendingCollision || !a.residualReady()) {
                pendingEventLevels=a.eventLevels;
                pendingPaired[0]=a.pre;pendingPaired[1]=a.post;pendingPaired[2]=a.kick;
                if (a.residualReady()) pendingMaskPost=a.maskPost;
                else pendingMaskPost.fill(0);
                pendingCollision=hasCollision;
            }
            pendingData=true;
        }
        const double period=1.0/setting;
        if(pendingData&&(!ready||nowSeconds+1e-8>=nextFrame)){
            const bool first=!ready;
            shown=pending;
            maskPost=pendingMaskPost;
            residualReady=a.residualReady();
            paired=pendingPaired;eventLevels=pendingEventLevels;clearPending();pendingData=false;
            nextFrame=first?nowSeconds+period:nextFrame+period*(std::floor((nowSeconds-nextFrame+1e-8)/period)+1);
            ready=true;++revision;
        }
    }

private:
    void clearPending() noexcept {
        pendingCollision=false;
        for (auto& values : pending) values.fill(-120.f);
        pendingMaskPost.fill(0);
    }
    std::array<Spectrum,3> pendingPaired{};
    RelationAnalyzer::CollisionLevels pendingEventLevels{};
    std::array<Spectrum,3> pending{};
    Spectrum pendingMaskPost{};
    double nextFrame=0,lastTrace=-1;
    bool pendingData=false,pendingCollision=false;
    double mode=-1;
};
}
