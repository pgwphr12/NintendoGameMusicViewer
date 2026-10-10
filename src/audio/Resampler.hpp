#pragma once
#include "StereoRing.hpp"
#include <array>
#include <cmath>
#include <stdexcept>
namespace ngmv {
class OutputResampler {
    double position_ = 0, step_ = 1;
    double kernelStep_ = 0;
    std::array<Stereo, 16> past_{};
    std::array<std::array<float, 32>, 257> kernels_{};
    void prepare(double step) {
        if (kernelStep_ == step)
            return;
        kernelStep_ = step;
        constexpr double pi = 3.14159265358979323846;
        const double cutoff = .94 / std::max(1., step);
        for (size_t phase = 0; phase < kernels_.size(); ++phase) {
            double sum = 0, t = double(phase) / 256;
            for (int k = 0; k < 32; ++k) {
                double x = k - 15 - t;
                double sinc = std::abs(x) < 1e-12 ? cutoff : std::sin(pi * cutoff * x) / (pi * x);
                double window = .42 + .5 * std::cos(pi * x / 17) + .08 * std::cos(2 * pi * x / 17);
                sum += kernels_[phase][k] = float(sinc * window);
            }
            for (auto &weight : kernels_[phase])
                weight = float(weight / sum);
        }
    }

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
        past_ = {};
    }
    template <size_t N>
    void pull(StereoRing<N> &ring, float *dest, size_t frames, float master, double speed = 1) {
        const double step = step_ * speed;
        prepare(step);
        for (size_t i = 0; i < frames; i++) {
            Stereo a, b;
            size_t advance = size_t(std::floor(position_ + step));
            if (ring.available() < std::max(size_t(2), advance) || !ring.peek(0, a) ||
                !ring.peek(1, b)) {
                dest[2 * i] = dest[2 * i + 1] = 0;
                underruns++;
                continue;
            }
            Stereo value{};
            if (step == 1 && position_ == 0)
                value = a;
            else {
                double phase = position_ * 256;
                auto p = size_t(phase);
                float blend = float(phase - p);
                const size_t available = ring.available();
                for (int k = 0; k < 32; ++k) {
                    int offset = k - 15;
                    Stereo sample{};
                    if (offset < 0)
                        sample = past_[16 + offset];
                    else
                        ring.peek(std::min(size_t(offset), available - 1), sample);
                    float weight = kernels_[p][k] + (kernels_[p + 1][k] - kernels_[p][k]) * blend;
                    value.l += sample.l * weight;
                    value.r += sample.r * weight;
                }
            }
            dest[2 * i] = value.l * master;
            dest[2 * i + 1] = value.r * master;
            position_ += step;
            auto n = size_t(std::floor(position_));
            position_ -= n;
            for (size_t j = 0; j < n; ++j) {
                std::move(past_.begin() + 1, past_.end(), past_.begin());
                ring.peek(j, past_.back());
            }
            ring.consume(n);
            consumed += n;
        }
    }
};
} // namespace ngmv
