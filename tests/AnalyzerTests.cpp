#include "Analyzer.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace bassdeamon;
using Analyzer=RelationAnalyzer;
constexpr double pi=3.14159265358979323846;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class Signal>std::unique_ptr<Analyzer> render(double rate,Signal signal){
    auto queue=std::make_unique<AnalysisQueue>();auto result=std::make_unique<Analyzer>();result->prepare(rate);
    for(size_t n=0;n<Analyzer::fftSize;++n)check(queue->push(signal(n)),"fixture queue overflow");
    result->consume(*queue);return result;
}
float tone(double hz,size_t n,double sr,float amplitude=.3f){return amplitude*float(std::sin(2*pi*hz*n/sr));}
size_t flagged(const Analyzer::Spectrum& mask){return size_t(std::count_if(mask.begin(),mask.end(),[](float v){return v>=.2f;}));}
int main(){try{
    for(double sr:{44100.,48000.,96000.}){
        const double hz=64*sr/Analyzer::fftSize;
        auto equal=render(sr,[&](size_t n){AnalysisFrame f;const float value=tone(hz,n,sr);f.pre={value,-value};f.post=f.pre;f.kick={value,value};return f;});
        check(equal->maskPre[64]>.9f,"equal significant tones missing");
        check(flagged(equal->maskPre)<=3,"coherent narrow overlap spread across unrelated bins");
        const double expected=20*std::log10(.3);
        check(std::abs(equal->pre[64]-expected)<.001,"coherent sine dBFS calibration");
        auto separate=render(sr,[&](size_t n){AnalysisFrame f;const float bass=tone(75,n,sr),kick=tone(150,n,sr);f.pre={bass,bass};f.post=f.pre;f.kick={kick,kick};return f;});
        check(flagged(separate->maskPre)==0,"different notes falsely collide through FFT leakage");
        auto unequal=render(sr,[&](size_t n){AnalysisFrame f;const float bass=tone(hz,n,sr),kick=bass*.0316228f;f.pre={bass,bass};f.post=f.pre;f.kick={kick,kick};return f;});
        check(flagged(unequal->maskPre)>0,"30 dB imbalance hid shared frequency");
        auto noiseFloor=render(sr,[&](size_t n){AnalysisFrame f;const float quiet=tone(300,n,sr,.0001f),bass=tone(75,n,sr)+quiet,kick=tone(150,n,sr)+quiet;f.pre={bass,bass};f.post=f.pre;f.kick={kick,kick};return f;});
        check(flagged(noiseFloor->maskPre)==0,"shared quiet residue or leakage marked as collision");
        auto harmonic=render(sr,[&](size_t n){AnalysisFrame f;const float bass=tone(75,n,sr)+tone(150,n,sr,.15f),kick=tone(75,n,sr)+tone(300,n,sr,.15f);f.pre={bass,bass};f.post=f.pre;f.kick={kick,kick};return f;});
        check(flagged(harmonic->maskPre)>0&&flagged(harmonic->maskPre)<=5,"shared partial not selective");
        for(size_t b=0;b<Analyzer::bins;++b)if(harmonic->maskPre[b]>=.2f)
            check(std::abs(double(b)*sr/Analyzer::fftSize-75)<3*sr/Analyzer::fftSize,"unrelated harmonics highlighted");
        auto alternating=render(sr,[&](size_t n){AnalysisFrame f;const float value=tone(hz,n,sr);if(n<Analyzer::fftSize/2)f.pre={value,value};else f.kick={value,value};f.post=f.pre;return f;});
        check(flagged(alternating->maskPre)==0,"non-simultaneous notes falsely collide in long FFT window");
        auto changedNote=render(sr,[&](size_t n){AnalysisFrame f;const float bass=tone(n<Analyzer::fftSize/2?75:150,n,sr);f.pre={bass,bass};f.post=f.pre;if(n>=Analyzer::fftSize/2){const float kick=tone(75,n,sr);f.kick={kick,kick};}return f;});
        check(flagged(changedNote->maskPre)==0,"old bass note falsely collides with current kick");
        auto stereo=render(sr,[&](size_t n){AnalysisFrame f;const float value=tone(hz,n,sr);f.pre={value,0};f.post=f.pre;f.kick={0,value};return f;});
        check(flagged(stereo->maskPre)==0,"different stereo channels falsely collide");
        auto post=render(sr,[&](size_t n){AnalysisFrame f;const float value=tone(hz,n,sr);f.pre={value,value};f.post={tone(hz*2,n,sr),tone(hz*2,n,sr)};f.kick=f.pre;return f;});
        check(post->maskPre[64]>.9f&&flagged(post->maskPost)==0,"POST ignores actual processed level");
        auto dc=render(sr,[&](size_t){AnalysisFrame f;f.pre={.2f,.2f};return f;});
        check(std::abs(dc->pre[0]-20*std::log10(.2))<.001,"DC has erroneous +6 dB boost");
        auto nyquist=render(sr,[&](size_t n){AnalysisFrame f;const float value=n%2?.2f:-.2f;f.pre={value,value};return f;});
        check(std::abs(nyquist->pre[Analyzer::fftSize/2]-20*std::log10(.2))<.001,"Nyquist has erroneous +6 dB boost");
        auto offset=render(sr,[&](size_t n){AnalysisFrame f;const float value=tone(75,n,sr);f.pre={value,value};f.post=f.pre;f.kick=f.pre;return f;});
        const auto peak=size_t(std::max_element(offset->pre.begin(),offset->pre.end())-offset->pre.begin());
        check(std::abs(double(peak)*sr/Analyzer::fftSize-75)<=sr/Analyzer::fftSize*.501,"off-bin frequency placement");
        check(offset->pre[peak]>=expected-1.43&&offset->pre[peak]<=expected+.001,"unexpected Blackman-Harris scalloping/calibration");
        check(flagged(offset->maskPre)<=5,"off-bin overlap too broad");
        std::cout<<"PASS "<<sr<<" Hz: calibrated FFT, narrow/shared tones, separated notes, quiet residue, stereo, time separation, POST\n";
    }
    auto queue=std::make_unique<AnalysisQueue>();auto analyzer=std::make_unique<Analyzer>();analyzer->prepare(48000);
    for(uint32_t n=0;n<queue->capacity;++n){AnalysisFrame f;const auto value=tone(75,n,48000);f.pre={value,value};f.kick=f.pre;check(queue->push(f),"gap setup");}
    check(!queue->push({}),"gap setup did not drop");analyzer->consume(*queue);
    for(size_t n=0;n<Analyzer::fftSize;++n){AnalysisFrame f;const auto value=tone(150,n,48000);f.pre={value,value};check(queue->push(f),"gap refill");}
    analyzer->consume(*queue);check(analyzer->discontinuities==1,"dropped audio not detected");
    check(flagged(analyzer->maskPre)==0,"old kick/history survived discontinuity");
    for(size_t n=0;n<Analyzer::fftSize;++n)check(queue->push({}),"silence refill");
    analyzer->consume(*queue);check(*std::max_element(analyzer->pre.begin(),analyzer->pre.end())<=-119.9f&&flagged(analyzer->maskPre)==0,"silence left stale collision");
    std::cout<<"PASS queue discontinuity reset and silence\n";
    // Message-thread cost only; audio callback still only writes the SPSC queue.
    const auto begin=std::chrono::steady_clock::now();
    for(int frame=0;frame<20;++frame){for(int n=0;n<1600;++n){AnalysisFrame f;const auto value=tone(75,size_t(frame*1600+n),48000);f.pre={value,value};f.post=f.pre;f.kick=f.pre;queue->push(f);}analyzer->consume(*queue);}
    std::cout<<"Analysis mean ms/frame: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/20<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
