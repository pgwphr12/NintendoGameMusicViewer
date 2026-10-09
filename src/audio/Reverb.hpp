#pragma once
#include "core/Models.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace ngmv {
// Damped room reflections, processed on the fixed-48k producer thread.
class Reverb {
    struct Comb {
        std::vector<Stereo> delay;
        size_t position = 0;
        Stereo filtered;
        float feedback = 0;
        explicit Comb(size_t length) : delay(length) {}
    };
    std::array<Comb, 4> combs_{{Comb(1423), Comb(1789), Comb(2131), Comb(2557)}};
    bool enabled_ = false;
    float wet_ = 0;
    float amount_ = .45f;
    float decaySeconds_ = 0;

  public:
    Reverb() {
        decay(1.2f);
    }
    // Approximate low-frequency RT60; damping shortens high-frequency tails.
    void decay(float seconds) {
        seconds = std::clamp(seconds, .1f, 3.f);
        if (seconds == decaySeconds_)
            return;
        decaySeconds_ = seconds;
        for (auto &comb : combs_)
            comb.feedback =
                float(std::pow(10., -3. * comb.delay.size() / (InternalRate * seconds)));
    }
    void reset() {
        for (auto &comb : combs_) {
            std::fill(comb.delay.begin(), comb.delay.end(), Stereo{});
            comb.position = 0;
            comb.filtered = {};
        }
        wet_ = 0;
    }
    void enabled(bool value) {
        if (value && !enabled_)
            reset();
        enabled_ = value;
    }
    void amount(float value) {
        amount_ = std::clamp(value, 0.f, 1.f);
    }
    Stereo process(Stereo input) {
        const float target = enabled_ ? amount_ : 0.f;
        wet_ += std::clamp(target - wet_, -.0005f, .0005f);
        if (!enabled_ && wet_ == 0)
            return input;
        Stereo reflected;
        for (auto &comb : combs_) {
            Stereo delayed{comb.delay[comb.position].l,
                           comb.delay[(comb.position + 37) % comb.delay.size()].r};
            comb.filtered.l += .3f * (delayed.l - comb.filtered.l);
            comb.filtered.r += .3f * (delayed.r - comb.filtered.r);
            comb.delay[comb.position] = {input.l + comb.feedback * comb.filtered.l,
                                         input.r + comb.feedback * comb.filtered.r};
            comb.position = (comb.position + 1) % comb.delay.size();
            reflected.l += delayed.l * .25f;
            reflected.r += delayed.r * .25f;
        }
        return {input.l + reflected.l * wet_, input.r + reflected.r * wet_};
    }
};
} // namespace ngmv
