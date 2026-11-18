#include "AutoMix.h"
#include "Analyzer.h"
#include "RealtimeAllocation.h"
#include <iostream>
#include <random>
#include <stdexcept>
#include <string_view>

using namespace bassdeamon;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    constexpr double sr = 44100;
    std::vector<AutoMixFrame> audio(size_t(8 * sr));
    Settings settings;
    settings.duckLowHz=110;
    for (size_t n = 0; n < audio.size(); ++n) {
        const double t = n / sr, hit = std::fmod(t, .5);
        const float kick = float(.8 * std::exp(-hit * 16) * std::sin(2 * pi * 52 * hit));
        const float bass = float(.35 * std::sin(2 * pi * 52 * t));
        audio[n] = {bass, bass, kick, kick, float(2 * t)};
    }
    const auto proposal = analyzeAutoMix(audio, sr, 120, 2, settings);
    require(proposal.hasKick && proposal.hasBass && proposal.reliableKick, "Stable kick rejected");
    require(!proposal.changes.empty() && proposal.overlapValid, "Missing Auto Mix proposal");
    require(proposal.overlapAfter < proposal.overlapBefore, "Proposal should reduce overlap");
    require(proposal.dryGainDb == 0 || proposal.wetGainDb == 0, "Match boosted the quieter version");
    require(proposal.duckTimingAvailable, "Auto Mix did not include Duck");
    for (const char* id : {"duck.on", "duck.attack", "duck.amount", "duck.release"})
        require(std::any_of(proposal.changes.begin(),proposal.changes.end(),[id](const auto& change) {
            return std::string_view(change.parameter)==id;
        }), "A Duck parameter was missing from the proposal");
    require(std::none_of(proposal.changes.begin(),proposal.changes.end(),[](const auto& change) {
        const std::string_view id=change.parameter;
        return id=="duck.body" || id=="duck.punch" || id=="duck.click";
    }), "Auto Mix proposed inactive legacy Duck coefficients for Low Band");
    const double slowRelease = analyzeDuckTiming(audio,sr).releaseMs;
    auto fastNotes=audio;
    for (size_t n=0; n<fastNotes.size(); ++n) {
        auto& f=fastNotes[n];
        const double hit=std::fmod(n/sr,.5);
        if (hit>=.088 && hit<.125) f.left=f.right=0;
    }
    const auto fastTiming=analyzeDuckTiming(fastNotes,sr);
    require(fastTiming.releaseMs>=20 && fastTiming.releaseMs<slowRelease,
            "Duck did not shorten release before an audible bass onset");
    std::cout<<"PASS Auto Mix Duck release "<<slowRelease<<" ms for sustained bass, "
             <<fastTiming.releaseMs<<" ms before the next note\n";
    for (auto& f : audio) { f.kickLeft = f.kickRight = 0; f.right = -f.left; }
    const auto missing = analyzeAutoMix(audio, sr, 120, 2, settings);
    require(missing.hasBass && !missing.hasKick && missing.changes.empty(), "Missing sidechain changed settings");
    require(monoLossSeverity(missing.mono.sub.worst) == MonoLossSeverity::high, "Sub cancellation missed");
    require(std::abs(allPassDegrees(90, 90, sr) + 90) < 1e-9, "Incorrect All-Pass angle at corner");

    std::vector<AutoMixFrame> dualBand(size_t(2*sr));
    for (size_t n=0;n<dualBand.size();++n) {
        const double t=n/sr, low=std::sin(2*pi*38*t), high=std::sin(2*pi*186*t);
        const float bass=float(.45*low+.28*high), kick=float(.35*low+.60*high);
        dualBand[n]={bass,bass,kick,kick,float(2*t)};
    }
    const auto suggestedFocus=[&](const std::vector<AutoMixFrame>& data, Settings setup) {
        setup.unmaskTrack=false;
        const auto mix=analyzeAutoMix(data,sr,120,1,setup);
        for (const auto& change:mix.changes)
            if (std::string_view(change.parameter)=="unmask.frequency") return change.value;
        return 0.f;
    };
    Settings selective;
    selective.glue=selective.phase=false;
    selective.duckMultiband=true;selective.duckLowHz=250;
    selective.duckAmountDb=18;selective.duckAttackMs=0;
    selective.unmaskDirect=true;selective.unmaskAttenuationDb=12;
    for (bool duck:{false,true}) {
        selective.duck=duck;
        Engine live;
        live.prepare(sr);
        std::array<float,256> bass{},kick{};
        float* output[]{bass.data()};const float* reference[]{kick.data()};
        for (size_t start=0;start<dualBand.size();start+=bass.size()) {
            const auto count=std::min(bass.size(),dualBand.size()-start);
            for (size_t n=0;n<count;++n) {
                bass[n]=dualBand[start+n].left;
                kick[n]=dualBand[start+n].kickLeft;
            }
            live.process(output,1,reference,1,int(count),selective,120);
        }
        const auto expected=duck?186:38;
        require(std::abs(live.getMeters().focusHz-expected)<3,"Auto Focus selected a peak from before Duck");
        require(suggestedFocus(dualBand,selective)==expected,"Auto Mix focus failed to follow the strongest post-Duck band");
    }
    for (size_t n=0;n<dualBand.size();++n) {
        const double t=n/sr;
        const float sine=float(std::sin(2*pi*75*t));
        dualBand[n]={.17f*sine,.17f*sine,.4f*sine,.4f*sine,float(2*t)};
    }
    selective.duckMultiband=false;selective.unmask=false;selective.unmaskFrequency=60;
    selective.duck=false;
    require(suggestedFocus(dualBand,selective)==75,"Auto Mix missed the unattenuated shared tone");
    selective.duck=true;
    require(suggestedFocus(dualBand,selective)==0,"Auto Mix proposed a cut already taken care of by Duck");
    std::cout<<"PASS after Duck: live and Auto Mix focus 38 -> 186 Hz; Auto Mix skipped the cleared band\n";

    Settings balance;
    balance.glue=balance.unmask=false;
    balance.outputDb=-9;balance.balanceTrimDb=-3;
    const auto wet=analyzeAutoMix(dualBand,sr,120,2,balance);
    for (int monitor : {0,1,2,3}) {
        auto audition=balance;
        audition.auditionDry=monitor==0;
        audition.matchEnabled=monitor==1;
        audition.unmask=audition.delta=monitor==2;
        audition.tube.enabled=audition.tube.audition=monitor==3;
        auto normal=audition;
        normal.auditionDry=normal.matchEnabled=normal.delta=normal.tube.audition=false;
        const auto baseline=monitor<2 ? wet : analyzeAutoMix(dualBand,sr,120,2,normal);
        const auto measured=analyzeAutoMix(dualBand,sr,120,2,audition);
        require(measured.balanceValid && measured.balanceSubDb==baseline.balanceSubDb &&
                measured.balanceBodyDb==baseline.balanceBodyDb && measured.sumPeakDb==baseline.sumPeakDb,
                "Auto Mix Balance measured an audition instead of the processed chain");
    }
    for (auto& f : dualBand) {f.right=-f.left;f.kickRight=-f.kickLeft;}
    const auto stereoOverlap=analyzeAutoMix(dualBand,sr,120,2,balance);
    require(stereoOverlap.overlapValid && stereoOverlap.overlapBefore>10,
            "Auto Mix lost a valid stereo overlap to a mono fold");
    for (auto& f : dualBand) {f.right=f.left;f.kickLeft=0;}
    const auto monoBass=analyzeAutoMix(dualBand,sr,120,1,balance);
    require(monoBass.balanceValid,"Auto Mix missed the right sidechain on a mono bass bus");
    std::cout<<"PASS processed Balance across monitor modes and stereo overlap\n";

    std::vector<AutoMixFrame> phaseSource(size_t(4*sr));
    std::mt19937 random(639);
    std::uniform_real_distribution<float> noise(-.45f,.45f);
    const auto lag=size_t(std::lround(.002*sr));
    for (size_t n=0;n<phaseSource.size();++n) {
        const double t=n/sr, age=std::fmod(t,.5);
        const float bass=noise(random)*float(.8*std::exp(-age/.1))+.12f*float(std::sin(2*pi*60*t));
        const float kick=n>=lag ? -phaseSource[n-lag].left : 0;
        phaseSource[n]={bass,bass,kick,kick,float(2*t)};
    }
    Settings phase;
    phase.glue=phase.unmask=false;
    phase.rotate=true;phase.phaseAngle=165;
    const auto alignment=analyzeAutoMix(phaseSource,sr,120,1,phase);
    const auto proposed=[&](std::string_view id) {
        for (const auto& change : alignment.changes)
            if (id==change.parameter) return change.value;
        return -1.f;
    };
    require(alignment.reliablePhase&&proposed("phase.on")==1&&proposed("phase.invert")==1&&
            std::abs(proposed("phase.delay")-2)<.26f&&proposed("phase.angle")==0&&proposed("phase.rotate")==0,
            "Learned Auto Mix alignment retained an incompatible manual rotation");

    const double corner = cornerForAllPassDegrees(-45, 50, sr);
    require(std::abs(allPassDegrees(50, corner, sr) + 45) < 1e-8, "Degree input round trip failed");
    BeatScope scope;
    scope.prepare(sr);
    scope.setPhaseFrequency(50);
    for (size_t n=0; n<size_t(sr*2); ++n) {
        const double t=n/sr;
        const float kick=float(.3*std::sin(2*pi*50*t));
        AnalysisFrame f;
        f.beat=t*2;
        f.kick={kick,kick};
        f.phase={-.8f*kick,.4f*kick};
        f.phaseValid=true;
        scope.tick(f,120,false);
    }
    require(scope.phaseMeasured && scope.phaseDelta>18, "Matched mono comparison missed cancellation");

    Engine engine;
    engine.prepare(sr);
    settings.glue = settings.unmask = false;
    settings.subMono = true;
    std::array<float,128> left{}, right{};
    float* channels[]{left.data(), right.data()};
    double low = 0, midError = 0, high = 0;
    size_t allocations = 0;
    for (double hz : {30., 2400.}) {
        engine.prepare(sr);
        double sideEnergy = 0, sourceEnergy = 0;
        for (int block = 0; block < 180; ++block) {
            for (size_t n = 0; n < left.size(); ++n) {
                const auto side = float(.2 * std::sin(2 * pi * hz * (block * left.size() + n) / sr));
                left[n] = .125f + side;
                right[n] = .125f - side;
                if (block > 60) sourceEnergy += side * side;
            }
            const auto before = allocation::count;
            allocation::watch = true;
            engine.process(channels, 2, nullptr, 0, int(left.size()), settings, 120);
            allocation::watch = false;
            allocations += allocation::count - before;
            if (block <= 60) continue;
            for (size_t n = 0; n < left.size(); ++n) {
                const double side = (left[n] - right[n]) * .5;
                sideEnergy += side * side;
                midError = std::max(midError, std::abs((left[n] + right[n]) * .5 - .125));
            }
        }
        (hz < 100 ? low : high) = 10 * std::log10(sideEnergy / sourceEnergy);
    }
    require(low < -12 && std::abs(high) < .05, "Sub Mono affected the wrong range");
    require(midError < 1e-7 && allocations == 0, "Sub Mono changed Mid or allocated in audio");
    std::cout << "PASS Auto Mix proposal, overlap and mono check / Sub Mono Side 30 Hz "
              << low << " dB, 2.4 kHz " << high << " dB / zero callback allocations\n";
}
