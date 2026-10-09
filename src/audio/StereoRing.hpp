#pragma once
#include "core/Models.hpp"
#include <algorithm>
#include <array>
#include <atomic>
namespace ngmv {
// One producer (music worker), one consumer (device callback). Reset only while both stopped.
template <size_t Capacity = 16384> class StereoRing {
    std::array<Stereo, Capacity> data_{};
    std::atomic<uint64_t> write_{0}, read_{0};

  public:
    size_t available() const {
        return size_t(write_.load(std::memory_order_acquire) -
                      read_.load(std::memory_order_acquire));
    }
    size_t free() const {
        return Capacity - available();
    }
    bool push(const Stereo *data, size_t n) {
        auto w = write_.load(std::memory_order_relaxed);
        if (n > Capacity - (w - read_.load(std::memory_order_acquire)))
            return false;
        for (size_t i = 0; i < n; i++)
            data_[(w + i) % Capacity] = data[i];
        write_.store(w + n, std::memory_order_release);
        return true;
    }
    bool peek(size_t offset, Stereo &out) const {
        auto r = read_.load(std::memory_order_relaxed);
        if (r + offset >= write_.load(std::memory_order_acquire))
            return false;
        out = data_[(r + offset) % Capacity];
        return true;
    }
    void consume(size_t n) {
        read_.fetch_add(n, std::memory_order_release);
    }
    void reset() {
        write_ = 0;
        read_ = 0;
    }
};
} // namespace ngmv
