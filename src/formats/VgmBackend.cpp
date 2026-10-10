#include "GmeBackend.hpp"
#include <set>
#include <stdexcept>
namespace ngmv {
std::unique_ptr<IMusicBackend> makeVgmBackend(const std::vector<uint8_t> &bytes) {
    auto fail = [](bool ok, const char *message) {
        if (!ok)
            throw std::runtime_error(message);
    };
    auto le = [&](size_t i) {
        fail(i + 4 <= bytes.size(), "Truncated VGM header.");
        return uint32_t(bytes[i]) | uint32_t(bytes[i + 1]) << 8 | uint32_t(bytes[i + 2]) << 16 |
               uint32_t(bytes[i + 3]) << 24;
    };
    fail(bytes.size() > 64, "Truncated VGM.");
    size_t end = uint64_t(le(4)) + 4;
    fail(end >= 65 && end <= bytes.size(), "Invalid VGM file length.");
    uint32_t version = le(8);
    fail(version >= 0x100 && version <= 0x171, "Unsupported VGM version.");
    size_t start = version >= 0x150 && le(0x34) ? uint64_t(le(0x34)) + 0x34 : 0x40;
    fail(start >= 64 && start < end, "Invalid VGM command offset.");
    for (size_t field : {size_t(12), size_t(16), size_t(44)})
        fail(!(le(field) & 0xc0000000) &&
                 (!le(field) || (le(field) >= 100000 && le(field) <= 20000000)),
             "Unsupported VGM clock or dual-chip/T6W28 configuration.");
    fail(le(12) || le(16) || le(44), "VGM has no SMS/Mega Drive sound chips.");
    fail(!le(48) && !(le(16) && le(44)), "Unsupported VGM chip combination.");
    size_t gd3 = le(20) ? uint64_t(le(20)) + 20 : 0;
    if (gd3)
        fail(gd3 + 12 <= end && uint64_t(le(gd3 + 8)) + gd3 + 12 <= end,
             "Invalid VGM GD3 metadata.");
    size_t loop = le(28) ? uint64_t(le(28)) + 28 : 0;
    fail(!loop || (loop >= start && loop < end), "Invalid VGM loop offset.");
    uint64_t ticks = 0, loopTicks = 0;
    size_t bank = 0, pcm = 0;
    bool loopSeen = false, ended = false;
    for (size_t p = start; p < end;) {
        if (p == loop) {
            loopSeen = true;
            loopTicks = ticks;
        }
        unsigned cmd = bytes[p];
        size_t n = 1;
        if (cmd == 0x4f || cmd == 0x50)
            n = 2;
        else if ((cmd >= 0x51 && cmd <= 0x53) || cmd == 0x61)
            n = 3;
        else if (cmd == 0xe0)
            n = 5;
        else if (cmd == 0x67) {
            fail(p + 7 <= end && bytes[p + 1] == 0x66 && bytes[p + 2] == 0,
                 "Unsupported or truncated VGM PCM data block.");
            n = uint64_t(le(p + 3)) + 7;
            bank = n - 7;
        } else if (cmd != 0x62 && cmd != 0x63 && cmd != 0x66 && !(cmd >= 0x70 && cmd <= 0x8f))
            throw std::runtime_error("Unsupported VGM command (other chips or streaming DAC). Use "
                                     "an SMS/Mega Drive VGM with legacy DAC writes.");
        fail(n <= end - p, "Truncated VGM command or PCM data.");
        if (cmd == 0xe0) {
            pcm = le(p + 1);
            fail(pcm <= bank, "Invalid VGM PCM seek.");
        }
        if (cmd >= 0x80 && cmd <= 0x8f) {
            fail(pcm < bank, "VGM PCM read exceeds data bank.");
            ++pcm;
            ticks += cmd & 15;
        }
        if (cmd == 0x61)
            ticks += bytes[p + 1] | unsigned(bytes[p + 2]) << 8;
        if (cmd == 0x62)
            ticks += 735;
        if (cmd == 0x63)
            ticks += 882;
        if (cmd >= 0x70 && cmd <= 0x7f)
            ticks += (cmd & 15) + 1;
        fail(ticks <= 44100ull * 3600, "VGM duration exceeds one hour.");
        p += n;
        if (cmd == 0x66) {
            ended = true;
            break;
        }
    }
    fail(ended && ticks && (!loop || (loopSeen && ticks > loopTicks)),
         "VGM has no valid timed end/loop.");
    // Remove native looping: the player handles repeat/folder advance at the actual end.
    // This also prevents unsafe DAC cursors carrying across repeated command loops.
    auto normalized = bytes;
    auto put = [&](size_t i, uint32_t n) {
        for (int j = 0; j < 4; ++j)
            normalized[i + j] = uint8_t(n >> (j * 8));
    };
    put(28, 0);
    put(32, 0);
    put(24, uint32_t(ticks));
    return std::make_unique<GmeBackend>(normalized, Format::Vgm);
}
} // namespace ngmv
