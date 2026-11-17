#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace bassdeamon {
class KickEqCapture {
public:
    void start(double sampleRate) noexcept {
        count = 0;
        position = 0;
        peakLevel = 0;
        stride = std::max(1, int(std::round(sampleRate / 2000.0)));
        rate = sampleRate / stride;
        pole = std::exp(-6.283185307179586 * 260 / sampleRate);
        filter = {};
    }

    bool push(const float* const* kick, int channels, int samples) noexcept {
        for (int n = 0; n < samples && count < capacity; ++n) {
            for (int c = 0; c < channels; ++c) {
                const double sample = std::isfinite(kick[c][n]) ? kick[c][n] : 0;
                filter[c][0] = sample + pole * (filter[c][0] - sample);
                filter[c][1] = filter[c][0] + pole * (filter[c][1] - filter[c][0]);
            }
            if (++position == stride) {
                for (int c = 0; c < 2; ++c) {
                    audio[c][count] = float(filter[c][1]);
                    peakLevel = std::max(peakLevel, std::abs(audio[c][count]));
                }
                position = 0;
                ++count;
            }
        }
        return count == capacity;
    }

    float progress() const noexcept { return float(count)/capacity; }
    bool hasSignal() const noexcept { return peakLevel >= .0015f; }

    // Called only after capture, on the message thread. Compare multiple
    // decaying tails and reject a result that moves between or within hits.
    double frequency() const noexcept {
        std::array<double, 2> energy{};
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < count; ++n) energy[c] += double(audio[c][n]) * audio[c][n];
        const auto& signal = audio[energy[1] > energy[0] ? 1 : 0];
        double peak = 0;
        for (int n = 0; n < count; ++n) peak = std::max(peak, std::abs(double(signal[n])));
        if (peak < .0015) return 0;

        const int refractory = int(rate * .18);
        const int minLag = int(rate / 130);
        const int maxLag = int(std::ceil(rate / 30));
        const int window = int(rate * .043);
        const int tailOffset = int(rate * .004);
        const int secondOffset = int(rate * .022);
        const int settledOffset = int(rate * .115);
        const int end = count - settledOffset - secondOffset - window - maxLag - 2;
        std::array<double, 24> estimates{};
        int accepted = 0, hits = 0, nextHit = 0;
        double envelope = 0;
        const double envelopePole = std::exp(-1 / (.007 * rate));
        bool armed = true;
        for (int n = 0; n < end; ++n) {
            envelope = std::abs(signal[n]) + envelopePole * (envelope - std::abs(signal[n]));
            if (envelope < peak * .065) armed = true;
            if (!armed || n < nextHit || envelope < peak * .16) continue;
            armed = false;
            nextHit = n + refractory;
            ++hits;
            const auto estimate = [&](int first) {
                std::array<double, 128> scores{};
                double power = 0;
                for (int i = first; i < first + window; ++i) power += double(signal[i]) * signal[i];
                if (power / window < std::max(1e-8, peak * peak * .00015)) return 0.0;
                for (int lag = minLag - 1; lag <= maxLag + 1; ++lag) {
                    double xy = 0, yy = 0;
                    for (int i = first; i < first + window; ++i) {
                        xy += double(signal[i]) * signal[i + lag];
                        yy += double(signal[i + lag]) * signal[i + lag];
                    }
                    if (yy / window >= std::max(1e-9, peak * peak * .000025))
                        scores[lag] = xy / std::sqrt(std::max(1e-20, power * yy));
                }
                // Pick the first confident period, avoiding octave-down multiples.
                for (int lag = minLag; lag <= maxLag; ++lag) {
                    const double a = scores[lag - 1], b = scores[lag], c = scores[lag + 1];
                    if (b < .85 || b <= a || b <= c) continue;
                    const double offset = .5 * (a - c) / (a - 2 * b + c);
                    const double hz = rate / (lag + offset);
                    return hz >= 30 && hz <= 130 ? hz : 0.0;
                }
                return 0.0;
            };
            // A repeatable kick can have a steep pitch sweep at its onset.
            // Prefer the settled tail; use early windows for short samples.
            const double settled = estimate(n + settledOffset);
            const double settledNext = estimate(n + settledOffset + secondOffset);
            if (settled && settledNext && std::abs(settled - settledNext) <= settled * .09) {
                estimates[accepted++] = (settled + settledNext) * .5;
                continue;
            }
            const double early = estimate(n + tailOffset);
            const double late = estimate(n + tailOffset + secondOffset);
            if (early && (!late || std::abs(early - late) <= early * .07))
                estimates[accepted++] = late ? (early + late) * .5 : early;
        }
        if (accepted < 2 || accepted * 2 < hits) return 0;
        std::sort(estimates.begin(), estimates.begin() + accepted);
        const double median = estimates[accepted / 2];
        const auto endOfEstimates = estimates.begin() + accepted;
        const auto agrees = std::count_if(estimates.begin(), endOfEstimates, [median](double hz) {
            return std::abs(hz - median) <= median * .06;
        });
        // A single different hit should not veto a clearly recurring pitch.
        if (agrees < 2 || 3 * agrees < 2 * accepted || 2 * agrees < hits)
            return 0;
        return median;
    }

private:
    static constexpr int capacity = 8000;
    std::array<std::array<float, capacity>, 2> audio{};
    std::array<std::array<double, 2>, 2> filter{};
    double rate = 2000, pole = 0;
    float peakLevel = 0;
    int stride = 1, position = 0, count = 0;
};
}
