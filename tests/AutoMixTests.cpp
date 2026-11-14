#include "AutoMix.h"
#include "Analyzer.h"
#include "RealtimeAllocation.h"
#include <iostream>
#include <stdexcept>

using namespace bassdeamon;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    constexpr double sr = 44100;
    std::vector<AutoMixFrame> audio(size_t(8 * sr));
    Settings settings;
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
    for (auto& f : audio) { f.kickLeft = f.kickRight = 0; f.right = -f.left; }
    const auto missing = analyzeAutoMix(audio, sr, 120, 2, settings);
    require(missing.hasBass && !missing.hasKick && missing.changes.empty(), "Missing sidechain changed settings");
    require(monoLossSeverity(missing.mono.sub.worst) == MonoLossSeverity::high, "Sub cancellation missed");
    require(std::abs(allPassDegrees(90, 90, sr) + 90) < 1e-9, "Incorrect All-Pass angle at corner");

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
