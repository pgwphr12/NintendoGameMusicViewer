#pragma once
#include "core/Models.hpp"
extern "C" {
#include <usf.h>
}
#include <algorithm>
#include <array>
#include <deque>
#include <vector>

namespace ngmv {
// ABI1/NAudio envelope-state addresses identify synthesizer slots, not MIDI instruments.
// Shadow dry buffers follow the same interleave/save/DMA path as the original samples.
class UsfVoiceTap {
  public:
    static constexpr size_t Voices = 30;
    using Frame = std::array<Stereo, Voices + 2>; // full mix, dry slots, residual effects
    usf_voice_tap callbacks{};
    std::deque<Frame> pending;
    size_t used = 0;
    bool supported = false;

  private:
    std::array<uint32_t, Voices> keys_{};
    using Shadow = std::array<std::array<float, 2048>, Voices>;
    Shadow shadow_{};
    struct Saved {
        uint32_t address = 0;
        unsigned count = 0;
        uint64_t serial = 0;
        Shadow samples{};
    };
    std::vector<Saved> saved_{32};
    uint64_t serial_ = 0;
    size_t slot(uint32_t key) {
        for (size_t i = 0; i < used; ++i)
            if (keys_[i] == key)
                return i;
        if (used == Voices)
            return Voices; // excess slots remain in the residual
        keys_[used] = key;
        return used++;
    }

  public:
    UsfVoiceTap() {
        callbacks.context = this;
        callbacks.supported = [](void *p) { static_cast<UsfVoiceTap *>(p)->supported = true; };
        callbacks.move = [](void *p, unsigned out, unsigned in, unsigned bytes) {
            auto &self = *static_cast<UsfVoiceTap *>(p);
            if ((out | in | bytes) & 1 || out + bytes > 4096 || in + bytes > 4096)
                return;
            for (auto &v : self.shadow_)
                for (unsigned i = 0; i < bytes / 2; ++i)
                    v[out / 2 + i] = v[in / 2 + i];
        };
        callbacks.add = [](void *p, unsigned out, unsigned in, unsigned bytes, float gain) {
            auto &self = *static_cast<UsfVoiceTap *>(p);
            if ((out | in | bytes) & 1 || out + bytes > 4096 || in + bytes > 4096)
                return;
            for (auto &v : self.shadow_)
                for (unsigned i = 0; i < bytes / 2; ++i)
                    v[out / 2 + i] += v[in / 2 + i] * gain;
        };
        callbacks.clear = [](void *p, unsigned dmem, unsigned bytes) {
            auto &self = *static_cast<UsfVoiceTap *>(p);
            if ((dmem | bytes) & 1 || dmem + bytes > 4096)
                return;
            for (auto &v : self.shadow_)
                std::fill_n(v.begin() + dmem / 2, bytes / 2, 0.f);
        };
        callbacks.mix = [](void *p, uint32_t key, unsigned left, unsigned right, unsigned index,
                           float l, float r) {
            auto &self = *static_cast<UsfVoiceTap *>(p);
            auto c = self.slot(key);
            if (c == Voices || left / 2 + index >= 2048 || right / 2 + index >= 2048)
                return;
            self.shadow_[c][left / 2 + index] += l;
            self.shadow_[c][right / 2 + index] += r;
        };
        callbacks.interleave = [](void *p, unsigned out, unsigned left, unsigned right,
                                  unsigned bytes) {
            auto &self = *static_cast<UsfVoiceTap *>(p);
            if ((out | left | right | bytes) & 3 || out + bytes * 2 > 4096 || left + bytes > 4096 ||
                right + bytes > 4096)
                return;
            for (auto &v : self.shadow_) {
                // Copy inputs first: output and input DMEM ranges may overlap.
                auto input = v;
                for (unsigned i = 0; i < bytes / 4; ++i) {
                    v[out / 2 + i * 4] = input[right / 2 + i * 2 + 1];
                    v[out / 2 + i * 4 + 1] = input[left / 2 + i * 2 + 1];
                    v[out / 2 + i * 4 + 2] = input[right / 2 + i * 2];
                    v[out / 2 + i * 4 + 3] = input[left / 2 + i * 2];
                }
            }
        };
        callbacks.save = [](void *p, unsigned dmem, uint32_t address, unsigned bytes) {
            auto &self = *static_cast<UsfVoiceTap *>(p);
            if ((dmem | bytes) & 1 || dmem + bytes > 4096 || bytes == 0)
                return;
            auto &record = self.saved_[self.serial_ % self.saved_.size()];
            record.address = address;
            record.count = bytes;
            record.serial = ++self.serial_;
            for (size_t c = 0; c < Voices; ++c)
                std::copy_n(self.shadow_[c].begin() + dmem / 2, bytes / 2,
                            record.samples[c].begin());
        };
        callbacks.dma = [](void *p, uint32_t address, const int16_t *pcm, unsigned frames) {
            auto &self = *static_cast<UsfVoiceTap *>(p);
            for (unsigned i = 0; i < frames; ++i) {
                Frame frame{};
                frame[0] = {pcm[i * 2 + 1] / 32768.f, pcm[i * 2] / 32768.f};
                const Saved *found = nullptr;
                uint32_t current = address + i * 4;
                for (const auto &record : self.saved_)
                    if (record.serial && current >= record.address &&
                        current - record.address + 4 <= record.count &&
                        (!found || record.serial > found->serial))
                        found = &record;
                Stereo sum{};
                if (found) {
                    auto j = (current - found->address) / 2;
                    for (size_t c = 0; c < Voices; ++c) {
                        frame[c + 1] = {found->samples[c][j + 1], found->samples[c][j]};
                        sum.l += frame[c + 1].l;
                        sum.r += frame[c + 1].r;
                    }
                }
                frame.back() = {frame[0].l - sum.l, frame[0].r - sum.r};
                self.pending.push_back(frame);
            }
        };
    }
};
} // namespace ngmv
