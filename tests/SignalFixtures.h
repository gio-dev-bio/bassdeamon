#pragma once
#include <vector>
#include <string>
#include <random>
#include <cmath>
constexpr double signalPi=3.14159265358979323846;
struct Signal { std::vector<float> l,r,k; };
Signal makeSignal(const std::string& name,int sr,int count) {
    Signal a; a.l.resize(count);a.r.resize(count);a.k.resize(count);
    std::mt19937 random(71537); double pink=0;
    for(int n=0;n<count;++n) {
        const double t=static_cast<double>(n)/sr;
        const double noise=(static_cast<double>(random())/4294967295.0*2-1)*0.2;
        pink=0.98*pink+0.02*noise;
        const double kick=0.6*std::exp(-std::fmod(t,.5)*32)*std::sin(2*signalPi*58*t);
        double x=0,y=0;
        if(name=="impulses") x=(n==0||n==63||n==127||n==sr||n==sr+1)?0.5:0;
        if(name=="sweep") { const double k=std::log(18000.0/20)/2; x=.2*std::sin(2*signalPi*20*std::expm1(k*t)/k); }
        if(name=="multitone") x=.12*std::sin(2*signalPi*47*t)+.08*std::sin(2*signalPi*113*t)+.03*std::sin(2*signalPi*997*t)+.015*std::sin(2*signalPi*10003*t);
        if(name=="white")x=noise;
        if(name=="lowpass_noise")x=pink;
        if(name=="bass_saw")x=.2*(2*std::fmod(t*73,1)-1);
        if(name=="kick_bass")x=.2*std::sin(2*signalPi*55*t)+kick*.3;
        if(name=="quiet")x=std::pow(10.,-90./20)*std::sin(2*signalPi*75*t);
        if(name=="tone80")x=.25*std::sin(2*signalPi*80*t);
        if(name=="subnormal")x=n%2?1e-39:-1e-39;
        if(name=="signed_zero")x=n%2?-0.0:0.0;
        if(name=="near_fullscale")x=.98*std::sin(2*signalPi*40*t);
        if(name=="dc")x=.05;
        if(name=="antiphase")x=.25*std::sin(2*signalPi*80*t);
        if(name=="independent")x=.25*std::sin(2*signalPi*73*t);
        y=name=="antiphase"?-x:name=="independent"?.13*std::sin(2*signalPi*211*t):x*.71;
        a.l[n]=static_cast<float>(x);a.r[n]=static_cast<float>(y);a.k[n]=static_cast<float>(kick);
    }
    return a;
}
