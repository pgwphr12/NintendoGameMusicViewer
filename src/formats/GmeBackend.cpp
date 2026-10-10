#include "GmeBackend.hpp"
#include <algorithm>
#include <climits>
#include <stdexcept>
namespace ngmv {
static void check(gme_err_t e) {
    if (e)
        throw std::runtime_error(e);
}
GmeBackend::Core GmeBackend::create(const std::vector<uint8_t> &bytes) {
    Music_Emu *p = nullptr;
    check(gme_open_data(bytes.data(), long(bytes.size()), &p, InternalRate));
    Core core(p);
    gme_ignore_silence(p, 1);
    gme_set_autoload_playback_limit(p, 0);
    gme_enable_accuracy(p, 1);
    return core;
}
GmeBackend::GmeBackend(const std::vector<uint8_t> &b, Format format) {
    full_ = create(b);
    int count = gme_voice_count(full_.get());
    if (count < 1 || count > 30)
        throw std::runtime_error("Unsupported voice count.");
    for (int i = 0; i < gme_track_count(full_.get()); i++) {
        gme_info_t *raw = nullptr;
        check(gme_track_info(full_.get(), &raw, i));
        std::unique_ptr<gme_info_t, decltype(&gme_free_info)> info(raw, gme_free_info);
        tracks_.push_back(
            {info->song, info->game, info->author, info->system, info->comment, info->length});
    }
    if (tracks_.empty())
        throw std::runtime_error("No tracks in file.");
    for (int i = 0; i < count; i++) {
        std::string name = gme_voice_name(full_.get(), i);
        if (format == Format::Nsf || format == Format::Nsfe) {
            const char *basic[] = {"PULSE 1", "PULSE 2", "TRIANGLE", "NOISE", "DMC"};
            if (i < 5)
                name = basic[i];
            else if (name == "Wave")
                name = "FDS WAVE";
            else if (name == "FM")
                name = "FM / EXPANSION";
        } else if (format == Format::Spc)
            name = "VOICE " + std::to_string(i + 1);
        else if (format == Format::Gbs) {
            const char *gb[] = {"SQUARE 1", "SQUARE 2", "WAVE", "NOISE"};
            if (i < 4)
                name = gb[i];
        }
        channels_.push_back({name});
        auto c = create(b);
        uint32_t all = (uint32_t(1) << count) - 1;
        gme_mute_voices(c.get(), int(all & ~(uint32_t(1) << i)));
        isolated_.push_back(std::move(c));
    }
    selectTrack(0);
}
void GmeBackend::selectTrack(int index) {
    if (index < 0 || index >= int(tracks_.size()))
        throw std::runtime_error("Invalid track index.");
    check(gme_start_track(full_.get(), index));
    for (auto &c : isolated_)
        check(gme_start_track(c.get(), index));
    track_ = index;
}
void GmeBackend::seek(int ms) {
    if (ms < 0)
        throw std::runtime_error("Invalid seek time.");
    check(gme_seek(full_.get(), ms));
    for (auto &c : isolated_)
        check(gme_seek(c.get(), ms));
}
void GmeBackend::render(size_t frames, AudioBlock &out) {
    if (frames > 8192)
        throw std::runtime_error("Audio block too large.");
    scratch_.resize(frames * 2);
    out.mix.resize(frames);
    out.voices.resize(isolated_.size());
    auto renderCore = [&](Music_Emu *c, std::vector<Stereo> &dst) {
        dst.resize(frames);
        check(gme_play(c, int(frames * 2), scratch_.data()));
        for (size_t j = 0; j < frames; j++)
            dst[j] = {scratch_[j * 2] / 32768.f, scratch_[j * 2 + 1] / 32768.f};
    };
    renderCore(full_.get(), out.mix);
    for (size_t i = 0; i < isolated_.size(); i++)
        renderCore(isolated_[i].get(), out.voices[i]);
}
} // namespace ngmv
