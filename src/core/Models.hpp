#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace ngmv {
constexpr int InternalRate = 48000;
struct Stereo {
    float l = 0, r = 0;
};
enum class Format { Nsf, Nsfe, Spc, Gbs, Gsf, TwoSf, Usf, Bcstm, Bcwav, Vgm };
struct TrackInfo {
    std::string title, game, author, system, comment;
    int lengthMs = -1;
};
struct ChannelInfo {
    std::string name;
};
struct AudioBlock {
    std::vector<Stereo> mix;
    std::vector<std::vector<Stereo>> voices;
};
struct ChannelState {
    bool mute = false;
    float volume = 1;
};
} // namespace ngmv
