#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace bassdeamon {
struct TubeSettings {
    bool enabled=false, audition=false, autoGain=false, gainLock=false;
    int oversampling=4;
    double driveDb=6, amount=.35, sourceHz=180, colour=.45;
};

// Parallel low-band nonlinear residual. Selectable 2x or 4x oversampling; dry has no added
// latency. The added branch has FIR/filter phase delay (12 base-rate samples
// from the FIR pair). It is not a physical valve model or a pure-harmonic split.
class SoftTube {
public:
    void prepare(double sr) noexcept {
        sampleRate=sr; smoothing=std::exp(-1/(.02*sr)); fadeLength=std::max(1,int(.02*sr));
        constexpr double p=3.14159265358979323846;
        for (size_t bank=0;bank<coefficients.size();++bank) {
            const size_t window=bank==0?25:taps;
            const double cutoff=bank==0?.45:.225;
            auto& kernel=coefficients[bank];
            double sum=0;
            for(size_t i=0;i<window;++i){
                const double n=double(i)-double(window-1)/2;
                const double sinc=n==0?cutoff:std::sin(p*cutoff*n)/(p*n);
                kernel[i]=sinc*(.42-.5*std::cos(2*p*i/(window-1))+.08*std::cos(4*p*i/(window-1)));
                sum+=kernel[i];
            }
            for(auto& c:kernel)c/=sum;
        }
        clear(); initialised=false;
    }
    void clear() noexcept {
        channels={}; residualPower=0;mix=target=auditionMix=0;remaining=0;counter=0;running=false;
    }
    void configure(const TubeSettings& s) noexcept {
        desiredDrive=std::pow(10.0,std::clamp(s.driveDb,0.0,18.0)/20);
        desiredAmount=std::clamp(s.amount,0.0,1.0);
        desiredHz=std::clamp(s.sourceHz,60.0,500.0);desiredColour=std::clamp(s.colour,0.0,1.0);
        auditionTarget=s.audition?1:0;
        requestedRatio=s.oversampling==2?2:4;
        if(!initialised || (mix==0 && target==0)) ratio=requestedRatio;
        enabled=s.enabled;
        const double next=enabled && ratio==requestedRatio?1:0;
        if(!initialised){drive=desiredDrive;amount=desiredAmount;sourceHz=desiredHz;colour=desiredColour;initialised=true;}
        if(next!=target){target=next;remaining=fadeLength;step=(target-mix)/remaining;}
    }
    bool active() const noexcept { return running||target>0||mix>0; }
    double wetMix() const noexcept { return mix; }
    double addedPower() const noexcept { return residualPower; }
    void frame(std::array<double,2>& x,int count) noexcept {
        if(remaining>0){mix+=step;if(--remaining==0)mix=target;}
        if(mix==0&&target==0){
            residualPower=0;
            if(running){channels={};running=false;}
            if(enabled && ratio!=requestedRatio){
                ratio=requestedRatio;
                target=1;remaining=fadeLength;step=1.0/fadeLength;
            }
            return;
        }
        const size_t window=ratio==2?25:taps;
        const auto& coeff=coefficients[ratio==2?0:1];
        running=true;
        drive=desiredDrive+smoothing*(drive-desiredDrive);
        amount=desiredAmount+smoothing*(amount-desiredAmount);
        colour=desiredColour+smoothing*(colour-desiredColour);
        sourceHz=desiredHz+smoothing*(sourceHz-desiredHz);
        auditionMix=auditionTarget+smoothing*(auditionMix-auditionTarget);
        if(counter++%32==0){
            constexpr double p=3.14159265358979323846;
            sourcePole=std::exp(-2*p*sourceHz/sampleRate);
            highPole=std::exp(-2*p*90/sampleRate);
            tonePole=std::exp(-2*p*(600+3400*colour)/sampleRate);
            bias=.08+.24*colour;offset=std::tanh(bias);normal=drive*(1-offset*offset);
        }
        double framePower=0;
        for(int c=0;c<count;++c){
            auto& state=channels[size_t(c)];const double dry=x[size_t(c)];
            state.low1=(1-sourcePole)*dry+sourcePole*state.low1;
            state.low2=(1-sourcePole)*state.low1+sourcePole*state.low2;
            state.input[state.inHead]=state.low2;
            double down=0;
            for(size_t phase=0;phase<ratio;++phase){
                double up=0;
                for(size_t i=phase,j=0;i<window;i+=ratio,++j)
                    up+=ratio*coeff[i]*state.input[(state.inHead+inputSize-j)%inputSize];
                const double curved=(std::tanh(drive*up+bias)-offset)/normal;
                state.output[state.outHead]=curved-up;
                if(phase==0)for(size_t j=0;j<window;++j)
                    down+=coeff[j]*state.output[(state.outHead+window-j)%window];
                state.outHead=(state.outHead+1)%window;
            }
            state.inHead=(state.inHead+1)%inputSize;
            state.hp1=(1-highPole)*down+highPole*state.hp1;double added=down-state.hp1;
            state.hp2=(1-highPole)*added+highPole*state.hp2;added-=state.hp2;
            state.tone=(1-tonePole)*added+tonePole*state.tone;added=amount*state.tone;
            framePower+=added*added/count;
            // Audition is the added residual before module AGC and output trim.
            x[size_t(c)]=dry+mix*(added-auditionMix*dry);
        }
        residualPower=framePower+smoothing*(residualPower-framePower);
    }
private:
    static constexpr size_t taps=49,inputSize=13;
    struct Channel {std::array<double,inputSize> input{};std::array<double,taps> output{};
        size_t inHead=0,outHead=0;double low1=0,low2=0,hp1=0,hp2=0,tone=0;};
    std::array<Channel,2> channels{};std::array<std::array<double,taps>,2> coefficients{};
    size_t ratio=4, requestedRatio=4;
    double residualPower=0;
    double sampleRate=48000,smoothing=0,sourcePole=0,highPole=0,tonePole=0;
    double drive=1,amount=0,sourceHz=180,colour=.45;
    double desiredDrive=1,desiredAmount=0,desiredHz=180,desiredColour=.45;
    double bias=.18,offset=0,normal=1,mix=0,target=0,step=0,auditionMix=0,auditionTarget=0;
    int fadeLength=960,remaining=0;uint32_t counter=0;bool initialised=false,running=false,enabled=false;
};
}
