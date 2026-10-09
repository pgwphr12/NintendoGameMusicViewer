#include "StreamBackend.hpp"
extern "C" {
#include <libvgmstream.h>
}
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <stdexcept>
namespace ngmv {
namespace {
class StreamBackend final : public IMusicBackend {
    std::unique_ptr<libvgmstream_t, decltype(&libvgmstream_free)> core_{nullptr, libvgmstream_free};
    std::vector<ChannelInfo> channels_;
    std::vector<TrackInfo> tracks_;
    std::deque<std::array<float, 30>> pending_;
    std::vector<float> scratch_;
    double position_ = 0;
    int rate_ = 0;
    uint32_t mask_ = 0;
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
                Stereo voice;
                if (channels_.size() == 1)
                    voice = {value, value};
                else if (c % 2 == 0)
                    voice = {value / float((channels_.size() + 1) / 2), 0};
                else
                    voice = {0, value / float(channels_.size() / 2)};
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
