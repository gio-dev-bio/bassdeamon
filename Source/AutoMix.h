#pragma once

#include "Analyzer.h"
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
    double kickRootHz=0,balanceSubDb=0,balanceBodyDb=0,sumPeakDb=-120,balanceBaseTrimDb=0;
    bool balanceValid=false;
    std::array<double,3> balanceEdges{30,70,150};
    bool duckTimingAvailable = false;
    bool unsupportedLength = false, analysisFailed = false;
    bool hasKick = false, hasBass = false, reliableKick = false, reliablePhase = false, overlapValid = false;
};

struct DuckTiming {
    double amountDb = 0, attackMs = 0, releaseMs = 0;
};

inline DuckTiming analyzeDuckTiming(const std::vector<AutoMixFrame>& audio, double rate) {
    // Short RMS bins track the kick body and bass onsets without phase cancellation.
    struct Energy { double bass = 0, kick = 0; };
    const size_t hop = std::max<size_t>(1, size_t(std::round(rate*.004)));
    const double binMs = 1000.*hop/rate;
    std::vector<Energy> bins(audio.size()/hop);
    double kickPeak = 0, bassPeak = 0;
    for (size_t i=0; i<bins.size(); ++i) {
        auto& energy=bins[i];
        for (size_t n=i*hop; n<(i+1)*hop; ++n) {
            const auto& f=audio[n];
            energy.bass += double(f.left)*f.left + double(f.right)*f.right;
            energy.kick += double(f.kickLeft)*f.kickLeft + double(f.kickRight)*f.kickRight;
        }
        energy.bass=std::sqrt(energy.bass/(2.*hop));
        energy.kick=std::sqrt(energy.kick/(2.*hop));
        bassPeak=std::max(bassPeak,energy.bass);
        kickPeak=std::max(kickPeak,energy.kick);
    }
    const double kickGate=std::max(.008,kickPeak*.12), bassGate=std::max(.004,bassPeak*.10);
    const size_t minHitGap=size_t(std::ceil(100/binMs));
    const size_t maxBody=size_t(std::ceil(160/binMs));
    const size_t maxSearch=size_t(std::ceil(300/binMs));
    std::vector<double> releases, depths, attacks;
    for (size_t start=2; start+maxSearch+2<bins.size(); ++start) {
        if (bins[start].kick<kickGate || bins[start-1].kick>=kickGate || bins[start-2].kick>=kickGate) continue;
        size_t last=start, peak=start;
        for (size_t i=start; i<start+maxBody; ++i) {
            if (i>last+2) break;
            if (i>last && bins[i].kick>=kickGate) last=i;
            if (bins[i].kick>bins[peak].kick) peak=i;
        }
        double activeBass=0;
        size_t next=start+maxSearch;
        for (size_t i=start; i<=last; ++i) activeBass=std::max(activeBass,bins[i].bass);
        for (size_t i=start+3; i<start+maxSearch; ++i) {
            if (i>last && bins[i].kick>=kickGate && bins[i-1].kick<kickGate) {next=i; break;}
            // A sustained rise after a local dip indicates an audible new note.
            const double prior=std::max({bins[i-1].bass,bins[i-2].bass,bins[i-3].bass});
            if (bins[i].bass>=bassGate && bins[i+1].bass>=bassGate &&
                std::min(bins[i].bass,bins[i+1].bass)>std::max(2.4*prior,bassPeak*.19)) {next=i; break;}
        }
        if (activeBass>=bassGate && last>start) {
            const double body=(last-start+1)*binMs;
            const double gap=(next-start)*binMs;
            const double room=std::max(0.,gap-body-18.);
            releases.push_back(std::round(std::clamp(std::min(30.+.8*body,room/1.5),20.,120.)));
            depths.push_back(std::clamp(1.5+2.*activeBass/std::max(.02,bins[peak].kick),1.5,4.));
            attacks.push_back(peak-start<=1?1.:1.5);
        }
        start=std::max(last,start+minHitGap-1);
    }
    if (releases.size()<2) return {};
    const auto median=[](std::vector<double>& values) {
        std::sort(values.begin(),values.end());
        return values[(values.size()-1)/2];
    };
    return {std::round(median(depths)*10)/10,median(attacks),median(releases)};
}

inline AutoMixResult analyzeAutoMix(const std::vector<AutoMixFrame>& audio, double rate,
                                   double bpm, int channels, Settings settings,
                                   const Engine::RuntimeState& runtime = {-24,2,20,180,0,0,0,75,0},
                                   std::array<double,3> balanceEdges={30,70,150}) {
    AutoMixResult result;
    if (audio.empty()) return result;

    const Settings originalSettings=settings;
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
    if(result.reliableKick){result.kickRootHz=root;propose("unmask.kickReferenceHz",settings.kickReferenceHz,root);settings.kickReferenceHz=root;}
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
    if (!settings.tube.enabled && result.reliableKick && *strongest / bassEnergy > .035 && overlap[8] + overlap[9] < *strongest * .2) {
        propose("tube.on", 0, 1);
        propose("tube.drive", settings.tube.driveDb, 3);
        propose("tube.amount", settings.tube.amount, .15);
        settings.tube.enabled = true; settings.tube.driveDb = 3; settings.tube.amount = .15;
    }

    const auto measureDuckBands=[&]{
        DuckBands bassBands,kickBands;
        std::array<double,3> shared{};
        const double bodyPole=std::exp(-2*pi*110/rate), punchPole=std::exp(-2*pi*800/rate);
        for (const auto& frame : audio) {
            const std::array<double,2> b{frame.left,frame.right}, k{frame.kickLeft,frame.kickRight};
            for(int channel=0;channel<channels;++channel) {
                const auto bass=bassBands.split(b[channel],channel,bodyPole,punchPole);
                const auto kick=kickBands.split(k[channel],channel,bodyPole,punchPole);
                for(size_t band=0;band<shared.size();++band)
                    shared[band]+=std::min(bass[band]*bass[band],kick[band]*kick[band]);
            }
        }
        const double strongestBand=*std::max_element(shared.begin(),shared.end());
        for (auto& value : shared)
            value=strongestBand>0?std::round(100*std::sqrt(value/strongestBand))/100:0;
        return shared;
    };
    const auto duck = analyzeDuckTiming(audio,rate);
    result.duckTimingAvailable = duck.amountDb>0;
    if (result.duckTimingAvailable) {
        propose("duck.on", settings.duck, 1);
        propose("duck.amount", settings.duckAmountDb, duck.amountDb);
        propose("duck.attack", settings.duckAttackMs, duck.attackMs);
        propose("duck.release", settings.duckReleaseMs, duck.releaseMs);
        propose("duck.timing", settings.duckSync, 0);
        settings.duck=true;
        settings.duckAmountDb=duck.amountDb;
        settings.duckAttackMs=duck.attackMs;
        settings.duckReleaseMs=duck.releaseMs;
        settings.duckSync=false;
        const auto bands=measureDuckBands();
        constexpr std::array bandIds{"duck.body","duck.punch","duck.click"};
        propose("duck.multiband",settings.duckMultiband,1);
        for (size_t band=0;band<bands.size();++band)
            propose(bandIds[band],settings.duckBands[band],bands[band]);
        settings.duckBands=bands;
        settings.duckMultiband=true;
    }

    settings.active = true;
    settings.auditionDry = settings.matchEnabled = settings.delta = settings.tube.audition = false;
    constexpr size_t blockSize = 256;
    std::array<float,blockSize> left{}, right{}, kickLeft{}, kickRight{};
    float* output[]{left.data(), right.data()};
    const float* reference[]{kickLeft.data(), kickRight.data()};
    const auto render = [&](const Settings& pass, auto&& measure) {
        Engine engine;
        engine.prepare(rate);
        engine.restoreRuntime(runtime);
        for (size_t start = 0; start < audio.size(); start += blockSize) {
            const auto count = std::min(blockSize, audio.size() - start);
            for (size_t n = 0; n < count; ++n) {
                const auto& frame = audio[start+n];
                left[n] = frame.left; right[n] = frame.right;
                kickLeft[n] = frame.kickLeft; kickRight[n] = frame.kickRight;
            }
            engine.process(output, channels, reference, 2, int(count), pass, bpm);
            measure(start,count);
        }
    };

    // Render the proposed upstream chain, including Duck, into the point
    // where Unmask receives audio. Downstream settings are neutral for this pass.
    auto beforeUnmask = settings;
    beforeUnmask.unmask = beforeUnmask.unmaskTrack = beforeUnmask.subMono = false;
    beforeUnmask.outputDb = beforeUnmask.balanceTrimDb = 0;
    constexpr size_t focusBands = 10;
    std::array<double,focusBands> remaining{};
    double bassAfterDuckEnergy=0;
    std::array<std::array<std::array<Biquad,2>,2>,focusBands> residualFilters{};
    for (size_t band=0;band<focusBands;++band)
        for (auto& channel : residualFilters[band])
            for (auto& filter : channel) filter.bandpass(bandFrequencies[band],2,rate);
    render(beforeUnmask,[&](size_t,size_t count) {
        for (int channel=0;channel<channels;++channel)
            for (size_t n=0;n<count;++n)
                bassAfterDuckEnergy+=double(output[channel][n])*output[channel][n]/channels;
        for (size_t band=0;band<focusBands;++band) {
            for (int channel=0;channel<channels;++channel) {
                auto& filters=residualFilters[band][channel];
                double bassPower=0, kickPower=0, joint=0;
                for (size_t n=0;n<count;++n) {
                    const double b=filters[0].tick(output[channel][n]);
                    const double k=filters[1].tick(reference[channel][n]);
                    bassPower+=b*b;
                    kickPower+=k*k;
                    joint+=std::min(b*b,k*k);
                }
                const float score=RelationAnalyzer::mask(float(db(std::sqrt(bassPower/count))),
                                                         float(db(std::sqrt(kickPower/count))));
                if (score>=RelationAnalyzer::collisionThreshold) remaining[band]+=joint/channels;
            }
        }
    });
    const auto strongestRemaining=std::max_element(remaining.begin(),remaining.end());
    const double fraction=bassAfterDuckEnergy>1e-8 ? *strongestRemaining/bassAfterDuckEnergy : 0;
    const double unmaskFocus=bandFrequencies[size_t(strongestRemaining-remaining.begin())];
    if (fraction>.012) {
        const double cut=std::clamp(1.5 + fraction * 14, 2., 6.);
        const double release=std::clamp(12000 / bpm, 50., 160.);
        propose("unmask.on", settings.unmask, 1);
        propose("unmask.controlDb", settings.unmaskDirect, 1);
        if (!settings.unmaskTrack)
            propose("unmask.frequency", settings.unmaskFrequency, unmaskFocus);
        propose("unmask.width", settings.unmaskWidth, .5);
        propose("unmask.attenuation", settings.unmaskAttenuationDb, cut);
        propose("unmask.attack", settings.unmaskAttackMs, 5);
        propose("unmask.release", settings.unmaskReleaseMs, release);
        propose("unmask.timing", settings.unmaskSync, 0);
        settings.unmask = settings.unmaskDirect = true; settings.unmaskSync = false;
        if (!settings.unmaskTrack) settings.unmaskFrequency = unmaskFocus;
        settings.unmaskWidth = .5;
        settings.unmaskAttenuationDb = cut; settings.unmaskAttackMs = 5; settings.unmaskReleaseMs = release;
    }

    // This comparison describes the full Auto Mix proposal against its input.
    Biquad beforeBand, afterBand, kickBand;
    const double focus = fraction>.012 ? unmaskFocus : bandFrequencies[size_t(strongest-overlap.begin())];
    for (auto* filter : {&beforeBand, &afterBand, &kickBand}) filter->bandpass(focus, 2, rate);
    double wetEnergy = 0, before = 0, after = 0, kickEnergy = 0;
    render(settings,[&](size_t start,size_t count) {
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
    });
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
    // Measure the CURRENT processed bass, independently from the Auto Mix
    // proposal. Stereo channel energies never cancel into a mono average.
    std::sort(balanceEdges.begin(),balanceEdges.end());
    balanceEdges[0]=std::clamp(balanceEdges[0],20.,100.);
    balanceEdges[1]=std::clamp(balanceEdges[1],balanceEdges[0]+5.,150.);
    balanceEdges[2]=std::clamp(balanceEdges[2],balanceEdges[1]+5.,300.);
    result.balanceEdges=balanceEdges;result.balanceBaseTrimDb=originalSettings.balanceTrimDb;
    std::array<std::array<std::array<Biquad,2>,2>,2> filters{};
    for(size_t band=0;band<2;++band)for(size_t source=0;source<2;++source)for(auto& filter:filters[band][source]) {
        const double lo=balanceEdges[band],hi=balanceEdges[band+1],centre=std::sqrt(lo*hi);
        filter.bandpass(centre,centre/(hi-lo),rate);
    }
    std::array<std::array<double,2>,2> energy{};double sumPeak=0;
    render(originalSettings,[&](size_t,size_t count) {
        for(size_t n=0;n<count;++n)for(size_t c=0;c<size_t(channels);++c) {
            const double b=output[c][n],k=reference[c][n];sumPeak=std::max(sumPeak,std::abs(b+k));
            for(size_t band=0;band<2;++band){const double bv=filters[band][0][c].tick(b),kv=filters[band][1][c].tick(k);energy[band][0]+=bv*bv;energy[band][1]+=kv*kv;}
        }
    });
    result.balanceValid=std::all_of(energy.begin(),energy.end(),[](const auto& band){return band[0]>1e-8&&band[1]>1e-8;});
    if(result.balanceValid){result.balanceSubDb=10*std::log10(energy[0][0]/energy[0][1]);result.balanceBodyDb=10*std::log10(energy[1][0]/energy[1][1]);}
    result.sumPeakDb=db(sumPeak);
    return result;
}

} // namespace bassdeamon
