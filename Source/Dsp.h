#pragma once
#include <complex>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "SoftTube.h"
#include "SubMono.h"

namespace bassdeamon {
constexpr double pi = 3.14159265358979323846;
inline double db(double x) noexcept { return 20.0 * std::log10(std::max(x, 1.0e-12)); }
inline double gain(double d) noexcept { return d == 0 ? 1.0 : std::pow(10.0, d / 20.0); }
inline double pole(double ms, double sr) noexcept { return ms > 0 ? std::exp(-1.0 / (0.001 * ms * sr)) : 0.0; }
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

inline double phaseDegreesAtReference(double degrees,double fromHz,double toHz,double sr) noexcept {
    if(std::abs(degrees)<1e-9)return 0;
    fromHz=std::clamp(fromHz,30.,130.);
    toHz=std::clamp(toHz,30.,130.);
    if(fromHz==toHz)return degrees;
    const double lag=(degrees>0?360-degrees:-degrees)*pi/720;
    const double corner=std::tan(pi*fromHz/sr)/std::tan(lag);
    double result=-720/pi*std::atan(std::tan(pi*toHz/sr)/corner);
    while(result < -180)result+=360;
    while(result > 180)result-=360;
    return result;
}
struct Settings {
    bool active = true;
    bool pausePhaseLearn = false;
    bool duck = false, duckSync = false;
    int duckReleaseNote = 2;
    bool duckMultiband = false;
    std::array<double,3> duckBands{1,.65,.35}; // Body, Punch, Click
    double duckLowHz = 0; // Zero retains the legacy three-band path.
    double duckAmountDb = 3, duckAttackMs = 1.5, duckReleaseMs = 65;
    bool subMono = false;
    double subMonoHz = 120, auditionDryDb = 0, auditionWetDb = 0;
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
    double phaseAngle=0, phaseReference=60, kickReferenceHz=0;
    bool unmask = true, delta = false;
    double unmaskAmount = 0.4, outputDb = 0.0, balanceTrimDb=0;
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
    double duckReductionDb = 0;
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
    void lowShelf(double frequency, double linearGain, double sr) noexcept {
        const double w = 2 * pi * frequency / sr, A = std::sqrt(linearGain);
        const double c = std::cos(w), beta = std::sqrt(2 * A) * std::sin(w);
        const double sum = A + 1, difference = A - 1;
        const double a0 = sum + difference * c + beta;
        b0 = A * (sum - difference * c + beta) / a0;
        b1 = 2 * A * (difference - sum * c) / a0;
        b2 = A * (sum - difference * c - beta) / a0;
        a1 = -2 * (difference + sum * c) / a0;
        a2 = (sum + difference * c - beta) / a0;
    }
    std::complex<double> transfer(std::complex<double> z) const noexcept {
        const auto z2 = z * z;
        return (b0 + b1 * z + b2 * z2) / (1.0 + a1 * z + a2 * z2);
    }
};

struct UnmaskFundamentalBand {
    void tune(double hz, double rate) noexcept {
        sections = {};
        for (auto& section : sections) section.bandpass(hz, 3.5, rate);
    }
    double tick(double sample, double* early = nullptr) noexcept {
        const double front = sections[0].tick(sample);
        if (early) *early = front;
        return sections[1].tick(front);
    }
private:
    std::array<Biquad, 2> sections{};
};

inline double unmaskActivity(double energy, double spanDb = 30) noexcept {
    if (energy <= 3.1622776601683792e-7) return 0;
    if (energy >= 3.1622776601683794e-4) return 1;
    return std::min((10 * std::log10(energy) + 65) / spanDb, 1.);
}

inline constexpr double kickPeakHoldMs = 300.;

inline double unmaskKeySpan(double peak) noexcept {
    // A quiet key can supply full evidence above the same absolute noise floor.
    return peak >= .063095734448 ? 30. : std::clamp(54 + db(peak), 15., 30.);
}

inline double unmaskOverlapActivity(double bass, double kick, double keySpan) noexcept {
    if (bass <= kick || keySpan == 30) return unmaskActivity(std::min(bass, kick));
    const double keyActivity = unmaskActivity(kick, keySpan);
    return keyActivity > 0 ? std::min(unmaskActivity(bass), keyActivity) : 0.;
}

inline double unmaskFundamentalActivity(double bass, double kick, double fullBass, double fullKick,
                                        double kickForBalance = 0., double keySpan = 30.) noexcept {
    // Reject the skirts of a much louder harmonic in either detector.
    if (bass <= fullBass * .004 || kick <= fullKick * .004) return 0;
    const double activity = unmaskOverlapActivity(bass, kick, keySpan);
    if (activity == 0) return 0;
    // Use a soft knee over the levels left by Duck. The full setting is
    // reserved for a bass that actually dominates the kick in this band.
    const double relative = std::sqrt(bass / std::max(kick, kickForBalance));
    const double weight = std::clamp((relative - .0631) / (1.4125 - .0631), 0., 1.);
    return activity * weight * weight * (3 - 2 * weight);
}

inline bool isUnmaskUpperBand(double hz, double root) noexcept {
    return hz > (root > 0 ? root * 1.45 : 130.);
}

inline double unmaskUpperCutDb(double bass, double kick, double fullBass, double fullKick) noexcept {
    const double activity = unmaskActivity(std::min(bass, kick));
    if (activity == 0) return 0;
    const double prominence = std::min(bass / std::max(fullBass, 1e-20), kick / std::max(fullKick, 1e-20));
    if (prominence < .02) return 0;
    const double balance = std::sqrt(bass / kick);
    const double weight = std::clamp((balance - .126) / (.50 - .126), 0., 1.);
    // Reserve the extra cut for a strong partial in both signals that can
    // compete with the kick at this frequency.
    const double severity = std::clamp(std::min({(balance - .63) / .37,
        (prominence - .12) / .18, (std::min(bass, kick) - .0025) / .0135}), 0., 1.);
    return activity * weight * (1.25 + 2.75 * severity);
}

inline double unmaskReferenceInRange(const Settings& settings) noexcept {
    const double hz = settings.kickReferenceHz;
    const auto limits = std::minmax(settings.unmaskFocusMinHz, settings.unmaskFocusMaxHz);
    return hz >= 30 && hz <= 130 && hz >= limits.first && hz <= limits.second ? hz : 0;
}

inline double allPassDegrees(double hz, double corner, double rate) noexcept {
    return -360 / pi * std::atan(std::tan(pi * hz / rate) / std::tan(pi * corner / rate));
}

// Instantaneous small-signal response of the UNMASK filters, before Auto Gain.
struct UnmaskResponse {
    std::array<Biquad, bandCount> distributed{};
    Biquad bell, upperBell;
    double wet = 0, direct = 0, activity = 0, centreHz=0;
    UnmaskStereoMode mode = UnmaskStereoMode::stereo;

    double gainDb(double frequency, double sampleRate) const noexcept {
        if (wet <= 1e-5) return 0;
        const double angle = 2 * pi * frequency / sampleRate;
        const std::complex<double> z{std::cos(angle), -std::sin(angle)};
        std::complex<double> legacy{1}, single{1};
        if (direct < 1) for (const auto& band : distributed) legacy *= band.transfer(z);
        if (direct > 0) single = bell.transfer(z) * upperBell.transfer(z);
        return db(std::abs(1.0 + wet * ((1.0 - direct) * legacy + direct * single - 1.0)));
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

inline double cornerForAllPassDegrees(double degrees, double hz, double rate) noexcept {
    const double angle = std::clamp(-degrees, .001, 179.999) * pi / 360;
    return std::clamp(rate / pi * std::atan(std::tan(pi * hz / rate) / std::tan(angle)), 20., 500.);
}

struct PhaseTap { float before = 0, after = 0; };

// Complementary bands sum to the original signal, including filter history.
struct DuckBands {
    std::array<double,2> body{}, broad{};
    std::array<double,3> split(double input,int channel,double bodyPole,double punchPole) noexcept {
        body[channel]=bodyPole*body[channel]+(1-bodyPole)*input;
        broad[channel]=punchPole*broad[channel]+(1-punchPole)*input;
        return {body[channel],broad[channel]-body[channel],input-broad[channel]};
    }
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
        duckEnvelope = duckReduction = 0;
        duckDetectorRelease = pole(12, sampleRate);
        duckBodyPole=std::exp(-2*pi*110/sampleRate);
        duckPunchPole=std::exp(-2*pi*800/sampleRate);
        duckModeInitialised = false;
        duckBassBands={};duckKickBands={};
        duckShelf = {}; duckShelfHz = 0;
        duckBandEnvelope.fill(0);duckBandReduction.fill(0);
        subMono = {}; subMonoMix.reset(0); subMonoCutoff = 0;
        monitorMix.reset(0);
        dryMonitorGain.reset(1);
        monitorGain.reset(1);
        wetPower = 0;
        glueSignalHoldSamples = static_cast<int>(sampleRate * 0.04);
        glueRestartSamples = static_cast<int>(sampleRate * 0.5);
        resetGlueReference();
        envelope = kickFast = kickSlow = 0;
        thresholdDb = -24; ratio = 2; attackMs = 20; releaseMs = 180;
        glueReduction = 0; onsetClock = lastOnset = 0; kickIntervalMs = 0;
        controlCounter = 0; smoothed = false; meters = {};
        directWeightDenominator = std::max(1e-12, *std::max_element(focusWeights.begin(),focusWeights.end()));
        setUnmaskFundamental(0);
        resetUnmaskUpperCut();
        unmaskKickWeight = 0;
        unmaskKeySpanDb = 30.;
        unmaskAgcSamples = 0;
        globalInitialised=globalPaused=false;globalRamp.reset(1);delayFade.reset(1);polarityRamp.reset(1);
        inputPowerMeter = outputPowerMeter = outputHeld = kickHeld = 0;
        focusHz = 75; trackedFocusHz = 75; focusCandidate=-1;focusCandidateSamples=0;focusAcquisitions=0;focusReferenceSeen=0;
        cachedRotationHz = -1; cachedAllpassA = 0;
        cutDb.fill(0); kickE.fill(0); bassE.fill(0); focusBassE.fill(0);directCutDb=0;directMix=0;directFilters={};
        allpassX.fill(0); allpassY.fill(0);
        angleX={};angleY={};angleMix=0;angleCoefficient=0;angleLag=0;angleTransition.reset(1);
        for (auto& d : delays) d.prepare(sampleRate);
        learner.prepare(sampleRate);tube.prepare(sampleRate);tubeAgc.prepare(sampleRate,5000);
        for (size_t b = 0; b < frequencies.size(); ++b) {
            for (int c = 0; c < 2; ++c) {
                detectorsBass[b][c] = {}; detectorsKick[b][c] = {}; filters[b][c] = {};
                detectorsBass[b][c].bandpass(frequencies[b], 2, sampleRate);
                focusBassFilters[b][c] = {};
                focusBassFilters[b][c].bandpass(frequencies[b], 4, sampleRate);
                detectorsKick[b][c].bandpass(frequencies[b], 2, sampleRate);
            }
        }
        for (size_t i = 0; i < agc.size(); ++i) agc[i].prepare(sampleRate, i == 2 ? 12000 : 5000);
        energyA = pole(300, sampleRate); rmsA = pole(10, sampleRate);
        smoothA = pole(20, sampleRate); bandA = pole(15, sampleRate);
        kickFastA = pole(3, sampleRate); kickSlowA = pole(100, sampleRate);
        parameterA = pole(500, sampleRate / 32); // ratio and automatic timing adapt over 500 ms
        cutAttackA = pole(8, sampleRate); cutReleaseA = pole(120, sampleRate);
        focusA = pole(200, sampleRate / 32); peakA = pole(kickPeakHoldMs, sampleRate);
        filterParameterA = pole(20, sampleRate / 32);
        attackA = pole(attackMs, sampleRate); releaseA = pole(releaseMs, sampleRate);
    }
    void startLearn() noexcept { learner.start(); }
    void cancelLearn() noexcept { learner.cancel(); }
    bool learning() const noexcept { return learner.learning(); }
    double learnProgress() const noexcept { return learner.progress(); }
    LearnResult learnResult() const noexcept { return learner.getResult(); }
    const Meters& getMeters() const noexcept { return meters; }
    bool hasWetOutput() const noexcept {
        return globalRamp.value() == 1 && monitorMix.value() == 0 && deltaMix < 1e-4
            && !tube.isAuditioning();
    }
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
        for (auto* bank : {&detectorsBass, &detectorsKick, &focusBassFilters})
            for (auto& band : *bank) for (auto& f : band) f.z1 = f.z2 = 0;
        filters={};cutDb.fill(0);
        allpassX.fill(0); allpassY.fill(0);
        angleX={};angleY={};angleMix=0;angleCoefficient=0;angleLag=0;angleTransition.reset(1);
        duckEnvelope = duckReduction = 0;
        meters.duckReductionDb = 0;
        duckModeInitialised = false;
        duckBassBands={};duckKickBands={};
        duckShelf = {}; duckShelfHz = 0;
        duckBandEnvelope.fill(0);duckBandReduction.fill(0);
        subMono.clear();
        bassE.fill(0); kickE.fill(0); focusBassE.fill(0);
        setUnmaskFundamental(0);
        resetUnmaskUpperCut();
        unmaskKickWeight = 0;
        unmaskKeySpanDb = 30.;
        unmaskAgcSamples = 0;
        focusCandidate=-1;focusCandidateSamples=0;
        resetGlueReference();
        envelope = glueReduction = kickFast = kickSlow = 0;
        inputPowerMeter = outputPowerMeter = outputHeld = kickHeld = 0;
        if(!preserveLearn)learner.cancel();
        tube.clear();directCutDb=0;directFilters={};
        for (auto& filter : kickEqFilters) filter.z1 = filter.z2 = 0;
        for (auto& filter : nextKickEqFilters) filter.z1 = filter.z2 = 0;
        wetPower = 0;
    }

    void process(float* const* bass, int channels, const float* const* kick, int kickChannels,
                 int samples, const Settings& s, double bpm = 0, PhaseTap* phaseTap = nullptr) noexcept {
        if (channels < 1 || channels > 2) return;
        tube.configure(s.tube);
        const int rampSamples=std::max(1,static_cast<int>(sampleRate*.02));
        if(!globalInitialised){globalRamp.reset(s.active?1:0);globalInitialised=true;}
        globalRamp.set(s.active?1:0,rampSamples);
        const auto passDry = [&](int first) {
            if(!globalPaused){clearHistory();globalPaused=true;}
            // Leave every input bit untouched, including IEEE signed zeros/subnormals.
            meters.matchGainDb=meters.kickPeak=0;
            meters.glueReductionDb=meters.unmaskReductionDb=meters.tubeAgcDb=0;meters.agcDb.fill(0);meters.cutsDb.fill(0);
            meters.tubeAddedDb=-120;
            meters.bassDb.fill(-120);meters.kickDb.fill(-120);
            for(int n=first;n<samples;++n){
                std::array<double,2> dry{};
                for(int c=0;c<channels;++c)dry[c]=std::isfinite(bass[c][n])?bass[c][n]:0;
                inputPowerMeter=follow(inputPowerMeter,power(dry,channels),energyA);
                outputHeld*=peakA;for(int c=0;c<channels;++c)outputHeld=std::max(outputHeld,std::abs(dry[c]));
            }
            meters.peak=outputHeld;meters.inputRmsDb=meters.outputRmsDb=db(std::sqrt(inputPowerMeter));
        };
        if (!s.active && globalRamp.finished()) {
            passDry(0);
            return;
        }
        globalPaused=false;
        const double midTarget = s.unmaskStereoMode == UnmaskStereoMode::side ? 0 : 1;
        const double sideTarget = s.unmaskStereoMode == UnmaskStereoMode::mid ? 0 : 1;
        const double focusLow = std::clamp(std::min(s.unmaskFocusMinHz, s.unmaskFocusMaxHz), 30.0, 250.0);
        const double focusHigh = std::clamp(std::max(s.unmaskFocusMinHz, s.unmaskFocusMaxHz), 30.0, 250.0);
        const double learnedRoot = s.unmaskDirect && s.unmaskTrack ? unmaskReferenceInRange(s) : 0;
        if (learnedRoot != unmaskRootHz) setUnmaskFundamental(learnedRoot);
        const double width = std::clamp(s.unmaskWidth, 0.0, 1.0);
        const double filterQTarget = 3.5 - 2.6 * width;
        const double directQTarget = s.unmaskTrack ? 8 - 2.5 * width : filterQTarget;
        if (!smoothed) {
            midMix.reset(midTarget);
            sideMix.reset(sideTarget);
            glueMix = s.glue ? 1 : 0; phaseMix = s.phase ? 1 : 0;
            unmaskMix = s.unmask ? 1 : 0; delaySamples = s.delayMs * sampleRate / 1000;
            polarity = s.invert ? -1 : 1; rotationMix = s.rotate ? 1 : 0;
            rotationHz = s.rotationHz; outputGain = gain(s.outputDb); balanceRamp.reset(gain(std::clamp(s.balanceTrimDb,-12.,12.))); smoothed = true;
            previousDelaySamples=delaySamples;delayFade.reset(1);polarityRamp.reset(polarity);
            deltaMix = s.delta && s.unmask && !(s.tube.enabled&&s.tube.audition) ? 1 : 0;
            directMix=s.unmaskDirect?1:0;
            smoothedFilterQ = filterQTarget;
            smoothedDirectQ = directQTarget;
            if (learnedRoot > 0 && !s.unmaskHold) focusHz = trackedFocusHz = learnedRoot;
        }
        meters.unmaskAttackMs=std::clamp(s.unmaskAttackMs,0.0,50.0);
        meters.unmaskReleaseMs=s.unmaskSync?noteMilliseconds(s.unmaskReleaseNote,bpm):std::clamp(s.unmaskReleaseMs,30.0,500.0);
        cutAttackA = pole(meters.unmaskAttackMs, sampleRate);
        cutReleaseA = pole(meters.unmaskReleaseMs, sampleRate);
        const double trimTarget = gain(std::clamp(s.outputDb, -24.0, 12.0));
        const double delayTarget = std::clamp(s.delayMs, 0.0, 5.0)*sampleRate/1000;
        const double rotationTarget = std::clamp(s.rotationHz, 20.0, 500.0);
        const double degrees=std::clamp(s.phaseAngle,-180.,180.);
        const bool useAngle=degrees!=0;
        const double requestedLag = degrees>0 ? 360-degrees : -degrees;
        const int angleFadeSamples = std::max(1,static_cast<int>(sampleRate * .006));
        // Two first-order sections give a full circle of phase at the
        // reference while retaining unity magnitude and no lookahead.
        double angleTarget=angleCoefficient;
        if (useAngle) {
            const double lag=requestedLag*pi/720;
            const double t=std::tan(pi*std::clamp(s.phaseReference,30.,130.)/sampleRate)/
                std::tan(lag);
            angleTarget=std::clamp((t-1)/(t+1),-.999999,.999999);
            if (angleMix==0) {
                angleCoefficient=angleTarget;
                angleLag=requestedLag;
                angleX={};angleY={};angleTransition.reset(1);
            }
        }
        const double glueAmount = std::clamp(s.glueAmount, 0.0, 1.0);
        const double unmaskDepth = std::clamp(s.unmaskMaxCutDb, 3.0, 12.0)*std::clamp(s.unmaskAmount, 0.0, 1.0);
        const double directDepth = std::clamp(s.unmaskAttenuationDb, 0.0, 24.0);
        const double widthOctaves = 0.25 + 1.25 * width;
        const bool unmaskAgcAvailable = s.agc[2].enabled && s.unmask && kick && kickChannels > 0
            && (s.unmaskDirect ? directDepth : unmaskDepth) > 0;
        const int unmaskAgcHold = static_cast<int>(sampleRate * .001 *
            std::clamp(noteMilliseconds(4, bpm) * 1.25, 500.0, 1250.0));
        if (!unmaskAgcAvailable) unmaskAgcSamples = 0;
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
        balanceRamp.set(gain(std::clamp(s.balanceTrimDb,-12.,12.)),rampSamples);
        const bool normalMonitoring = !s.delta && !(s.tube.enabled && s.tube.audition);
        monitorMix.set(s.auditionDry && normalMonitoring ? 1 : 0, rampSamples);
        const bool match = s.matchEnabled && normalMonitoring;
        if (!match) {
            monitorGain.set(gain(s.auditionWetDb), rampSamples);
            wetPower = 0;
        }
        dryMonitorGain.set(gain(s.auditionDryDb), rampSamples);
        subMonoMix.set(channels == 2 && s.subMono ? 1 : 0, rampSamples);
        const bool runSubMono = channels == 2 && (s.subMono || !subMonoMix.finished());
        const double subMonoTarget = std::clamp(s.subMonoHz, 40., 250.);
        if (!runSubMono) { subMono.clear(); subMonoCutoff = 0; }
        else if (subMonoCutoff == 0) subMonoCutoff = subMonoTarget;
        const bool monitor = match || !monitorMix.finished() || monitorMix.value() != 0 || !monitorGain.finished() || monitorGain.value() != 1 || !dryMonitorGain.finished();
        const bool duckEnabled = s.duck && s.active && !(s.tube.enabled && s.tube.audition) && kick && kickChannels > 0;
        constexpr double maxDuckDepth = 18.;
        const double minimumDuckGain = gain(-maxDuckDepth);
        const double duckDepth = duckEnabled ? std::clamp(s.duckAmountDb,0.,maxDuckDepth) : 0.;
        const double duckBandTarget = s.duckMultiband ? 1 : 0;
        if (!duckModeInitialised) {
            duckBandMix.reset(duckBandTarget);
            duckModeInitialised = true;
        }
        duckBandMix.set(duckBandTarget,rampSamples);
        const bool lowBand = s.duckLowHz > 0;
        const bool runDuck = duckDepth > 0 || duckReduction > 0
            || std::any_of(duckBandReduction.begin(),duckBandReduction.end(),[](double v){return v>0;});
        const bool runDuckShelf = runDuck && lowBand && (s.duckMultiband || duckBandMix.value() > 0);
        const double duckShelfTarget = std::clamp(s.duckLowHz, 60., 250.);
        if (runDuckShelf && duckShelfHz == 0) {
            duckShelfHz = duckShelfTarget;
            for (auto& filter : duckShelf) filter.lowShelf(duckShelfHz, minimumDuckGain, sampleRate);
        } else if (!runDuckShelf && duckShelfHz != 0) {
            duckShelf = {}; duckShelfHz = 0;
        }
        const double duckAttack = runDuck ? pole(std::clamp(s.duckAttackMs,0.,20.),sampleRate) : 0.;
        const double duckReleaseMs = s.duckSync ? noteMilliseconds(s.duckReleaseNote,bpm)
                                            : std::clamp(s.duckReleaseMs,20.,350.);
        const double duckRelease = runDuck ? pole(duckDepth > 0 ? duckReleaseMs : 5.,sampleRate) : 0.;
        if (duckDepth == 0) {
            duckEnvelope=0;
            duckBandEnvelope.fill(0);
        }
        if (!runDuck) {
            duckBassBands={};duckKickBands={};
            duckBandMix.reset(duckBandTarget);
        }
        double displayedDuckReduction=0;
        double glueGain = 1;
        for (int n = 0; n < samples; ++n) {
            const float dryLeft=bass[0][n],dryRight=channels==2?bass[1][n]:0;
            const double activeMix=globalRamp.next();
            if (activeMix==0) {
                // Finish the bypass at its exact sample, even inside a big
                // host block. Resuming must not reuse pre-bypass history.
                passDry(n);
                return;
            }
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
            // Keep the reference through short gaps and reacquire it on the next phrase.
            if (inputPower > 1e-8) { // -80 dBFS
                glueQuietSamples = 0;
            } else if (glueQuietSamples < glueRestartSamples && ++glueQuietSamples == glueRestartSamples) {
                resetGlueReference();
            }
            const bool updateGlueReference = glueQuietSamples < glueSignalHoldSamples;
            if (updateGlueReference) {
                bassEnergy = follow(bassEnergy, inputPower, energyA);
                glueReferenceWeight = follow(glueReferenceWeight, 1, energyA);
            }
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
                smoothedDirectQ = follow(smoothedDirectQ, directQTarget, filterParameterA);
                if (runDuckShelf && duckShelfHz != duckShelfTarget) {
                    duckShelfHz = follow(duckShelfHz, duckShelfTarget, filterParameterA);
                    if (std::abs(duckShelfHz - duckShelfTarget) < .01) duckShelfHz = duckShelfTarget;
                    for (auto& filter : duckShelf) filter.lowShelf(duckShelfHz, minimumDuckGain, sampleRate);
                }
                unmaskKeySpanDb = s.unmaskDirect ? unmaskKeySpan(kickHeld) : 30.;
                if (!s.glueLocked && updateGlueReference) {
                    // Normalise the RMS over the samples observed so far. A second
                    // average in dB would keep the threshold low during a fade-in.
                    const double referencePower = bassEnergy / glueReferenceWeight;
                    thresholdDb = db(std::sqrt(referencePower)) - 3;
                    glueReferenceReady = true;
                    const double crest = std::clamp(db(std::sqrt(std::max(envelope, 1e-20) / referencePower)), 0.0, 12.0);
                    ratio = follow(ratio, 1.5 + 2 * glueAmount, parameterA);
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
                if (s.unmaskTrack && !s.unmaskHold) {
                    updateAutoFocus(s.kickReferenceHz, focusLow, focusHigh);
                } else {
                    focusCandidate = -1;
                    focusCandidateSamples = 0;
                }
                double targetFocus = s.unmaskTrack ? std::clamp(trackedFocusHz, focusLow, focusHigh) : std::clamp(s.unmaskFrequency, 30.0, 250.0);
                // A nearby detector band represents the same conflict as
                // the learned frequency, even when its overlap is light.
                if (learnedRoot > 0 && std::abs(targetFocus - learnedRoot) <= learnedRoot * .15)
                    targetFocus = learnedRoot;
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
                directWeightDenominator = std::max(1e-12, *std::max_element(focusWeights.begin(),focusWeights.end()));
            }
            glueMix = follow(glueMix, s.glue ? 1 : 0, smoothA);
            phaseMix = follow(phaseMix, s.phase ? 1 : 0, smoothA);
            unmaskMix = follow(unmaskMix, s.unmask ? 1 : 0, smoothA);
            const double knee = 6, slope = 1 - 1 / ratio;
            double targetReduction = 0;
            if (s.glueLocked || glueReferenceReady) {
                const double over = db(std::sqrt(envelope)) - thresholdDb;
                const double desired = over <= -knee/2 ? 0 : (over >= knee/2 ? slope * over
                                        : slope * (over+knee/2)*(over+knee/2)/(2*knee));
                targetReduction = desired * glueAmount;
            }
            glueReduction = follow(glueReduction, targetReduction, targetReduction > glueReduction ? attackA : releaseA);
            double budget = 6;
            const auto compensate = [&](int module, double in, double out, double mix, bool available = true) {
                auto cfg = s.agc[static_cast<size_t>(module)];
                if (mix < 1e-6 || !available) cfg.enabled = false;
                const double g = agc[static_cast<size_t>(module)].tick(in, out, cfg, budget);
                const double applied = 1 + mix * (g - 1);
                budget -= std::max(0.0, db(applied));
                meters.agcDb[static_cast<size_t>(module)] = db(applied);
                return applied;
            };
            glueGain = 1 + glueMix * (gain(-glueReduction) - 1);
            for (auto& v : x) v *= glueGain;
            const double glueAgc = compensate(0, inputPower, power(x, channels), glueMix);
            for (auto& v : x) v *= glueAgc;

            if (learner.learning() && !s.pausePhaseLearn) {
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
            if (useAngle) {
                const bool crossesZero=std::abs(requestedLag-angleLag)>180.;
                angleTransition.set(crossesZero?0:1,angleFadeSamples);
                if (crossesZero && angleTransition.value()==0) {
                    // At zero the all-pass changes branches. Pass through the
                    // unrotated signal before changing its coefficient.
                    angleX={};angleY={};
                    angleCoefficient=angleTarget;
                    angleLag=requestedLag;
                } else if (!crossesZero) {
                    angleCoefficient=follow(angleCoefficient,angleTarget,smoothA);
                    angleLag=requestedLag;
                }
            }
            const double angleBlend=angleTransition.next();
            if (useAngle || angleMix>1e-9)
                angleMix=follow(angleMix,useAngle?1:0,smoothA);
            else angleMix=0;
            if (phaseTap) phaseTap[n].before = float(channels == 2 ? (x[0]+x[1])*.5 : x[0]);
            double phaseReference = 0;
            for (int c = 0; c < channels; ++c) {
                delays[c].push(x[c]);
                double d = delays[c].read(std::clamp(delaySamples, 0.0, sampleRate * 0.005));
                if(delayBlend<1){const double old=delays[c].read(previousDelaySamples);d=old+delayBlend*(d-old);}
                phaseReference += ((1-phaseMix)*x[c]*x[c] + phaseMix*d*d) / channels;
                const double rotated = a*d + allpassX[c] - a*allpassY[c];
                allpassX[c] = d; allpassY[c] = std::abs(rotated) < 1e-25 ? 0 : rotated;
                double shifted=d+rotationMix*(rotated-d);
                if (angleMix>0) {
                    double angleSample=d;
                    for (size_t stage=0;stage<angleX.size();++stage) {
                        const double result=angleCoefficient*angleSample+angleX[stage][c]-angleCoefficient*angleY[stage][c];
                        angleX[stage][c]=angleSample;
                        angleY[stage][c]=std::abs(result)<1e-25?0:result;
                        angleSample=result;
                    }
                    const double angleOutput=angleBlend==1 ? angleSample : d+angleBlend*(angleSample-d);
                    shifted+=angleMix*(angleOutput-shifted);
                }
                x[c] += phaseMix * (shifted*polarity - x[c]);
            }
            const double phaseAgc = compensate(1, phaseReference, power(x, channels), phaseMix);
            for (auto& v : x) v *= phaseAgc;
            if (phaseTap) phaseTap[n].after = float(channels == 2 ? (x[0]+x[1])*.5 : x[0]);

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
            if (runDuck) {
                if (duckDepth > 0)
                    duckEnvelope = std::max(std::max(std::abs(k[0]),std::abs(k[1])),
                                            duckEnvelope*duckDetectorRelease);
                // Follow the sidechain level so a quiet kick can reach the
                // requested depth. Keep an absolute gate for very low signals.
                constexpr double duckGate = .004;
                const double fullDepthLevel = std::clamp(kickHeld * .2, .0063, .063);
                const double weight = std::clamp((duckEnvelope-duckGate)/(fullDepthLevel-duckGate),0.,1.);
                const double target = duckDepth * weight * weight * (3 - 2 * weight);
                duckReduction = follow(duckReduction,target,target>duckReduction?duckAttack:duckRelease);
                if (duckReduction < 1e-7) duckReduction = 0;
                const double bandMix=duckBandMix.next();
                const double duckGain=gain(-duckReduction);
                displayedDuckReduction=(lowBand?1:1-bandMix)*duckReduction;
                if (bandMix>0 && lowBand) {
                    // Interpolate the shelf in linear gain so the low end
                    // follows the reduction and the dry path is exact at 0 dB.
                    const double shelfMix = bandMix * (1 - duckGain) / (1 - minimumDuckGain);
                    for (int c=0;c<channels;++c) {
                        const double filtered = duckShelf[c].tick(x[c]);
                        x[c] += (1 - bandMix) * x[c] * (duckGain - 1) + shelfMix * (filtered - x[c]);
                    }
                } else if (bandMix>0) {
                    const auto left=duckKickBands.split(k[0],0,duckBodyPole,duckPunchPole);
                    const auto right=duckKickBands.split(k[1],1,duckBodyPole,duckPunchPole);
                    std::array<double,3> bandGain{};
                    const double legacyDepthLevel = std::min(fullDepthLevel, .048);
                    for (size_t band=0;band<bandGain.size();++band) {
                        const double peak=std::max(std::abs(left[band]),std::abs(right[band]));
                        duckBandEnvelope[band]=duckDepth>0?std::max(peak,duckBandEnvelope[band]*duckDetectorRelease):0;
                        const double level=std::clamp((duckBandEnvelope[band]-.003)/(legacyDepthLevel-.003),0.,1.);
                        const double bandTarget=duckDepth*std::clamp(s.duckBands[band],0.,1.)*level*level*(3-2*level);
                        auto& reduction=duckBandReduction[band];
                        reduction=follow(reduction,bandTarget,bandTarget>reduction?duckAttack:duckRelease);
                        if (reduction<1e-7) reduction=0;
                        bandGain[band]=gain(-reduction);
                        displayedDuckReduction=std::max(displayedDuckReduction,bandMix*reduction);
                    }
                    for (int c=0;c<channels;++c) {
                        const auto bands=duckBassBands.split(x[c],c,duckBodyPole,duckPunchPole);
                        const double filtered=x[c]+bands[0]*(bandGain[0]-1)+bands[1]*(bandGain[1]-1)+bands[2]*(bandGain[2]-1);
                        x[c]+=(1-bandMix)*x[c]*(duckGain-1)+bandMix*(filtered-x[c]);
                    }
                } else {
                    duckBandReduction.fill(0);
                    for (auto& channel : x) channel*=duckGain;
                }
            }
            const auto beforeUnmask = x;
            const auto selectedPower = [&](const std::array<double, 2>& signal) {
                if (linked) return power(signal, channels);
                if (channels == 1) return mid * signal[0] * signal[0];
                const double centre = (signal[0] + signal[1]) * .5;
                const double difference = (signal[0] - signal[1]) * .5;
                return mid * centre * centre + side * difference * difference;
            };
            const double unmaskInputPower = selectedPower(beforeUnmask);
            const double keySpan = s.unmaskDirect ? unmaskKeySpanDb : 30.;
            if (s.unmaskDirect && s.unmaskTrack) {
                unmaskBassReference = follow(unmaskBassReference, unmaskInputPower, bandA);
                unmaskKickReference = follow(unmaskKickReference, kp, bandA);
            }
            if (learnedRoot > 0) {
                std::array<double, 2> lowBass{}, lowKick{}, earlyKick{};
                for (int c = 0; c < channels; ++c) lowBass[c] = fundamentalBass[c].tick(beforeUnmask[c]);
                for (int c = 0; c < 2; ++c) lowKick[c] = fundamentalKick[c].tick(k[c], &earlyKick[c]);
                rootBassPower = follow(rootBassPower, selectedPower(lowBass), bandA);
                rootKickPower = follow(rootKickPower, power(lowKick, 2), bandA);
                earlyRootKickPower = follow(earlyRootKickPower, power(earlyKick, 2), kickFastA);
                // The first section responds sooner at the start of a hit.
                // Use it for balance while the narrower detector confirms overlap.
                if (controlCounter == 0)
                    rootActivity = unmaskFundamentalActivity(rootBassPower, rootKickPower, unmaskBassReference,
                                                              unmaskKickReference, earlyRootKickPower * .5, keySpan);
                for (auto& harmonic : harmonicDetectors) {
                    if (harmonic.frequency == 0) break;
                    std::array<double, 2> highBass{}, highKick{};
                    for (int c = 0; c < channels; ++c) highBass[c] = harmonic.bass[c].tick(beforeUnmask[c]);
                    for (int c = 0; c < 2; ++c) highKick[c] = harmonic.kick[c].tick(k[c]);
                    harmonic.bassPower = follow(harmonic.bassPower, selectedPower(highBass), bandA);
                    harmonic.kickPower = follow(harmonic.kickPower, power(highKick, 2), bandA);
                }
            }
            // Auto Focus and gain detection share the input of Unmask. The
            // tracker cannot chase bass energy that Duck has already removed.
            if (s.unmaskTrack) {
                for (size_t band = 0; band < frequencies.size(); ++band) {
                    std::array<double, 2> filtered{};
                    for (int c = 0; c < channels; ++c)
                        filtered[c] = focusBassFilters[band][c].tick(beforeUnmask[c]);
                    focusBassE[band] = follow(focusBassE[band], selectedPower(filtered), bandA);
                }
            }
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
            double maxCut = 0, directActivity = 0, legacyTarget = 0;
            size_t directBand = 0;
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
                const double activity = unmaskActivity(overlap);
                const double target = -unmaskDepth * activity * focusWeights[b];
                legacyTarget = std::max(legacyTarget, -target);
                cutDb[b] = follow(cutDb[b], target, target < cutDb[b] ? cutAttackA : cutReleaseA);
                maxCut = std::max(maxCut, -cutDb[b]);
                const double directEvidence = keySpan < 30 && kickE[b] < bassE[b]
                    ? unmaskOverlapActivity(bassE[b], kickE[b], keySpan) : activity;
                const double weightedActivity = directEvidence * focusWeights[b];
                if (weightedActivity > directActivity) {
                    directActivity = weightedActivity;
                    directBand = b;
                }
                for (int c = 0; c < channels; ++c) legacy[c] = filters[b][c].tick(legacy[c]);
            }
            const bool nearRoot = learnedRoot > 0 && std::abs(focusHz - learnedRoot) <= learnedRoot * .10;
            const bool upperFocus = s.unmaskTrack && !nearRoot && isUnmaskUpperBand(focusHz, learnedRoot);
            if (controlCounter == 0 && s.unmaskTrack) {
                // As the kick fades, its weak tail must not drive a stronger
                // cut just because the bass is now louder by comparison.
                const double level = std::sqrt(kickFast) / std::max(kickHeld, 1e-12);
                const double body = std::clamp((level - .0631) / (.32 - .0631), 0., 1.);
                unmaskKickWeight = body * body * (3 - 2 * body);
            }
            const double kickWeight = s.unmaskTrack ? unmaskKickWeight : 1;
            double targetDepth = directDepth;
            if (s.unmaskTrack && !nearRoot) {
                if (controlCounter == 0 && upperFocus)
                    upperFocusLimitDb = kickWeight > 0 ? unmaskFocusedUpperCut(directBand, focusLow, focusHigh) : 0;
                targetDepth = std::min(targetDepth, upperFocus ? upperFocusLimitDb : 6.);
            }
            const double directTarget = -targetDepth * kickWeight * (nearRoot ? rootActivity : directActivity / directWeightDenominator);
            directCutDb=follow(directCutDb,directTarget,directTarget<directCutDb?cutAttackA:cutReleaseA);
            if (controlCounter == 0) {
                setUnmaskBell(directFilters, focusHz, smoothedDirectQ, directCutDb);
                updateUnmaskUpperCut(nearRoot && s.unmask ? kickWeight : 0., s.unmaskHold,
                                      focusLow, focusHigh, directDepth);
            }
            const double effectiveUpperTarget = nearRoot ? upperTargetDb : 0;
            if (upperCutDb > 0 || effectiveUpperTarget > 0) {
                const double release = !nearRoot || upperRetuning ? smoothA : cutReleaseA;
                upperCutDb = follow(upperCutDb, effectiveUpperTarget, effectiveUpperTarget > upperCutDb ? cutAttackA : release);
                if (upperCutDb < 1e-9) upperCutDb = 0;
            }
            if (controlCounter == 0 && (upperCutDb > 0 || upperFilters[0].b0 != 1))
                setUnmaskBell(upperFilters, upperFrequency, 6., -upperCutDb);
            const bool upperBellActive = upperFilters[0].b0 != 1;
            directMix=follow(directMix,s.unmaskDirect?1:0,smoothA);
            for (int c = 0; c < channels; ++c){
                const double focused = directFilters[c].tick(beforeUnmask[c]);
                const double direct = upperBellActive ? upperFilters[c].tick(focused) : focused;
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
            if (unmaskAgcAvailable) {
                if ((s.unmaskDirect ? std::max(-directTarget, effectiveUpperTarget) : legacyTarget) > .10) unmaskAgcSamples = unmaskAgcHold;
                else if (unmaskAgcSamples > 0) --unmaskAgcSamples;
            }
            const double unmaskAgc = compensate(2, unmaskInputPower, selectedPower(x), unmaskMix, unmaskAgcSamples > 0);
            if (linked) {
                for (int c=0;c<channels;++c) x[c] *= unmaskAgc;
            } else {
                applySelectedChange(x, x[0] * (unmaskAgc - 1), x[1] * (unmaskAgc - 1));
            }
            if (runSubMono) {
                if (controlCounter % 32 == 0 || (n == 0 && subMonoCutoff == subMonoTarget)) {
                    subMonoCutoff = follow(subMonoCutoff, subMonoTarget, filterParameterA);
                    subMono.setCutoff(subMonoCutoff, sampleRate);
                }
                const double lowSide = subMonoMix.next() * subMono.lowSide((x[0] - x[1]) * .5);
                x[0] -= lowSide;
                x[1] += lowSide;
            }
            outputGain = follow(outputGain, trimTarget, smoothA);
            deltaMix = follow(deltaMix, s.delta && s.unmask && !(s.tube.enabled&&s.tube.audition) ? 1 : 0, smoothA);
            const double listenDry = monitor ? monitorMix.next() : 0;
            const double listenGain = monitor ? monitorGain.next() : 1;
            const double dryGain = monitor ? dryMonitorGain.next() : 1;
            const double finalBalance=balanceRamp.next();
            double wet = 0;
            outputHeld *= peakA;
            for (int c = 0; c < channels; ++c) {
                // Delta is removed bass BEFORE AGC/output trim; never includes kick.
                const double normal = x[c]*outputGain;
                double out = normal + deltaMix*(beforeUnmask[c]-afterUnmask[c]-normal);
                const float dry=c==0?dryLeft:dryRight;
                if (match) wet += out * out;
                if (normalMonitoring) out *= finalBalance;
                if (monitor) {
                    const double reference = std::isfinite(dry) ? dry : 0;
                    const double matched = out * listenGain;
                    const double drySignal = reference * dryGain;
                    out = listenDry == 1 ? drySignal : matched + listenDry * (drySignal - matched);
                }
                bass[c][n] = static_cast<float>(activeMix==1 ? out : static_cast<double>(dry)+activeMix*(out-dry));
                outputHeld = std::max(std::abs(static_cast<double>(bass[c][n])), outputHeld);
            }
            if (match) {
                wetPower = follow(wetPower, wet / channels, energyA);
                // Use the same sample clock in live, offline and GUI renders.
                if (controlCounter==0) {
                    const double matchDb = inputPowerMeter > 1e-9 && wetPower > 1e-9
                        ? std::clamp(10 * std::log10(inputPowerMeter / wetPower), -12.0, 12.0) : 0;
                    monitorGain.set(gain(matchDb + s.auditionWetDb), rampSamples);
                }
            }
            outputPowerMeter = follow(outputPowerMeter,
                static_cast<double>(bass[0][n])*bass[0][n]/channels
                + (channels == 2 ? static_cast<double>(bass[1][n])*bass[1][n]/channels : 0), energyA);
            meters.unmaskReductionDb = (s.unmaskDirect ? std::max(-directCutDb, upperCutDb) : maxCut) * unmaskMix;
        }
        meters.duckReductionDb = displayedDuckReduction;
        if (samples > 0) meters.glueReductionDb = -db(glueGain);
        meters.matchGainDb = match ? db(monitorGain.value()) : 0;
        meters.tubeAddedDb=db(std::sqrt(tube.addedPower()));
        meters.peak = outputHeld; meters.kickPeak = kickHeld;
        meters.inputRmsDb = db(std::sqrt(inputPowerMeter)); meters.outputRmsDb = db(std::sqrt(outputPowerMeter));
        meters.focusHz = focusHz; meters.filterQ = s.unmaskDirect ? smoothedDirectQ : smoothedFilterQ;
        meters.attackMs = attackMs; meters.releaseMs = releaseMs;
        for (size_t b = 0; b < frequencies.size(); ++b) {
            meters.bassDb[b] = db(std::sqrt(bassE[b])); meters.kickDb[b] = db(std::sqrt(kickE[b]));
            meters.cutsDb[b] = cutDb[b] * unmaskMix;
        }
    }
    UnmaskResponse unmaskResponse(const Settings& settings, int channels) const noexcept {
        UnmaskResponse response;
        if (settings.auditionDry || settings.delta || (settings.tube.enabled && settings.tube.audition))
            return response;
        response.centreHz=focusHz;
        response.mode = settings.unmaskStereoMode;
        const double projection = response.mode == UnmaskStereoMode::side
            ? (channels == 2 ? sideMix.value() : 0.) : midMix.value();
        response.wet = unmaskMix * globalRamp.value() * projection;
        response.direct = directMix < 1e-6 ? 0 : directMix > 1 - 1e-6 ? 1 : directMix;
        if (response.wet > 1e-5) {
            response.activity = meters.unmaskReductionDb * globalRamp.value() * projection;
            if (response.direct < 1)
                for (size_t band = 0; band < bandCount; ++band) response.distributed[band] = filters[band][0];
            if (response.direct > 0) {
                response.bell = directFilters[0];
                response.upperBell = upperFilters[0];
            }
        }
        return response;
    }
private:
    void setUnmaskFundamental(double hz) noexcept {
        unmaskRootHz = hz;
        rootBassPower = rootKickPower = earlyRootKickPower = unmaskBassReference = unmaskKickReference = rootActivity = 0;
        harmonicDetectors = {};
        if (hz > 0) {
            for (auto* bank : {&fundamentalBass, &fundamentalKick})
                for (auto& filter : *bank) filter.tune(hz, sampleRate);
            for (size_t i = 0; i < harmonicDetectors.size() && (i + 2) * hz <= 250.; ++i) {
                auto& harmonic = harmonicDetectors[i];
                harmonic.frequency = (i + 2) * hz;
                for (auto* bank : {&harmonic.bass, &harmonic.kick})
                    for (auto& filter : *bank) filter.bandpass(harmonic.frequency, 7., sampleRate);
            }
        }
    }

    void setUnmaskBell(std::array<Biquad, 2>& pair, double frequency, double q, double cut) noexcept {
        auto& left = pair[0];
        auto& right = pair[1];
        left.peak(frequency, q, cut, sampleRate);
        right.b0 = left.b0; right.b1 = left.b1; right.b2 = left.b2;
        right.a1 = left.a1; right.a2 = left.a2;
        if (std::abs(cut) < 1e-12) right.z1 = right.z2 = 0;
    }

    void resetUnmaskUpperCut() noexcept {
        upperFilters = {};
        upperFrequency = upperTargetDb = upperCutDb = upperFocusLimitDb = 0;
        upperRetuning = false;
    }

    double unmaskFocusedUpperCut(size_t band, double low, double high) const noexcept {
        double bass = focusBassE[band], kick = kickE[band], closest = .15;
        for (const auto& harmonic : harmonicDetectors) {
            if (harmonic.frequency == 0) break;
            if (harmonic.frequency < low || harmonic.frequency > high) continue;
            const double distance = std::abs(frequencies[band] / harmonic.frequency - 1);
            if (distance < closest) {
                closest = distance;
                bass = harmonic.bassPower;
                kick = harmonic.kickPower;
            }
        }
        return unmaskUpperCutDb(bass, kick, unmaskBassReference, unmaskKickReference);
    }

    void updateUnmaskUpperCut(double kickWeight, bool hold, double low, double high, double depth) noexcept {
        double strongest = 0, current = 0, frequency = 0;
        if (kickWeight > 0 && depth > 0) {
            for (const auto& harmonic : harmonicDetectors) {
                const double hz = harmonic.frequency;
                if (hz < low || hz > high || (hold && hz != upperFrequency)) continue;
                const double cut = unmaskUpperCutDb(harmonic.bassPower, harmonic.kickPower,
                                                    unmaskBassReference, unmaskKickReference);
                if (hz == upperFrequency) current = cut;
                if (cut > strongest) { strongest = cut; frequency = hz; }
            }
        }
        if (current > 0 && strongest < current + .20) {
            strongest = current;
            frequency = upperFrequency;
        }
        upperTargetDb = 0;
        upperRetuning = frequency > 0 && frequency != upperFrequency && upperCutDb > .02;
        if (frequency == 0 || upperRetuning) return;
        if (frequency != upperFrequency) {
            upperFrequency = frequency;
            upperFilters = {};
        }
        // The upper-band ceiling includes the skirt of the main bell.
        const double angle = 2 * pi * frequency / sampleRate;
        const double mainCut = -db(std::abs(directFilters[0].transfer({std::cos(angle), -std::sin(angle)})));
        upperTargetDb = std::max(0., std::min(strongest, depth * .5) * kickWeight - mainCut);
    }

    void resetGlueReference() noexcept {
        bassEnergy = glueReferenceWeight = 0;
        glueQuietSamples = glueRestartSamples;
        glueReferenceReady = false;
    }

    void updateAutoFocus(double root, double low, double high) noexcept {
        if (std::abs(root - focusReferenceSeen) > .5) {
            focusReferenceSeen = root;
            focusAcquisitions = 0;
            focusCandidate = -1;
            focusCandidateSamples = 0;
        }
        size_t best = 0, current = 0;
        double score = 0, distanceToCurrent = 1e9;
        std::array<double, bandCount> scores{};
        // At around 24 dB below the recent kick peak, its tail must not
        // retune the filter as Duck releases and the bass gets louder again.
        const bool kickBody = kickFast > .004 * kickHeld * kickHeld;
        if (kickBody && unmaskRootHz > 0 && rootActivity > .025) {
            if (trackedFocusHz != unmaskRootHz) ++focusAcquisitions;
            trackedFocusHz = unmaskRootHz;
            focusCandidate = -1;
            focusCandidateSamples = 0;
            return;
        }
        for (size_t band = 0; band < frequencies.size(); ++band) {
            const double hz = frequencies[band];
            if (!kickBody || hz < low || hz > high) continue;
            // The bass peak must also be present in the kick. A learned root
            // favours its neighbourhood but cannot veto a stronger overlap.
            const double bass = focusBassE[band];
            const double kick = kickE[band];
            double value = std::sqrt(bass * std::min(bass, kick));
            if (root >= 30 && root <= 130) {
                const double distance = std::log2(hz / root) / .48;
                value *= .16 + .84 * std::exp(-.5 * distance * distance);
            }
            scores[band] = value;
            if (value > score) { score = value; best = band; }
            const double distance = std::abs(std::log(hz / trackedFocusHz));
            if (distance < distanceToCurrent) { distanceToCurrent = distance; current = band; }
        }
        if (score < 2e-7) {
            // Short kicks can confirm a change over several hits. Between
            // bodies, let pending evidence decay without counting the tail.
            focusCandidateSamples = std::max(0., focusCandidateSamples - 1.);
            if (focusCandidateSamples == 0) focusCandidate = -1;
            return;
        }
        const bool first = focusAcquisitions == 0;
        const double margin = scores[current] * (first ? 1.08 : 1.30);
        if (best == current || score <= margin) {
            focusCandidate = -1;
            focusCandidateSamples = 0;
            return;
        }
        if (focusCandidate < 0 || scores[size_t(focusCandidate)] < std::max(margin, score * .82)) {
            focusCandidate = int(best);
            focusCandidateSamples = 0;
        }
        focusCandidateSamples += 32;
        if (focusCandidateSamples >= sampleRate * (first ? .045 : .065)) {
            trackedFocusHz = frequencies[size_t(focusCandidate)];
            ++focusAcquisitions;
            focusCandidate = -1;
            focusCandidateSamples = 0;
        }
    }

    std::array<Biquad, 2> kickEqFilters{}, nextKickEqFilters{};
    Ramp kickEqMix, kickEqRetune, monitorMix, monitorGain, balanceRamp;
    double kickEqHz = 0, kickEqDb = 0, wetPower = 0;
    static constexpr auto frequencies = bandFrequencies;
    std::array<std::array<Biquad, 2>, bandCount> detectorsBass{}, detectorsKick{}, focusBassFilters{}, filters{};
    std::array<UnmaskFundamentalBand, 2> fundamentalBass{}, fundamentalKick{};
    struct HarmonicDetector {
        std::array<Biquad, 2> bass{}, kick{};
        double frequency = 0, bassPower = 0, kickPower = 0;
    };
    std::array<HarmonicDetector, 7> harmonicDetectors{};
    std::array<Biquad, 2> upperFilters{};
    double upperFrequency = 0, upperTargetDb = 0, upperCutDb = 0, upperFocusLimitDb = 0;
    bool upperRetuning = false;
    double unmaskRootHz = 0, rootBassPower = 0, rootKickPower = 0, earlyRootKickPower = 0;
    double unmaskBassReference = 0, unmaskKickReference = 0, rootActivity = 0;
    double unmaskKickWeight = 0;
    int unmaskAgcSamples = 0;
    std::array<double, bandCount> bassE{}, kickE{}, focusBassE{}, cutDb{}, focusWeights{};
    std::array<Delay, 2> delays;
    std::array<AutoGain, 3> agc;
    PhaseLearner learner;
    SoftTube tube;AutoGain tubeAgc;
    Meters meters;
    std::array<double, 2> allpassX{}, allpassY{};
    std::array<std::array<double,2>,2> angleX{},angleY{};
    Ramp angleTransition;
    double angleMix=0,angleCoefficient=0,angleLag=0;
    double sampleRate = 48000, bassEnergy = 0, envelope = 0, kickFast = 0, kickSlow = 0;
    double glueReferenceWeight = 0;
    int glueSignalHoldSamples = 0, glueRestartSamples = 0, glueQuietSamples = 0;
    bool glueReferenceReady = false;
    double thresholdDb = -24, ratio = 2, attackMs = 20, releaseMs = 180, glueReduction = 0;
    double energyA = 0, rmsA = 0, smoothA = 0, bandA = 0, parameterA = 0;
    double kickFastA = 0, kickSlowA = 0, attackA = 0, releaseA = 0, cutAttackA = 0, cutReleaseA = 0;
    std::array<Biquad,2> directFilters{};double directCutDb=0,directMix=0;
    double glueMix = 0, phaseMix = 0, unmaskMix = 0, delaySamples = 0, polarity = 1;
    double rotationMix = 0, rotationHz = 90, outputGain = 1, kickIntervalMs = 0;
    double deltaMix = 0;
    double cachedRotationHz = -1, cachedAllpassA = 0;
    double smoothedFilterQ = 2, smoothedDirectQ = 2, filterParameterA = 0;
    double inputPowerMeter = 0, outputPowerMeter = 0, outputHeld = 0, kickHeld = 0;
    double unmaskKeySpanDb = 30.;
    int focusCandidate=-1,focusAcquisitions=0;double focusCandidateSamples=0,focusReferenceSeen=0;
    double focusHz = 75, trackedFocusHz = 75, focusA = 0, peakA = 0;
    std::uint64_t onsetClock = 0, lastOnset = 0;
    double directWeightDenominator = 1e-12;
    int controlCounter = 0;
    bool smoothed = false;
    double duckEnvelope = 0, duckReduction = 0, duckDetectorRelease = 0;
    double duckBodyPole=0,duckPunchPole=0;
    DuckBands duckBassBands,duckKickBands;
    std::array<Biquad,2> duckShelf{};
    double duckShelfHz = 0;
    std::array<double,3> duckBandEnvelope{},duckBandReduction{};
    Ramp duckBandMix;
    bool duckModeInitialised = false;
    SubMono subMono;
    Ramp subMonoMix, dryMonitorGain;
    double subMonoCutoff = 0;
    Ramp globalRamp,delayFade,polarityRamp,midMix,sideMix;
    bool globalInitialised=false,globalPaused=false;
    double previousDelaySamples=0;
};
} // namespace bassdeamon
