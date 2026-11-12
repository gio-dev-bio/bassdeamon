#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace bassdeamon {

struct MonoCompatibility {
    double correlation = 0;
    double foldLossDb = 0;
    bool active = false;
    bool correlationValid = false;
};

class StereoPower {
public:
    void add(double left, double right) noexcept {
        leftPower += left * left;
        rightPower += right * right;
        crossPower += left * right;
        ++sampleCount;
    }

    void merge(const StereoPower& other) noexcept {
        leftPower += other.leftPower;
        rightPower += other.rightPower;
        crossPower += other.crossPower;
        sampleCount += other.sampleCount;
    }

    double meanPower() const noexcept {
        return sampleCount ? (leftPower + rightPower) / (2. * sampleCount) : 0.;
    }

    MonoCompatibility measure() const noexcept {
        MonoCompatibility result;
        if (meanPower() <= 1e-9) return result;

        result.active = true;
        result.correlationValid = leftPower > 1e-9 * sampleCount && rightPower > 1e-9 * sampleCount;
        if (result.correlationValid)
            result.correlation = std::clamp(crossPower / std::sqrt(leftPower * rightPower), -1., 1.);

        const double stereoPower = leftPower + rightPower;
        const double ratio = (stereoPower + 2 * crossPower) / (2 * stereoPower);
        result.foldLossDb = 10 * std::log10(std::clamp(ratio, 1e-8, 1.));
        return result;
    }

private:
    double leftPower = 0, rightPower = 0, crossPower = 0;
    std::size_t sampleCount = 0;
};

enum class MonoLossSeverity { noSignal, low, caution, high };

inline MonoLossSeverity monoLossSeverity(const MonoCompatibility& measurement) noexcept {
    if (!measurement.active) return MonoLossSeverity::noSignal;
    if (measurement.foldLossDb <= -8.) return MonoLossSeverity::high;
    if (measurement.foldLossDb <= -4.5) return MonoLossSeverity::caution;
    return MonoLossSeverity::low;
}

struct StereoPhaseBandReport {
    MonoCompatibility overall, worst;
    std::size_t worstBeat = 0, affectedBeats = 0, measuredBeats = 0;
};

struct StereoPhaseReport {
    StereoPhaseBandReport fullBand, sub;
    double subFrequencyHz = 120.;
};

// Offline analysis of one bass version. The caller supplies matching complete
// beats for Dry and Auto Mix, using host musical timing. No sidechain samples.
class StereoPhaseAnalysis {
public:
    void prepare(double sampleRate, double subFrequencyHz = 120.) noexcept {
        *this = StereoPhaseAnalysis{};
        if (!std::isfinite(sampleRate) || sampleRate < 1000.) return;
        if (std::isfinite(subFrequencyHz))
            subHz = std::clamp(subFrequencyHz, 20., std::min(250., sampleRate * .25));

        constexpr double pi = 3.14159265358979323846;
        const double omega = 2 * pi * subHz / sampleRate;
        const double cosine = std::cos(omega);
        const double alpha = std::sin(omega) / std::sqrt(2.);
        b0 = (1 - cosine) / (2 * (1 + alpha));
        a1 = -2 * cosine / (1 + alpha);
        a2 = (1 - alpha) / (1 + alpha);
        prepared = true;
    }

    void add(double left, double right) noexcept {
        if (!prepared || !std::isfinite(left) || !std::isfinite(right)) return;
        const double subLeft = lowPass(left, filters[0]);
        const double subRight = lowPass(right, filters[1]);
        full.current.add(left, right);
        sub.current.add(subLeft, subRight);
    }

    // Zero-based within the analyzed pass. Invoke off the audio thread.
    void finishBeat(std::size_t beat) {
        full.finish(beat);
        sub.finish(beat);
    }

    StereoPhaseReport report() const {
        return {full.report(), sub.report(), subHz};
    }

private:
    struct Band {
        struct Beat {
            std::size_t index;
            StereoPower power;
        };
        StereoPower current;
        std::vector<Beat> beats;

        void finish(std::size_t index) {
            beats.push_back({index, current});
            current = {};
        }

        StereoPhaseBandReport report() const noexcept {
            StereoPhaseBandReport result;
            StereoPower overall;
            double loudest = 0;
            for (const auto& beat : beats) {
                overall.merge(beat.power);
                loudest = std::max(loudest, beat.power.meanPower());
            }
            result.overall = overall.measure();

            // Ignore tails more than 30 dB below this band's loudest beat.
            for (const auto& beat : beats) {
                if (beat.power.meanPower() < loudest * .001) continue;
                const auto measurement = beat.power.measure();
                if (!measurement.active) continue;

                ++result.measuredBeats;
                if (monoLossSeverity(measurement) >= MonoLossSeverity::caution)
                    ++result.affectedBeats;
                if (!result.worst.active || measurement.foldLossDb < result.worst.foldLossDb) {
                    result.worst = measurement;
                    result.worstBeat = beat.index;
                }
            }
            return result;
        }
    };

    struct FilterState { double z1 = 0, z2 = 0; };

    double lowPass(double sample, FilterState& state) const noexcept {
        const double output = b0 * sample + state.z1;
        state.z1 = 2 * b0 * sample - a1 * output + state.z2;
        state.z2 = b0 * sample - a2 * output;
        return output;
    }

    Band full, sub;
    std::array<FilterState, 2> filters{};
    double subHz = 120., b0 = 0, a1 = 0, a2 = 0;
    bool prepared = false;
};

} // namespace bassdeamon
