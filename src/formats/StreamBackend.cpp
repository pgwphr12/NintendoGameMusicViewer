#include "StreamBackend.hpp"
extern "C" {
#include <libvgmstream.h>
}
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <fstream>
#include <stdexcept>
namespace ngmv {
namespace {
class StreamBackend final : public IMusicBackend {
    std::unique_ptr<libvgmstream_t, decltype(&libvgmstream_free)> core_{nullptr, libvgmstream_free};
    std::vector<ChannelInfo> channels_;
    std::vector<TrackInfo> tracks_;
    std::deque<std::array<float, 30>> pending_;
    std::vector<float> scratch_;
    std::vector<Stereo> routing_;
    double position_ = 0;
    int rate_ = 0;
    uint32_t mask_ = 0;
    void routing(const std::filesystem::path &path, size_t channels) {
        const float groups = float((channels + 1) / 2);
        routing_.resize(channels);
        for (size_t c = 0; c < channels; ++c)
            routing_[c] = channels == 1 || (channels % 2 && c + 1 == channels)
                              ? Stereo{1 / groups, 1 / groups}
                          : c % 2 ? Stereo{0, 1 / groups}
                                  : Stereo{1 / groups, 0};
        // BCSTM track tables identify stereo pairs and mono stems. Do not assume
        // alternating L/R for a mono drum stem following several stereo pairs.
        std::ifstream file(path, std::ios::binary);
        std::array<uint8_t, 64> header{};
        if (!file.read(reinterpret_cast<char *>(header.data()), header.size()) ||
            std::memcmp(header.data(), "CSTM", 4))
            return;
        auto u32 = [](const uint8_t *p) {
            return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
                   uint32_t(p[3]) << 24;
        };
        unsigned sections = header[16] | unsigned(header[17]) << 8;
        uint32_t offset = 0, size = 0;
        for (unsigned i = 0; i < std::min(sections, 3u); ++i)
            if (header[20 + i * 12] == 0 && header[21 + i * 12] == 0x40) {
                offset = u32(header.data() + 24 + i * 12);
                size = u32(header.data() + 28 + i * 12);
            }
        if (size < 32 || size > 65536)
            return;
        std::vector<uint8_t> info(size);
        file.seekg(offset);
        if (!file.read(reinterpret_cast<char *>(info.data()), size))
            return;
        auto ref = [&](size_t base, size_t field) -> size_t {
            if (field + 4 > size)
                return size;
            return base + uint64_t(u32(info.data() + field));
        };
        size_t table = ref(8, 20);
        if (table + 4 > size)
            return;
        unsigned count = u32(info.data() + table);
        if (!count || count > 30 || table + 4 + count * 8 > size)
            return;
        std::vector<Stereo> candidate(channels);
        std::vector<bool> assigned(channels);
        for (unsigned t = 0; t < count; ++t) {
            size_t track = ref(table, table + 8 + t * 8);
            if (track + 12 > size)
                return;
            size_t indices = ref(track, track + 8);
            if (indices + 4 > size)
                return;
            unsigned n = u32(info.data() + indices);
            if (n < 1 || n > 2 || indices + 4 + n > size)
                return;
            float gain = info[track] / (127.f * count);
            float left = std::min(1.f, (127 - info[track + 1]) / 63.f);
            float right = std::min(1.f, info[track + 1] / 64.f);
            for (unsigned i = 0; i < n; ++i) {
                size_t c = info[indices + 4 + i];
                if (c >= channels || assigned[c])
                    return;
                assigned[c] = true;
                candidate[c] = n == 1 ? Stereo{gain * left, gain * right}
                               : i    ? Stereo{0, gain * right}
                                      : Stereo{gain * left, 0};
            }
        }
        if (std::all_of(assigned.begin(), assigned.end(), [](bool a) { return a; }))
            routing_ = std::move(candidate);
    }
    void fill() {
        scratch_.resize(1024 * channels_.size());
        if (libvgmstream_fill(core_.get(), scratch_.data(), 1024) < 0)
            throw std::runtime_error("Cannot decode 3DS audio stream.");
        for (size_t i = 0; i < 1024; i++) {
            std::array<float, 30> frame{};
            std::copy_n(scratch_.data() + i * channels_.size(), channels_.size(), frame.data());
            pending_.push_back(frame);
        }
    }

  public:
    explicit StreamBackend(const std::filesystem::path &path) {
        using File = std::unique_ptr<libstreamfile_t, decltype(&libstreamfile_close)>;
        File file(libstreamfile_open_from_stdio(path.u8string().c_str()), libstreamfile_close);
        if (!file)
            throw std::runtime_error("Cannot open 3DS audio stream.");
        libvgmstream_config_t config{};
        config.force_sfmt = LIBVGMSTREAM_SFMT_FLOAT;
        config.ignore_loop = true;
        config.ignore_fade = true;
        config.disable_config_override = true;
        core_.reset(libvgmstream_create(file.get(), 0, &config));
        if (!core_)
            throw std::runtime_error("Unsupported or damaged BCSTM/BCWAV codec.");
        const auto &info = *core_->format;
        if (info.channels < 1 || info.channels > 30 || info.sample_rate < 8000 ||
            info.sample_rate > 192000 || info.play_samples <= 0)
            throw std::runtime_error("Invalid 3DS stream channels, rate or duration.");
        rate_ = info.sample_rate;
        routing(path, info.channels);
        for (int c = 0; c < info.channels; c++)
            channels_.push_back({info.channels == 1   ? "MONO"
                                 : info.channels == 2 ? (c ? "RIGHT" : "LEFT")
                                                      : "STREAM " + std::to_string(c + 1)});
        tracks_.push_back({path.stem().u8string(), "", "", "Nintendo 3DS · Audio Stream",
                           info.codec_name,
                           int(std::min(int64_t(2147483647), info.play_samples * 1000 / rate_))});
    }
    const std::vector<TrackInfo> &tracks() const override {
        return tracks_;
    }
    const std::vector<ChannelInfo> &channels() const override {
        return channels_;
    }
    void selectTrack(int index) override {
        if (index != 0)
            throw std::runtime_error("Stream file contains one track.");
        seek(0);
    }
    void muteMask(uint32_t mask) override {
        mask_ = mask;
    }
    void seek(int ms) override {
        if (ms < 0)
            throw std::runtime_error("Invalid stream seek time.");
        libvgmstream_seek(core_.get(), int64_t(ms) * rate_ / 1000);
        pending_.clear();
        position_ = 0;
    }
    void render(size_t frames, AudioBlock &out) override {
        if (frames > 8192)
            throw std::runtime_error("Audio block too large.");
        out.mix.assign(frames, {});
        out.voices.resize(channels_.size());
        for (auto &voice : out.voices)
            voice.assign(frames, {});
        for (size_t i = 0; i < frames; i++) {
            while (pending_.size() < 2)
                fill();
            for (size_t c = 0; c < channels_.size(); c++) {
                float value = float(pending_[0][c] + (pending_[1][c] - pending_[0][c]) * position_);
                if (!std::isfinite(value))
                    throw std::runtime_error("Non-finite stream audio.");
                Stereo voice{value * routing_[c].l, value * routing_[c].r};
                out.voices[c][i] = voice;
                if (!(mask_ & (1u << c))) {
                    out.mix[i].l += voice.l;
                    out.mix[i].r += voice.r;
                }
            }
            position_ += double(rate_) / InternalRate;
            while (position_ >= 1) {
                if (pending_.size() < 2)
                    fill();
                pending_.pop_front();
                position_ -= 1;
            }
        }
    }
};
} // namespace
std::unique_ptr<IMusicBackend> makeStreamBackend(const std::filesystem::path &path) {
    return std::make_unique<StreamBackend>(path);
}
} // namespace ngmv
