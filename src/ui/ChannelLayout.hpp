#pragma once
#include <algorithm>
#include <cstddef>
namespace ngmv {
struct ChannelCell {
    int x, y, width, height;
    bool split, compact;
    int labelX() const {
        return x + 22;
    }
    int waveLeft() const {
        return split ? x + 176 : 280;
    }
    int waveRight() const {
        return x + width - 32;
    }
    int buttonSize() const {
        return compact ? 18 : split ? 24 : 32;
    }
    int volumeY() const {
        return y + height - (compact ? 24 : 40);
    }
    int sliderLeft() const {
        return labelX() + (compact ? 24 : split ? 30 : 40);
    }
    int sliderRight() const {
        return labelX() + (compact ? 78 : split ? 94 : 136);
    }
    int plusX() const {
        return labelX() + (compact ? 86 : split ? 102 : 148);
    }
};
inline ChannelCell channelCell(size_t index, size_t count, bool recording) {
    bool split = count > 8;
    size_t rows = split ? (count + 1) / 2 : count;
    int top = recording ? 205 : 276, bottom = recording ? 1064 : 1000;
    int height = (bottom - top) / int(std::max(size_t(1), rows));
    int column = split ? int(index / rows) : 0;
    return {32 + column * 944,
            top + int(index % std::max(size_t(1), rows)) * height,
            split ? 912 : 1856,
            height,
            split,
            height < 70};
}
} // namespace ngmv
