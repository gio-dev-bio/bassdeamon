#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>

namespace bassdeamon {
struct AnalysisFrame {std::array<float,2> pre{},post{},kick{};double beat=-1;uint64_t sequence=0;};
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
    std::array<Wave,5> low{},high{};
    uint64_t completed=0;double milliseconds=500;
    void prepare(double sr) noexcept {rate=sr;fallback=0;lastBeat=-1;cycle=-1;completed=0;valid=false;low={};high={};reset();}
    void tick(const AnalysisFrame& f,double bpm,bool hold) noexcept {
        const double tempo=std::isfinite(bpm)&&bpm>=20&&bpm<=400?bpm:120;
        const double step=tempo/(60*rate);
        const double beat=f.beat>=0&&std::isfinite(f.beat)?f.beat:fallback;
        fallback=beat+step;
        const auto nextCycle=int64_t(std::floor(beat));const double phase=beat-std::floor(beat);
        const size_t bin=std::min(size-1,size_t(phase*size));
        if(nextCycle!=cycle){
            if(valid&&lastBin>=size-2&&!hold){low=buildingLow;high=buildingHigh;++completed;milliseconds=60000/tempo;}
            reset();cycle=nextCycle;valid=bin==0;
        } else if(lastBeat>=0&&std::abs(beat-lastBeat-step)>step*2)valid=false;
        const std::array<float,5> values{f.pre[0],f.post[0],f.kick[0],f.pre[0]+f.kick[0],f.post[0]+f.kick[0]};
        for(size_t t=0;t<5;++t){buildingLow[t][bin]=std::min(buildingLow[t][bin],values[t]);buildingHigh[t][bin]=std::max(buildingHigh[t][bin],values[t]);}
        lastBeat=beat;lastBin=bin;
    }
private:
    void reset() noexcept {buildingLow={};buildingHigh={};lastBin=0;}
    std::array<Wave,5> buildingLow{},buildingHigh{};
    double rate=48000,fallback=0,lastBeat=-1;int64_t cycle=-1;size_t lastBin=0;bool valid=false;
};

// Message-thread analysis only. Stereo powers are averaged, not mono-summed:
// anti-phase stereo must not disappear from the frequency display.
class RelationAnalyzer {
public:
    static constexpr size_t fftSize=16384,bins=fftSize/2+1,waveSize=512,fastSize=4096,fastBins=fastSize/2+1;
    using Spectrum=std::array<float,bins>;
    BeatScope scope;
    Spectrum pre{},post{},kick{},maskPre{},maskPost{};
    uint64_t discontinuities=0;
    std::array<float,waveSize> wavePre{},wavePost{},waveKick{},sumPre{},sumPost{};
    void prepare(double sr) noexcept {
        scope.prepare(sr);rate=sr;haveSequence=false;discontinuities=0;resetAnalysis();
        wavePre.fill(0);wavePost.fill(0);waveKick.fill(0);sumPre.fill(0);sumPost.fill(0);
        temporalPole=std::exp(-1/(.005*sr));peakPole=std::exp(-1/(.25*sr));
        windowSum=0;for(size_t i=0;i<fftSize;++i){window[i]=.5-.5*std::cos(2*pi*i/(fftSize-1));windowSum+=window[i];}
        fastWindowSum=0;for(size_t i=0;i<fastSize;++i){fastWindow[i]=.5-.5*std::cos(2*pi*i/(fastSize-1));fastWindowSum+=fastWindow[i];}
    }
    void consume(AnalysisQueue& queue,double bpm=120,bool hold=false) noexcept {
        AnalysisFrame f;while(queue.pop(f)){
            if(haveSequence&&f.sequence!=nextSequence){resetAnalysis();scope.prepare(rate);++discontinuities;}
            haveSequence=true;nextSequence=f.sequence+1;
            for(auto* stream:{&f.pre,&f.post,&f.kick})for(auto& v:*stream)if(!std::isfinite(v))v=0;
            for(size_t channel=0;channel<2;++channel){
                const std::array<double,3> energy{double(f.pre[channel])*f.pre[channel],double(f.post[channel])*f.post[channel],double(f.kick[channel])*f.kick[channel]};
                for(size_t stream=0;stream<3;++stream){
                    fastPower[stream][channel]=energy[stream]+temporalPole*(fastPower[stream][channel]-energy[stream]);
                    recentPower[stream][channel]=std::max(fastPower[stream][channel],recentPower[stream][channel]*peakPole);
                }
                for(size_t stream=0;stream<2;++stream){
                    const double joint=std::min(energy[stream],energy[2]);
                    jointPower[stream][channel]=joint+temporalPole*(jointPower[stream][channel]-joint);
                }
            }
            scope.tick(f,bpm,hold);frames[count%fftSize]=f;++count;
        }
        if(count==lastCount)return;
        lastCount=count;
        // Do not present a zero-padded startup as a calibrated full-window FFT.
        if(count>=fftSize){analyze(0,pre);analyze(1,post);analyze(2,kick);
            analyze(0,pre,true);analyze(1,post,true);analyze(2,kick,true);updateMask();}
        for(size_t i=0;i<waveSize;++i){
            const auto peak=[&](size_t t){return std::abs(scope.low[t][i])>std::abs(scope.high[t][i])?scope.low[t][i]:scope.high[t][i];};
            wavePre[i]=peak(0);wavePost[i]=peak(1);waveKick[i]=peak(2);sumPre[i]=peak(3);sumPost[i]=peak(4);
        }
    }
    static float mask(float bass,float reference,float bassPeak=-120,float referencePeak=-120) noexcept {
        // Conservative spectral conflict, not a perceptual masking model.
        // Reject FFT leakage/noise far below each source's useful spectrum.
        const float bassFloor=std::max(-60.f,bassPeak-18.f),kickFloor=std::max(-60.f,referencePeak-18.f);
        const float activity=std::clamp(std::min(bass-bassFloor,reference-kickFloor)/6.f,0.f,1.f);
        return activity*std::clamp(1.f-std::abs(bass-reference)/12.f,0.f,1.f);
    }
    double sampleRate() const noexcept{return rate;}
private:
    static constexpr double pi=3.14159265358979323846;
    void resetAnalysis() noexcept {
        count=lastCount=0;frames={};pre.fill(-120);post.fill(-120);kick.fill(-120);maskPre.fill(0);maskPost.fill(0);
        for(auto& stream:channelLevels)for(auto& channel:stream)channel.fill(-120);
        for(auto& stream:fastLevels)for(auto& channel:stream)channel.fill(-120);
        fastPower={};recentPower={};jointPower={};
    }
    void updateMask() noexcept {
        std::array<std::array<float,2>,3> peaks{},fastPeaks{};for(auto& p:peaks)p.fill(-120);for(auto& p:fastPeaks)p.fill(-120);
        for(size_t b=1;b<bins;++b){const double hz=double(b)*rate/fftSize;
            if(hz<20||hz>20000)continue;
            for(size_t stream=0;stream<3;++stream)for(size_t channel=0;channel<2;++channel)
                peaks[stream][channel]=std::max(peaks[stream][channel],channelLevels[stream][channel][b]);
        }
        for(size_t b=1;b<fastBins;++b){const double hz=double(b)*rate/fastSize;
            if(hz<20||hz>20000)continue;
            for(size_t stream=0;stream<3;++stream)for(size_t channel=0;channel<2;++channel)
                fastPeaks[stream][channel]=std::max(fastPeaks[stream][channel],fastLevels[stream][channel][b]);
        }
        for(size_t b=0;b<bins;++b){maskPre[b]=maskPost[b]=0;const double hz=double(b)*rate/fftSize;
            if(hz<20||hz>20000)continue;
            for(size_t stream=0;stream<2;++stream)for(size_t channel=0;channel<2;++channel){
                const double reference=std::min(recentPower[stream][channel],recentPower[2][channel]);
                const double floor=std::max(-70.0,10*std::log10(std::max(1e-20,reference))-20);
                const double jointDb=10*std::log10(std::max(1e-20,jointPower[stream][channel]));
                const float simultaneous=float(std::clamp((jointDb-floor)/8,0.0,1.0));
                const auto currentLevel=[&](size_t source){
                    const double position=double(b)*fastSize/fftSize;const auto lo=std::min(fastBins-1,size_t(position)),hi=std::min(fastBins-1,lo+1);
                    const double fraction=position-double(lo);
                    const double power=(1-fraction)*std::pow(10.0,fastLevels[source][channel][lo]/10.0)+fraction*std::pow(10.0,fastLevels[source][channel][hi]/10.0);
                    return float(10*std::log10(std::max(1e-12,power)));
                };
                const float detail=mask(channelLevels[stream][channel][b],channelLevels[2][channel][b],peaks[stream][channel],peaks[2][channel]);
                // Fine frequency support must also exist in the recent short
                // window: a previous bass note must not collide with a new kick.
                const float current=detail>0?mask(currentLevel(stream),currentLevel(2),fastPeaks[stream][channel],fastPeaks[2][channel]):0;
                const float value=simultaneous*std::min(detail,current);
                auto& result=stream==0?maskPre[b]:maskPost[b];result=std::max(result,value);
            }
        }
    }
    void analyze(int stream,Spectrum& destination,bool fast=false) noexcept {
        powers.fill(0);
        const size_t length=fast?fastSize:fftSize,valid=std::min(count,length),numBins=length/2+1;
        const double sum=fast?fastWindowSum:windowSum;
        for(size_t channel=0;channel<2;++channel){
            for(size_t i=0;i<length;++i){
                double sample=0;if(i>=length-valid){const auto& f=frames[(count-valid+i-(length-valid))%fftSize];
                    sample=stream==0?f.pre[channel]:stream==1?f.post[channel]:f.kick[channel];}
                work[i]=sample*(fast?fastWindow[i]:window[i]);
            }
            for(size_t i=1,j=0;i<length;++i){size_t bit=length>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(work[i],work[j]);}
            for(size_t stage=2;stage<=length;stage<<=1){
                const auto step=std::polar(1.0,-2*pi/stage);
                for(size_t base=0;base<length;base+=stage){std::complex<double> w{1,0};
                    for(size_t j=0;j<stage/2;++j){const auto u=work[base+j],v=w*work[base+j+stage/2];work[base+j]=u+v;work[base+j+stage/2]=u-v;w*=step;}}
            }
            for(size_t b=0;b<numBins;++b){
                const double amplitudeScale=(b==0||b==length/2)?1:2;
                const double power=std::norm(work[b])*amplitudeScale*amplitudeScale/(sum*sum);
                const float level=float(10*std::log10(std::max(1e-12,power)));
                if(fast)fastLevels[size_t(stream)][channel][b]=level;
                else channelLevels[size_t(stream)][channel][b]=level;
                powers[b]+=.5*power;
            }
        }
        if(!fast)for(size_t b=0;b<bins;++b)destination[b]=float(10*std::log10(std::max(1e-12,powers[b])));
    }
    double rate=48000,windowSum=1;size_t count=0,lastCount=0;
    bool haveSequence=false;uint64_t nextSequence=0;
    double temporalPole=0,peakPole=0;
    std::array<std::array<Spectrum,2>,3> channelLevels{};
    std::array<std::array<std::array<float,fastBins>,2>,3> fastLevels{};
    std::array<std::array<double,2>,3> fastPower{},recentPower{};
    std::array<std::array<double,2>,2> jointPower{};
    std::array<AnalysisFrame,fftSize> frames{};
    std::array<double,fftSize> window{};std::array<double,bins> powers{};
    std::array<double,fastSize> fastWindow{};double fastWindowSum=1;
    std::array<std::complex<double>,fftSize> work{};
};
}
