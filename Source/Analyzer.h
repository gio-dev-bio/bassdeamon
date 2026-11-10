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
    static constexpr size_t fftSize=16384,bins=fftSize/2+1,waveSize=512,fastSize=8192,fastBins=fastSize/2+1;
    using Spectrum=std::array<float,bins>;
    using FastSpectrum=std::array<float,fastBins>;
    BeatScope scope;
    Spectrum pre{},post{},kick{},maskPre{},maskPost{};
    uint64_t discontinuities=0;
    std::array<float,waveSize> wavePre{},wavePost{},waveKick{},sumPre{},sumPost{};
    void prepare(double sr) noexcept {
        rate=sr;scope.prepare(sr);haveSequence=false;discontinuities=0;resetAnalysis();
        wavePre.fill(0);wavePost.fill(0);waveKick.fill(0);sumPre.fill(0);sumPost.fill(0);
        hop=std::max(size_t(256),size_t(sr*.010));
        windowSum=fastWindowSum=0;
        for(size_t i=0;i<fftSize;++i){window[i]=blackmanHarris(i,fftSize);windowSum+=window[i];}
        for(size_t i=0;i<fastSize;++i){fastWindow[i]=blackmanHarris(i,fastSize);fastWindowSum+=fastWindow[i];}
    }
    void consume(AnalysisQueue& queue,double bpm=120,bool hold=false) noexcept {
        const auto budget=queue.pending();
        AnalysisFrame f;for(uint32_t consumed=0;consumed<budget&&queue.pop(f);++consumed){
            if(haveSequence&&f.sequence!=nextSequence){resetAnalysis();scope.prepare(rate);++discontinuities;}
            haveSequence=true;nextSequence=f.sequence+1;
            for(auto* stream:{&f.pre,&f.post,&f.kick})for(auto& v:*stream)if(!std::isfinite(v))v=0;
            scope.tick(f,bpm,hold);frames[count%fftSize]=f;++count;
            // Fixed sample hops catch short kicks even if they finish between
            // two editor updates. Collision history is NOT a gate at the end
            // of a long FFT window.
            if(count>=fastSize&&count-lastFastCount>=hop){
                analyze(0,pre,true);analyze(1,post,true);analyze(2,kick,true);
                updateEvents();lastFastCount=count;
            }
        }
        if(count==lastCount)return;
        lastCount=count;
        if(count>=fftSize){analyze(0,pre);analyze(1,post);analyze(2,kick);updateMask();}
        for(size_t i=0;i<waveSize;++i){
            const auto peak=[&](size_t t){return std::abs(scope.low[t][i])>std::abs(scope.high[t][i])?scope.low[t][i]:scope.high[t][i];};
            wavePre[i]=peak(0);wavePost[i]=peak(1);waveKick[i]=peak(2);sumPre[i]=peak(3);sumPost[i]=peak(4);
        }
    }
    // Shared energy remains a conflict at unequal levels; a loud kick can
    // mask a much quieter bass. This helper is only an absolute activity gate.
    static float mask(float bass,float reference,float=-120,float=-120) noexcept {
        return std::clamp((std::min(bass,reference)+110.f)/12.f,0.f,1.f);
    }
    double sampleRate() const noexcept{return rate;}
    bool ready() const noexcept{return count>=fftSize;}
    uint64_t streamSequence() const noexcept{return haveSequence?nextSequence:0;}
    float referenceLevel() const noexcept{return referenceDb;}
    struct Summary {int regions=0;double strongestHz=0;};
    Summary summary(bool output) const noexcept {
        const auto& values=output?post:pre;const auto& map=output?maskPost:maskPre;
        Summary result;bool inside=false;float loudest=-121;size_t strongest=0;
        for(size_t b=1;b+1<bins;++b){const bool active=map[b]>=.12f;
            if(active&&!inside)++result.regions;inside=active;
            if(active&&values[b]>loudest){loudest=values[b];strongest=b;}}
        if(strongest){const double a=values[strongest-1],v=values[strongest],c=values[strongest+1],den=a-2*v+c;
            const double offset=std::abs(den)>1e-12?std::clamp(.5*(a-c)/den,-.5,.5):0;
            result.strongestHz=(double(strongest)+offset)*rate/fftSize;}
        return result;
    }
private:
    static constexpr double pi=3.14159265358979323846;
    static double blackmanHarris(size_t n,size_t length) noexcept {
        const double phase=2*pi*double(n)/double(length-1);
        return .35875-.48829*std::cos(phase)+.14128*std::cos(2*phase)-.01168*std::cos(3*phase);
    }
    void resetAnalysis() noexcept {
        count=lastCount=lastFastCount=0;frames={};referenceDb=-120;
        pre.fill(-120);post.fill(-120);kick.fill(-120);maskPre.fill(0);maskPost.fill(0);
        recentPre.fill(0);recentPost.fill(0);
        for(auto& stream:channelLevels)for(auto& channel:stream)channel.fill(-120);
        for(auto& stream:fastLevels)for(auto& channel:stream)channel.fill(-120);
    }
    template<size_t N> void buildSupport(const std::array<float,N>& levels,std::array<float,N>& support,size_t length) noexcept {
        support.fill(0);float maximum=-120;
        for(size_t b=1;b+1<N;++b){const double hz=double(b)*rate/length;if(hz>=20&&hz<=20000)maximum=std::max(maximum,levels[b]);}
        const float floor=std::max(-110.f,maximum-60.f);
        for(size_t p=1;p+1<N;++p){const double hz=double(p)*rate/length;const float level=levels[p];
            if(hz<20||hz>20000||level<=floor||level<=levels[p-1]||level<levels[p+1])continue;
            // Each real partial has its own local contour. Weak harmonics are
            // not discarded just because another partial is >18 dB louder.
            // BH sidelobes are ~92 dB below a stationary tone, outside this gate.
            const float strength=std::clamp((level-floor)/6.f,0.f,1.f),edge=level-12.f;
            size_t lo=p,hi=p;
            while(lo>1&&levels[lo-1]<=levels[lo]&&levels[lo-1]>edge)--lo;
            while(hi+1<N-1&&levels[hi+1]<=levels[hi]&&levels[hi+1]>edge)++hi;
            for(size_t b=lo;b<=hi;++b){const float shape=std::clamp((levels[b]-edge)/6.f,0.f,1.f);
                support[b]=std::max(support[b],shape*strength);}
        }
    }
    void updateEvents() noexcept {
        std::array<std::array<double,2>,3> energy{};
        std::array<std::array<double,2>,2> joint{};
        // Actual concurrent samples in this SAME short window, not two
        // independently smoothed envelopes from different points in time.
        for(size_t i=0;i<fastSize;++i){const auto& f=frames[(count-fastSize+i)%fftSize];const double weight=fastWindow[i]*fastWindow[i];
            for(size_t c=0;c<2;++c){const std::array<double,3> e{double(f.pre[c])*f.pre[c],double(f.post[c])*f.post[c],double(f.kick[c])*f.kick[c]};
                for(size_t t=0;t<3;++t)energy[t][c]+=weight*e[t];
                for(size_t t=0;t<2;++t)joint[t][c]+=weight*std::min(e[t],e[2]);}}
        std::array<std::array<float,2>,2> coincidence{};
        for(size_t t=0;t<2;++t)for(size_t c=0;c<2;++c){const double base=std::min(energy[t][c],energy[2][c]);
            coincidence[t][c]=base>1e-20?float(std::clamp((joint[t][c]/base-.01)/.10,0.0,1.0)):0;}
        std::array<float,3> active{};active.fill(-120);
        for(size_t t=0;t<3;++t)for(size_t c=0;c<2;++c)
            active[t]=std::max(active[t],*std::max_element(fastLevels[t][c].begin(),fastLevels[t][c].end()));
        referenceDb=active[2];
        const double elapsed=lastFastCount?double(count-lastFastCount)/rate:double(hop)/rate;
        const float decay=float(std::exp(-elapsed/.16)); // readable recent events, ~340 ms to -17 dB
        for(size_t b=0;b<fastBins;++b)for(size_t t=0;t<2;++t){float shared=0,event=0;
            for(size_t c=0;c<2;++c){const float common=std::sqrt(fastSupport[t][c][b]*fastSupport[2][c][b]);
                shared=std::max(shared,common);event=std::max(event,common*coincidence[t][c]);}
            auto& recent=t==0?recentPre[b]:recentPost[b];
            // A continuing bass has changed note (or panning): discard obsolete
            // frequency support. A finished short kick may retain its recent hit.
            if(active[t]>-110&&active[2]>-110&&shared<.02f)recent=0;
            else recent=std::max(recent*decay,event);
        }
    }
    void updateMask() noexcept {
        for(size_t b=0;b<bins;++b){maskPre[b]=maskPost[b]=0;const double hz=double(b)*rate/fftSize;
            if(hz<20||hz>20000)continue;
            const double position=double(b)*fastSize/fftSize;const size_t lo=std::min(fastBins-1,size_t(position)),hi=std::min(fastBins-1,lo+1);
            const float fraction=float(position-double(lo));
            for(size_t t=0;t<2;++t){float fine=0;
                for(size_t c=0;c<2;++c)fine=std::max(fine,std::sqrt(fineSupport[t][c][b]*fineSupport[2][c][b]));
                const auto& recent=t==0?recentPre:recentPost;
                const float event=recent[lo]+fraction*(recent[hi]-recent[lo]);
                (t==0?maskPre[b]:maskPost[b])=std::min(fine,event);
            }
        }
    }
    void analyze(int stream,Spectrum& destination,bool fast=false) noexcept {
        powers.fill(0);
        const size_t length=fast?fastSize:fftSize,valid=std::min(count,length),numBins=length/2+1;
        const double sum=fast?fastWindowSum:windowSum;
        for(size_t channel=0;channel<2;++channel){
            bool nonzero=false;
            for(size_t i=0;i<length;++i){double sample=0;if(i>=length-valid){const auto& f=frames[(count-valid+i-(length-valid))%fftSize];
                sample=stream==0?f.pre[channel]:stream==1?f.post[channel]:f.kick[channel];}
                nonzero|=sample!=0;work[i]=sample*(fast?fastWindow[i]:window[i]);}
            if(nonzero){
                for(size_t i=1,j=0;i<length;++i){size_t bit=length>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(work[i],work[j]);}
                for(size_t stage=2;stage<=length;stage<<=1){const auto step=std::polar(1.0,-2*pi/stage);
                    for(size_t base=0;base<length;base+=stage){std::complex<double> w{1,0};for(size_t j=0;j<stage/2;++j){const auto u=work[base+j],v=w*work[base+j+stage/2];work[base+j]=u+v;work[base+j+stage/2]=u-v;w*=step;}}}
            }
            for(size_t b=0;b<numBins;++b){const double scale=(b==0||b==length/2)?1:2;
                const double power=std::norm(work[b])*scale*scale/(sum*sum);const float level=float(10*std::log10(std::max(1e-12,power)));
                if(fast)fastLevels[size_t(stream)][channel][b]=level;else channelLevels[size_t(stream)][channel][b]=level;
                powers[b]+=.5*power;}
            if(fast){
                auto& levels=fastLevels[size_t(stream)][channel];auto& support=fastSupport[size_t(stream)][channel];
                buildSupport(levels,support,fastSize);
                // A short window cannot resolve a weak partial beside a loud
                // fundamental. Allow broadband temporal evidence here; the
                // long, high-resolution spectrum must ALSO contain a real peak.
                const float peak=*std::max_element(levels.begin(),levels.end());
                const float floor=std::max(-110.f,peak-60.f);
                for(size_t b=1;b+1<fastBins;++b)
                    support[b]=std::max(support[b],std::clamp((levels[b]-floor)/12.f,0.f,1.f));
            }
            else buildSupport(channelLevels[size_t(stream)][channel],fineSupport[size_t(stream)][channel],fftSize);
        }
        if(!fast)for(size_t b=0;b<bins;++b)destination[b]=float(10*std::log10(std::max(1e-12,powers[b])));
    }
    double rate=48000,windowSum=1,fastWindowSum=1;size_t count=0,lastCount=0,lastFastCount=0,hop=480;
    bool haveSequence=false;uint64_t nextSequence=0;float referenceDb=-120;
    std::array<std::array<Spectrum,2>,3> channelLevels{},fineSupport{};
    std::array<std::array<FastSpectrum,2>,3> fastLevels{},fastSupport{};
    FastSpectrum recentPre{},recentPost{};
    std::array<AnalysisFrame,fftSize> frames{};
    std::array<double,fftSize> window{};std::array<double,bins> powers{};
    std::array<double,fastSize> fastWindow{};
    std::array<std::complex<double>,fftSize> work{};
};
}
