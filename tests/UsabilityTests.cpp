#include "Dsp.h"
#include "Analyzer.h"
#include <iostream>
#include <random>
#include <stdexcept>
using namespace bassdeamon;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
    // Quarter-note timing must follow the host, including triplets and dotted notes.
    check(noteMilliseconds(4,120)==500,"quarter at 120");
    check(noteMilliseconds(2,60)==250,"sixteenth at 60");
    check(std::abs(noteMilliseconds(7,120)-166.6666667)<1e-5,"triplet timing");
    check(noteMilliseconds(8,120)==375,"dotted timing");
    check(noteMilliseconds(4,0)==500,"missing host tempo fallback");
    for(double sr:{44100.,48000.,96000.})for(double bpm:{60.,120.,173.}){
        BeatScope scope;scope.prepare(sr);const int period=int(std::ceil(sr*60/bpm));
        for(int i=0;i<period;++i){AnalysisFrame f;f.beat=i*bpm/(60*sr);f.pre[0]=i==50?1.f:0;scope.tick(f,bpm,false);}
        check(scope.completed==0,"scope must hold incomplete quarter");
        AnalysisFrame next;next.beat=period*bpm/(60*sr);scope.tick(next,bpm,false);
        check(scope.completed==1,"scope must publish a full quarter");
        check(*std::max_element(scope.high[0].begin(),scope.high[0].end())==1,"scope lost transient");
        const auto frozen=scope.high;
        for(int i=period+1;i<3*period;++i){AnalysisFrame f;f.beat=i*bpm/(60*sr);f.pre[0]=.3f;scope.tick(f,bpm,true);}
        check(scope.high==frozen,"scope HOLD moved");
        check(std::abs(scope.milliseconds-60000/bpm)<.001,"scope time scale wrong");
    }
    // The old state starts with automatic GLUE and manual UNMASK milliseconds.
    Engine e;e.prepare(48000);Settings s;s.glueTiming=2;s.glueAttackNote=3;s.glueReleaseNote=4;
    s.unmaskSync=true;s.unmaskAttackNote=2;s.unmaskReleaseNote=5;
    std::array<float,256> x{};float* out[]{x.data()};
    e.process(out,1,nullptr,0,256,s,120);
    check(e.getMeters().attackMs==250&&e.getMeters().releaseMs==500,"GLUE sync conversion");
    check(e.getMeters().unmaskAttackMs==125&&e.getMeters().unmaskReleaseMs==1000,"UNMASK sync conversion");
    e.process(out,1,nullptr,0,256,s,60);
    check(e.getMeters().releaseMs==1000&&e.getMeters().unmaskReleaseMs==2000,"tempo change ignored");
    // Learn must reach its decision across short looping transport seeks.
    e.prepare(48000);e.startLearn();s=Settings{};s.glue=s.unmask=false;
    std::mt19937 rng(91);std::uniform_real_distribution<float> rnd(-.4f,.4f);
    std::vector<float> source(4*48000+256);for(auto& v:source)v=rnd(rng);
    std::array<float,256> k{};const float* side[]{k.data()};
    for(int pos=0;pos<4*48000;pos+=256){
        if(pos>0&&pos%12032==0)e.clearHistory(true);
        for(int n=0;n<256;++n){x[n]=source[size_t(pos+n)];k[n]=pos+n>=96?-source[size_t(pos+n-96)]:0;}
        e.process(out,1,side,1,256,s);
    }
    check(!e.learning()&&e.learnResult().valid&&e.learnResult().invert,"loop reset broke learner");
    check(std::abs(e.learnResult().delayMs-2)<.26,"loop learner delay wrong");
    // ADDED ONLY must remove dry, and reconstruct the normal Tube output.
    Engine wet,added;wet.prepare(48000);added.prepare(48000);
    Settings w;w.glue=w.phase=w.unmask=false;w.tube.enabled=true;w.tube.driveDb=18;w.tube.amount=1;
    auto a=w;a.tube.audition=true;
    std::array<float,256> y{};float* addOut[]{y.data()};double residual=0,error=0,dryPower=0;
    for(int pos=0;pos<48000;pos+=256){for(int n=0;n<256;++n)x[n]=y[n]=float(.6*std::sin(2*pi*75*(pos+n)/48000.));
        wet.process(out,1,nullptr,0,256,w);added.process(addOut,1,nullptr,0,256,a);
        if(pos>24000)for(int n=0;n<256;++n){const double dry=.6*std::sin(2*pi*75*(pos+n)/48000.);residual+=y[n]*y[n];dryPower+=dry*dry;error+=std::pow(x[n]-y[n]-dry,2);}
    }
    std::cout<<"tube added level relative to dry: "<<10*std::log10(residual/dryPower)<<" dB; reconstruction error "<<10*std::log10(error/dryPower)<<" dB\n";
    check(residual/dryPower>.00001,"Tube generator has no measurable effect");
    check(error/dryPower<1e-12,"Tube ADDED ONLY does not reconstruct output");
    // Measure actual frequency attenuation; Auto Gain must not hide this test.
    for(double frequency:{55.,75.,110.,220.})for(double depth:{.4,1.}){
        Engine unmask;unmask.prepare(48000);Settings u;u.glue=false;u.unmaskAmount=depth;u.unmaskTrack=false;u.unmaskFrequency=frequency;
        double input=0,output=0;
        for(int pos=0;pos<3*48000;pos+=256){for(int n=0;n<256;++n){x[n]=float(.3*std::sin(2*pi*frequency*(pos+n)/48000.));k[n]=x[n]*1.5f;}
            unmask.process(out,1,side,1,256,u,120);
            if(pos>2*48000)for(int n=0;n<256;++n){input+=k[n]*k[n]/2.25;output+=x[n]*x[n];}
        }
        const double attenuation=10*std::log10(output/input);
        std::cout<<"UNMASK "<<frequency<<" Hz depth "<<depth<<": "<<attenuation<<" dB actual output / meter "<<unmask.getMeters().unmaskReductionDb<<" dB (largest band)\n";
        check(attenuation<-.2,"UNMASK no measurable cut with overlapping signals");
    }
    std::cout<<"PASS usability / timing / quarter scope / learning across loops / Tube audition / UNMASK response\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
