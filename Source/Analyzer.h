#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>

namespace bassdeamon {
struct AnalysisFrame {std::array<float,2> pre{},post{},kick{};double beat=-1;};
// One audio-thread producer, one editor-thread consumer. Never waits for the UI.
class AnalysisQueue {
public:
    static_assert(std::atomic<uint32_t>::is_always_lock_free,"Analysis counters must be lock free");
    static constexpr uint32_t capacity=8192;
    bool push(const AnalysisFrame& frame) noexcept {
        const auto w=write.load(std::memory_order_relaxed),r=read.load(std::memory_order_acquire);
        if(w-r>=capacity){dropped.fetch_add(1,std::memory_order_relaxed);return false;}
        frames[w%capacity]=frame;write.store(w+1,std::memory_order_release);return true;
    }
    bool pop(AnalysisFrame& frame) noexcept {
        const auto r=read.load(std::memory_order_relaxed),w=write.load(std::memory_order_acquire);
        if(r==w)return false;frame=frames[r%capacity];read.store(r+1,std::memory_order_release);return true;
    }
    std::atomic<bool> enabled{false};std::atomic<uint32_t> dropped{0};
private:
    std::array<AnalysisFrame,capacity> frames{};
    std::atomic<uint32_t> write{0},read{0};
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
    static constexpr size_t fftSize=8192,bins=fftSize/2+1,waveSize=512;
    using Spectrum=std::array<float,bins>;
    BeatScope scope;
    Spectrum pre{},post{},kick{},maskPre{},maskPost{};
    std::array<float,waveSize> wavePre{},wavePost{},waveKick{},sumPre{},sumPost{};
    void prepare(double sr) noexcept {
        scope.prepare(sr);rate=sr;count=lastCount=0;frames={};pre.fill(-120);post.fill(-120);kick.fill(-120);
        maskPre.fill(0);maskPost.fill(0);wavePre.fill(0);wavePost.fill(0);waveKick.fill(0);sumPre.fill(0);sumPost.fill(0);
        windowSum=0;for(size_t i=0;i<fftSize;++i){window[i]=.5-.5*std::cos(2*pi*i/(fftSize-1));windowSum+=window[i];}
    }
    void consume(AnalysisQueue& queue,double bpm=120,bool hold=false) noexcept {
        AnalysisFrame f;while(queue.pop(f)){
            for(auto* stream:{&f.pre,&f.post,&f.kick})for(auto& v:*stream)if(!std::isfinite(v))v=0;
            scope.tick(f,bpm,hold);frames[count%fftSize]=f;++count;
        }
        if(count==lastCount)return;lastCount=count;
        analyze(0,pre);analyze(1,post);analyze(2,kick);
        for(size_t b=0;b<bins;++b){maskPre[b]=mask(pre[b],kick[b]);maskPost[b]=mask(post[b],kick[b]);}
        for(size_t i=0;i<waveSize;++i){
            const auto peak=[&](size_t t){return std::abs(scope.low[t][i])>std::abs(scope.high[t][i])?scope.low[t][i]:scope.high[t][i];};
            wavePre[i]=peak(0);wavePost[i]=peak(1);waveKick[i]=peak(2);sumPre[i]=peak(3);sumPost[i]=peak(4);
        }
    }
    static float mask(float bass,float reference) noexcept {
        // Potential masking only: both must be audible above a fixed analysis
        // floor and within 24 dB. Not a psychoacoustic audibility certificate.
        const float activity=std::clamp((std::min(bass,reference)+72.f)/24.f,0.f,1.f);
        return activity*std::clamp(1.f-std::abs(bass-reference)/24.f,0.f,1.f);
    }
    double sampleRate() const noexcept{return rate;}
private:
    static constexpr double pi=3.14159265358979323846;
    void analyze(int stream,Spectrum& destination) noexcept {
        powers.fill(0);
        const size_t valid=std::min<size_t>(count,fftSize),oldest=count>=fftSize?count%fftSize:0;
        for(size_t channel=0;channel<2;++channel){
            for(size_t i=0;i<fftSize;++i){
                double sample=0;if(i>=fftSize-valid){const auto& f=frames[(oldest+i-(fftSize-valid))%fftSize];
                    sample=stream==0?f.pre[channel]:stream==1?f.post[channel]:f.kick[channel];}
                work[i]=sample*window[i];
            }
            for(size_t i=1,j=0;i<fftSize;++i){size_t bit=fftSize>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(work[i],work[j]);}
            for(size_t length=2;length<=fftSize;length<<=1){
                const auto step=std::polar(1.0,-2*pi/length);
                for(size_t base=0;base<fftSize;base+=length){std::complex<double> w{1,0};
                    for(size_t j=0;j<length/2;++j){const auto u=work[base+j],v=w*work[base+j+length/2];work[base+j]=u+v;work[base+j+length/2]=u-v;w*=step;}}
            }
            for(size_t b=0;b<bins;++b)powers[b]+=.5*std::norm(work[b])*4/(windowSum*windowSum);
        }
        for(size_t b=0;b<bins;++b)destination[b]=float(10*std::log10(std::max(1e-12,powers[b])));
    }
    double rate=48000,windowSum=1;size_t count=0,lastCount=0;
    std::array<AnalysisFrame,fftSize> frames{};
    std::array<double,fftSize> window{};std::array<double,bins> powers{};
    std::array<std::complex<double>,fftSize> work{};
};
}
