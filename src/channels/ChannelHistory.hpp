#pragma once
#include "core/Models.hpp"
#include <algorithm>
#include <mutex>
namespace ngmv {
class ChannelHistory {
    static constexpr size_t Capacity = 131072;
    mutable std::mutex mutex_;
    std::vector<std::vector<float>> data_;
    uint64_t end_ = 0, base_ = 0;

  public:
    void reset(size_t channels, uint64_t start = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.assign(channels, std::vector<float>(Capacity, 0));
        end_ = base_ = start;
    }
    void append(const AudioBlock &block) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t ch = 0; ch < data_.size(); ch++)
            for (size_t i = 0; i < block.voices[ch].size(); i++)
                data_[ch][(end_ + i) % Capacity] =
                    (block.voices[ch][i].l + block.voices[ch][i].r) * .5f;
        end_ += block.mix.size();
    }
    std::vector<std::vector<float>> snapshot(uint64_t consumed, size_t frames = 4096) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::vector<float>> out(data_.size(), std::vector<float>(frames, 0));
        uint64_t endpoint = std::min(consumed, end_);
        uint64_t oldest = std::max(base_, end_ > Capacity ? end_ - Capacity : uint64_t(0));
        for (size_t j = 0; j < frames; j++) {
            if (endpoint + j < frames)
                continue;
            uint64_t p = endpoint + j - frames;
            if (p < oldest || p >= end_)
                continue;
            for (size_t ch = 0; ch < data_.size(); ch++)
                out[ch][j] = data_[ch][p % Capacity];
        }
        return out;
    }
};
} // namespace ngmv
