#include "Dsp.h"
#include "baselines/Dsp020.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <new>
#include <string>

#include "RealtimeAllocation.h"
using Clock=std::chrono::steady_clock;
constexpr double pi=3.14159265358979323846;
volatile double checksum=0;

template<class S>S settings(const std::string& mode){
    S s;s.glue=s.phase=s.unmask=false;
    if(mode=="glue")s.glue=true;
    if(mode=="unmask")s.unmask=true;
    if(mode=="all"||mode=="agc"||mode=="learn"||mode=="automation"||mode=="notes_all"||mode=="tail")s.glue=s.phase=s.unmask=true;
    if(mode=="agc")for(auto& a:s.agc)a.enabled=true;
    if(mode=="all"||mode=="automation"){s.delayMs=1.375;s.rotate=true;}
    return s;
}
inline float bassSignal(int n,double sr){const double t=n/sr,f=75;return static_cast<float>(0.28*std::sin(2*pi*f*t)+0.06*std::sin(2*pi*2*f*t));}
inline float kickSignal(int n,double sr){const double t=n/sr,b=std::fmod(t,0.5);return static_cast<float>(0.65*std::exp(-b*24)*std::sin(2*pi*57*t));}

template<class E,class S>void performance(std::ofstream& csv,const char* revision,double sr,int block,const std::string& mode){
    constexpr int repeats=3;const int blocks=static_cast<int>(sr*0.6/block);
    std::vector<float> source(static_cast<size_t>((blocks+30)*block)),side(source.size()),l(static_cast<size_t>(block)),r(l.size());
    for(size_t i=0;i<source.size();++i){source[i]=bassSignal(static_cast<int>(i),sr);side[i]=kickSignal(static_cast<int>(i),sr);}
    std::vector<double> durations;durations.reserve(static_cast<size_t>(blocks*repeats));double total=0;size_t allocs=0;
    for(int repeat=0;repeat<repeats;++repeat){E e;e.prepare(sr);auto s=settings<S>(mode);if(mode=="learn")e.startLearn();
        for(int b=0;b<blocks+30;++b){const size_t offset=static_cast<size_t>(b*block);
            for(int i=0;i<block;++i){l[static_cast<size_t>(i)]=source[offset+static_cast<size_t>(i)];r[static_cast<size_t>(i)]=l[static_cast<size_t>(i)]*0.75f;}
            float* out[]{l.data(),r.data()};const float* kick[]{side.data()+offset};
            const auto before=allocation::count;allocation::watch=true;const auto begin=Clock::now();
            e.process(out,2,kick,1,block,s,120);
            const auto end=Clock::now();allocation::watch=false;allocs+=allocation::count-before;
            if(b>=30){const double us=std::chrono::duration<double,std::micro>(end-begin).count();durations.push_back(us);total+=us;}
            checksum+=l.back();
        }
    }
    std::sort(durations.begin(),durations.end());const auto percentile=[&](double p){return durations[static_cast<size_t>(p*static_cast<double>(durations.size()-1))];};
    const double deadline=1e6*block/sr,mean=total/static_cast<double>(durations.size());
    csv<<revision<<','<<sr<<','<<block<<','<<mode<<','<<mean<<','<<percentile(0.5)<<','<<percentile(0.95)<<','<<percentile(0.99)<<','<<durations.back()<<','<<mean/deadline*100<<','<<allocs<<'\n';
}

void nullTests(const std::filesystem::path& root){
    std::ofstream csv(root/"null_comparison.csv");csv<<"mode,max_absolute_error,rms_error,different_samples,nonfinite\n";
    for(const std::string mode:{"bypass","glue","unmask","all","agc","learn","automation"}){
        bassdeamon::Engine current;bassdeamon_baseline::Engine baseline;current.prepare(48000);baseline.prepare(48000);
        auto s=settings<bassdeamon::Settings>(mode);auto ref=settings<bassdeamon_baseline::Settings>(mode);
        if(mode=="learn"){current.startLearn();baseline.startLearn();}
        double maximum=0,power=0;size_t different=0,nonfinite=0;std::array<float,64>a{},b{},side{};
        for(int block=0;block<3750;++block){
            if(mode=="automation"){
                const bool on=(block/75)%2==0;
                s.glue=ref.glue=on;s.unmaskWidth=ref.unmaskWidth=on?0.05:0.95;
                s.delayMs=ref.delayMs=on?0.125:4.25;s.invert=ref.invert=!on;
                s.rotationHz=ref.rotationHz=on?35:440;s.outputDb=ref.outputDb=on?-3:0;
            }
            for(int i=0;i<64;++i){a[static_cast<size_t>(i)]=b[static_cast<size_t>(i)]=bassSignal(block*64+i,48000);side[static_cast<size_t>(i)]=kickSignal(block*64+i,48000);}
            float* x[]{a.data()};float* y[]{b.data()};const float* k[]{side.data()};
            current.process(x,1,k,1,64,s,120);baseline.process(y,1,k,1,64,ref,120);
            for(size_t i=0;i<a.size();++i){const double d=static_cast<double>(a[i])-b[i];maximum=std::max(maximum,std::abs(d));power+=d*d;different+=a[i]!=b[i];nonfinite+=!std::isfinite(a[i]);}
        }
        csv<<mode<<','<<maximum<<','<<std::sqrt(power/240000)<<','<<different<<','<<nonfinite<<'\n';
    }
}

template<class E,class S>void fixtures(const std::filesystem::path& root){
    std::filesystem::create_directories(root);
    std::ofstream manifest(root/"fixtures.csv");manifest<<"file,mode,sr,frequency,amplitude,seconds\n";
    const auto render=[&](const std::string& name,const std::string& mode,double f,double amplitude,int seconds){
        E e;e.prepare(48000);auto s=settings<S>(mode);
        if(mode=="phase"){s.phase=true;s.delayMs=1.375;s.rotate=true;}
        if(mode=="unmask"||mode=="width_toggle"){s.glue=false;s.unmask=true;s.unmaskTrack=false;s.unmaskFrequency=f;s.unmaskAmount=1;}
        if(mode=="bypass_toggle"){s.glue=s.unmask=true;}
        std::ofstream out(root/(name+".f32"),std::ios::binary);std::array<float,64> input{},audio{},kick{};
        for(int block=0;block<seconds*750;++block){
            const double time=block*64/48000.0;
            if(mode=="width_toggle")s.unmaskWidth=static_cast<int>(time*4)%2==0?0:1;
            if(mode=="bypass_toggle")s.glue=s.unmask=static_cast<int>(time*2)%2==0;
            if(mode=="automation_phase"){s.phase=true;s.delayMs=static_cast<int>(time*2)%2==0?0:5;s.invert=static_cast<int>(time)%2==0;}
            for(int i=0;i<64;++i){const int n=block*64+i;const double t=n/48000.0;
                double frequency=f;if(mode=="notes_all")frequency=static_cast<int>(t)%3==0?55:static_cast<int>(t)%3==1?65.406391:73.416192;
                double v=amplitude*std::sin(2*pi*frequency*t);
                if(mode=="notes_all"){const double pos=std::fmod(t,1.0),gate=std::clamp(std::min(pos*100,(1-pos)*100),0.0,1.0);v*=0.5-0.5*std::cos(pi*gate);}
                if(mode=="tail"&&t>2)v=0;
                input[static_cast<size_t>(i)]=audio[static_cast<size_t>(i)]=static_cast<float>(v);
                kick[static_cast<size_t>(i)]=mode=="silence"?0:static_cast<float>(0.5*std::sin(2*pi*f*t));
                if(mode=="notes_all"||mode=="all")kick[static_cast<size_t>(i)]=kickSignal(n,48000);
            }
            float* a[]{audio.data()};const float* k[]{kick.data()};e.process(a,1,k,1,64,s,120);
            for(size_t i=0;i<audio.size();++i){const float frame[]{input[i],audio[i],kick[i]};out.write(reinterpret_cast<const char*>(frame),sizeof(frame));}
        }
        manifest<<name<<','<<mode<<",48000,"<<f<<','<<amplitude<<','<<seconds<<'\n';
    };
    for(double f:{40.0,80.0,160.0})for(const std::string mode:{"bypass","glue","unmask","phase"})render("tone_"+mode+"_"+std::to_string(static_cast<int>(f)),mode,f,0.25,10);
    render("quiet_unmask_80","unmask",80,0.025,10);
    render("silence","silence",80,0,4);render("notes_all","notes_all",55,0.3,8);
    render("width_toggle","width_toggle",80,0.25,8);render("bypass_toggle","bypass_toggle",80,0.25,8);
    render("automation_phase","automation_phase",80,0.25,8);render("tail","tail",80,0.25,8);
}
int main(int argc,char** argv){
    const std::filesystem::path root=argc>1?argv[1]:"benchmark-results";std::filesystem::create_directories(root);
    std::ofstream csv(root/"cpu.csv");csv<<std::setprecision(10)<<"revision,sample_rate,buffer,mode,mean_us,p50_us,p95_us,p99_us,max_us,one_core_percent,callback_allocations\n";
    for(double sr:{44100.0,48000.0,96000.0})for(int block:{64,128,256,512})for(const std::string mode:{"bypass","glue","unmask","all","agc","learn"}){
        performance<bassdeamon_baseline::Engine,bassdeamon_baseline::Settings>(csv,"0.2.0",sr,block,mode);
        performance<bassdeamon::Engine,bassdeamon::Settings>(csv,"0.2.2",sr,block,mode);
    }
    nullTests(root);fixtures<bassdeamon::Engine,bassdeamon::Settings>(root);fixtures<bassdeamon_baseline::Engine,bassdeamon_baseline::Settings>(root/"reference");std::cout<<"Benchmark complete: "<<root<<" checksum "<<checksum<<'\n';
}
