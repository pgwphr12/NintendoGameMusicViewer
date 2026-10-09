#pragma once
#include "Models.hpp"
#include <filesystem>
#include <memory>
namespace ngmv {
class IMusicBackend {
  public:
    virtual ~IMusicBackend() = default;
    virtual const std::vector<TrackInfo> &tracks() const = 0;
    virtual const std::vector<ChannelInfo> &channels() const = 0;
    virtual void selectTrack(int index) = 0;
    virtual void seek(int milliseconds) = 0;
    virtual void muteMask(uint32_t mask) = 0;
    virtual void render(size_t frames, AudioBlock &out) = 0;
};
Format identify(const std::vector<uint8_t> &bytes);
std::vector<uint8_t> readMusicFile(const std::filesystem::path &path);
std::unique_ptr<IMusicBackend> makeBackend(const std::vector<uint8_t> &bytes, const std::filesystem::path &path = {});
} // namespace ngmv
