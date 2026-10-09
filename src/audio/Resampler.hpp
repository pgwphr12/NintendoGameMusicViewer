#pragma once
#include "StereoRing.hpp"
#include <cmath>
#include <stdexcept>
namespace ngmv {
class OutputResampler {
    double position_ = 0, step_ = 1;

  public:
    uint64_t consumed = 0, underruns = 0;
    void rate(int rate) {
        if (rate < 8000 || rate > 192000)
            throw std::runtime_error("Unsupported device rate.");
        step_ = double(InternalRate) / rate;
    }
    void reset() {
        position_ = 0;
        consumed = 0;
        underruns = 0;
    }
    template <size_t N>
    void pull(StereoRing<N> &ring, float *dest, size_t frames, float master, double speed = 1) {
        const double step = step_ * speed;
        for (size_t i = 0; i < frames; i++) {
            Stereo a, b;
            size_t advance = size_t(std::floor(position_ + step));
            if (ring.available() < std::max(size_t(2), advance) || !ring.peek(0, a) ||
                !ring.peek(1, b)) {
                dest[2 * i] = dest[2 * i + 1] = 0;
                underruns++;
                continue;
            }
            float t = float(position_);
            dest[2 * i] = (a.l + (b.l - a.l) * t) * master;
            dest[2 * i + 1] = (a.r + (b.r - a.r) * t) * master;
            position_ += step;
            auto n = size_t(std::floor(position_));
            position_ -= n;
            ring.consume(n);
            consumed += n;
        }
    }
};
} // namespace ngmv
