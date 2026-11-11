#include "Dsp.h"
#include "KickEqLearn.h"
#include "RealtimeAllocation.h"
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace bassdeamon;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

double learn(double sr, double hz, bool unstable, bool continuous, double level=.75) {
    KickEqCapture capture;
    capture.start(sr);
    std::array<float,128> left{}, right{};
    const float* kick[]{left.data(),right.data()};
    bool done = false;
    for (int block=0; !done && block < int(sr*4.2/128); ++block) {
        for (int i=0; i<128; ++i) {
            const double t=(block*128+i)/sr;
            const int beat=int(t/.5);
            const double age=continuous?t:std::fmod(t,.5);
            const double pitch=hz+(unstable&&beat%2?25:0);
            const double tail=continuous?1:std::exp(-age*21);
            const double click=continuous?0:.25*std::exp(-age*300)*std::sin(2*pi*720*age);
            left[i]=float(level*(tail*std::sin(2*pi*pitch*age)+click));
            right[i]=-left[i];
        }
        allocation::watch=true;
        done=capture.push(kick,2,128);
        allocation::watch=false;
    }
    require(done,"Kick EQ capture did not finish");
    return capture.frequency();
}

double render(double sr,double hz,Settings settings) {
    Engine engine;
    engine.prepare(sr);
    std::array<float,128> bass{};
    float* output[]{bass.data()};
    double input=0,outputPower=0;
    for(int block=0;block<int(sr*2.5/128);++block) {
        for(int i=0;i<128;++i) {
            bass[i]=float(.35*std::sin(2*pi*hz*(block*128+i)/sr));
            if(block*128>sr*2)input+=double(bass[i])*bass[i];
        }
        allocation::watch=true;
        engine.process(output,1,nullptr,0,128,settings);
        allocation::watch=false;
        for(float sample:bass) {
            require(std::isfinite(sample),"Nonfinite Kick EQ output");
            if(block*128>sr*2)outputPower+=double(sample)*sample;
        }
    }
    return 10*std::log10(outputPower/input);
}

void benchmark() {
    using Clock=std::chrono::steady_clock;
    constexpr int blocks=5000;
    constexpr int size=128;
    constexpr double sr=48000;
    volatile double sink=0;
    std::cout<<"mode,median_us,p95_us,median_pct_audio_budget\n";
    for(int mode=0;mode<5;++mode) {
        Engine engine;engine.prepare(sr);
        KickEqCapture capture;capture.start(sr);
        Settings settings;settings.kickEqEnabled=mode==2||mode==4;settings.kickEqFrequency=60;
        settings.matchEnabled=mode==3||mode==4;
        std::array<float,size> left{},right{},kick{};
        std::array<double,blocks> times{};
        float* output[]{left.data(),right.data()};const float* reference[]{kick.data()};
        for(int b=-300;b<blocks;++b) {
            for(int i=0;i<size;++i) {
                const double t=(b*size+i)/sr;
                left[i]=float(.35*std::sin(2*pi*60*t));right[i]=left[i]*.85f;
                kick[i]=float(.6*std::exp(-22*std::abs(std::fmod(t,.5)))*std::sin(2*pi*60*t));
            }
            const auto start=Clock::now();
            if(mode==1 && capture.push(reference,1,size))capture.start(sr);
            engine.process(output,2,reference,1,size,settings);
            const auto end=Clock::now();
            if(b>=0)times[b]=std::chrono::duration<double,std::micro>(end-start).count();
            sink+=left.back();
        }
        std::sort(times.begin(),times.end());
        const char* names[]{"features_off","kick_learn_capture","kick_eq_locked","level_match","locked_and_matched"};
        std::cout<<names[mode]<<','<<times[blocks/2]<<','<<times[blocks*95/100]<<','<<times[blocks/2]*sr/(size*1e4)<<'\n';
    }
}

int main(int argc,char**) {
    try {
        if(argc>1) {benchmark();return 0;}
        allocation::count=0;
        for(double sr:{44100.,48000.,96000.,192000.}) {
            for(double hz:{34.,47.,60.,78.,110.,126.})
                require(std::abs(learn(sr,hz,false,false)-hz)<1.0,"Stable kick fundamental missed");
            require(learn(sr,55,true,false)==0,"Unstable kick accepted");
            require(learn(sr,55,false,true)==0,"Sustained tone accepted as repeated hits");
            require(learn(sr,55,false,false,0)==0,"Silence accepted");
            Settings settings;settings.glue=settings.phase=settings.unmask=false;
            settings.kickEqEnabled=true;settings.kickEqFrequency=60;settings.kickEqCut=4;
            require(std::abs(render(sr,60,settings)+4)<.03,"Centre attenuation is not -4 dB");
            require(std::abs(render(sr,120,settings))<.25,"Second harmonic cut is too wide");
            settings.matchEnabled=true;
            require(std::abs(render(sr,60,settings))<.08,"Level match did not converge");
            settings.matchEnabled=false;settings.auditionDry=true;
            require(std::abs(render(sr,60,settings))<.001,"Dry audition changed level");
            settings.auditionDry=false;settings.kickEqEnabled=false;
            require(std::abs(render(sr,60,settings))<.001,"Disabled EQ changed level");
        }
        require(allocation::count==0,"Allocation in real-time processing");
        std::cout<<"PASS kick capture, unstable/silent rejection, static bell, level matching, dry audition and no real-time allocations\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
