#include "Dsp.h"
#include "Analyzer.h"
#include "Dsp022Baseline.h"
#include "RealtimeAllocation.h"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>

namespace {
int checks=0;
void require(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
std::vector<float> render(int block,double sr,bassdeamon::Settings s){
    bassdeamon::Engine e;e.prepare(sr);std::vector<float> result(size_t(sr*.35)),side(result.size());
    for(size_t i=0;i<result.size();++i){const double t=i/sr;result[i]=float(.35*std::sin(2*bassdeamon::pi*55*t));side[i]=float(.5*std::exp(-std::fmod(t,.5)*25)*std::sin(2*bassdeamon::pi*57*t));}
    for(size_t pos=0;pos<result.size();pos+=size_t(block)){float* out[]{result.data()+pos};const float* k[]{side.data()+pos};
        const auto before=allocation::count;allocation::watch=true;
        e.process(out,1,k,1,std::min(block,int(result.size()-pos)),s,120);allocation::watch=false;
        require(allocation::count==before,"allocation in new DSP path");}
    return result;
}
void contracts(){
    for(double sr:{44100.,48000.,96000.}){
        for(int block:{17,128,511}){
            bassdeamon::Engine now;bassdeamon022::Engine old;now.prepare(sr);old.prepare(sr);
            bassdeamon::Settings s;bassdeamon022::Settings previous;
            s.phase=previous.phase=true;s.delayMs=previous.delayMs=1.25;s.rotate=previous.rotate=true;
            for(int b=0;b<80;++b){std::array<float,511> a{},ar{},c{},cr{},k{};
                for(int i=0;i<block;++i){const double t=(b*block+i)/sr;a[size_t(i)]=c[size_t(i)]=float(.3*std::sin(2*bassdeamon::pi*73*t));
                    ar[size_t(i)]=cr[size_t(i)]=-a[size_t(i)];k[size_t(i)]=float(.4*std::sin(2*bassdeamon::pi*55*t));}
                float* x[]{a.data(),ar.data()};float* y[]{c.data(),cr.data()};const float* kick[]{k.data()};
                now.process(x,2,kick,1,block,s,120);old.process(y,2,kick,1,block,previous,120);
                require(std::memcmp(a.data(),c.data(),size_t(block)*sizeof(float))==0,"tube off differs from 0.2.2 L");
                require(std::memcmp(ar.data(),cr.data(),size_t(block)*sizeof(float))==0,"tube off differs from 0.2.2 R");
            }
        }
        bassdeamon::Settings s;s.glue=s.phase=s.unmask=false;s.tube.enabled=true;
        auto one=render(1,sr,s),many=render(511,sr,s);
        require(std::memcmp(one.data(),many.data(),one.size()*sizeof(float))==0,"tube buffer-size dependence");
        for(float x:many)require(std::isfinite(x),"tube nonfinite");
        bassdeamon::Engine e;e.prepare(sr);std::array<float,128> left{},right{};float* out[]{left.data(),right.data()};
        for(int b=0;b<50;++b){for(int n=0;n<128;++n)left[size_t(n)]=float(.35*std::sin(2*bassdeamon::pi*55*(b*128+n)/sr));
            e.process(out,2,nullptr,0,128,s);for(float x:right)require(x==0,"tube stereo crosstalk");}
        s.tube.enabled=false;
        for(int b=0;b<100;++b){left.fill(0);right.fill(0);e.process(out,2,nullptr,0,128,s);}
        left.fill(.125f);e.process(out,2,nullptr,0,128,s);for(float x:left)require(x==.125f,"tube off after fade not exact");
        s.tube.enabled=true;s.active=false;
        for(int b=0;b<100;++b){left.fill(.125f);e.process(out,2,nullptr,0,128,s);}
        for(float x:left)require(x==.125f,"global bypass with tube not exact");
        e.prepare(sr);s.active=true;s.tube.enabled=true;
        for(int b=0;b<200;++b){
            s.tube.enabled=b%5!=0;s.tube.audition=b%11==0;
            s.tube.driveDb=b%2?18:0;s.tube.colour=b%2?1:0;s.tube.sourceHz=b%2?500:60;
            s.tube.amount=b%3?1:0;s.tube.autoGain=b%7==0;
            for(int n=0;n<128;++n){left[size_t(n)]=float(.4*std::sin(2*bassdeamon::pi*73*(b*128+n)/sr));right[size_t(n)]=0;}
            e.process(out,2,nullptr,0,128,s);
            for(int n=0;n<128;++n)require(std::isfinite(left[size_t(n)])&&right[size_t(n)]==0,"tube automation stability/isolation");
        }
    }
    bassdeamon::AnalysisQueue q;bassdeamon::RelationAnalyzer analyzer;analyzer.prepare(48000);
    for(size_t i=0;i<bassdeamon::RelationAnalyzer::fftSize;++i){
        const float x=float(.4*std::sin(2*bassdeamon::pi*64*i/4096));
        bassdeamon::AnalysisFrame f;f.pre={x,-x};f.post={2*x,-2*x};f.kick={x,x};require(q.push(f),"queue capacity");}
    analyzer.consume(q);require(analyzer.pre[64]>-10,"anti-phase spectrum disappeared");
    require(std::abs(analyzer.post[64]-analyzer.pre[64]-6.0206f)<.001f,"post FFT not actual signal");
    require(analyzer.maskPre[64]>.9,"equal levels must flag potential overlap");
    require(bassdeamon::RelationAnalyzer::mask(-120,-10)==0,"silent bass masking");
    require(bassdeamon::RelationAnalyzer::mask(-10,-120)==0,"silent kick masking");
    require(bassdeamon::RelationAnalyzer::mask(-120,-120)==0,"silence masking");
    for(uint32_t i=0;i<q.capacity;++i)require(q.push({}),"queue fill");
    require(!q.push({})&&q.dropped.load()==1,"full queue must drop without blocking");
    bassdeamon::AnalysisFrame frame;while(q.pop(frame)){}
    // Concurrency/order test: producer never touches consumer indices.
    std::atomic<bool> done{false};std::thread producer([&]{for(int i=0;i<200000;++i){bassdeamon::AnalysisFrame f;f.pre[0]=float(i);while(!q.push(f))std::this_thread::yield();}done.store(true);});
    int received=0;while(!done.load()||received<200000){if(q.pop(frame)){require(frame.pre[0]==float(received),"SPSC ordering");++received;}else std::this_thread::yield();}producer.join();
    require(received==200000,"SPSC lost frames");
}
volatile double checksum=0;
void performance(){
    using Clock=std::chrono::steady_clock;
    std::cout<<"mode,mean_us,p99_us,max_us,core_percent,allocations\n";
    for(int mode=0;mode<3;++mode){bassdeamon::Engine e;e.prepare(48000);bassdeamon::Settings s;s.tube.enabled=mode>0;s.tube.autoGain=mode==2;
        std::array<float,128> l{},r{},k{};std::vector<double> times;times.reserve(1200);size_t allocs=0;
        for(int b=0;b<1300;++b){for(size_t i=0;i<l.size();++i){const double t=(b*128+int(i))/48000.;l[i]=float(.3*std::sin(2*bassdeamon::pi*55*t));r[i]=l[i]*.8f;k[i]=float(.5*std::sin(2*bassdeamon::pi*57*t));}
            float* out[]{l.data(),r.data()};const float* side[]{k.data()};const auto before=allocation::count;allocation::watch=true;const auto start=Clock::now();
            e.process(out,2,side,1,128,s,120);const auto stop=Clock::now();allocation::watch=false;allocs+=allocation::count-before;
            if(b>=100)times.push_back(std::chrono::duration<double,std::micro>(stop-start).count());checksum+=l.back();}
        std::sort(times.begin(),times.end());double mean=0;for(auto t:times)mean+=t/times.size();
        std::cout<<(mode==0?"tube_off":mode==1?"tube_on":"tube_agc")<<','<<mean<<','<<times[size_t(.99*(times.size()-1))]<<','<<times.back()<<','<<mean/2666.6667*100<<','<<allocs<<'\n';
        require(allocs==0,"allocation in benchmark");}
}
}
int main(int argc,char** argv){try{contracts();performance();std::cout<<"PASS "<<checks<<" checks; fixed queue storage "<<sizeof(bassdeamon::AnalysisQueue)<<" bytes\n";
    if(argc>1){std::ofstream file(argv[1],std::ios::binary);bassdeamon::Engine e;e.prepare(48000);bassdeamon::Settings s;s.glue=s.phase=s.unmask=false;s.tube.enabled=true;
        for(int b=0;b<2250;++b){std::array<float,64> dry{},wet{};for(int n=0;n<64;++n)dry[size_t(n)]=wet[size_t(n)]=float(.4*std::sin(2*bassdeamon::pi*50*(b*64+n)/48000.));float* out[]{wet.data()};e.process(out,1,nullptr,0,64,s);
            for(size_t n=0;n<64;++n){const float frame[]{dry[n],wet[n]};file.write(reinterpret_cast<const char*>(frame),sizeof(frame));}}}
    return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
