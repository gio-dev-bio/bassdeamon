#include "SpectrumDisplay.h"
#include "Dsp.h"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace bassdeamon;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
float near(const RelationAnalyzer::Spectrum& a,double hz,double sr){float result=0;for(size_t b=1;b<a.size();++b)if(std::abs(double(b)*sr/RelationAnalyzer::fftSize-hz)<8)result=std::max(result,a[b]);return result;}
int main(){try{
    int cases=0;
    for(double sr:{44100.,48000.,96000.})for(double reduction:{0.,6.,12.,20.,24.})for(int mode=0;mode<4;++mode){
        auto a=std::make_unique<RelationAnalyzer>();auto q=std::make_unique<AnalysisQueue>();auto display=std::make_unique<SpectrumDisplay>();a->prepare(sr);
        Engine engine;engine.prepare(sr);Settings s;s.glue=s.phase=false;s.unmaskDirect=true;s.unmaskAttenuationDb=reduction;s.unmaskTrack=false;s.unmaskFrequency=75;s.unmaskAttackMs=1;
        std::array<float,128> dry{},wet{},kick{};float* out[]{wet.data()};const float* side[]{kick.data()};double next=sr/30;uint64_t last=0;
        for(int pos=0;pos<int(sr*1.7);pos+=128){for(int n=0;n<128;++n)dry[size_t(n)]=wet[size_t(n)]=kick[size_t(n)]=float(.4*std::sin(2*pi*75*(pos+n)/sr));
            engine.process(out,1,side,1,128,s);
            for(int n=0;n<128;++n){AnalysisFrame f;f.pre={dry[size_t(n)],dry[size_t(n)]};f.post={wet[size_t(n)],wet[size_t(n)]};f.kick={kick[size_t(n)],kick[size_t(n)]};require(q->push(f),"Response queue overflow");}
            if(pos+128>=next){a->consume(*q);display->push(*a,a->consumedFrames-last,(pos+128)/sr,mode);last=a->consumedFrames;next+=sr/30;}
        }
        a->consume(*q);display->push(*a,a->consumedFrames-last,1.7,mode);
        const float pre=near(display->shown[3],75,sr),post=near(display->shown[4],75,sr);
        require(pre>.8f,"PRE collision missing in response test");
        const float expected=float(std::pow(10.,-reduction/20));
        if(std::abs(post/pre-expected)>.025f){std::cerr<<sr<<" mode "<<mode<<" cut "<<reduction<<" scores "<<pre<<" "<<post<<" expected "<<expected<<'\n';return 1;}
        if(reduction>=20)require(post<.12f,"20+ dB actual cut still displays same red collision");
        const size_t peakBin=size_t(std::round(75*RelationAnalyzer::fftSize/sr));
        const auto& paired=display->paired;
        require(std::abs(paired[0][peakBin]-paired[2][peakBin])<.01,"BASS - KICK readout not calibrated");
        require(std::abs(paired[1][peakBin]-paired[0][peakBin]+reduction)<.3,"POST - PRE readout not calibrated");
        ++cases;
    }
    // Knob says 12 dB, final trim restores all 12 dB: POST must still flag.
    {
        Engine e;e.prepare(48000);Settings s;s.glue=s.phase=false;s.unmaskDirect=true;s.unmaskAttenuationDb=12;s.outputDb=12;s.unmaskTrack=false;s.unmaskFrequency=75;s.unmaskAttackMs=1;
        auto a=std::make_unique<RelationAnalyzer>();auto q=std::make_unique<AnalysisQueue>();a->prepare(48000);std::array<float,128>x{},k{};float* out[]{x.data()};const float* side[]{k.data()};
        for(int b=0;b<700;++b){for(int n=0;n<128;++n)x[size_t(n)]=k[size_t(n)]=float(.1*std::sin(2*pi*75*(b*128+n)/48000.));e.process(out,1,side,1,128,s);
            for(int n=0;n<128;++n){AnalysisFrame f;f.pre={k[size_t(n)],k[size_t(n)]};f.post={x[size_t(n)],x[size_t(n)]};f.kick=f.pre;q->push(f);}if(b%12==0)a->consume(*q);}
        a->consume(*q);require(near(a->maskPost,75,48000)>.8,"Detector trusts knob instead of actual output");
    }
    for(int mode=0;mode<4;++mode){auto a=std::make_unique<RelationAnalyzer>();auto q=std::make_unique<AnalysisQueue>();auto d=std::make_unique<SpectrumDisplay>();a->prepare(48000);
        uint64_t last=0;int lastTick=0;bool pulseSeen=false;size_t revisionsAfterReady=0;
        for(int n=0;n<3*48000;++n){double t=n/48000.;const double since=t-.6;
            const float bass=float(.4*std::sin(2*pi*75*t));const float kick=since>=0&&since<.02?float(.4*std::sin(pi*since/.02)*std::sin(2*pi*75*t)):0;
            AnalysisFrame f;f.pre={bass,bass};f.post=f.pre;f.kick={kick,kick};if(t>1.2){f.kick=f.pre;f.post={bass*.1f,bass*.1f};}
            require(q->push(f),"Rate fixture queue overflow");
            if(n/1600>lastTick){lastTick=n/1600;a->consume(*q);const auto oldRevision=d->revision;const auto old=d->shown[0];d->push(*a,a->consumedFrames-last,t,mode);last=a->consumedFrames;
                if(d->revision==oldRevision)require(old==d->shown[0],"Visible frame changed between selected refreshes");
                if(t>.6&&t<1.2&&near(d->shown[3],75,48000)>=.12)pulseSeen=true;
                if(t>2.2)require(near(d->shown[4],75,48000)<.12,"Slow display retains old POST collision after actual attenuation");
                revisionsAfterReady=d->revision;
            }
        }
        require(pulseSeen,"Slow rate lost 20 ms collision event");
        const double expected=(3.-double(RelationAnalyzer::fftSize)/48000)*SpectrumDisplay::fps(mode);
        if(std::abs(double(revisionsAfterReady)-expected)>=3){std::cerr<<"Rate "<<SpectrumDisplay::fps(mode)<<" actual frames "<<revisionsAfterReady<<" expected "<<expected<<"\n";return 1;}
    }
    // Musical use case: repeated short kicks, a sustained bass plus harmonic,
    // real attack/release and slow display. The 20 dB target is not assumed to
    // equal 20 dB throughout the transient: inspect actual processed spectra.
    {
        Engine e;e.prepare(48000);Settings s;s.glue=s.phase=false;s.unmaskDirect=true;s.unmaskAttenuationDb=20;s.unmaskTrack=false;s.unmaskFrequency=75;s.unmaskAttackMs=1;s.unmaskReleaseMs=500;
        auto a=std::make_unique<RelationAnalyzer>();auto q=std::make_unique<AnalysisQueue>();auto d=std::make_unique<SpectrumDisplay>();a->prepare(48000);
        std::array<float,128> dry{},wet{},kick{};float* out[]{wet.data()};const float* side[]{kick.data()};uint64_t last=0;float worstPost=0,strongestPre=0;
        for(int b=0;b<1500;++b){for(int n=0;n<128;++n){const double t=(b*128+n)/48000.,phase=std::fmod(t,.5);
            dry[size_t(n)]=wet[size_t(n)]=float(.4*std::sin(2*pi*75*t)+.08*std::sin(2*pi*150*t));
            kick[size_t(n)]=phase<.02?float(.4*std::sin(pi*phase/.02)*std::sin(2*pi*75*t)):0;}
            e.process(out,1,side,1,128,s);for(int n=0;n<128;++n){AnalysisFrame f;f.pre={dry[size_t(n)],dry[size_t(n)]};f.post={wet[size_t(n)],wet[size_t(n)]};f.kick={kick[size_t(n)],kick[size_t(n)]};q->push(f);}
            if(b%12==0){a->consume(*q);d->push(*a,a->consumedFrames-last,(b+1)*128/48000.,3);last=a->consumedFrames;
                if(b>750){strongestPre=std::max(strongestPre,near(d->shown[3],75,48000));worstPost=std::max(worstPost,near(d->shown[4],75,48000));}}
        }
        require(strongestPre>.8f&&worstPost<.95f,"Real repeated kick ducking still looks identical in PRE and POST");
        std::cout<<"Repeated 20 ms kicks / actual 20 dB target / 4 fps: strongest PRE "<<strongestPre<<" worst steady POST "<<worstPost<<" (target and actual window attenuation distinguished)\n";
    }
    std::cout<<"PASS "<<cases<<" real UNMASK/PRE/POST severity cases at four rates; 20 ms events retained; old markers expire; output-trim cancellation detected\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
