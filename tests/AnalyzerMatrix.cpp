#include "Analyzer.h"
#include "Dsp.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace bassdeamon;
using A=RelationAnalyzer;
float near(const A::Spectrum& map,double hz,double sr){float peak=0;
    for(size_t b=1;b<A::bins;++b)if(std::abs(double(b)*sr/A::fftSize-hz)<std::max(8.,2*sr/A::fftSize))peak=std::max(peak,map[b]);return peak;}
int main(int argc,char** argv){try{
    std::ofstream csv(argc>1?argv[1]:"analyzer-matrix.csv");
    csv<<"sample_rate,gui_ms,kick_relative_db,profile,bass_peak_db,hits_expected,hits_detected,false_frames,pre_score,post_score\n";
    int cases=0,expected=0,detected=0,negativeFrames=0,falseFrames=0,failures=0;
    const char* names[]{"short20ms","short70ms","shared_harmonic100","chirp90to60","separated75_180","exclusive75","no_kick","different_stereo","quiet_residue"};
    if(!(argc>2&&std::string(argv[2])=="--tube-only"))
    for(double sr:{44100.,48000.,96000.})for(double gui:{.017,.033,.090})for(double balance:{-30.,-20.,0.,10.})for(int profile=0;profile<9;++profile){
        auto analyzer=std::make_unique<A>();auto queue=std::make_unique<AnalysisQueue>();analyzer->prepare(sr);
        const int levelIndex=(profile+int(gui*1000))%3;const double level=levelIndex==0?-12:levelIndex==1?-30:-48;
        const double amplitude=std::pow(10.,level/20),kickAmplitude=amplitude*std::pow(10.,balance/20);
        const bool positive=profile<4;const double target=profile==2?100:75;
        bool hits[2]{};float maxPre=0,maxPost=0;int falseInCase=0;double nextGui=gui;
        const auto inspect=[&](double time){analyzer->consume(*queue);if(!analyzer->ready())return;
            const float preScore=near(analyzer->maskPre,target,sr),postScore=near(analyzer->maskPost,target,sr);
            maxPre=std::max(maxPre,preScore);maxPost=std::max(maxPost,postScore);
            if(positive){for(int event=0;event<2;++event){const double start=.45+.5*event;
                if(time>=start&&time<=start+.32&&preScore>=.12f)hits[event]=true;}
                if(profile==2&&postScore>=.12f)++falseInCase;
            }else{++negativeFrames;float any=0;for(size_t b=1;b<A::bins&&double(b)*sr/A::fftSize<1000;++b)any=std::max(any,analyzer->maskPre[b]);
                if(any>=.12f){++falseInCase;++falseFrames;}}
        };
        for(int n=0;n<int(sr*1.35);++n){const double t=n/sr,phase=(balance+30)*.017;
            const double since=t>=.95?t-.95:t>=.45?t-.45:-1;
            const double width=profile==0?.020:profile==1?.070:.120;
            const bool hit=since>=0&&since<width;
            const double envelope=hit?std::pow(std::sin(pi*since/width),2)*std::exp(-since/(width*.8)):0;
            double bass=amplitude*std::sin(2*pi*75*t),post=bass,kick=kickAmplitude*envelope*std::sin(2*pi*75*t+phase);
            if(profile==2){bass=amplitude*(std::sin(2*pi*50*t)+.5*std::sin(2*pi*100*t)+.25*std::sin(2*pi*150*t));
                post=amplitude*(std::sin(2*pi*50*t)+.25*std::sin(2*pi*150*t));kick=kickAmplitude*envelope*std::sin(2*pi*100*t+phase);}
            if(profile==3)kick=hit?kickAmplitude*envelope*std::sin(2*pi*(90*since-15*since*since/width)+phase):0;
            if(profile==4)kick=kickAmplitude*std::sin(2*pi*180*t+phase);
            if(profile==5){if(hit)bass=post=0;}
            if(profile==6)kick=0;
            if(profile==7)kick=kickAmplitude*std::sin(2*pi*75*t+phase);
            if(profile==8){const double quiet=amplitude*.0001*std::sin(2*pi*300*t);bass=post=bass+quiet;kick=kickAmplitude*std::sin(2*pi*180*t)+quiet;}
            AnalysisFrame f;f.pre={float(bass),profile==7?0.f:float(-bass)};f.post={float(post),profile==7?0.f:float(-post)};
            f.kick=profile==7?std::array<float,2>{0,float(kick)}:std::array<float,2>{float(kick),float(kick)};
            if(!queue->push(f))throw std::runtime_error("matrix lost samples");
            if(t>=nextGui){inspect(t);nextGui+=gui;}
        }
        inspect(1.35);const int found=int(hits[0])+int(hits[1]);
        if(positive){expected+=2;detected+=found;}
        const bool fail=(positive&&found<2)||falseInCase>0;
        if(fail){++failures;if(failures<24)std::cerr<<"FAIL "<<names[profile]<<" sr "<<sr<<" gui "<<gui<<" balance "<<balance<<" level "<<level<<" hits "<<found<<" false "<<falseInCase<<" max "<<maxPre<<'\n';}
        csv<<sr<<','<<gui*1000<<','<<balance<<','<<names[profile]<<','<<level<<','<<(positive?2:0)<<','<<found<<','<<falseInCase<<','<<maxPre<<','<<maxPost<<'\n';++cases;
    }
    // REAL nonlinear processor: a 50 Hz input acquires a shared 100 Hz partial
    // after Soft Tube. PRE must stay clear there and POST must detect it.
    for(double sr:{44100.,48000.,96000.}){
    Engine engine;engine.prepare(sr);Settings settings;settings.glue=settings.phase=settings.unmask=false;
    settings.tube.enabled=true;settings.tube.driveDb=18;settings.tube.amount=1;settings.tube.sourceHz=200;
    auto q=std::make_unique<AnalysisQueue>();auto a=std::make_unique<A>();a->prepare(sr);
    std::array<float,256> dry{},wet{},kick{};float* out[]{wet.data()};const float* side[]{kick.data()};
    for(int block=0;block<240;++block){for(int n=0;n<256;++n){const double t=(block*256+n)/sr;dry[size_t(n)]=wet[size_t(n)]=float(.4*std::sin(2*pi*50*t));kick[size_t(n)]=float(.2*std::sin(2*pi*100*t));}
        engine.process(out,1,side,1,256,settings,120);
        for(int n=0;n<256;++n){AnalysisFrame f;f.pre={dry[size_t(n)],dry[size_t(n)]};f.post={wet[size_t(n)],wet[size_t(n)]};f.kick={kick[size_t(n)],kick[size_t(n)]};q->push(f);}
        if(block%6==0)a->consume(*q);
    }
    a->consume(*q);const float generated=near(a->maskPost,100,sr),original=near(a->maskPre,100,sr);
    std::cout<<"Real Soft Tube "+std::to_string(int(sr))+" Hz, 50 -> shared 100 Hz: PRE "<<original<<" POST "<<generated<<'\n';
    for(double hz:{50.,100.,150.,200.}){const auto bin=size_t(std::round(hz*A::fftSize/sr));std::cout<<hz<<" Hz PRE "<<a->pre[bin]<<" POST "<<a->post[bin]<<" dBFS\n";}
    if(original>=.12f||generated<.12f)++failures;
    }
    std::cout<<"Matrix: "<<cases<<" cases; pulse detections "<<detected<<'/'<<expected<<"; false negative-profile frames "<<falseFrames<<'/'<<negativeFrames<<"; failed cases "<<failures<<'\n';
    return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
