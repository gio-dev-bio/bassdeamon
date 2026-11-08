#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>

namespace bassdeamon {
struct AnalysisFrame {std::array<float,2> pre{},post{},kick{};};
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

// Message-thread analysis only. Stereo powers are averaged, not mono-summed:
// anti-phase stereo must not disappear from the frequency display.
class RelationAnalyzer {
public:
    static constexpr size_t fftSize=4096,bins=fftSize/2+1,waveSize=512;
    using Spectrum=std::array<float,bins>;
    Spectrum pre{},post{},kick{},maskPre{},maskPost{};
    std::array<float,waveSize> wavePre{},wavePost{},waveKick{},sumPre{},sumPost{};
    void prepare(double sr) noexcept {
        rate=sr;count=lastCount=0;frames={};pre.fill(-120);post.fill(-120);kick.fill(-120);
        maskPre.fill(0);maskPost.fill(0);wavePre.fill(0);wavePost.fill(0);waveKick.fill(0);sumPre.fill(0);sumPost.fill(0);
        windowSum=0;for(size_t i=0;i<fftSize;++i){window[i]=.5-.5*std::cos(2*pi*i/(fftSize-1));windowSum+=window[i];}
    }
    void consume(AnalysisQueue& queue) noexcept {
        AnalysisFrame f;while(queue.pop(f)){
            for(auto* stream:{&f.pre,&f.post,&f.kick})for(auto& v:*stream)if(!std::isfinite(v))v=0;
            frames[count%fftSize]=f;++count;
        }
        if(count==lastCount)return;lastCount=count;
        analyze(0,pre);analyze(1,post);analyze(2,kick);
        for(size_t b=0;b<bins;++b){maskPre[b]=mask(pre[b],kick[b]);maskPost[b]=mask(post[b],kick[b]);}
        // ~85 ms at 48 kHz, decimated scope samples. Display L;
        // spectrum above uses both channels. Sum is a visual preview, never audio.
        const size_t valid=std::min<size_t>(count,fftSize),oldest=count>=fftSize?count%fftSize:0;
        for(size_t i=0;i<waveSize;++i){
            const size_t pos=i*fftSize/waveSize;
            const AnalysisFrame v=pos<fftSize-valid?AnalysisFrame{}:frames[(oldest+pos-(fftSize-valid))%fftSize];
            wavePre[i]=v.pre[0];wavePost[i]=v.post[0];waveKick[i]=v.kick[0];sumPre[i]=v.pre[0]+v.kick[0];sumPost[i]=v.post[0]+v.kick[0];
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
