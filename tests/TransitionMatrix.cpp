#include "Dsp.h"
#include "RealtimeAllocation.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>

int main(int argc,char** argv){
    std::ofstream csv(argc>1?argv[1]:"transition-matrix.csv");
    csv<<"sample_rate,buffer,toggle_ms,exact_compared_samples,bit_differences,nonfinite,callback_allocations\n";
    int failures=0;
    for(int sr:{44100,48000,96000})for(int block:{17,128,511,2048})for(int periodMs:{125,5}){
        bassdeamon::Engine engine;engine.prepare(sr);bassdeamon::Settings s;s.phase=s.rotate=s.invert=true;s.delayMs=4.25;s.outputDb=6;
        for(auto& a:s.agc)a.enabled=true;
        std::vector<float> l(block),r(block),dl(block),dr(block),k(block);std::mt19937 random(79);std::uniform_real_distribution<float> rnd(-.3f,.3f);
        const int period=sr*periodMs/1000,total=sr,ramp=sr*20/1000;
        int offStart=-1;size_t bits=0,nonfinite=0,compared=0,allocs=0;
        for(int index=0;index<total;){
            const bool on=index<sr*3/4?(index/period)%2==0:false;
            if(on)offStart=-1;else if(offStart<0)offStart=index;
            const int edge=index<sr*3/4?std::min((index/period+1)*period,sr*3/4):total;
            const int n=std::min(block,edge-index);s.active=on;
            for(int i=0;i<n;++i){l[i]=(index+i)%17==0?-0.0f:(index+i)%19==0?-1e-39f:rnd(random);r[i]=(index+i)%23==0?1e-39f:rnd(random);dl[i]=l[i];dr[i]=r[i];k[i]=rnd(random);}
            float* output[]{l.data(),r.data()};const float* side[]{k.data()};
            const auto before=allocation::count;allocation::watch=true;engine.process(output,2,side,1,n,s,120);allocation::watch=false;allocs+=allocation::count-before;
            for(int i=0;i<n;++i){nonfinite+=!std::isfinite(l[i])||!std::isfinite(r[i]);if(!on&&index+i>=offStart+ramp){bits+=std::memcmp(&l[i],&dl[i],4)!=0;bits+=std::memcmp(&r[i],&dr[i],4)!=0;compared+=2;}}
            index+=n;
        }
        csv<<sr<<','<<block<<','<<periodMs<<','<<compared<<','<<bits<<','<<nonfinite<<','<<allocs<<'\n';
        if(bits||nonfinite||allocs||!compared)++failures;
    }
    std::cout<<"Master transition matrix: 24 cases, "<<failures<<" failures\n";return failures?1:0;
}
