#pragma once
#include "core/Models.hpp"
#include <algorithm>
#include <cmath>
namespace ngmv {
// Fixed format trim followed by a stereo-linked peak limiter, before master volume.
// No silence detection/automatic makeup gain: quiet passages stay quiet.
class OutputLevel {
    float gain_ = 1, attenuation_ = 1;
    const float release_ = 1.f - std::exp(-1.f / (InternalRate * .15f));

  public:
    void configure(Format format, bool fds) {
        gain_ = format == Format::Spc ? std::pow(10.f, 10.f / 20.f)
                : fds                 ? std::pow(10.f, 4.f / 20.f)
                                      : 1.f;
        reset();
    }
    void reset() {
        attenuation_ = 1;
    }
    float gain() const {
        return gain_;
    }
    Stereo process(Stereo value) {
        value.l *= gain_;
        value.r *= gain_;
        const float peak = std::max(std::abs(value.l), std::abs(value.r));
        const float target = peak > .98f ? .98f / peak : 1.f;
        if (target < attenuation_)
            attenuation_ = target;
        else
            attenuation_ += (target - attenuation_) * release_;
        return {value.l * attenuation_, value.r * attenuation_};
    }
};
} // namespace ngmv
