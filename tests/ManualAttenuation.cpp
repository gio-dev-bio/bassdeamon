#include "Dsp.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include "RealtimeAllocation.h"
int main(){try{
    int cases=0;double worst=0;
    for(double sr:{44100.,48000.,96000.})for(double hz:{40.,55.,75.,110.,220.})for(double amount:{0.,3.5,6.,12.,18.,24.}){
        bassdeamon::Engine e;e.prepare(sr);bassdeamon::Settings s;s.glue=s.phase=false;s.unmaskDirect=true;s.unmaskAttenuationDb=amount;s.unmaskTrack=false;s.unmaskFrequency=hz;
        s.unmaskAttackMs=1;s.unmaskReleaseMs=30;std::array<float,128>x{},k{};float* out[]{x.data()};const float* side[]{k.data()};double input=0,output=0;
        for(int pos=0;pos<int(sr*2);pos+=128){for(int n=0;n<128;++n){x[size_t(n)]=k[size_t(n)]=float(.4*std::sin(2*bassdeamon::pi*hz*(pos+n)/sr));if(pos>sr*1.5)input+=double(x[size_t(n)])*x[size_t(n)];}
            const auto before=allocation::count;allocation::watch=true;e.process(out,1,side,1,128,s);allocation::watch=false;
            if(allocation::count!=before)throw std::runtime_error("Manual dB allocates in audio");
            for(float v:x){if(!std::isfinite(v))throw std::runtime_error("Manual dB nonfinite");if(pos>sr*1.5)output+=double(v)*v;}
        }
        const double measured=-10*std::log10(output/input),error=std::abs(measured-amount);worst=std::max(worst,error);
        if(error>.25){std::cerr<<sr<<" "<<hz<<" requested "<<amount<<" measured "<<measured<<"\n";return 1;}++cases;
    }
    // No reference: exact unity at every tested sample, not full-band ducking.
    bassdeamon::Engine e;e.prepare(48000);bassdeamon::Settings s;s.glue=s.phase=false;s.unmaskDirect=true;s.unmaskAttenuationDb=24;
    std::array<float,128>x{};float* out[]{x.data()};for(int b=0;b<300;++b){x.fill(.125f);e.process(out,1,nullptr,0,128,s);for(float v:x)if(v!=.125f)throw std::runtime_error("Manual dB ducks without kick");}
    std::cout<<"PASS manual attenuation "<<cases<<" calibrated cases; worst error "<<worst<<" dB; no-kick unity; no callback allocations\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
