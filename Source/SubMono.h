#pragma once

#include <cmath>

namespace bassdeamon {

// A complementary split in Mid/Side: subtracting this low-pass leaves a
// first-order high-pass on Side while preserving Mid exactly.
class SubMono {
public:
    void setCutoff(double frequency, double rate) noexcept {
        if (frequency == cutoff) return;
        cutoff = frequency;
        const double t = std::tan(3.14159265358979323846 * frequency / rate);
        feed = t / (1 + t);
        feedback = (t - 1) / (t + 1);
    }
    double lowSide(double side) noexcept {
        const double input = feed * side;
        const double output = input + state;
        state = input - feedback * output;
        if (std::abs(state) < 1e-25) state = 0;
        return output;
    }
    void clear() noexcept { state = 0; }
private:
    double feed = 0, feedback = 0, state = 0, cutoff = -1;
};

} // namespace bassdeamon
