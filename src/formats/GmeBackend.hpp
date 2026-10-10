#pragma once
#include "core/IMusicBackend.hpp"
#include <gme.h>
namespace ngmv {
std::unique_ptr<IMusicBackend> makeVgmBackend(const std::vector<uint8_t> &bytes);
class GmeBackend : public IMusicBackend {
    struct Delete {
        void operator()(Music_Emu *p) const {
            gme_delete(p);
        }
    };
    using Core = std::unique_ptr<Music_Emu, Delete>;
    Core full_;
    std::vector<Core> isolated_;
    std::vector<TrackInfo> tracks_;
    std::vector<ChannelInfo> channels_;
    std::vector<short> scratch_;
    int track_ = 0;
    Core create(const std::vector<uint8_t> &data);

  public:
    GmeBackend(const std::vector<uint8_t> &bytes, Format format);
    const std::vector<TrackInfo> &tracks() const override {
        return tracks_;
    }
    const std::vector<ChannelInfo> &channels() const override {
        return channels_;
    }
    void selectTrack(int index) override;
    void seek(int milliseconds) override;
    void muteMask(uint32_t mask) override {
        gme_mute_voices(full_.get(), int(mask));
    }
    void render(size_t frames, AudioBlock &out) override;
};
class NSFBackend final : public GmeBackend {
  public:
    explicit NSFBackend(const std::vector<uint8_t> &b) : GmeBackend(b, Format::Nsf) {}
};
class NSFEBackend final : public GmeBackend {
  public:
    explicit NSFEBackend(const std::vector<uint8_t> &b) : GmeBackend(b, Format::Nsfe) {}
};
class SPCBackend final : public GmeBackend {
  public:
    explicit SPCBackend(const std::vector<uint8_t> &b) : GmeBackend(b, Format::Spc) {}
};
class GBSBackend final : public GmeBackend {
  public:
    explicit GBSBackend(const std::vector<uint8_t> &b) : GmeBackend(b, Format::Gbs) {}
};
} // namespace ngmv
