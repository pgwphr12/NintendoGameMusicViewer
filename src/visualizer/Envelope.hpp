#pragma once
#include <vector>
namespace ngmv {
struct EnvelopePoint {
    float minimum = 0, maximum = 0, center = 0;
};
std::vector<EnvelopePoint> envelope(const std::vector<float> &samples, size_t columns,
                                    size_t window = 2048);
} // namespace ngmv
