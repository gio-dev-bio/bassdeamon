#include "AutoMix.h"
#include <iostream>
#include <stdexcept>
using namespace bassdeamon;
void ensure(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
    std::vector<AutoMixFrame> audio(4*48000);
    for(size_t i=0;i<audio.size();++i){const double t=i/48000.;const float k=float(.1*std::sin(2*pi*50*t)+.05*std::sin(2*pi*110*t));audio[i]={2*k,2*k,k,k,float(t*2)};}
    Settings s;s.glue=s.phase=s.unmask=false;
    auto r=analyzeAutoMix(audio,48000,120,2,s);
    ensure(r.balanceValid,"Balance rejected known reference");
    ensure(std::abs(r.balanceSubDb-6.0206)<.01&&std::abs(r.balanceBodyDb-6.0206)<.01,"Band ratios are not calibrated");
    s.balanceTrimDb=-6.020599913;
    r=analyzeAutoMix(audio,48000,120,2,s);
    ensure(std::abs(r.balanceSubDb)<.01&&std::abs(r.balanceBodyDb)<.01,"Balance measurement ignored internal output trim");
    // Anti-phase stereo bass must retain its energy in the balance meter.
    for(auto& f:audio)f.right=-f.left;
    r=analyzeAutoMix(audio,48000,120,2,s);
    ensure(r.balanceValid&&std::abs(r.balanceSubDb)<.01,"Stereo cancellation corrupted balance levels");
    Engine e;e.prepare(48000);s=Settings{};s.glue=s.phase=false;s.kickReferenceHz=60;s.unmaskFocusMaxHz=130;
    std::array<float,256>b{},k{};float* out[]{b.data()};const float* side[]{k.data()};
    for(int block=0;block<700;++block){for(size_t n=0;n<256;++n){const double t=(block*256+n)/48000.;b[n]=float(.3*std::sin(2*pi*60*t)+.5*std::sin(2*pi*118*t));k[n]=float(.4*std::sin(2*pi*60*t)+.8*std::sin(2*pi*118*t));}e.process(out,1,side,1,256,s);}
    ensure(std::abs(e.getMeters().focusHz-60)<3,"Auto Focus chased louder kick harmonic instead of learned root");
    Engine unity,trimmed;unity.prepare(48000);trimmed.prepare(48000);
    Settings control;control.glue=control.phase=false;control.unmaskTrack=false;control.unmaskFrequency=75;control.unmaskDirect=true;control.unmaskAttenuationDb=6;control.matchEnabled=true;control.agc[2].enabled=true;
    Settings adjusted=control;adjusted.balanceTrimDb=-6.020599913;
    std::array<float,128> u{},v{},sc{};double eu=0,ev=0;
    for(int block=0;block<900;++block){for(size_t n=0;n<128;++n)u[n]=v[n]=sc[n]=float(.3*std::sin(2*pi*75*(block*128+n)/48000));float* up[]{u.data()};float* vp[]{v.data()};const float* kp[]{sc.data()};unity.process(up,1,kp,1,128,control);trimmed.process(vp,1,kp,1,128,adjusted);if(block>700)for(size_t n=0;n<128;++n){eu+=u[n]*u[n];ev+=v[n]*v[n];}}
    ensure(std::abs(10*std::log10(ev/eu)+6.020599913)<.01,"Auto Gain or Level Match cancelled Balance Trim");
    std::cout<<"PASS calibrated balance / output trim / stereo energy / fundamental-priority focus\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
