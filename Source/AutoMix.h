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

struct DynamicsBin { double bass = 0, kick = 0; };

inline double activeBassRangeDb(const std::vector<DynamicsBin>& bins, double binMs, double gate) {
    // Measure variations within active notes rather than mistaking gaps
    // between equally loud notes for excess dynamics.
    const size_t window = size_t(std::lround(24 / binMs));
    std::vector<double> levels;
    double power = 0;
    size_t active = 0;
    for (size_t i = 0; i < bins.size(); ++i) {
        const double current = bins[i].bass;
        power += current * current;
        active += current >= gate;
        if (i >= window) {
            const double previous = bins[i-window].bass;
            power -= previous * previous;
            active -= previous >= gate;
        }
        if (i+1 >= window && 4 * active >= 3 * window)
            levels.push_back(power / window);
    }
    if (levels.size() < 6) return 0;
    std::sort(levels.begin(), levels.end());
    const size_t last = levels.size() - 1;
    return 10 * std::log10(levels[last * 17 / 20] / levels[last / 5]);
}

struct AutoMixDynamics {
    DuckTiming duck;
    double bassRangeDb = 0;
};

inline AutoMixDynamics analyzeDynamics(const std::vector<AutoMixFrame>& audio, double rate) {
    // Short RMS bins track the kick body and bass onsets without phase cancellation.
    const size_t hop = std::max<size_t>(1, size_t(std::round(rate*.004)));
    const double binMs = 1000.*hop/rate;
    std::vector<DynamicsBin> bins(audio.size()/hop);
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
    AutoMixDynamics result;
    result.bassRangeDb = activeBassRangeDb(bins, binMs, bassGate);
    // Bridge zero crossings in the kick. A dip within the body must not
    // be interpreted as a new hit ending the release window.
    const size_t bridge = size_t(std::lround(16 / binMs));
    struct Hit { size_t start, last, peak; };
    std::vector<Hit> hits;
    for (size_t start=0; start<bins.size(); ++start) {
        if (bins[start].kick < kickGate) continue;
        Hit hit{start,start,start};
        for (size_t i=start+1; i<bins.size() && i<=hit.last+bridge; ++i) {
            if (bins[i].kick >= kickGate) hit.last = i;
            if (bins[i].kick > bins[hit.peak].kick) hit.peak = i;
        }
        if (hit.last + bridge < bins.size()) hits.push_back(hit);
        start = hit.last;
    }
    const size_t maxSearch = size_t(std::ceil(300 / binMs));
    std::vector<double> releases, depths, attacks;
    for (size_t index=0; index<hits.size(); ++index) {
        const auto [start, last, peak] = hits[index];
        double bassPower = 0, kickPower = 0;
        for (size_t i=start; i<=last; ++i) {
            kickPower += bins[i].kick * bins[i].kick;
            if (bins[i].bass >= bassGate) bassPower += bins[i].bass * bins[i].bass;
        }
        const double balance = std::sqrt(bassPower / kickPower);
        const double weight = std::clamp((balance - .126) / (.90 - .126), 0., 1.);
        const double depth = 4 * weight * weight * (3 - 2 * weight);
        if (depth < .15) continue;

        size_t next = index+1 < hits.size() ? hits[index+1].start : bins.size();
        const size_t searchEnd = std::min({next, start+maxSearch, bins.size()-1});
        for (size_t i=start+3; i<searchEnd; ++i) {
            // A sustained rise after a local dip indicates an audible new note.
            const double prior=std::max({bins[i-1].bass,bins[i-2].bass,bins[i-3].bass});
            if (bins[i].bass>=bassGate && bins[i+1].bass>=bassGate &&
                std::min(bins[i].bass,bins[i+1].bass)>std::max(2.4*prior,bassPeak*.19)) {next=i; break;}
        }
        const double body = (last-start+1) * binMs;
        double release = 30 + .8 * body;
        if (next < bins.size())
            release = std::min(release, ((next-start) * binMs - body - 18) / 1.5);
        releases.push_back(std::round(std::clamp(release,20.,120.)));
        depths.push_back(depth);
        const double riseMs = (peak-start) * binMs;
        attacks.push_back(std::clamp(std::round(10 * .35 * riseMs) / 10,1.,8.));
    }
    if (releases.size()<2) return result;
    const auto median=[](std::vector<double>& values) {
        std::sort(values.begin(),values.end());
        return values[(values.size()-1)/2];
    };
    result.duck = {std::round(median(depths)*10)/10,median(attacks),median(releases)};
    return result;
}

inline AutoMixResult analyzeAutoMix(const std::vector<AutoMixFrame>& audio, double rate,
                                   double bpm, int channels, Settings settings,
                                   const Engine::RuntimeState& runtime = {-24,2,20,180,0,0,0,75,0},
                                   std::array<double,3> balanceEdges={30,70,150}) {
    AutoMixResult result;
    if (audio.empty()) return result;

    // Analysis and the proposal use the normal processing path. Monitor
    // switches and comparison gains must not affect the Balance report.
    settings.auditionDry = settings.matchEnabled = settings.delta = settings.tube.audition = false;
    settings.auditionDryDb = settings.auditionWetDb = 0;
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
    double bassEnergy = 0;
    std::array<double,bandCount> overlap{};
    std::array<std::array<std::array<Biquad,2>,2>,bandCount> bands{};
    for (size_t i = 0; i < bandCount; ++i)
        for (auto& channel : bands[i])
            for (auto& filter : channel) filter.bandpass(bandFrequencies[i], 2, rate);
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
        const std::array<double,2> bassChannels{frame.left,frame.right}, kickChannels{frame.kickLeft,frame.kickRight};
        for (size_t i = 0; i < bandCount; ++i)
            for (size_t c=0;c<2;++c) {
                auto& filters=bands[i][c];
                const double lowBass=filters[0].tick(bassChannels[c]), lowKick=filters[1].tick(kickChannels[c]);
                overlap[i]+=.5*std::min(lowBass*lowBass,lowKick*lowKick);
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
    const auto dynamics = analyzeDynamics(audio, rate);
    if (!settings.glueLocked && dynamics.bassRangeDb > 6) {
        const double amount = std::clamp((dynamics.bassRangeDb - 4) / 18, .18, .45);
        propose("glue.on", settings.glue, 1);
        propose("glue.amount", settings.glueAmount, amount);
        settings.glue = true;
        settings.glueAmount = amount;
    }
    result.reliablePhase = phaseAccepted > 0 && 2 * phaseAccepted > phaseWindows && !phaseConflict;
    if (result.reliablePhase && (alignment.delayMs >= .25 || alignment.invert)) {
        propose("phase.on", settings.phase, 1);
        propose("phase.delay", settings.delayMs, alignment.delayMs);
        propose("phase.invert", settings.invert, alignment.invert);
        // Learn solves for an absolute delay and polarity, with no rotation.
        propose("phase.rotate", settings.rotate, 0);
        propose("phase.angle", settings.phaseAngle, 0);
        settings.phase = true; settings.delayMs = alignment.delayMs; settings.invert = alignment.invert;
        settings.rotate=false;settings.phaseAngle=0;
    }
    const auto measureDuckBands=[&]{
        DuckBands bassBands,kickBands;
        std::array<double,3> shared{};
        const double bodyPole=std::exp(-2*pi*110/rate), punchPole=std::exp(-2*pi*800/rate);
        for (const auto& frame : audio) {
            const std::array<double,2> b{frame.left,frame.right}, k{frame.kickLeft,frame.kickRight};
            for(int channel=0;channel<2;++channel) {
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
    const auto& duck = dynamics.duck;
    result.duckTimingAvailable = duck.amountDb>0;
    if (result.duckTimingAvailable && (!settings.duck || settings.duckAmountDb <= duck.amountDb)) {
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
        propose("duck.multiband",settings.duckMultiband,1);
        settings.duckMultiband=true;
        // Body/Punch/Click are read only by the legacy crossover. The
        // current Low Band mode uses the configured cutoff instead.
        if (settings.duckLowHz<=0) {
            const auto weights=measureDuckBands();
            constexpr std::array bandIds{"duck.body","duck.punch","duck.click"};
            for (size_t band=0;band<weights.size();++band)
                propose(bandIds[band],settings.duckBands[band],weights[band]);
            settings.duckBands=weights;
        }
    }

    settings.active = true;
    constexpr size_t blockSize = 256;
    std::array<float,blockSize> left{}, right{}, kickLeft{}, kickRight{};
    float* output[]{left.data(), right.data()};
    const float* reference[]{kickLeft.data(), kickRight.data()};
    const auto processedBass = [&](size_t channel, size_t n) {
        return output[channels==1 ? 0 : channel][n];
    };
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
    std::array<double,focusBands> upperCutEnergy{};
    double bassAfterDuckEnergy=0;
    std::array<std::array<std::array<Biquad,2>,2>,focusBands> residualFilters{};
    const double learnedRoot = unmaskReferenceInRange(settings);
    const double measuredRoot = result.reliableKick ? root : learnedRoot;
    std::array<std::array<UnmaskFundamentalBand, 2>, 2> fundamentalFilters{};
    double fundamentalRemaining = 0, fundamentalKickEnergy = 0;
    const auto measureResidual = [&](auto& filterBank, size_t count, bool fundamental, double* upperCut = nullptr) {
        double overlapEnergy = 0;
        for (size_t channel=0;channel<2;++channel) {
            auto& filters=filterBank[channel];
            double bassPower=0, kickPower=0, joint=0, fullBass=0, fullKick=0;
            for (size_t n=0;n<count;++n) {
                const double dryBass=processedBass(channel,n), dryKick=reference[channel][n];
                const double b=filters[0].tick(dryBass), k=filters[1].tick(dryKick);
                bassPower+=b*b;
                kickPower+=k*k;
                joint+=std::min(b*b,k*k);
                if (fundamental || upperCut) {
                    fullBass+=dryBass*dryBass;
                    fullKick+=dryKick*dryKick;
                }
            }
            if (fundamental) {
                const double activity=unmaskFundamentalActivity(bassPower/count,kickPower/count,fullBass/count,fullKick/count);
                fundamentalKickEnergy += .5 * kickPower;
                overlapEnergy += .5 * joint * activity;
            } else {
                const float score=RelationAnalyzer::mask(float(db(std::sqrt(bassPower/count))),
                                                         float(db(std::sqrt(kickPower/count))));
                if (score>=RelationAnalyzer::collisionThreshold) {
                    overlapEnergy += .5 * joint;
                    if (upperCut)
                        *upperCut += .5 * joint * unmaskUpperCutDb(bassPower/count, kickPower/count,
                                                                  fullBass/count, fullKick/count);
                }
            }
        }
        return overlapEnergy;
    };
    const auto scanResidual = [&] {
        remaining.fill(0);
        upperCutEnergy.fill(0);
        bassAfterDuckEnergy = fundamentalRemaining = fundamentalKickEnergy = 0;
        residualFilters = {};
        for (size_t band=0; band<focusBands; ++band)
            for (auto& channel : residualFilters[band])
                for (auto& filter : channel) filter.bandpass(bandFrequencies[band],2,rate);
        if (measuredRoot > 0)
            for (auto& channel : fundamentalFilters)
                for (auto& filter : channel) filter.tune(measuredRoot, rate);
        render(beforeUnmask,[&](size_t,size_t count) {
            for (int channel=0;channel<channels;++channel)
                for (size_t n=0;n<count;++n)
                    bassAfterDuckEnergy+=double(output[channel][n])*output[channel][n]/channels;
            for (size_t band=0;band<focusBands;++band) {
                const double hz = bandFrequencies[band];
                if (learnedRoot == 0 || std::abs(hz - learnedRoot) > learnedRoot * .15)
                    remaining[band] += measureResidual(residualFilters[band], count, false,
                        isUnmaskUpperBand(hz, learnedRoot) ? &upperCutEnergy[band] : nullptr);
            }
            if (measuredRoot > 0) fundamentalRemaining += measureResidual(fundamentalFilters,count,true);
        });
    };
    const auto fundamentalOverlaps = [&] {
        return fundamentalRemaining > std::max(audio.size() * 3.162e-7, fundamentalKickEnergy * .12);
    };
    scanResidual();
    // A static notch needs overlap at the stable fundamental itself.
    // Leave light shared energy to the dynamic modules.
    const double rootShare = fundamentalKickEnergy > 0 ? fundamentalRemaining / fundamentalKickEnergy : 0;
    const double eqCut = std::clamp(3 * (rootShare - .20), 0., 1.5);
    if (result.reliableKick && (!settings.kickEqEnabled || settings.kickEqFrequency < 30)
        && fundamentalOverlaps() && eqCut >= .2) {
        propose("kickEq.on", settings.kickEqEnabled, 1);
        propose("kickEq.frequency", settings.kickEqFrequency, measuredRoot);
        propose("kickEq.cut", settings.kickEqCut, eqCut);
        beforeUnmask.kickEqEnabled = settings.kickEqEnabled = true;
        beforeUnmask.kickEqFrequency = settings.kickEqFrequency = measuredRoot;
        beforeUnmask.kickEqCut = settings.kickEqCut = eqCut;
        scanResidual();
    }
    const auto strongestRemaining=std::max_element(remaining.begin(),remaining.end());
    // Assess the fundamental against the kick's own energy there. Normalising
    // by the full bass would bury a short, real overlap under loud harmonics.
    const bool preferFundamental = learnedRoot > 0 && fundamentalOverlaps();
    const double remainingOverlap = preferFundamental ? fundamentalRemaining : *strongestRemaining;
    const double fraction=bassAfterDuckEnergy>1e-8 ? remainingOverlap/bassAfterDuckEnergy : 0;
    const size_t residualBand = size_t(strongestRemaining-remaining.begin());
    const double unmaskFocus=preferFundamental ? learnedRoot : bandFrequencies[residualBand];
    const bool hasResidualOverlap = preferFundamental || fraction > .012;
    const double upperLimit = isUnmaskUpperBand(unmaskFocus, learnedRoot) && remainingOverlap > 0
        ? upperCutEnergy[residualBand] / remainingOverlap : 6.;
    const double cut = std::min(std::clamp(1.5 + fraction * 14, 2., 6.), upperLimit);
    if (hasResidualOverlap && cut > .10) {
        propose("unmask.on", settings.unmask, 1);
        propose("unmask.controlDb", settings.unmaskDirect, 1);
        if (!settings.unmaskTrack)
            propose("unmask.frequency", settings.unmaskFrequency, unmaskFocus);
        propose("unmask.width", settings.unmaskWidth, .5);
        propose("unmask.attenuation", settings.unmaskAttenuationDb, cut);
        settings.unmask = settings.unmaskDirect = true;
        if (!settings.unmaskTrack) settings.unmaskFrequency = unmaskFocus;
        settings.unmaskWidth = .5;
        settings.unmaskAttenuationDb = cut;
        if (result.duckTimingAvailable) {
            const double release = std::max(30., duck.releaseMs);
            propose("unmask.release", settings.unmaskReleaseMs, release);
            propose("unmask.timing", settings.unmaskSync, 0);
            settings.unmaskReleaseMs = release;
            settings.unmaskSync = false;
        }
    }

    // This comparison describes the full Auto Mix proposal against its input.
    std::array<Biquad,2> beforeBand{}, afterBand{}, kickBand{};
    const double focus = hasResidualOverlap ? unmaskFocus : bandFrequencies[size_t(strongest-overlap.begin())];
    for (auto* bank : {&beforeBand,&afterBand,&kickBand})
        for (auto& filter : *bank) filter.bandpass(focus, preferFundamental ? 6 : 2, rate);
    double wetEnergy = 0, before = 0, after = 0, kickEnergy = 0;
    render(settings,[&](size_t start,size_t count) {
        for (size_t n = 0; n < count; ++n) {
            const auto& frame = audio[start + n];
            const double l = processedBass(0,n), r = processedBass(1,n);
            wetEnergy += (l*l + r*r) * .5;
            const std::array<double,2> dry{frame.left,frame.right}, wet{l,r};
            for (size_t c=0;c<2;++c) {
                const double k=kickBand[c].tick(reference[c][n]);
                const double b=beforeBand[c].tick(dry[c]);
                const double w=afterBand[c].tick(wet[c]);
                before+=std::min(b*b,k*k);
                after+=std::min(w*w,k*k);
                kickEnergy+=k*k;
            }
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
        for(size_t n=0;n<count;++n)for(size_t c=0;c<2;++c) {
            const double b=processedBass(c,n),k=reference[c][n];sumPeak=std::max(sumPeak,std::abs(b+k));
            for(size_t band=0;band<2;++band){const double bv=filters[band][0][c].tick(b),kv=filters[band][1][c].tick(k);energy[band][0]+=bv*bv;energy[band][1]+=kv*kv;}
        }
    });
    result.balanceValid=std::all_of(energy.begin(),energy.end(),[](const auto& band){return band[0]>1e-8&&band[1]>1e-8;});
    if(result.balanceValid){result.balanceSubDb=10*std::log10(energy[0][0]/energy[0][1]);result.balanceBodyDb=10*std::log10(energy[1][0]/energy[1][1]);}
    result.sumPeakDb=db(sumPeak);
    return result;
}

} // namespace bassdeamon
