#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

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

struct AgcSettings { bool enabled = false, locked = false; };
struct Settings {
    bool active = true;
    bool glue = true, glueLocked = false;
    double glueAmount = 0.35;
    bool phase = false, invert = false, rotate = false;
    double delayMs = 0.0, rotationHz = 90.0;
    bool unmask = true, delta = false;
    double unmaskAmount = 0.4, outputDb = 0.0;
    std::array<AgcSettings, 3> agc{};
    bool unmaskTrack = true, unmaskHold = false;
    double unmaskFrequency = 75, unmaskWidth = 0.55;
    double unmaskAttackMs = 8, unmaskReleaseMs = 120, unmaskMaxCutDb = 9;
};
constexpr size_t bandCount = 12;
inline constexpr std::array<double, bandCount> bandFrequencies{30, 38, 48, 60, 75, 94, 118, 148, 186, 234, 295, 370};
struct Meters {
    double glueReductionDb = 0.0, unmaskReductionDb = 0.0;
    std::array<double, 3> agcDb{};
    double peak = 0.0, kickPeak = 0.0, inputRmsDb = -120, outputRmsDb = -120;
    double focusHz = 75, attackMs = 20, releaseMs = 180;
    std::array<double, bandCount> bassDb{}, kickDb{}, cutsDb{};
    double filterQ = 2;
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
    LearnRejection rejection = LearnRejection::insufficient;
};
class PhaseLearner {
public:
    void prepare(double sr) {
        sampleRate = sr; decimation = std::max(1, static_cast<int>(sr / 2000));
        lowPole = std::exp(-2 * pi * 250 / sr); history.prepare(sr); start(); active = false;
    }
    void start() noexcept {
        for(auto* a:{&cross,&bassPower,&bassSum,&firstCross,&firstBassPower,&firstBassSum})a->fill(0);
        kickPower=kickSum=firstKickPower=firstKickSum=0;count=firstCount=0;
        elapsed = 0; lowBass = lowKick = 0; active = true; result = {};
    }
    void cancel() noexcept { active = false; }
    void tick(double bass, double kick) noexcept {
        // start() resets the low-pass states, and ignores the first 20 ms (history <=5 ms).
        // No need to run the learner while it is not listening.
        if (!active) return;
        lowBass = follow(lowBass, bass, lowPole); lowKick = follow(lowKick, kick, lowPole);
        history.push(lowBass);
        ++elapsed;
        if (elapsed > static_cast<int>(sampleRate * 0.02) && elapsed % decimation == 0
            && lowKick * lowKick > 1e-8) {
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
            for(size_t i=0;i<cross.size();++i)if(std::abs(static_cast<int>(i)-static_cast<int>(bestIndex))>=4)
                competitor=std::max(competitor,std::abs(correlations[i]));
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
            const bool stable=first[2]>=.55&&second[2]>=.55&&first[1]==second[1]&&std::abs(first[0]-second[0])<=2;
            result.rejection=count<600||firstCount<200||count-firstCount<200||kickPower<=1e-5?LearnRejection::insufficient
                :best<.55?LearnRejection::weak:result.separation<.08?LearnRejection::ambiguous:!stable?LearnRejection::unstable:LearnRejection::none;
            result.valid=result.rejection==LearnRejection::none;
        }
    }
    bool learning() const noexcept { return active; }
    LearnResult getResult() const noexcept { return result; }
private:
    std::array<double,21> cross{},bassPower{},bassSum{},firstCross{},firstBassPower{},firstBassSum{};
    double sampleRate = 48000, lowPole = 0, lowBass = 0, lowKick = 0, kickPower = 0;
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
        bassEnergy = envelope = kickFast = kickSlow = 0;
        thresholdDb = -24; ratio = 2; attackMs = 20; releaseMs = 180;
        glueReduction = 0; onsetClock = lastOnset = 0; kickIntervalMs = 0;
        controlCounter = 0; smoothed = false; meters = {};
        globalInitialised=globalPaused=false;globalRamp.reset(1);delayFade.reset(1);polarityRamp.reset(1);
        inputPowerMeter = outputPowerMeter = outputHeld = kickHeld = 0;
        focusHz = 75; trackedFocusHz = 75;
        cachedRotationHz = -1; cachedAllpassA = 0;
        cutDb.fill(0); kickE.fill(0); bassE.fill(0);
        allpassX.fill(0); allpassY.fill(0);
        for (auto& d : delays) d.prepare(sampleRate);
        learner.prepare(sampleRate);
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
    LearnResult learnResult() const noexcept { return learner.getResult(); }
    const Meters& getMeters() const noexcept { return meters; }
    // Frozen adaptation/AGC values are part of the session state, not only UI parameters.
    using RuntimeState = std::array<double, 8>;
    RuntimeState runtimeState() const noexcept {
        return {thresholdDb, ratio, attackMs, releaseMs,
                agc[0].learnedDb(), agc[1].learnedDb(), agc[2].learnedDb(), focusHz};
    }
    void restoreRuntime(const RuntimeState& r) noexcept {
        thresholdDb = std::clamp(r[0], -100.0, 12.0); ratio = std::clamp(r[1], 1.0, 4.0);
        attackMs = std::clamp(r[2], 5.0, 80.0); releaseMs = std::clamp(r[3], 50.0, 500.0);
        attackA = pole(attackMs, sampleRate); releaseA = pole(releaseMs, sampleRate);
        for (size_t i = 0; i < agc.size(); ++i) agc[i].restore(r[4+i]);
        focusHz = trackedFocusHz = std::clamp(r[7], 30.0, 250.0);
    }
    void clearHistory() noexcept {
        for (auto& d : delays) d.clear();
        for (auto* bank : {&detectorsBass, &detectorsKick, &filters})
            for (auto& band : *bank) for (auto& f : band) f.z1 = f.z2 = 0;
        allpassX.fill(0); allpassY.fill(0);
        bassE.fill(0); kickE.fill(0); bassEnergy = envelope = kickFast = kickSlow = 0;
        inputPowerMeter = outputPowerMeter = outputHeld = kickHeld = 0;
        learner.cancel();
    }

    void process(float* const* bass, int channels, const float* const* kick, int kickChannels,
                 int samples, const Settings& s, double bpm = 0) noexcept {
        if (channels < 1 || channels > 2) return;
        const int rampSamples=std::max(1,static_cast<int>(sampleRate*.02));
        if(!globalInitialised){globalRamp.reset(s.active?1:0);globalInitialised=true;}
        globalRamp.set(s.active?1:0,rampSamples);
        if(!s.active&&globalRamp.finished()){
            if(!globalPaused){clearHistory();globalPaused=true;}
            // Leave every input bit untouched, including IEEE signed zeros/subnormals.
            meters.glueReductionDb=meters.unmaskReductionDb=0;meters.agcDb.fill(0);meters.cutsDb.fill(0);
            for(int n=0;n<samples;++n){
                std::array<double,2> dry{};for(int c=0;c<channels;++c)dry[c]=bass[c][n];
                inputPowerMeter=follow(inputPowerMeter,power(dry,channels),energyA);
                outputHeld*=peakA;for(int c=0;c<channels;++c)outputHeld=std::max(outputHeld,std::abs(dry[c]));
            }
            meters.peak=outputHeld;meters.inputRmsDb=meters.outputRmsDb=db(std::sqrt(inputPowerMeter));
            return;
        }
        globalPaused=false;
        if (!smoothed) {
            glueMix = s.glue ? 1 : 0; phaseMix = s.phase ? 1 : 0;
            unmaskMix = s.unmask ? 1 : 0; delaySamples = s.delayMs * sampleRate / 1000;
            polarity = s.invert ? -1 : 1; rotationMix = s.rotate ? 1 : 0;
            rotationHz = s.rotationHz; outputGain = gain(s.outputDb); smoothed = true;
            previousDelaySamples=delaySamples;delayFade.reset(1);polarityRamp.reset(polarity);
            deltaMix = s.delta && s.unmask ? 1 : 0;
            smoothedFilterQ = 3.5 - 2.6 * std::clamp(s.unmaskWidth, 0.0, 1.0);
        }
        cutAttackA = pole(std::clamp(s.unmaskAttackMs, 1.0, 50.0), sampleRate);
        cutReleaseA = pole(std::clamp(s.unmaskReleaseMs, 30.0, 500.0), sampleRate);
        const double trimTarget = gain(std::clamp(s.outputDb, -24.0, 12.0));
        const double delayTarget = std::clamp(s.delayMs, 0.0, 5.0)*sampleRate/1000;
        const double rotationTarget = std::clamp(s.rotationHz, 20.0, 500.0);
        const double glueAmount = std::clamp(s.glueAmount, 0.0, 1.0);
        const double unmaskDepth = std::clamp(s.unmaskMaxCutDb, 3.0, 12.0)*std::clamp(s.unmaskAmount, 0.0, 1.0);
        const double widthOctaves = 0.25 + 1.25 * std::clamp(s.unmaskWidth, 0.0, 1.0);
        const double filterQTarget = 3.5 - 2.6 * std::clamp(s.unmaskWidth, 0.0, 1.0);
        polarityRamp.set(s.invert?-1:1,rampSamples);
        for (int n = 0; n < samples; ++n) {
            const float dryLeft=bass[0][n],dryRight=channels==2?bass[1][n]:0;
            const double activeMix=globalRamp.next();
            std::array<double, 2> x{}, k{};
            for (int c = 0; c < channels; ++c) x[c] = std::isfinite(bass[c][n]) ? bass[c][n] : 0;
            if (kick && kickChannels > 0) {
                // Detector powers are linked; opposite-polarity stereo never cancels here.
                k[0] = std::isfinite(kick[0][n]) ? kick[0][n] : 0;
                k[1] = kickChannels > 1 ? (std::isfinite(kick[1][n]) ? kick[1][n] : 0) : k[0];
            }
            const double inputPower = power(x, channels), kp = power(k, 2);
            inputPowerMeter = follow(inputPowerMeter, inputPower, energyA);
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
                attackA = pole(attackMs, sampleRate); releaseA = pole(releaseMs, sampleRate);
                // Track the strongest overlapping low-end band, not an asserted musical fundamental.
                // Hysteresis prevents bouncing between adjacent resonances on each kick.
                if (s.unmaskTrack && !s.unmaskHold) {
                    size_t best = 0, current = 0;
                    double score = 0, nearest = 1e9;
                    for (size_t b = 0; b < frequencies.size(); ++b) {
                        if (frequencies[b] > 250) continue;
                        const double overlap = std::min(bassE[b], kickE[b]);
                        if (overlap > score) { score = overlap; best = b; }
                        const double distance = std::abs(std::log(frequencies[b] / trackedFocusHz));
                        if (distance < nearest) { nearest = distance; current = b; }
                    }
                    if (score > 1e-8 && (score > std::min(bassE[current], kickE[current]) * 1.6
                        || std::min(bassE[current], kickE[current]) < 1e-9))
                        trackedFocusHz = std::clamp(frequencies[best], 30.0, 250.0);
                }
                const double targetFocus = s.unmaskTrack ? trackedFocusHz : std::clamp(s.unmaskFrequency, 30.0, 250.0);
                if (!s.unmaskHold || !s.unmaskTrack) focusHz = std::exp(follow(std::log(focusHz), std::log(targetFocus), focusA));
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

            const double monoBass = channels == 2 ? (x[0]+x[1])*0.5 : x[0];
            learner.tick(monoBass, (k[0]+k[1])*0.5);
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

            const auto beforeUnmask = x;
            double maxCut = 0;
            for (size_t b = 0; b < frequencies.size(); ++b) {
                double be = 0, ke = 0;
                for (int c = 0; c < channels; ++c) {
                    const double v = detectorsBass[b][c].tick(beforeUnmask[c]); be += v*v/channels;
                }
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
                for (int c = 0; c < channels; ++c) x[c] = filters[b][c].tick(x[c]);
            }
            for (int c = 0; c < channels; ++c) x[c] = beforeUnmask[c] + unmaskMix*(x[c]-beforeUnmask[c]);
            const auto afterUnmask = x;
            const double unmaskAgc = compensate(2, power(beforeUnmask, channels), power(x, channels), unmaskMix);
            outputGain = follow(outputGain, trimTarget, smoothA);
            deltaMix = follow(deltaMix, s.delta && s.unmask ? 1 : 0, smoothA);
            for (int c = 0; c < channels; ++c) {
                // Delta is removed bass BEFORE AGC/output trim; never includes kick.
                const double normal = x[c]*unmaskAgc*outputGain;
                const double out = normal + deltaMix*(beforeUnmask[c]-afterUnmask[c]-normal);
                const float dry=c==0?dryLeft:dryRight;
                bass[c][n] = activeMix==0?dry:static_cast<float>(activeMix==1?out:static_cast<double>(dry)+activeMix*(out-dry));
                outputHeld = std::max(std::abs(static_cast<double>(bass[c][n])), outputHeld * peakA);
            }
            outputPowerMeter = follow(outputPowerMeter,
                static_cast<double>(bass[0][n])*bass[0][n]/channels
                + (channels == 2 ? static_cast<double>(bass[1][n])*bass[1][n]/channels : 0), energyA);
            meters.glueReductionDb = glueReduction * glueMix;
            meters.unmaskReductionDb = maxCut * unmaskMix; // maximum individual band, not total cut
        }
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
    static constexpr auto frequencies = bandFrequencies;
    std::array<std::array<Biquad, 2>, bandCount> detectorsBass{}, detectorsKick{}, filters{};
    std::array<double, bandCount> bassE{}, kickE{}, cutDb{}, focusWeights{};
    std::array<Delay, 2> delays;
    std::array<AutoGain, 3> agc;
    PhaseLearner learner;
    Meters meters;
    std::array<double, 2> allpassX{}, allpassY{};
    double sampleRate = 48000, bassEnergy = 0, envelope = 0, kickFast = 0, kickSlow = 0;
    double thresholdDb = -24, ratio = 2, attackMs = 20, releaseMs = 180, glueReduction = 0;
    double energyA = 0, rmsA = 0, smoothA = 0, bandA = 0, parameterA = 0;
    double kickFastA = 0, kickSlowA = 0, attackA = 0, releaseA = 0, cutAttackA = 0, cutReleaseA = 0;
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
    Ramp globalRamp,delayFade,polarityRamp;
    bool globalInitialised=false,globalPaused=false;
    double previousDelaySamples=0;
};
} // namespace bassdeamon
