#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace bassdeamon {
constexpr double pi = 3.14159265358979323846;
inline double db(double x) noexcept { return 20.0 * std::log10(std::max(x, 1.0e-12)); }
inline double gain(double d) noexcept { return std::pow(10.0, d / 20.0); }
inline double pole(double ms, double sr) noexcept { return std::exp(-1.0 / (0.001 * ms * sr)); }
inline double follow(double current, double target, double a) noexcept {
    return target + a * (current - target);
}
inline double power(const std::array<double, 2>& x, int channels) noexcept {
    return (x[0] * x[0] + (channels == 2 ? x[1] * x[1] : 0.0)) / channels;
}

struct AgcSettings { bool enabled = false, locked = false; };
struct Settings {
    bool glue = true, glueLocked = false;
    double glueAmount = 0.35;
    bool phase = false, invert = false, rotate = false;
    double delayMs = 0.0, rotationHz = 90.0;
    bool unmask = true, delta = false;
    double unmaskAmount = 0.4, outputDb = 0.0;
    std::array<AgcSettings, 3> agc{};
};
struct Meters {
    double glueReductionDb = 0.0, unmaskReductionDb = 0.0;
    std::array<double, 3> agcDb{};
    double peak = 0.0;
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

struct LearnResult { double delayMs = 0, correlation = 0; bool invert = false, valid = false; };
class PhaseLearner {
public:
    void prepare(double sr) {
        sampleRate = sr; decimation = std::max(1, static_cast<int>(sr / 2000));
        lowPole = std::exp(-2 * pi * 250 / sr); history.prepare(sr); start(); active = false;
    }
    void start() noexcept {
        cross.fill(0); bassPower.fill(0); kickPower = 0; count = 0;
        elapsed = 0; lowBass = lowKick = 0; active = true; result = {};
    }
    void cancel() noexcept { active = false; }
    void tick(double bass, double kick) noexcept {
        lowBass = follow(lowBass, bass, lowPole); lowKick = follow(lowKick, kick, lowPole);
        history.push(lowBass);
        if (!active) return;
        ++elapsed;
        if (elapsed > static_cast<int>(sampleRate * 0.02) && elapsed % decimation == 0
            && lowKick * lowKick > 1e-8) {
            for (size_t i = 0; i < cross.size(); ++i) {
                const double b = history.read(static_cast<double>(i) * 0.00025 * sampleRate);
                cross[i] += b * lowKick; bassPower[i] += b * b;
            }
            kickPower += lowKick * lowKick; ++count;
        }
        if (elapsed >= static_cast<int>(4 * sampleRate)) {
            active = false;
            double best = 0;
            for (size_t i = 0; i < cross.size(); ++i) {
                const double corr = cross[i] / std::sqrt(std::max(1e-30, bassPower[i] * kickPower));
                if (std::abs(corr) > best) {
                    best = std::abs(corr); result = {static_cast<double>(i) * 0.25, best, corr < 0, false};
                }
            }
            result.valid = count >= 200 && kickPower > 1e-5 && best >= 0.15;
        }
    }
    bool learning() const noexcept { return active; }
    LearnResult getResult() const noexcept { return result; }
private:
    std::array<double, 21> cross{}, bassPower{};
    double sampleRate = 48000, lowPole = 0, lowBass = 0, lowKick = 0, kickPower = 0;
    int decimation = 24, elapsed = 0, count = 0;
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
        cutAttackA = pole(5, sampleRate); cutReleaseA = pole(100, sampleRate);
        attackA = pole(attackMs, sampleRate); releaseA = pole(releaseMs, sampleRate);
    }
    void startLearn() noexcept { learner.start(); }
    void cancelLearn() noexcept { learner.cancel(); }
    bool learning() const noexcept { return learner.learning(); }
    LearnResult learnResult() const noexcept { return learner.getResult(); }
    const Meters& getMeters() const noexcept { return meters; }
    // Frozen adaptation/AGC values are part of the session state, not only UI parameters.
    using RuntimeState = std::array<double, 7>;
    RuntimeState runtimeState() const noexcept {
        return {thresholdDb, ratio, attackMs, releaseMs,
                agc[0].learnedDb(), agc[1].learnedDb(), agc[2].learnedDb()};
    }
    void restoreRuntime(const RuntimeState& r) noexcept {
        thresholdDb = std::clamp(r[0], -100.0, 12.0); ratio = std::clamp(r[1], 1.0, 4.0);
        attackMs = std::clamp(r[2], 5.0, 80.0); releaseMs = std::clamp(r[3], 50.0, 500.0);
        attackA = pole(attackMs, sampleRate); releaseA = pole(releaseMs, sampleRate);
        for (size_t i = 0; i < agc.size(); ++i) agc[i].restore(r[4+i]);
    }
    void clearHistory() noexcept {
        for (auto& d : delays) d.clear();
        for (auto* bank : {&detectorsBass, &detectorsKick, &filters})
            for (auto& band : *bank) for (auto& f : band) f.z1 = f.z2 = 0;
        allpassX.fill(0); allpassY.fill(0);
        bassE.fill(0); kickE.fill(0); bassEnergy = envelope = kickFast = kickSlow = 0;
        learner.cancel();
    }

    void process(float* const* bass, int channels, const float* const* kick, int kickChannels,
                 int samples, const Settings& s, double bpm = 0) noexcept {
        if (channels < 1 || channels > 2) return;
        if (!smoothed) {
            glueMix = s.glue ? 1 : 0; phaseMix = s.phase ? 1 : 0;
            unmaskMix = s.unmask ? 1 : 0; delaySamples = s.delayMs * sampleRate / 1000;
            polarity = s.invert ? -1 : 1; rotationMix = s.rotate ? 1 : 0;
            rotationHz = s.rotationHz; outputGain = gain(s.outputDb); smoothed = true;
            deltaMix = s.delta && s.unmask ? 1 : 0;
        }
        meters.peak = 0;
        for (int n = 0; n < samples; ++n) {
            std::array<double, 2> x{}, k{};
            for (int c = 0; c < channels; ++c) x[c] = std::isfinite(bass[c][n]) ? bass[c][n] : 0;
            if (kick && kickChannels > 0) {
                // Detector powers are linked; opposite-polarity stereo never cancels here.
                k[0] = std::isfinite(kick[0][n]) ? kick[0][n] : 0;
                k[1] = kickChannels > 1 ? (std::isfinite(kick[1][n]) ? kick[1][n] : 0) : k[0];
            }
            const double inputPower = power(x, channels), kp = power(k, 2);
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
                for (size_t b = 0; b < frequencies.size(); ++b)
                    for (int c = 0; c < 2; ++c) filters[b][c].peak(frequencies[b], 2, cutDb[b], sampleRate);
            }
            glueMix = follow(glueMix, s.glue ? 1 : 0, smoothA);
            phaseMix = follow(phaseMix, s.phase ? 1 : 0, smoothA);
            unmaskMix = follow(unmaskMix, s.unmask ? 1 : 0, smoothA);
            const double over = db(std::sqrt(envelope)) - thresholdDb;
            const double knee = 6, slope = 1 - 1 / ratio;
            const double desired = over <= -knee/2 ? 0 : (over >= knee/2 ? slope * over
                                    : slope * (over+knee/2)*(over+knee/2)/(2*knee));
            glueReduction = follow(glueReduction, desired * std::clamp(s.glueAmount, 0.0, 1.0),
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
            delaySamples = follow(delaySamples, std::clamp(s.delayMs, 0.0, 5.0)*sampleRate/1000, smoothA);
            polarity = follow(polarity, s.invert ? -1 : 1, smoothA);
            rotationMix = follow(rotationMix, s.rotate ? 1 : 0, smoothA);
            rotationHz = follow(rotationHz, std::clamp(s.rotationHz, 20.0, 500.0), smoothA);
            const double t = std::tan(pi * rotationHz / sampleRate), a = (t-1)/(t+1);
            double phaseReference = 0;
            for (int c = 0; c < channels; ++c) {
                delays[c].push(x[c]);
                const double d = delays[c].read(std::clamp(delaySamples, 0.0, sampleRate * 0.005));
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
                const double activity = std::clamp((10*std::log10(std::max(overlap,1e-20))+65)/30, 0.0, 1.0);
                const double target = -4 * std::clamp(s.unmaskAmount, 0.0, 1.0)*activity;
                cutDb[b] = follow(cutDb[b], target, target < cutDb[b] ? cutAttackA : cutReleaseA);
                maxCut = std::max(maxCut, -cutDb[b]);
                for (int c = 0; c < channels; ++c) x[c] = filters[b][c].tick(x[c]);
            }
            for (int c = 0; c < channels; ++c) x[c] = beforeUnmask[c] + unmaskMix*(x[c]-beforeUnmask[c]);
            const auto afterUnmask = x;
            const double unmaskAgc = compensate(2, power(beforeUnmask, channels), power(x, channels), unmaskMix);
            outputGain = follow(outputGain, gain(std::clamp(s.outputDb, -24.0, 12.0)), smoothA);
            deltaMix = follow(deltaMix, s.delta && s.unmask ? 1 : 0, smoothA);
            for (int c = 0; c < channels; ++c) {
                // Delta is removed bass BEFORE AGC/output trim; never includes kick.
                const double normal = x[c]*unmaskAgc*outputGain;
                const double out = normal + deltaMix*(beforeUnmask[c]-afterUnmask[c]-normal);
                bass[c][n] = static_cast<float>(out);
                meters.peak = std::max(meters.peak, std::abs(out));
            }
            meters.glueReductionDb = glueReduction * glueMix;
            meters.unmaskReductionDb = maxCut * unmaskMix; // maximum individual band, not total cut
        }
    }
private:
    static constexpr std::array<double, 6> frequencies{40, 55, 75, 105, 145, 200};
    std::array<std::array<Biquad, 2>, 6> detectorsBass{}, detectorsKick{}, filters{};
    std::array<double, 6> bassE{}, kickE{}, cutDb{};
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
    std::uint64_t onsetClock = 0, lastOnset = 0;
    int controlCounter = 0;
    bool smoothed = false;
};
} // namespace bassdeamon
