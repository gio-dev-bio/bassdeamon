#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include "StereoPhaseAnalysis.h"
#include "SubMono.h"

namespace bassdeamon {
struct AnalysisFrame {
    std::array<float,2> pre{},post{},kick{};
    double beat=-1;
    uint64_t sequence=0;
    std::array<float,2> phase{};
    bool phaseValid=false, wetOutput=true;
};
// One audio-thread producer, one editor-thread consumer. Never waits for the UI.
class AnalysisQueue {
public:
    static_assert(std::atomic<uint32_t>::is_always_lock_free,"Analysis counters must be lock free");
    static constexpr uint32_t capacity=16384;
    bool push(const AnalysisFrame& frame) noexcept {
        const auto serial=produced++;
        const auto w=write.load(std::memory_order_relaxed),r=read.load(std::memory_order_acquire);
        if(w-r>=capacity){dropped.fetch_add(1,std::memory_order_relaxed);return false;}
        frames[w%capacity]=frame;frames[w%capacity].sequence=serial;write.store(w+1,std::memory_order_release);return true;
    }
    bool pop(AnalysisFrame& frame) noexcept {
        const auto r=read.load(std::memory_order_relaxed),w=write.load(std::memory_order_acquire);
        if(r==w)return false;
        frame=frames[r%capacity];read.store(r+1,std::memory_order_release);return true;
    }
    // Consumer-side snapshot: one UI update must never chase an offline
    // producer forever. Only consume frames pending at the start of the tick.
    uint32_t pending() const noexcept {
        const auto r=read.load(std::memory_order_relaxed),w=write.load(std::memory_order_acquire);
        return w-r;
    }
    std::atomic<bool> enabled{false};std::atomic<uint32_t> dropped{0};
private:
    std::array<AnalysisFrame,capacity> frames{};
    std::atomic<uint32_t> write{0},read{0};
    uint64_t produced=0; // audio-thread only, includes dropped frames
};

// Completed quarter-note captures, aligned to host PPQ. Min/max per pixel
// retains short kick transients without a large full-rate history or aliasing
// a decimated waveform. The display never scrolls during an incomplete beat.
class BeatScope {
public:
    static constexpr size_t size=512;
    using Wave=std::array<float,size>;
    enum Stream : size_t { bassPre, bassPost, kick, preLeft, postLeft, preRight, postRight, preMono, postMono, phaseOff, phaseOn, streamCount };
    std::array<Wave,streamCount> low{},high{};
    MonoCompatibility monoCheck, subMonoCheck;
    bool phaseMeasured=false;
    double phaseDelta=0, measuredPhaseHz=0;
    void setPhaseFrequency(double hz) noexcept {
        if (hz==phaseHz) return;
        phaseHz=hz;
        phaseMeasured=false;
        phaseSamples=0;
        phaseEnergy={};
        phaseFilters={};
        for (auto& filter : phaseFilters) filter.setFrequency(hz,rate);
    }
    void setSubFrequency(double hz) noexcept { for (auto& filter : subFilters) filter.setCutoff(hz,rate); }
    uint64_t completed=0;double milliseconds=500;
    void prepare(double sr) noexcept {rate=sr;fallback=0;lastBeat=-1;cycle=-1;completed=0;valid=false;low={};high={};monoCheck={};subMonoCheck={};subFilters={};setSubFrequency(120);phaseHz=0;setPhaseFrequency(60);reset();}
    void tick(const AnalysisFrame& f,double bpm,bool hold) noexcept {
        const double tempo=std::isfinite(bpm)&&bpm>=20&&bpm<=400?bpm:120;
        const double step=tempo/(60*rate);
        const double beat=f.beat>=0&&std::isfinite(f.beat)?f.beat:fallback;
        fallback=beat+step;
        const auto nextCycle=int64_t(std::floor(beat));const double phase=beat-std::floor(beat);
        const size_t bin=std::min(size-1,size_t(phase*size));
        if(nextCycle!=cycle){
            if(valid&&lastBin>=size-2&&!hold){
                low=buildingLow;high=buildingHigh;++completed;milliseconds=60000/tempo;
                monoCheck=stereoPower.measure();
                subMonoCheck=subPower.measure();
                phaseMeasured=phaseSamples>rate*60/tempo*.95 && phaseEnergy[0]>1e-5 &&
                    phaseEnergy[1]>1e-5 && phaseEnergy[2]>1e-5;
                if (phaseMeasured) {
                    measuredPhaseHz=phaseHz;
                    const double match=std::sqrt(phaseEnergy[0]/phaseEnergy[1]);
                    const double common=phaseEnergy[0]+phaseEnergy[2];
                    const double off=common+2*phaseEnergy[3], on=common+2*phaseEnergy[4]*match;
                    phaseDelta=10*std::log10(std::max(on,common*1e-6)/std::max(off,common*1e-6));
                }
            }
            reset();cycle=nextCycle;valid=bin==0;
        } else if(lastBeat>=0&&std::abs(beat-lastBeat-step)>step*2)valid=false;
        const float preL=f.pre[0]+f.kick[0], postL=f.post[0]+f.kick[0];
        const float preR=f.pre[1]+f.kick[1], postR=f.post[1]+f.kick[1];
        const float phaseKick=(f.kick[0]+f.kick[1])*.5f;
        if (f.phaseValid) {
            const double off=phaseFilters[0].tick(f.phase[0]), on=phaseFilters[1].tick(f.phase[1]);
            const double k=phaseFilters[2].tick(phaseKick);
            phaseEnergy[0]+=off*off; phaseEnergy[1]+=on*on; phaseEnergy[2]+=k*k;
            phaseEnergy[3]+=off*k; phaseEnergy[4]+=on*k;
            ++phaseSamples;
        }
        const std::array<float,streamCount> values{f.pre[0],f.post[0],f.kick[0],
            preL,postL,preR,postR,(preL+preR)*.5f,(postL+postR)*.5f, f.phase[0]+phaseKick, f.phase[1]+phaseKick};
        stereoPower.add(f.post[0],f.post[1]);
        subPower.add(subFilters[0].lowSide(f.post[0]),subFilters[1].lowSide(f.post[1]));
        for(size_t t=0;t<values.size();++t){buildingLow[t][bin]=std::min(buildingLow[t][bin],values[t]);buildingHigh[t][bin]=std::max(buildingHigh[t][bin],values[t]);}
        lastBeat=beat;lastBin=bin;
    }
private:
    void reset() noexcept {
        buildingLow={};buildingHigh={};lastBin=0;
        stereoPower={}; subPower={}; phaseEnergy={}; phaseSamples=0;
    }
    std::array<Wave,streamCount> buildingLow{},buildingHigh{};
    struct PhaseBand {
        double b=0,a1=0,a2=0,z1=0,z2=0;
        void setFrequency(double hz,double sr) noexcept {
            const double w=6.283185307179586*hz/sr, alpha=std::sin(w)*.25;
            b=alpha/(1+alpha); a1=-2*std::cos(w)/(1+alpha); a2=(1-alpha)/(1+alpha);
        }
        double tick(double x) noexcept {
            const double y=b*x+z1;
            z1=z2-a1*y; z2=-b*x-a2*y;
            return y;
        }
    };
    double phaseHz=0;
    size_t phaseSamples=0;
    std::array<double,5> phaseEnergy{};
    std::array<PhaseBand,3> phaseFilters{};
    StereoPower stereoPower, subPower;
    std::array<SubMono,2> subFilters;
    double rate=48000,fallback=0,lastBeat=-1;int64_t cycle=-1;size_t lastBin=0;bool valid=false;
};

// Message-thread analysis only. Stereo powers are averaged, not mono-summed:
// anti-phase stereo must not disappear from the frequency display.
class RelationAnalyzer {
public:
    static constexpr size_t fftSize=16384,bins=fftSize/2+1,waveSize=512,fastSize=8192,fastBins=fastSize/2+1;
    using Spectrum=std::array<float,bins>;
    using FastSpectrum=std::array<float,fastBins>;
    // 250 Hz at a sample rate of at least 8 kHz fits in this readout.
    static constexpr size_t readoutBins=fftSize/32+1;
    using LevelTriplet=std::array<float,3>;
    using CollisionLevels=std::array<LevelTriplet,readoutBins>;
    BeatScope scope;
    Spectrum pre{},post{},kick{},maskPost{};
    uint64_t discontinuities=0,consumedFrames=0;
    std::array<float,waveSize> wavePre{},wavePost{},waveKick{},sumPre{},sumPost{};
    struct Frame {
        Spectrum pre{}, post{}, kick{}, maskPost{};
        CollisionLevels eventLevels{};
        bool hasSpectrum=false, hasResidual=false;
        double rate=48000;
        double sampleRate() const noexcept { return rate; }
        bool ready() const noexcept { return hasSpectrum; }
        bool residualReady() const noexcept { return hasResidual; }
    };
    void copyFrame(Frame& frame) const noexcept {
        frame.pre=pre; frame.post=post; frame.kick=kick;
        frame.maskPost=maskPost; frame.eventLevels=eventLevels;
        frame.hasSpectrum=ready(); frame.hasResidual=residualReady(); frame.rate=rate;
    }
    static constexpr double collisionMinHz=30, collisionMaxHz=250;
    static constexpr float collisionThreshold=.12f;
    CollisionLevels eventLevels{};
    struct CollisionBand {
        size_t first=0, last=0, peak=0;
        double hz=0;
        float strength=0;
    };
    template<class Visitor>
    static void visitCollisionBands(const Spectrum& mask,const Spectrum& levels,double sampleRate,
                                    double minimum,double maximum,Visitor&& visit) noexcept {
        minimum=std::max(minimum,collisionMinHz);
        maximum=std::min(maximum,collisionMaxHz);
        if (maximum<minimum) return;
        const double binHz=sampleRate/fftSize;
        const auto firstBin=std::max(size_t(1),size_t(std::ceil(minimum/binHz)));
        const auto lastBin=std::min(bins-2,size_t(std::floor(maximum/binHz)));
        const double mergeHz=std::max(12.,3*binHz);
        if (firstBin>lastBin) return;
        size_t bin=firstBin;
        while (bin<=lastBin) {
            if (mask[bin]<collisionThreshold) { ++bin; continue; }
            CollisionBand band{bin,bin,bin,0,mask[bin]};
            // A gap narrower than the resolution of the window is still
            // one band. Measure the spacing in Hz, independent of zoom.
            while (++bin<=lastBin) {
                if ((bin-band.last)*binHz>mergeHz) break;
                if (mask[bin]<collisionThreshold) continue;
                band.last=bin;
                if (levels[bin]>levels[band.peak]) band.peak=bin;
                band.strength=std::max(band.strength,mask[bin]);
            }
            const size_t b=band.peak;
            const double a=levels[b-1],v=levels[b],c=levels[b+1],den=a-2*v+c;
            const double offset=den< -1e-12?std::clamp(.5*(a-c)/den,-.5,.5):0;
            band.hz=std::clamp((b+offset)*binHz, minimum,maximum);
            visit(band);
        }
    }
    void prepare(double sr) noexcept {
        rate=sr;scope.prepare(sr);haveSequence=false;discontinuities=0;consumedFrames=0;resetAnalysis();
        wavePre.fill(0);wavePost.fill(0);waveKick.fill(0);sumPre.fill(0);sumPost.fill(0);
        hop=std::max(size_t(256),size_t(sr*.020));
        if (!fftPrepared) {
            for(size_t i=0;i<fftSize/2;++i) fftRoots[i]=std::polar(1.,-2*pi*i/fftSize);
            for(size_t i=1,j=0;i<fftSize;++i){
                size_t bit=fftSize>>1;
                for(;j&bit;bit>>=1)j^=bit;
                fftOrder[i]=uint16_t(j^=bit);
            }
            fftPrepared=true;
        }
        windowSum=fastWindowSum=0;
        for(size_t i=0;i<fftSize;++i){window[i]=blackmanHarris(i,fftSize);windowSum+=window[i];}
        for(size_t i=0;i<fastSize;++i){fastWindow[i]=blackmanHarris(i,fastSize);fastWindowSum+=fastWindow[i];}
    }
    void consume(AnalysisQueue& queue,double bpm=120,bool hold=false) noexcept {
        const auto budget=queue.pending();
        AnalysisFrame f;for(uint32_t consumed=0;consumed<budget&&queue.pop(f);++consumed){
            ++consumedFrames;
            if(haveSequence&&f.sequence!=nextSequence){resetAnalysis();scope.prepare(rate);++discontinuities;}
            haveSequence=true;nextSequence=f.sequence+1;
            for(auto* stream:{&f.pre,&f.post,&f.kick})for(auto& v:*stream)if(!std::isfinite(v))v=0;
            scope.tick(f,bpm,hold);frames[count%fftSize]=f;++count;
            // Audition samples still feed the traces and scope. A residual
            // requires two FFT windows containing only the processed output.
            if (!f.wetOutput) {
                if (wetSamples>0) clearCollisionHistory();
                wetSamples=0;
                continue;
            }
            wetSamples=std::min(wetSamples+1,fftSize);
            // Fixed sample hops catch short kicks even if they finish between
            // two editor updates. Collision history is NOT a gate at the end
            // of a long FFT window.
            if(wetSamples>=fastSize&&count-lastFastCount>=hop){
                prepareKickGate();
                analyze(1,post,true);analyze(2,kick,true);
                updateEvents();lastFastCount=count;
            }
        }
        if(count==lastCount)return;
        lastCount=count;
        if (ready()) {
            analyze(0,pre);analyze(1,post);analyze(2,kick);
            if (residualReady()) updateMask();
        }
        for(size_t i=0;i<waveSize;++i){
            const auto peak=[&](size_t t){return std::abs(scope.low[t][i])>std::abs(scope.high[t][i])?scope.low[t][i]:scope.high[t][i];};
            wavePre[i]=peak(0);wavePost[i]=peak(1);waveKick[i]=peak(2);sumPre[i]=peak(3);sumPost[i]=peak(4);
        }
    }
    // Significant interference with the kick, not a claim of psychoacoustic masking.
    static float mask(float bass,float reference,float=-120,float=-120) noexcept {
        const float level=std::clamp((std::min(bass,reference)+90.f)/24.f,0.f,1.f);
        const float relative=std::clamp((bass-reference+24.f)/24.f,0.f,1.f);
        return level*relative*relative*(3.f-2.f*relative);
    }
    double sampleRate() const noexcept{return rate;}
    bool ready() const noexcept{return count>=fftSize;}
    bool residualReady() const noexcept{return wetSamples>=fftSize;}
    uint64_t streamSequence() const noexcept{return haveSequence?nextSequence:0;}
    struct Summary {int regions=0;double strongestHz=0;};
    Summary summary() const noexcept {
        Summary result;
        float loudest=-121;
        visitCollisionBands(maskPost,post,rate,collisionMinHz,collisionMaxHz,[&](const CollisionBand& band){
            ++result.regions;
            if (post[band.peak]>loudest) { loudest=post[band.peak]; result.strongestHz=band.hz; }
        });
        return result;
    }
private:
    static constexpr double pi=3.14159265358979323846;
    static double blackmanHarris(size_t n,size_t length) noexcept {
        const double phase=2*pi*double(n)/double(length-1);
        return .35875-.48829*std::cos(phase)+.14128*std::cos(2*phase)-.01168*std::cos(3*phase);
    }
    void resetAnalysis() noexcept {
        // Both FFTs read only samples written since count was reset.
        // Leave the old ring slots alone to avoid a full-size stack temporary.
        count=lastCount=lastFastCount=wetSamples=0;
        pre.fill(-120);post.fill(-120);kick.fill(-120);
        clearCollisionHistory();
        for(auto& stream:channelLevels)for(auto& channel:stream)channel.fill(-120);
        for(auto& stream:fastLevels)for(auto& channel:stream)channel.fill(-120);
    }
    void clearCollisionHistory() noexcept {
        maskPost.fill(0);
        for (auto& channel : recentCollision) channel.fill(0);
        recentConfidence=0;
    }
    size_t collisionAnalysisEnd(size_t length) const noexcept {
        // Leave a guard band above the 250 Hz display range for peak
        // detection. The spectrum itself still uses the full FFT.
        return std::min(length/2,size_t(std::ceil(1000.*length/rate))+4);
    }
    template<size_t N> void buildSupport(const std::array<float,N>& levels,std::array<float,N>& support,size_t length) noexcept {
        support.fill(0);float maximum=-120;
        const size_t end=std::min(N-1,collisionAnalysisEnd(length));
        for(size_t b=1;b<end;++b){const double hz=double(b)*rate/length;if(hz>=20&&hz<=20000)maximum=std::max(maximum,levels[b]);}
        const float floor=std::max(-110.f,maximum-60.f);
        for(size_t p=1;p<end;++p){const double hz=double(p)*rate/length;const float level=levels[p];
            if(hz<20||hz>20000||level<=floor||level<=levels[p-1]||level<levels[p+1])continue;
            // Each real partial has its own local contour. Weak harmonics are
            // not discarded just because another partial is >18 dB louder.
            // BH sidelobes are ~92 dB below a stationary tone, outside this gate.
            const float strength=std::clamp((level-floor)/6.f,0.f,1.f),edge=level-12.f;
            size_t lo=p,hi=p;
            while(lo>1&&levels[lo-1]<=levels[lo]&&levels[lo-1]>edge)--lo;
            while(hi+1<end&&levels[hi+1]<=levels[hi]&&levels[hi+1]>edge)++hi;
            for(size_t b=lo;b<=hi;++b){const float shape=std::clamp((levels[b]-edge)/6.f,0.f,1.f);
                support[b]=std::max(support[b],shape*strength);}
        }
    }
    void updateEvents() noexcept {
        std::array<double,2> bassEnergy{}, kickEnergy{}, joint{};
        // Use concurrent output and sidechain samples from the same window.
        for (size_t i=0;i<fastSize;++i) {
            const auto& f=frames[(count-fastSize+i)%fftSize];
            const double windowedGate=fastWindow[i]*kickGate[i];
            const double weight=windowedGate*windowedGate;
            for (size_t c=0;c<2;++c) {
                const double bass=double(f.post[c])*f.post[c], kickSample=double(f.kick[c])*f.kick[c];
                bassEnergy[c]+=weight*bass;
                kickEnergy[c]+=weight*kickSample;
                joint[c]+=weight*std::min(bass,kickSample);
            }
        }
        const size_t end=collisionAnalysisEnd(fastSize);
        float confidence=0;
        for (size_t channel=0;channel<2;++channel) {
            const double base=std::min(bassEnergy[channel],kickEnergy[channel]);
            const float coincidence=base>1e-20?float(std::clamp((joint[channel]/base-.01)/.10,0.,1.)):0;
            for (size_t b=0;b<end;++b) {
                const float common=std::sqrt(fastSupport[1][channel][b]*fastSupport[2][channel][b]);
                const float event=common*coincidence*mask(fastLevels[1][channel][b],fastLevels[2][channel][b]);
                currentCollision[channel][b]=event;
                confidence=std::max(confidence,event);
            }
        }
        if (confidence>=collisionThreshold || recentConfidence<collisionThreshold) {
            recentCollision=currentCollision;
            recentConfidence=confidence;
            if (confidence>=collisionThreshold) {
                // The input spectrum provides a dB comparison for a measured
                // residual. It contributes neither bands nor detection score.
                analyze(0,pre,true);
                measuredFastLevels=fastLevels;
            }
        } else {
            // Retain one short kick as one frame across display updates.
            const float decay=float(std::exp(-double(count-lastFastCount)/(rate*.12)));
            recentConfidence*=decay;
            for (auto& channel : recentCollision)
                for (size_t b=0;b<end;++b) channel[b]*=decay;
        }
    }
    void updateMask() noexcept {
        const auto end=collisionAnalysisEnd(fftSize);
        std::array<std::array<float,2>,3> loudest{};
        for (size_t stream=1;stream<3;++stream) for (size_t channel=0;channel<2;++channel)
            loudest[stream][channel]=*std::max_element(channelLevels[stream][channel].begin()+1,
                                                       channelLevels[stream][channel].begin()+end);
        for(size_t b=0;b<end;++b){maskPost[b]=0;const double hz=double(b)*rate/fftSize;
            if(hz<20||hz>20000)continue;
            const double position=double(b)*fastSize/fftSize;const size_t lo=std::min(fastBins-1,size_t(position)),hi=std::min(fastBins-1,lo+1);
            const float fraction=float(position-double(lo));
            float score=0;
            size_t measuredChannel=0;
            for (size_t channel=0;channel<2;++channel) {
                const auto& levels=channelLevels[1][channel];
                const auto& kickLevels=channelLevels[2][channel];
                const float prominence=std::max(levels[b]-loudest[1][channel],kickLevels[b]-loudest[2][channel]);
                const float weight=std::clamp((prominence+36.f)/16.f,0.f,1.f);
                const float fine=weight*std::sqrt(fineSupport[1][channel][b]*fineSupport[2][channel][b]);
                const auto& recent=recentCollision[channel];
                const float event=recent[lo]+fraction*(recent[hi]-recent[lo]);
                if (const float combined=std::min(fine,event);combined>score) {
                    score=combined;
                    measuredChannel=channel;
                }
            }
            maskPost[b]=score;
            if (score>=collisionThreshold && hz>=collisionMinHz && hz<=collisionMaxHz && b<readoutBins) {
                for (size_t signal=0;signal<3;++signal) {
                    const auto& levels=measuredFastLevels[signal][measuredChannel];
                    eventLevels[b][signal]=levels[lo]+fraction*(levels[hi]-levels[lo]);
                }
            }
        }
    }
    void prepareKickGate() noexcept {
        constexpr size_t slices=32, sliceSize=fastSize/slices;
        constexpr size_t historySlices=fftSize/sliceSize;
        std::array<double,historySlices> energy{};
        const size_t valid=std::min(count,fftSize);
        for (size_t i=fftSize-valid;i<fftSize;++i) {
            const auto& kickFrame=frames[(count-(fftSize-i))%fftSize].kick;
            energy[i/sliceSize]+=.5*(double(kickFrame[0])*kickFrame[0]+double(kickFrame[1])*kickFrame[1]);
        }
        const size_t lookback=std::min(historySlices-slices,size_t(std::ceil(.115*rate/sliceSize)));
        std::array<double,slices> weights{};
        for (size_t i=0;i<slices;++i) {
            const size_t slice=historySlices-slices+i;
            // Refer to the preceding body of the kick even when it has left
            // the fast FFT. Otherwise a weak tail resets its own threshold.
            const size_t end=std::min(historySlices,slice+3);
            const double peak=*std::max_element(energy.begin()+slice-lookback,energy.begin()+end);
            const double weight=peak>1e-16?std::clamp((energy[slice]/peak-.035)/.12,0.,1.):0.;
            weights[i]=weight*weight*(3-2*weight);
        }
        // Interpolate between slice centres to avoid making new FFT partials
        // by abruptly switching the audio on and off in the analysis window.
        for (size_t i=0;i<fastSize;++i) {
            const double position=std::clamp(double(i)/sliceSize-.5,0.,double(slices-1));
            const size_t lo=size_t(position), hi=std::min(lo+1,slices-1);
            kickGate[i]=float(weights[lo]+(position-lo)*(weights[hi]-weights[lo]));
        }
    }
    void analyze(int stream,Spectrum& destination,bool fast=false) noexcept {
        const size_t length=fast?fastSize:fftSize, numBins=length/2+1;
        const double sum=fast?fastWindowSum:windowSum;
        const double scale=1/(sum*sum);
        const size_t end=collisionAnalysisEnd(length);
        bool nonzero=false;
        for (size_t i=0;i<length;++i) {
            const auto& frame=frames[(count-length+i)%fftSize];
            const auto& stereo=stream==0?frame.pre:stream==1?frame.post:frame.kick;
            const double weight=fast?fastWindow[i]*kickGate[i]:window[i];
            const auto sample=std::complex<double>(stereo[0]*weight,stereo[1]*weight);
            nonzero|=sample.real()!=0 || sample.imag()!=0;
            work[fast?fftOrder[i]>>1:fftOrder[i]]=sample;
        }
        if (nonzero) {
            for (size_t stage=2;stage<=length;stage<<=1) {
                const size_t rootStep=fftSize/stage;
                for (size_t base=0;base<length;base+=stage)
                    for (size_t j=0;j<stage/2;++j) {
                        const auto u=work[base+j], z=work[base+j+stage/2], w=fftRoots[j*rootStep];
                        const std::complex<double> v(w.real()*z.real()-w.imag()*z.imag(),
                                                     w.real()*z.imag()+w.imag()*z.real());
                        work[base+j]=u+v;work[base+j+stage/2]=u-v;
                    }
            }
        }
        const auto level=[](double power){return float(10*std::log10(std::max(1e-12,power)));};
        for (size_t b=0;b<(fast?end:numBins);++b) {
            const auto plus=work[b],minus=std::conj(work[b?length-b:0]);
            const double normalization=(b==0||b==length/2)?.25*scale:scale;
            const double left=std::norm(plus+minus)*normalization;
            const double right=std::norm(plus-minus)*normalization;
            const auto t=size_t(stream);
            if (fast) {fastLevels[t][0][b]=level(left);fastLevels[t][1][b]=level(right);}
            else {
                if (stream!=0 && b<end) {channelLevels[t][0][b]=level(left);channelLevels[t][1][b]=level(right);}
                destination[b]=level(.5*(left+right));
            }
        }
        if (stream==0) return;
        for (size_t channel=0;channel<2;++channel) {
            if(fast){
                auto& levels=fastLevels[size_t(stream)][channel];auto& support=fastSupport[size_t(stream)][channel];
                buildSupport(levels,support,fastSize);
                // A short window cannot resolve a weak partial beside a loud
                // fundamental. Allow broadband temporal evidence here; the
                // long, high-resolution spectrum must ALSO contain a real peak.
                const float peak=*std::max_element(levels.begin(),levels.begin()+end);
                const float floor=std::max(-110.f,peak-60.f);
                for(size_t b=1;b+1<end;++b)
                    support[b]=std::max(support[b],std::clamp((levels[b]-floor)/12.f,0.f,1.f));
            }
            else buildSupport(channelLevels[size_t(stream)][channel],fineSupport[size_t(stream)][channel],fftSize);
        }
    }
    double rate=48000,windowSum=1,fastWindowSum=1;size_t count=0,lastCount=0,lastFastCount=0,hop=480;
    bool haveSequence=false;uint64_t nextSequence=0;
    size_t wetSamples=0;
    float recentConfidence=0;
    std::array<std::array<Spectrum,2>,3> channelLevels{},fineSupport{};
    std::array<std::array<FastSpectrum,2>,3> fastLevels{},fastSupport{},measuredFastLevels{};
    std::array<FastSpectrum,2> recentCollision{},currentCollision{};
    std::array<AnalysisFrame,fftSize> frames{};
    std::array<double,fftSize> window{};
    std::array<double,fastSize> fastWindow{};
    std::array<float,fastSize> kickGate{};
    std::array<std::complex<double>,fftSize> work{};
    std::array<std::complex<double>,fftSize/2> fftRoots{};
    std::array<uint16_t,fftSize> fftOrder{};
    bool fftPrepared=false;
};
}
