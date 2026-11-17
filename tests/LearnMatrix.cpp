#include "Dsp.h"
#include <fstream>
#include <iostream>
#include <random>

int main(int argc,char** argv){
    std::ofstream csv(argc>1?argv[1]:"learn-matrix.csv");
    csv<<"sample_rate,delay_ms,invert,estimated_ms,estimated_invert,correlation,separation,accepted\n";
    int failed=0;
    for(int sr:{44100,48000,96000}){
        std::vector<float> noise(static_cast<size_t>(sr*4));std::mt19937 random(639);std::uniform_real_distribution<float> r(-.5f,.5f);
        for(auto& x:noise)x=r(random);
        for(double delay:{0.0,.5,2.0,4.75})for(bool invert:{false,true}){
            bassdeamon::PhaseLearner learner;learner.prepare(sr);learner.start();
            const size_t lag=static_cast<size_t>(std::lround(delay*sr/1000));
            for(size_t n=0;n<noise.size();++n)learner.tick(noise[n],n>=lag?noise[n-lag]*(invert?-1:1):0);
            const auto a=learner.getResult();
            csv<<sr<<','<<delay<<','<<invert<<','<<a.delayMs<<','<<a.invert<<','<<a.correlation<<','<<a.separation<<','<<a.valid<<'\n';
            if(!a.valid||a.invert!=invert||std::abs(a.delayMs-delay)>.25)++failed;
        }
    }
    std::cout<<"Learn known-alignment matrix: 24 cases, "<<failed<<" failures\n";
    return failed?1:0;
}
