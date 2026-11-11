#include "Dsp.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include "RealtimeAllocation.h"
static void checkStereoModes() {
    using namespace bassdeamon;
    constexpr double sampleRate = 48000, frequency = 75;
    for (auto mode : {UnmaskStereoMode::mid, UnmaskStereoMode::side}) {
        for (bool autoGain : {false, true}) {
            Engine engine;
            engine.prepare(sampleRate);
            Settings settings;
            settings.glue = settings.phase = settings.unmaskTrack = false;
            settings.unmaskDirect = true;
            settings.unmaskAttenuationDb = 12;
            settings.unmaskFrequency = frequency;
            settings.unmaskStereoMode = mode;
            settings.agc[2].enabled = autoGain;
            std::array<float, 128> left{}, right{}, kick{}, originalLeft{}, originalRight{};
            float* output[]{left.data(), right.data()};
            const float* reference[]{kick.data()};
            double selectedInput = 0, selectedOutput = 0;
            for (int block = 0; block < 750; ++block) {
                for (int n = 0; n < 128; ++n) {
                    const double phase = 2 * pi * frequency * (block * 128 + n) / sampleRate;
                    const double mid = .24 * std::sin(phase), side = .19 * std::cos(phase);
                    originalLeft[n] = left[n] = float(mid + side);
                    originalRight[n] = right[n] = float(mid - side);
                    kick[n] = float(.4 * std::sin(phase));
                }
                engine.process(output, 2, reference, 1, 128, settings);
                const double sign = mode == UnmaskStereoMode::mid ? 1 : -1;
                for (size_t n = 0; n < left.size(); ++n) {
                    const double dryOther = double(originalLeft[n]) - sign * originalRight[n];
                    const double wetOther = double(left[n]) - sign * right[n];
                    if (std::abs(dryOther - wetOther) > 6e-8)
                        throw std::runtime_error("UNMASK changed the other M/S component");
                    if (block > 700) {
                        const double dry = double(originalLeft[n]) + sign * originalRight[n];
                        const double wet = double(left[n]) + sign * right[n];
                        selectedInput += dry * dry;
                        selectedOutput += wet * wet;
                    }
                }
            }
            if (!autoGain && std::abs(-10 * std::log10(selectedOutput / selectedInput) - 12) > .25)
                throw std::runtime_error("M/S calibrated attenuation differs from requested cut");
        }
    }
    Engine mono;
    mono.prepare(sampleRate);
    Settings settings;
    settings.glue = settings.phase = false;
    settings.unmaskDirect = true;
    settings.unmaskAttenuationDb = 24;
    settings.unmaskStereoMode = UnmaskStereoMode::side;
    std::array<float, 128> bass{}, kick{};
    float* output[]{bass.data()};
    const float* reference[]{kick.data()};
    for (int block = 0; block < 300; ++block) {
        for (int n = 0; n < 128; ++n)
            bass[n] = kick[n] = float(.4 * std::sin(2 * pi * frequency * (block * 128 + n) / sampleRate));
        mono.process(output, 1, reference, 1, 128, settings);
        if (bass != kick) throw std::runtime_error("SIDE processed mono bass");
    }
    settings.unmaskStereoMode = UnmaskStereoMode::stereo;
    settings.unmaskFocusMinHz = 40;
    settings.unmaskFocusMaxHz = 60;
    mono.process(output, 1, reference, 1, 128, settings);
    const double focus = mono.getMeters().focusHz;
    if (focus < 40 || focus > 60) throw std::runtime_error("Auto Focus outside requested range");
    std::cout << "PASS M/S isolation with and without AGC, calibrated cut, mono SIDE unity and focus limits\n";
}
int main(){try{
    checkStereoModes();
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
