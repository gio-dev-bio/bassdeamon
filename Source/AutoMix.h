#pragma once

#include "Dsp.h"
#include "KickEqLearn.h"
#include "StereoPhaseAnalysis.h"

namespace bassdeamon {

struct AutoMixFrame {
    float left, right, kickLeft, kickRight;
    float beat;
};

struct AutoMixResult {
    struct Change { const char* parameter; float value; };
    std::vector<Change> changes;
    StereoPhaseReport mono;
    double dryGainDb = 0, wetGainDb = 0, overlapBefore = 0, overlapAfter = 0;
    bool unsupportedLength = false, analysisFailed = false;
    bool hasKick = false, hasBass = false, reliableKick = false, reliablePhase = false, overlapValid = false;
};

inline AutoMixResult analyzeAutoMix(const std::vector<AutoMixFrame>& audio, double rate,
                                   double bpm, int channels, Settings settings,
                                   const Engine::RuntimeState& runtime = {-24,2,20,180,0,0,0,75,0}) {
    AutoMixResult result;
    if (audio.empty()) return result;

    StereoPhaseAnalysis stereo;
    stereo.prepare(rate, settings.subMonoHz);
    KickEqCapture kick;
    kick.start(rate);
    std::vector<double> roots;
    int kickWindows = 0;
    bool kickPresent = false;
    PhaseLearner phase;
    phase.prepare(rate);
    phase.start();
    int phaseWindows = 0, phaseAccepted = 0;
    LearnResult alignment;
    bool phaseConflict = false;
    double bassEnergy = 0, bassPeak = 0;
    std::array<double,bandCount> overlap{};
    std::array<std::array<Biquad,2>,bandCount> bands{};
    for (size_t i = 0; i < bandCount; ++i)
        for (auto& filter : bands[i]) filter.bandpass(bandFrequencies[i], 2, rate);
    size_t beat = 0;
    for (const auto& frame : audio) {
        const auto nextBeat = size_t(std::max(0.f, std::floor(frame.beat)));
        if (nextBeat != beat) { stereo.finishBeat(beat); beat = nextBeat; }
        stereo.add(frame.left, frame.right);
        const double b = (frame.left + frame.right) * .5;
        const double k = (frame.kickLeft + frame.kickRight) * .5;
        const float* reference[]{&frame.kickLeft, &frame.kickRight};
        if (kick.push(reference, 2, 1)) {
            ++kickWindows;
            kickPresent |= kick.hasSignal();
            const double root = kick.frequency();
            if (root > 0) roots.push_back(root);
            kick.start(rate);
        }
        phase.tick(b, k);
        if (!phase.learning()) {
            ++phaseWindows;
            const auto window = phase.getResult();
            if (window.valid) {
                if (phaseAccepted && (alignment.invert != window.invert ||
                    std::abs(alignment.delayMs - window.delayMs) > .5)) phaseConflict = true;
                if (!phaseAccepted || window.correlation > alignment.correlation) alignment = window;
                ++phaseAccepted;
            }
            phase.start();
        }
        const double power = (double(frame.left) * frame.left + double(frame.right) * frame.right) * .5;
        bassEnergy += power;
        bassPeak = std::max(bassPeak, power);
        for (size_t i = 0; i < bandCount; ++i) {
            const double lowBass = bands[i][0].tick(b), lowKick = bands[i][1].tick(k);
            overlap[i] += std::min(lowBass * lowBass, lowKick * lowKick);
        }
    }
    stereo.finishBeat(beat);
    result.mono = stereo.report();
    result.hasKick = kickPresent || kick.hasSignal();
    if (kick.progress() >= .45f) {
        ++kickWindows;
        const double root = kick.frequency();
        if (root > 0) roots.push_back(root);
    }
    result.hasBass = bassEnergy / audio.size() > 1e-8;
    if (!result.hasBass || !result.hasKick) return result;

    const auto propose = [&](const char* id, double from, double to) {
        if (std::abs(from - to) > .001) result.changes.push_back({id, float(to)});
    };
    const auto strongest = std::max_element(overlap.begin(), overlap.begin() + 10);
    std::sort(roots.begin(), roots.end());
    const double root = roots.empty() ? 0 : roots[roots.size() / 2];
    const auto agreeing = std::count_if(roots.begin(), roots.end(), [root](double hz) {
        return std::abs(hz - root) <= root * .06;
    });
    result.reliableKick = agreeing > 0 && 2 * agreeing > kickWindows;
    if (result.reliableKick && *strongest > bassEnergy * .005) {
        propose("kickEq.on", settings.kickEqEnabled, 1);
        propose("kickEq.frequency", settings.kickEqFrequency, root);
        propose("kickEq.cut", settings.kickEqCut, 1.5);
        settings.kickEqEnabled = true; settings.kickEqFrequency = root; settings.kickEqCut = 1.5;
    }
    const double crest = 10 * std::log10(bassPeak * audio.size() / bassEnergy);
    if (crest > 9) {
        const double amount = std::clamp((crest - 6) / 20, .18, .45);
        const double attack = std::clamp(1200 / bpm, 5., 20.);
        const double release = std::clamp(15000 / bpm, 65., 210.);
        propose("glue.on", settings.glue, 1);
        propose("glue.lock", settings.glueLocked, 0);
        propose("glue.timing", settings.glueTiming, 1);
        propose("glue.amount", settings.glueAmount, amount);
        propose("glue.attackMs", settings.glueAttackMs, attack);
        propose("glue.releaseMs", settings.glueReleaseMs, release);
        settings.glue = true; settings.glueLocked = false; settings.glueTiming = 1;
        settings.glueAmount = amount; settings.glueAttackMs = attack; settings.glueReleaseMs = release;
    }
    result.reliablePhase = phaseAccepted > 0 && 2 * phaseAccepted > phaseWindows && !phaseConflict;
    if (result.reliablePhase && (alignment.delayMs >= .25 || alignment.invert)) {
        propose("phase.on", settings.phase, 1);
        propose("phase.delay", settings.delayMs, alignment.delayMs);
        propose("phase.invert", settings.invert, alignment.invert);
        settings.phase = true; settings.delayMs = alignment.delayMs; settings.invert = alignment.invert;
    }
    const double fraction = *strongest / bassEnergy;
    if (fraction > .012) {
        const double focus = bandFrequencies[size_t(strongest - overlap.begin())];
        const double cut = std::clamp(1.5 + fraction * 14, 2., 6.);
        const double release = std::clamp(12000 / bpm, 50., 160.);
        propose("unmask.on", settings.unmask, 1);
        propose("unmask.controlDb", settings.unmaskDirect, 1);
        propose("unmask.track", settings.unmaskTrack, 0);
        propose("unmask.frequency", settings.unmaskFrequency, focus);
        propose("unmask.width", settings.unmaskWidth, .5);
        propose("unmask.attenuation", settings.unmaskAttenuationDb, cut);
        propose("unmask.attack", settings.unmaskAttackMs, 5);
        propose("unmask.release", settings.unmaskReleaseMs, release);
        propose("unmask.timing", settings.unmaskSync, 0);
        settings.unmask = settings.unmaskDirect = true; settings.unmaskTrack = settings.unmaskSync = false;
        settings.unmaskFrequency = focus; settings.unmaskWidth = .5;
        settings.unmaskAttenuationDb = cut; settings.unmaskAttackMs = 5; settings.unmaskReleaseMs = release;
    }
    if (!settings.tube.enabled && result.reliableKick && fraction > .035 && overlap[8] + overlap[9] < *strongest * .2) {
        propose("tube.on", 0, 1);
        propose("tube.drive", settings.tube.driveDb, 3);
        propose("tube.amount", settings.tube.amount, .15);
        settings.tube.enabled = true; settings.tube.driveDb = 3; settings.tube.amount = .15;
    }

    settings.active = true;
    settings.auditionDry = settings.matchEnabled = settings.delta = settings.tube.audition = false;
    Engine preview;
    preview.prepare(rate);
    preview.restoreRuntime(runtime);
    Biquad beforeBand, afterBand, kickBand;
    const double focus = bandFrequencies[size_t(strongest - overlap.begin())];
    for (auto* filter : {&beforeBand, &afterBand, &kickBand}) filter->bandpass(focus, 2, rate);
    double wetEnergy = 0, before = 0, after = 0, kickEnergy = 0;
    constexpr size_t blockSize = 256;
    std::array<float,blockSize> left{}, right{}, kickLeft{}, kickRight{};
    float* output[]{left.data(), right.data()};
    const float* reference[]{kickLeft.data(), kickRight.data()};
    for (size_t start = 0; start < audio.size(); start += blockSize) {
        const auto count = std::min(blockSize, audio.size() - start);
        for (size_t n = 0; n < count; ++n) {
            const auto& frame = audio[start + n];
            left[n] = frame.left; right[n] = frame.right;
            kickLeft[n] = frame.kickLeft; kickRight[n] = frame.kickRight;
        }
        preview.process(output, channels, reference, 2, int(count), settings, bpm);
        for (size_t n = 0; n < count; ++n) {
            const auto& frame = audio[start + n];
            const double l = left[n], r = channels == 1 ? l : right[n];
            wetEnergy += (l*l + r*r) * .5;
            const double k = kickBand.tick((frame.kickLeft + frame.kickRight) * .5);
            const double b = beforeBand.tick((frame.left + frame.right) * .5);
            const double w = afterBand.tick((l + r) * .5);
            before += std::min(b*b, k*k);
            after += std::min(w*w, k*k);
            kickEnergy += k*k;
        }
    }
    if (wetEnergy > 1e-8) {
        const double delta = 10 * std::log10(wetEnergy / bassEnergy);
        result.dryGainDb = std::min(0., delta);
        result.wetGainDb = std::min(0., -delta);
    }
    result.overlapValid = kickEnergy > 1e-8;
    if (result.overlapValid) {
        result.overlapBefore = 100 * before / kickEnergy;
        result.overlapAfter = 100 * after / kickEnergy;
    }
    return result;
}

} // namespace bassdeamon
