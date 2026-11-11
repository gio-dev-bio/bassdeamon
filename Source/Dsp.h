#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "SoftTube.h"

namespace bassdeamon {
constexpr double pi = 3.14159265358979323846;
inline double db(double x) noexcept { return 20.0 * std::log10(std::max(x, 1.0e-12)); }
inline double gain(double d) noexcept { return d == 0 ? 1.0 : std::pow(10.0, d / 20.0); }
inline double pole(double ms, double sr) noexcept { return std::exp(-1.0 / (0.001 * ms * sr)); }
inline double follow(double current, double target, double a) noexcept {
    return target + a * (current - target);
}
inline double power(const std::array<double, 2>& x, int channels) noexcept {
    return (x[0] * x[0] + (channels == 2 ? x[1] * x[1] : 0.0)) / channels;
}

inline constexpr std::array<double,9> noteBeats{.0625,.125,.25,.5,1,2,4,1.0/3,.75};
inline double noteMilliseconds(int note,double bpm) noexcept {
    const double tempo=std::isfinite(bpm)&&bpm>=20&&bpm<=400?bpm:120;
    return 60000/tempo*noteBeats[size_t(std::clamp(note,0,8))];
}
struct AgcSettings { bool enabled = false, locked = false; };
enum class UnmaskStereoMode { stereo, mid, side };

struct Settings {
    bool active = true;
    bool kickEqEnabled = false, matchEnabled = false, auditionDry = false;
    double kickEqFrequency = 0, kickEqCut = 1.5;
    TubeSettings tube;
    bool glue = true, glueLocked = false;
    double glueAmount = 0.35;
    int glueTiming=0,glueAttackNote=0,glueReleaseNote=2; // AUTO / MS / SYNC
    double glueAttackMs=20,glueReleaseMs=180;
    bool unmaskSync=false;int unmaskAttackNote=0,unmaskReleaseNote=2;
    bool phase = false, invert = false, rotate = false;
    double delayMs = 0.0, rotationHz = 90.0;
    bool unmask = true, delta = false;
    double unmaskAmount = 0.4, outputDb = 0.0;
    bool unmaskDirect=false;double unmaskAttenuationDb=6;
    std::array<AgcSettings, 3> agc{};
    UnmaskStereoMode unmaskStereoMode = UnmaskStereoMode::stereo;
    double unmaskFocusMinHz = 30, unmaskFocusMaxHz = 250;
    bool unmaskTrack = true, unmaskHold = false;
    double unmaskFrequency = 75, unmaskWidth = 0.55;
    double unmaskAttackMs = 8, unmaskReleaseMs = 120, unmaskMaxCutDb = 9;
};
constexpr size_t bandCount = 12;
inline constexpr std::array<double, bandCount> bandFrequencies{30, 38, 48, 60, 75, 94, 118, 148, 186, 234, 295, 370};
struct Meters {
    double matchGainDb = 0;
    double glueReductionDb = 0.0, unmaskReductionDb = 0.0;
    std::array<double, 3> agcDb{};
    double peak = 0.0, kickPeak = 0.0, inputRmsDb = -120, outputRmsDb = -120;
    double focusHz = 75, attackMs = 20, releaseMs = 180;
    std::array<double, bandCount> bassDb{}, kickDb{}, cutsDb{};
    double filterQ = 2, tubeAgcDb=0,tubeAddedDb=-120,unmaskAttackMs=8,unmaskReleaseMs=120;
};

// Double precision RBJ cookbook filters, transposed direct form II.
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;
    double tick(double x) noexcept {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        if (std::abs(z1) < 1e-25) z1 = 0;
        if (std::abs(z2) < 1e-25) z2 = 0;
        return y;
    }
    void bandpass(double frequency, double q, double sr) noexcept {
        const double w = 2 * pi * frequency / sr;
        const double alpha = std::sin(w) / (2 * q), a0 = 1 + alpha;
        b0 = alpha / a0; b1 = 0; b2 = -b0;
        a1 = -2 * std::cos(w) / a0; a2 = (1 - alpha) / a0;
    }
    void peak(double frequency, double q, double cutDb, double sr) noexcept {
        if (std::abs(cutDb) < 1e-12) {
            // Explicit identity avoids recursive roundoff/FMA residue at exactly 0 dB.
            b0 = 1; b1 = b2 = a1 = a2 = z1 = z2 = 0; return;
        }
        const double w = 2 * pi * frequency / sr, A = gain(cutDb / 2);
        const double alpha = std::sin(w) / (2 * q), a0 = 1 + alpha / A;
        b0 = (1 + alpha * A) / a0; b1 = -2 * std::cos(w) / a0;
        b2 = (1 - alpha * A) / a0; a1 = b1; a2 = (1 - alpha / A) / a0;
    }
};

class AutoGain {
public:
    void prepare(double sr, double adaptationMs) noexcept {
        *this = AutoGain{};
        energyPole = pole(400, sr); adaptPole = pole(adaptationMs, sr); bypassPole = pole(20, sr);
    }
    double tick(double in, double out, AgcSettings s, double budgetDb) noexcept {
        input = follow(input, in, energyPole); output = follow(output, out, energyPole);
        if (s.enabled && !s.locked && in > 1e-10 && input > 1e-9 && output > 1e-9) {
            const double target = std::clamp(10 * std::log10(input / output), -3.0, 3.0);
            compensation = follow(compensation, target, adaptPole);
        }
        // Smooth the independent AGC switch too, while preserving its learned value.
        if (!initialised) { mix = s.enabled ? 1 : 0; initialised = true; }
        mix = follow(mix, s.enabled ? 1 : 0, bypassPole);
        if (mix < 1e-12) mix = 0;
        applied = std::clamp(compensation * mix, -3.0, std::min(3.0, budgetDb));
        return gain(applied);
    }
    double appliedDb() const noexcept { return applied; }
    double learnedDb() const noexcept { return compensation; }
    void restore(double value) noexcept { compensation = std::clamp(value, -3.0, 3.0); }
private:
    double input = 0, output = 0, compensation = 0, applied = 0;
    double energyPole = 0, adaptPole = 0, bypassPole = 0, mix = 0;
    bool initialised = false;
};

// All storage is allocated in prepare(), never in tick()/process().
class Delay {
public:
    void prepare(double sr) { data.assign(static_cast<size_t>(std::ceil(sr * 0.012)) + 8, 0); head = 0; }
    void push(double x) noexcept { data[head] = x; head = (head + 1) % data.size(); }
    void clear() noexcept { std::fill(data.begin(), data.end(), 0); head = 0; }
    double read(double samples) const noexcept {
        const auto whole = static_cast<size_t>(samples);
        if (samples == 0) return data[(head + data.size() - 1) % data.size()];
        const double f = samples - static_cast<double>(whole);
        const auto at = [&](size_t d) { return data[(head + data.size() - 1 - d) % data.size()]; };
        // Causal third-order Lagrange fractional delay. Exact identity at zero.
        return at(whole) * (1-f)*(2-f)*(3-f)/6
             + at(whole+1) * f*(2-f)*(3-f)/2
             - at(whole+2) * f*(1-f)*(3-f)/2
             + at(whole+3) * f*(1-f)*(2-f)/6;
    }
private:
    std::vector<double> data;
    size_t head = 0;
};

// Finite ramps reach their exact endpoint; retargeting starts from the current value.
class Ramp {
public:
    void reset(double value) noexcept { current=target=value; remaining=0; step=0; }
    void set(double value,int samples) noexcept {
        if(value==target)return;
        target=value;remaining=std::max(1,samples);step=(target-current)/remaining;
    }
    double next() noexcept { if(remaining>0){current+=step;if(--remaining==0)current=target;}return current; }
    double value() const noexcept { return current; }
    bool finished() const noexcept { return remaining==0; }
private:
    double current=0,target=0,step=0;int remaining=0;
};
enum class LearnRejection { none, insufficient, weak, ambiguous, unstable };
struct LearnResult {
    double delayMs = 0, correlation = 0; bool invert = false, valid = false;
    double separation = 0;
    bool hasReference = false;
    LearnRejection rejection = LearnRejection::insufficient;
};
class PhaseLearner {
public:
    void prepare(double sr) {
        sampleRate = sr; decimation = std::max(1, static_cast<int>(sr / 2000));
        lowPole = std::exp(-2 * pi * 250 / sr); peakDecay = pole(120, sr); history.prepare(sr); start(); active = false;
    }
    void start() noexcept {
        for(auto* a:{&cross,&bassPower,&bassSum,&firstCross,&firstBassPower,&firstBassSum})a->fill(0);
        kickPower=kickSum=firstKickPower=firstKickSum=0;count=firstCount=0;
        elapsed = 0; lowBass = lowKick = kickEnvelope = 0; active = true; result = {};
    }
    void cancel() noexcept { active = false; }
    void tick(double bass, double kick) noexcept {
        // start() resets the low-pass states, and ignores the first 20 ms (history <=5 ms).
        // No need to run the learner while it is not listening.
        if (!active) return;
        lowBass = follow(lowBass, bass, lowPole); lowKick = follow(lowKick, kick, lowPole);
        history.push(lowBass);
        const double kickMagnitude = std::abs(lowKick);
        kickEnvelope = std::max(kickMagnitude, kickEnvelope * peakDecay);
        result.hasReference |= kickMagnitude > .001;
        ++elapsed;
        if (elapsed > static_cast<int>(sampleRate * 0.02) && elapsed % decimation == 0
            && kickMagnitude > std::max(.0001, kickEnvelope*.14)) {
            for (size_t i = 0; i < cross.size(); ++i) {
                const double b = history.read(static_cast<double>(i) * 0.00025 * sampleRate);
                cross[i] += b * lowKick; bassPower[i] += b * b;
                bassSum[i]+=b;
                if(elapsed<=static_cast<int>(2*sampleRate)){
                    firstCross[i]+=b*lowKick;firstBassPower[i]+=b*b;firstBassSum[i]+=b;
                }
            }
            kickPower += lowKick * lowKick; ++count;
            kickSum+=lowKick;
            if(elapsed<=static_cast<int>(2*sampleRate)){firstKickPower+=lowKick*lowKick;firstKickSum+=lowKick;++firstCount;}
        }
        if (elapsed >= static_cast<int>(4 * sampleRate)) {
            active = false;
            const auto corr=[](double xy,double xx,double sx,double yy,double sy,int n){
                if(n<1)return 0.0;
                const double vx=std::max(0.0,xx-sx*sx/n),vy=std::max(0.0,yy-sy*sy/n);
                // DC or near-constant signals do not establish an alignment.
                if(vx<std::max(1e-12,xx*.001)||vy<std::max(1e-12,yy*.001))return 0.0;
                return std::clamp((xy-sx*sy/n)/std::sqrt(vx*vy),-1.0,1.0);
            };
            std::array<double,21> correlations{};
            double best = 0;size_t bestIndex=0;
            for (size_t i = 0; i < cross.size(); ++i) {
                const double c=corr(cross[i],bassPower[i],bassSum[i],kickPower,kickSum,count);correlations[i]=c;
                if (std::abs(c) > best) {
                    best = std::abs(c);bestIndex=i;result.delayMs=static_cast<double>(i)*.25;result.correlation=best;result.invert=c<0;
                }
            }
            double competitor=0;
            for(size_t i=0;i<correlations.size();++i){
                if(i==bestIndex)continue;
                const double peak=std::abs(correlations[i]);
                const bool rising=i==0||peak>std::abs(correlations[i-1]);
                const bool falling=i+1==correlations.size()||peak>=std::abs(correlations[i+1]);
                if(rising&&falling)competitor=std::max(competitor,peak);
            }
            result.separation=best-competitor;
            const auto halfBest=[&](bool first){
                double peak=0;int index=0;bool invert=false;
                for(size_t i=0;i<cross.size();++i){
                    const double c=first?corr(firstCross[i],firstBassPower[i],firstBassSum[i],firstKickPower,firstKickSum,firstCount)
                        :corr(cross[i]-firstCross[i],bassPower[i]-firstBassPower[i],bassSum[i]-firstBassSum[i],kickPower-firstKickPower,kickSum-firstKickSum,count-firstCount);
                    if(std::abs(c)>peak){peak=std::abs(c);index=static_cast<int>(i);invert=c<0;}
                }
                return std::array<double,3>{static_cast<double>(index),invert?1.0:0.0,peak};
            };
            const auto first=halfBest(true),second=halfBest(false);
            const bool stable=first[2]>=.45&&second[2]>=.45&&first[1]==second[1]&&std::abs(first[0]-second[0])<=2;
            result.rejection=count<180||firstCount<60||count-firstCount<60||kickPower<=1e-5?LearnRejection::insufficient
                :best<.5?LearnRejection::weak:result.separation<.08?LearnRejection::ambiguous:!stable?LearnRejection::unstable:LearnRejection::none;
            result.valid=result.rejection==LearnRejection::none;
        }
    }
    double progress() const noexcept { return std::clamp(elapsed/(4*sampleRate),0.0,1.0); }
    bool learning() const noexcept { return active; }
    LearnResult getResult() const noexcept { return result; }
private:
    std::array<double,21> cross{},bassPower{},bassSum{},firstCross{},firstBassPower{},firstBassSum{};
    double sampleRate = 48000, lowPole = 0, lowBass = 0, lowKick = 0, kickPower = 0;
    double kickEnvelope = 0, peakDecay = 0;
    double kickSum=0,firstKickPower=0,firstKickSum=0;
    int decimation = 24, elapsed = 0, count = 0,firstCount=0;
    bool active = false;
    Delay history;
    LearnResult result;
};

class Engine {
public:
    void prepare(double sr) {
        sampleRate = std::clamp(sr, 8000.0, 384000.0);
        kickEqFilters = {};
        nextKickEqFilters = {};
        kickEqRetune.reset(1);
        kickEqHz = kickEqDb = 0;
        kickEqMix.reset(0);
        monitorMix.reset(0);
        monitorGain.reset(1);
        wetPower = 0;
        bassEnergy = envelope = kickFast = kickSlow = 0;
        thresholdDb = -24; ratio = 2; attackMs = 20; releaseMs = 180;
        glueReduction = 0; onsetClock = lastOnset = 0; kickIntervalMs = 0;
        controlCounter = 0; smoothed = false; meters = {};
        globalInitialised=globalPaused=false;globalRamp.reset(1);delayFade.reset(1);polarityRamp.reset(1);
        inputPowerMeter = outputPowerMeter = outputHeld = kickHeld = 0;
        focusHz = 75; trackedFocusHz = 75;
        cachedRotationHz = -1; cachedAllpassA = 0;
        cutDb.fill(0); kickE.fill(0); bassE.fill(0);directCutDb=0;directMix=0;directFilters={};
        allpassX.fill(0); allpassY.fill(0);
        for (auto& d : delays) d.prepare(sampleRate);
        learner.prepare(sampleRate);tube.prepare(sampleRate);tubeAgc.prepare(sampleRate,5000);
        for (size_t b = 0; b < frequencies.size(); ++b) {
            for (int c = 0; c < 2; ++c) {
                detectorsBass[b][c] = {}; detectorsKick[b][c] = {}; filters[b][c] = {};
                detectorsBass[b][c].bandpass(frequencies[b], 2, sampleRate);
                detectorsKick[b][c].bandpass(frequencies[b], 2, sampleRate);
            }
        }
        for (size_t i = 0; i < agc.size(); ++i) agc[i].prepare(sampleRate, i == 2 ? 12000 : 5000);
        energyA = pole(300, sampleRate); rmsA = pole(10, sampleRate);
        smoothA = pole(20, sampleRate); bandA = pole(15, sampleRate);
        kickFastA = pole(3, sampleRate); kickSlowA = pole(100, sampleRate);
        parameterA = pole(500, sampleRate / 32); // coefficients adapt over 500 ms
        cutAttackA = pole(8, sampleRate); cutReleaseA = pole(120, sampleRate);
        focusA = pole(350, sampleRate / 32); peakA = pole(300, sampleRate);
        filterParameterA = pole(20, sampleRate / 32);
        attackA = pole(attackMs, sampleRate); releaseA = pole(releaseMs, sampleRate);
    }
    void startLearn() noexcept { learner.start(); }
    void cancelLearn() noexcept { learner.cancel(); }
    bool learning() const noexcept { return learner.learning(); }
    double learnProgress() const noexcept { return learner.progress(); }
    LearnResult learnResult() const noexcept { return learner.getResult(); }
    const Meters& getMeters() const noexcept { return meters; }
    // Frozen adaptation/AGC values are part of the session state, not only UI parameters.
    using RuntimeState = std::array<double, 9>;
    RuntimeState runtimeState() const noexcept {
        return {thresholdDb, ratio, attackMs, releaseMs,
                agc[0].learnedDb(), agc[1].learnedDb(), agc[2].learnedDb(), focusHz,tubeAgc.learnedDb()};
    }
    void restoreRuntime(const RuntimeState& r) noexcept {
        thresholdDb = std::clamp(r[0], -100.0, 12.0); ratio = std::clamp(r[1], 1.0, 4.0);
        attackMs = std::clamp(r[2], 5.0, 80.0); releaseMs = std::clamp(r[3], 50.0, 500.0);
        attackA = pole(attackMs, sampleRate); releaseA = pole(releaseMs, sampleRate);
        for (size_t i = 0; i < agc.size(); ++i) agc[i].restore(r[4+i]);
        focusHz = trackedFocusHz = std::clamp(r[7], 30.0, 250.0);
        tubeAgc.restore(r[8]);
    }
    void clearHistory(bool preserveLearn=false) noexcept {
        for (auto& d : delays) d.clear();
        for (auto* bank : {&detectorsBass, &detectorsKick, &filters})
            for (auto& band : *bank) for (auto& f : band) f.z1 = f.z2 = 0;
        allpassX.fill(0); allpassY.fill(0);
        bassE.fill(0); kickE.fill(0); bassEnergy = envelope = kickFast = kickSlow = 0;
        inputPowerMeter = outputPowerMeter = outputHeld = kickHeld = 0;
        if(!preserveLearn)learner.cancel();
        tube.clear();directCutDb=0;directFilters={};
        for (auto& filter : kickEqFilters) filter.z1 = filter.z2 = 0;
        for (auto& filter : nextKickEqFilters) filter.z1 = filter.z2 = 0;
        wetPower = 0;
    }

    void process(float* const* bass, int channels, const float* const* kick, int kickChannels,
                 int samples, const Settings& s, double bpm = 0) noexcept {
        if (channels < 1 || channels > 2) return;
        tube.configure(s.tube);
        const int rampSamples=std::max(1,static_cast<int>(sampleRate*.02));
        if(!globalInitialised){globalRamp.reset(s.active?1:0);globalInitialised=true;}
        globalRamp.set(s.active?1:0,rampSamples);
        if(!s.active&&globalRamp.finished()){
            if(!globalPaused){clearHistory();globalPaused=true;}
            // Leave every input bit untouched, including IEEE signed zeros/subnormals.
            meters.matchGainDb=0;
            meters.glueReductionDb=meters.unmaskReductionDb=meters.tubeAgcDb=0;meters.agcDb.fill(0);meters.cutsDb.fill(0);
            for(int n=0;n<samples;++n){
                std::array<double,2> dry{};for(int c=0;c<channels;++c)dry[c]=bass[c][n];
                inputPowerMeter=follow(inputPowerMeter,power(dry,channels),energyA);
                outputHeld*=peakA;for(int c=0;c<channels;++c)outputHeld=std::max(outputHeld,std::abs(dry[c]));
            }
            meters.peak=outputHeld;meters.inputRmsDb=meters.outputRmsDb=db(std::sqrt(inputPowerMeter));
            return;
        }
        globalPaused=false;
        const double midTarget = s.unmaskStereoMode == UnmaskStereoMode::side ? 0 : 1;
        const double sideTarget = s.unmaskStereoMode == UnmaskStereoMode::mid ? 0 : 1;
        const double focusLow = std::clamp(std::min(s.unmaskFocusMinHz, s.unmaskFocusMaxHz), 30.0, 250.0);
        const double focusHigh = std::clamp(std::max(s.unmaskFocusMinHz, s.unmaskFocusMaxHz), 30.0, 250.0);
        if (!smoothed) {
            midMix.reset(midTarget);
            sideMix.reset(sideTarget);
            glueMix = s.glue ? 1 : 0; phaseMix = s.phase ? 1 : 0;
            unmaskMix = s.unmask ? 1 : 0; delaySamples = s.delayMs * sampleRate / 1000;
            polarity = s.invert ? -1 : 1; rotationMix = s.rotate ? 1 : 0;
            rotationHz = s.rotationHz; outputGain = gain(s.outputDb); smoothed = true;
            previousDelaySamples=delaySamples;delayFade.reset(1);polarityRamp.reset(polarity);
            deltaMix = s.delta && s.unmask && !(s.tube.enabled&&s.tube.audition) ? 1 : 0;
            directMix=s.unmaskDirect?1:0;
            smoothedFilterQ = 3.5 - 2.6 * std::clamp(s.unmaskWidth, 0.0, 1.0);
        }
        meters.unmaskAttackMs=std::clamp(s.unmaskAttackMs,1.0,50.0);
        meters.unmaskReleaseMs=s.unmaskSync?noteMilliseconds(s.unmaskReleaseNote,bpm):std::clamp(s.unmaskReleaseMs,30.0,500.0);
        cutAttackA = pole(meters.unmaskAttackMs, sampleRate);
        cutReleaseA = pole(meters.unmaskReleaseMs, sampleRate);
        const double trimTarget = gain(std::clamp(s.outputDb, -24.0, 12.0));
        const double delayTarget = std::clamp(s.delayMs, 0.0, 5.0)*sampleRate/1000;
        const double rotationTarget = std::clamp(s.rotationHz, 20.0, 500.0);
        const double glueAmount = std::clamp(s.glueAmount, 0.0, 1.0);
        const double unmaskDepth = std::clamp(s.unmaskMaxCutDb, 3.0, 12.0)*std::clamp(s.unmaskAmount, 0.0, 1.0);
        const double widthOctaves = 0.25 + 1.25 * std::clamp(s.unmaskWidth, 0.0, 1.0);
        const double filterQTarget = 3.5 - 2.6 * std::clamp(s.unmaskWidth, 0.0, 1.0);
        polarityRamp.set(s.invert?-1:1,rampSamples);
        midMix.set(midTarget, rampSamples);
        sideMix.set(sideTarget, rampSamples);
        const bool eqOn = s.kickEqEnabled && s.kickEqFrequency >= 30 && s.kickEqCut > 0;
        const double eqHz = std::clamp(s.kickEqFrequency, 30.0, 130.0);
        const double eqDb = std::clamp(s.kickEqCut, 0.0, 4.0);
        if (eqOn && kickEqRetune.finished() && (eqHz != kickEqHz || eqDb != kickEqDb)) {
            kickEqHz = eqHz;
            kickEqDb = eqDb;
            const bool retune = kickEqMix.value() > 0;
            auto& filters = retune ? nextKickEqFilters : kickEqFilters;
            filters = {};
            for (auto& filter : filters)
                filter.peak(kickEqHz, 3.0, -kickEqDb, sampleRate);
            if (retune) {
                kickEqRetune.reset(0);
                kickEqRetune.set(1, rampSamples);
            }
        }
        kickEqMix.set(eqOn ? 1 : 0, rampSamples);
        const bool runEq = eqOn || !kickEqMix.finished();
        if (!runEq) {
            for (auto& filter : kickEqFilters) filter.z1 = filter.z2 = 0;
            if (!kickEqRetune.finished()) {
                kickEqFilters = nextKickEqFilters;
                for (auto& filter : kickEqFilters) filter.z1 = filter.z2 = 0;
                kickEqRetune.reset(1);
            }
        }
        const bool normalMonitoring = !s.delta && !(s.tube.enabled && s.tube.audition);
        monitorMix.set(s.auditionDry && normalMonitoring ? 1 : 0, rampSamples);
        const bool match = s.matchEnabled && normalMonitoring;
        const double matchDb = match && inputPowerMeter > 1e-9 && wetPower > 1e-9
            ? std::clamp(10 * std::log10(inputPowerMeter / wetPower), -12.0, 12.0) : 0;
        monitorGain.set(matchDb == 0 ? 1 : gain(matchDb), rampSamples);
        const bool monitor = match || !monitorMix.finished() || monitorMix.value() != 0 || !monitorGain.finished();
        if (!match) wetPower = 0;
        for (int n = 0; n < samples; ++n) {
            const float dryLeft=bass[0][n],dryRight=channels==2?bass[1][n]:0;
            const double activeMix=globalRamp.next();
            const double mid = midMix.next();
            const double side = channels == 2 ? sideMix.next() : 0;
            const bool linked = mid == 1 && (channels == 1 || side == 1);
            std::array<double, 2> x{}, k{};
            for (int c = 0; c < channels; ++c) x[c] = std::isfinite(bass[c][n]) ? bass[c][n] : 0;
            if (kick && kickChannels > 0) {
                // Detector powers are linked; opposite-polarity stereo never cancels here.
                k[0] = std::isfinite(kick[0][n]) ? kick[0][n] : 0;
                k[1] = kickChannels > 1 ? (std::isfinite(kick[1][n]) ? kick[1][n] : 0) : k[0];
            }
            double inputPower = power(x, channels);
            inputPowerMeter = follow(inputPowerMeter, inputPower, energyA);
            if (runEq) {
                const double mix = kickEqMix.next();
                if (kickEqRetune.finished()) {
                    for (int c = 0; c < channels; ++c)
                        x[c] += mix * (kickEqFilters[c].tick(x[c]) - x[c]);
                } else {
                    const double fade = kickEqRetune.next();
                    for (int c = 0; c < channels; ++c) {
                        const double old = kickEqFilters[c].tick(x[c]);
                        const double next = nextKickEqFilters[c].tick(x[c]);
                        x[c] += mix * (old + fade * (next - old) - x[c]);
                    }
                    if (kickEqRetune.finished()) kickEqFilters = nextKickEqFilters;
                }
                inputPower = power(x, channels);
            }
            const double kp = power(k, 2);
            kickHeld = std::max(std::max(std::abs(k[0]), std::abs(k[1])), kickHeld * peakA);
            bassEnergy = follow(bassEnergy, inputPower, energyA);
            envelope = follow(envelope, inputPower, rmsA);
            kickFast = follow(kickFast, kp, kickFastA); kickSlow = follow(kickSlow, kp, kickSlowA);
            ++onsetClock;
            if (kickFast > 1e-7 && kickFast > kickSlow * 2.5
                && onsetClock - lastOnset > static_cast<std::uint64_t>(sampleRate * 0.12)) {
                const double interval = 1000.0 * static_cast<double>(onsetClock - lastOnset) / sampleRate;
                if (lastOnset > 0 && interval < 2000) kickIntervalMs = interval;
                lastOnset = onsetClock;
            }
            if (++controlCounter == 32) {
                controlCounter = 0;
                smoothedFilterQ = follow(smoothedFilterQ, filterQTarget, filterParameterA);
                if (!s.glueLocked && bassEnergy > 1e-10) {
                    thresholdDb = follow(thresholdDb, db(std::sqrt(bassEnergy)) - 3, parameterA);
                    const double crest = std::clamp(db(std::sqrt(std::max(envelope, 1e-20) / bassEnergy)), 0.0, 12.0);
                    ratio = follow(ratio, 1.5 + 2 * std::clamp(s.glueAmount, 0.0, 1.0), parameterA);
                    attackMs = follow(attackMs, 15 + crest, parameterA);
                    const bool validBpm = std::isfinite(bpm) && bpm >= 20 && bpm <= 400;
                    const bool recentKick = onsetClock - lastOnset < static_cast<std::uint64_t>(sampleRate * 2) && kickIntervalMs > 0;
                    const double context = recentKick ? (validBpm ? (kickIntervalMs + 60000 / bpm) / 2 : kickIntervalMs)
                                                     : (validBpm ? 60000 / bpm : 450);
                    releaseMs = follow(releaseMs, std::clamp(context * 0.4, 80.0, 300.0), parameterA);
                }
                if(s.glueTiming!=0){
                    attackMs=std::clamp(s.glueAttackMs,.1,500.0);
                    releaseMs=s.glueTiming==2?noteMilliseconds(s.glueReleaseNote,bpm):std::clamp(s.glueReleaseMs,1.0,4000.0);
                }
                attackA = pole(attackMs, sampleRate); releaseA = pole(releaseMs, sampleRate);
                // Track the strongest overlapping low-end band, not an asserted musical fundamental.
                // Hysteresis prevents bouncing between adjacent resonances on each kick.
                if (s.unmaskTrack && !s.unmaskHold) {
                    size_t best = 0, current = 0;
                    double score = 0, nearest = 1e9;
                    for (size_t b = 0; b < frequencies.size(); ++b) {
                        if (frequencies[b] < focusLow || frequencies[b] > focusHigh) continue;
                        const double overlap = std::min(bassE[b], kickE[b]);
                        if (overlap > score) { score = overlap; best = b; }
                        const double distance = std::abs(std::log(frequencies[b] / trackedFocusHz));
                        if (distance < nearest) { nearest = distance; current = b; }
                    }
                    if (score > 1e-8 && (score > std::min(bassE[current], kickE[current]) * 1.6
                        || std::min(bassE[current], kickE[current]) < 1e-9))
                        trackedFocusHz = frequencies[best];
                }
                const double targetFocus = s.unmaskTrack ? std::clamp(trackedFocusHz, focusLow, focusHigh) : std::clamp(s.unmaskFrequency, 30.0, 250.0);
                if (!s.unmaskHold || !s.unmaskTrack) {
                    focusHz = std::exp(follow(std::log(focusHz), std::log(targetFocus), focusA));
                    if (s.unmaskTrack && (focusLow > 30 || focusHigh < 250))
                        focusHz = std::clamp(focusHz, focusLow, focusHigh);
                }
                double weightSum = 0;
                for (size_t b = 0; b < frequencies.size(); ++b) {
                    const double octaves = std::log2(frequencies[b] / focusHz) / widthOctaves;
                    focusWeights[b] = std::exp(-0.5 * octaves * octaves);
                    weightSum += focusWeights[b];
                }
                double commandedCutSum = 0;
                for (double value : cutDb) commandedCutSum -= value;
                const double cap = std::clamp(s.unmaskMaxCutDb, 3.0, 12.0);
                const double correction = commandedCutSum > cap ? cap / commandedCutSum : 1;
                for (size_t b = 0; b < frequencies.size(); ++b) {
                    cutDb[b] *= correction;
                    focusWeights[b] /= std::max(weightSum, 1e-9);
                    auto& left = filters[b][0]; auto& right = filters[b][1];
                    left.peak(frequencies[b], smoothedFilterQ, cutDb[b], sampleRate);
                    right.b0=left.b0; right.b1=left.b1; right.b2=left.b2; right.a1=left.a1; right.a2=left.a2;
                    if (std::abs(cutDb[b]) < 1e-12) right.z1=right.z2=0;
                }
            }
            glueMix = follow(glueMix, s.glue ? 1 : 0, smoothA);
            phaseMix = follow(phaseMix, s.phase ? 1 : 0, smoothA);
            unmaskMix = follow(unmaskMix, s.unmask ? 1 : 0, smoothA);
            const double over = db(std::sqrt(envelope)) - thresholdDb;
            const double knee = 6, slope = 1 - 1 / ratio;
            const double desired = over <= -knee/2 ? 0 : (over >= knee/2 ? slope * over
                                    : slope * (over+knee/2)*(over+knee/2)/(2*knee));
            glueReduction = follow(glueReduction, desired * glueAmount,
                                   desired * s.glueAmount > glueReduction ? attackA : releaseA);
            double budget = 6;
            const auto compensate = [&](int module, double in, double out, double mix) {
                auto cfg = s.agc[static_cast<size_t>(module)];
                if (mix < 1e-6) cfg.enabled = false;
                const double g = agc[static_cast<size_t>(module)].tick(in, out, cfg, budget);
                const double applied = 1 + mix * (g - 1);
                budget -= std::max(0.0, db(applied));
                meters.agcDb[static_cast<size_t>(module)] = db(applied);
                return applied;
            };
            const double glueG = 1 + glueMix * (gain(-glueReduction) - 1);
            for (auto& v : x) v *= glueG;
            const double glueAgc = compensate(0, inputPower, power(x, channels), glueMix);
            for (auto& v : x) v *= glueAgc;

            if (learner.learning()) {
                const double monoBass = channels == 2 ? (x[0]+x[1])*0.5 : x[0];
                learner.tick(monoBass, (k[0]+k[1])*0.5);
            }
            // Crossfade two fixed delay taps instead of sweeping delay/pitch.
            // Rapid changes are coalesced until the current finite fade completes.
            if(delayFade.finished()&&delaySamples!=delayTarget){previousDelaySamples=delaySamples;delaySamples=delayTarget;delayFade.reset(0);delayFade.set(1,rampSamples);}
            const double delayBlend=delayFade.next();
            polarity = polarityRamp.next();
            rotationMix = follow(rotationMix, s.rotate ? 1 : 0, smoothA);
            rotationHz = follow(rotationHz, rotationTarget, smoothA);
            if (rotationHz != cachedRotationHz) {
                const double t = std::tan(pi * rotationHz / sampleRate);
                cachedAllpassA = (t-1)/(t+1); cachedRotationHz = rotationHz;
            }
            const double a = cachedAllpassA;
            double phaseReference = 0;
            for (int c = 0; c < channels; ++c) {
                delays[c].push(x[c]);
                double d = delays[c].read(std::clamp(delaySamples, 0.0, sampleRate * 0.005));
                if(delayBlend<1){const double old=delays[c].read(previousDelaySamples);d=old+delayBlend*(d-old);}
                phaseReference += ((1-phaseMix)*x[c]*x[c] + phaseMix*d*d) / channels;
                const double rotated = a*d + allpassX[c] - a*allpassY[c];
                allpassX[c] = d; allpassY[c] = std::abs(rotated) < 1e-25 ? 0 : rotated;
                x[c] += phaseMix * ((d + rotationMix*(rotated-d))*polarity - x[c]);
            }
            const double phaseAgc = compensate(1, phaseReference, power(x, channels), phaseMix);
            for (auto& v : x) v *= phaseAgc;

            // Optional parallel Soft Tube before UNMASK. No arithmetic is added
            // to x in the disabled/settled branch, preserving the 0.2.2 output.
            const double tubeInput=power(x,channels);
            tube.frame(x,channels);
            auto tubeCfg=AgcSettings{s.tube.autoGain&&!s.tube.audition,s.tube.gainLock};
            if(!tube.active())tubeCfg.enabled=false;
            const double learnedTubeGain=tubeAgc.tick(tubeInput,power(x,channels),tubeCfg,budget);
            const double tubeGain=1+tube.wetMix()*(learnedTubeGain-1);
            if(tubeGain!=1)for(auto& v:x)v*=tubeGain;
            meters.tubeAgcDb=db(tubeGain);budget-=std::max(0.0,meters.tubeAgcDb);
            const auto beforeUnmask = x;
            const auto selectedPower = [&](const std::array<double, 2>& signal) {
                if (linked) return power(signal, channels);
                if (channels == 1) return mid * signal[0] * signal[0];
                const double centre = (signal[0] + signal[1]) * .5;
                const double difference = (signal[0] - signal[1]) * .5;
                return mid * centre * centre + side * difference * difference;
            };
            const auto applySelectedChange = [&](std::array<double, 2>& signal, double left, double right) {
                if (channels == 1) {
                    signal[0] += mid * left;
                    return;
                }
                const double centre = mid * (left + right) * .5;
                const double difference = side * (left - right) * .5;
                signal[0] += centre + difference;
                signal[1] += centre - difference;
            };
            double maxCut = 0,directActivity=0;
            const double maxWeight=*std::max_element(focusWeights.begin(),focusWeights.end());
            auto legacy=x;
            for (size_t b = 0; b < frequencies.size(); ++b) {
                double be = 0, ke = 0;
                std::array<double, 2> detectedBass{};
                for (int c = 0; c < channels; ++c) {
                    const double v = detectorsBass[b][c].tick(beforeUnmask[c]);
                    detectedBass[c] = v;
                    if (linked) be += v*v/channels;
                }
                if (!linked) be = selectedPower(detectedBass);
                for (int c = 0; c < 2; ++c) {
                    const double v = detectorsKick[b][c].tick(k[c]); ke += v*v/2;
                }
                bassE[b] = follow(bassE[b], be, bandA); kickE[b] = follow(kickE[b], ke, bandA);
                // Overlap requires both signals. No kick => decay to unity, never duck full-band.
                const double overlap = std::min(bassE[b], kickE[b]);
                // Exact saturated branches avoid log10 for inactive or fully active bands.
                const double activity = overlap <= 3.1622776601683792e-7 ? 0.0
                    : overlap >= 3.1622776601683794e-4 ? 1.0
                    : (10*std::log10(overlap)+65)/30;
                const double target = -unmaskDepth * activity * focusWeights[b];
                cutDb[b] = follow(cutDb[b], target, target < cutDb[b] ? cutAttackA : cutReleaseA);
                maxCut = std::max(maxCut, -cutDb[b]);
                directActivity=std::max(directActivity,activity*focusWeights[b]/std::max(1e-12,maxWeight));
                for (int c = 0; c < channels; ++c) legacy[c] = filters[b][c].tick(legacy[c]);
            }
            // dB control is a calibrated single dynamic bell: the requested
            // attenuation is its centre gain at full detector activity. Legacy
            // sessions keep the former distributed-band path until dB is selected.
            const double directTarget=-std::clamp(s.unmaskAttenuationDb,0.0,24.0)*directActivity;
            directCutDb=follow(directCutDb,directTarget,directTarget<directCutDb?cutAttackA:cutReleaseA);
            if(controlCounter%32==0){
                directFilters[0].peak(focusHz,smoothedFilterQ,directCutDb,sampleRate);
                auto& a=directFilters[0];auto& b=directFilters[1];
                b.b0=a.b0;b.b1=a.b1;b.b2=a.b2;b.a1=a.a1;b.a2=a.a2;
                if(std::abs(directCutDb)<1e-12)b.z1=b.z2=0;
            }
            directMix=follow(directMix,s.unmaskDirect?1:0,smoothA);
            for (int c = 0; c < channels; ++c){
                const double direct=directFilters[size_t(c)].tick(beforeUnmask[c]);
                const double processed=directMix==0?legacy[c]:directMix==1?direct:legacy[c]+directMix*(direct-legacy[c]);
                x[c]=beforeUnmask[c]+unmaskMix*(processed-beforeUnmask[c]);
            }
            // Identical L/R filters commute with the M/S matrix. Project their
            // correction, keeping filter history continuous across mode changes.
            if (!linked) {
                const auto filtered = x;
                x = beforeUnmask;
                applySelectedChange(x, filtered[0] - x[0], filtered[1] - x[1]);
            }
            const auto afterUnmask = x;
            const double unmaskAgc = compensate(2, selectedPower(beforeUnmask), selectedPower(x), unmaskMix);
            if (!linked)
                applySelectedChange(x, x[0] * (unmaskAgc - 1), x[1] * (unmaskAgc - 1));
            outputGain = follow(outputGain, trimTarget, smoothA);
            deltaMix = follow(deltaMix, s.delta && s.unmask && !(s.tube.enabled&&s.tube.audition) ? 1 : 0, smoothA);
            const double listenDry = monitor ? monitorMix.next() : 0;
            const double listenGain = monitor ? monitorGain.next() : 1;
            double wet = 0;
            for (int c = 0; c < channels; ++c) {
                // Delta is removed bass BEFORE AGC/output trim; never includes kick.
                const double normal = (linked ? x[c]*unmaskAgc : x[c])*outputGain;
                double out = normal + deltaMix*(beforeUnmask[c]-afterUnmask[c]-normal);
                const float dry=c==0?dryLeft:dryRight;
                if (monitor) {
                    if (match) wet += out * out;
                    const double reference = std::isfinite(dry) ? dry : 0;
                    const double matched = out * listenGain;
                    out = matched + listenDry * (reference - matched);
                }
                bass[c][n] = activeMix==0?dry:static_cast<float>(activeMix==1?out:static_cast<double>(dry)+activeMix*(out-dry));
                outputHeld = std::max(std::abs(static_cast<double>(bass[c][n])), outputHeld * peakA);
            }
            if (match) wetPower = follow(wetPower, wet / channels, energyA);
            outputPowerMeter = follow(outputPowerMeter,
                static_cast<double>(bass[0][n])*bass[0][n]/channels
                + (channels == 2 ? static_cast<double>(bass[1][n])*bass[1][n]/channels : 0), energyA);
            meters.glueReductionDb = glueReduction * glueMix;
            meters.unmaskReductionDb = (s.unmaskDirect?-directCutDb:maxCut) * unmaskMix; // maximum individual band, not total cut
        }
        meters.matchGainDb = match ? db(monitorGain.value()) : 0;
        meters.tubeAddedDb=db(std::sqrt(tube.addedPower()));
        meters.peak = outputHeld; meters.kickPeak = kickHeld;
        meters.inputRmsDb = db(std::sqrt(inputPowerMeter)); meters.outputRmsDb = db(std::sqrt(outputPowerMeter));
        meters.focusHz = focusHz; meters.filterQ = smoothedFilterQ;
        meters.attackMs = attackMs; meters.releaseMs = releaseMs;
        for (size_t b = 0; b < frequencies.size(); ++b) {
            meters.bassDb[b] = db(std::sqrt(bassE[b])); meters.kickDb[b] = db(std::sqrt(kickE[b]));
            meters.cutsDb[b] = cutDb[b] * unmaskMix;
        }
    }
private:
    std::array<Biquad, 2> kickEqFilters{}, nextKickEqFilters{};
    Ramp kickEqMix, kickEqRetune, monitorMix, monitorGain;
    double kickEqHz = 0, kickEqDb = 0, wetPower = 0;
    static constexpr auto frequencies = bandFrequencies;
    std::array<std::array<Biquad, 2>, bandCount> detectorsBass{}, detectorsKick{}, filters{};
    std::array<double, bandCount> bassE{}, kickE{}, cutDb{}, focusWeights{};
    std::array<Delay, 2> delays;
    std::array<AutoGain, 3> agc;
    PhaseLearner learner;
    SoftTube tube;AutoGain tubeAgc;
    Meters meters;
    std::array<double, 2> allpassX{}, allpassY{};
    double sampleRate = 48000, bassEnergy = 0, envelope = 0, kickFast = 0, kickSlow = 0;
    double thresholdDb = -24, ratio = 2, attackMs = 20, releaseMs = 180, glueReduction = 0;
    double energyA = 0, rmsA = 0, smoothA = 0, bandA = 0, parameterA = 0;
    double kickFastA = 0, kickSlowA = 0, attackA = 0, releaseA = 0, cutAttackA = 0, cutReleaseA = 0;
    std::array<Biquad,2> directFilters{};double directCutDb=0,directMix=0;
    double glueMix = 0, phaseMix = 0, unmaskMix = 0, delaySamples = 0, polarity = 1;
    double rotationMix = 0, rotationHz = 90, outputGain = 1, kickIntervalMs = 0;
    double deltaMix = 0;
    double cachedRotationHz = -1, cachedAllpassA = 0;
    double smoothedFilterQ = 2, filterParameterA = 0;
    double inputPowerMeter = 0, outputPowerMeter = 0, outputHeld = 0, kickHeld = 0;
    double focusHz = 75, trackedFocusHz = 75, focusA = 0, peakA = 0;
    std::uint64_t onsetClock = 0, lastOnset = 0;
    int controlCounter = 0;
    bool smoothed = false;
    Ramp globalRamp,delayFade,polarityRamp,midMix,sideMix;
    bool globalInitialised=false,globalPaused=false;
    double previousDelaySamples=0;
};
} // namespace bassdeamon
