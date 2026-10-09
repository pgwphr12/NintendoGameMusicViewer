#include "GmeBackend.hpp"
#include "PsfBackend.hpp"
#include "StreamBackend.hpp"
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace ngmv {
Format identify(const std::vector<uint8_t> &b) {
    if (b.size() >= 64 && !memcmp(b.data(), "CSTM", 4))
        return Format::Bcstm;
    if (b.size() >= 64 && !memcmp(b.data(), "CWAV", 4))
        return Format::Bcwav;
    if (b.size() >= 128 && !memcmp(b.data(), "NESM\x1a", 5))
        return Format::Nsf;
    if (b.size() >= 4 && !memcmp(b.data(), "NSFE", 4))
        return Format::Nsfe;
    if (b.size() >= 0x10200 && !memcmp(b.data(), "SNES-SPC700 Sound File Data", 26))
        return Format::Spc;
    if (b.size() >= 112 && !memcmp(b.data(), "GBS", 3))
        return Format::Gbs;
    if (b.size() >= 16 && !memcmp(b.data(), "PSF", 3)) {
        if (b[3] == 0x21)
            return Format::Usf;
        if (b[3] == 0x22)
            return Format::Gsf;
        if (b[3] == 0x24)
            return Format::TwoSf;
    }
    throw std::runtime_error("Unsupported or truncated music file. Open NSF / NSFE / SPC / GBS / "
                             "GSF / 2SF / USF / BCSTM / BCWAV; "
                             "game ROMs are not supported.");
}
std::vector<uint8_t> readMusicFile(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f)
        throw std::runtime_error("Cannot open music file.");
    auto n = f.tellg();
    if (n >= 64) {
        std::vector<uint8_t> header(64);
        f.seekg(0);
        f.read(reinterpret_cast<char *>(header.data()), 64);
        if (!memcmp(header.data(), "CSTM", 4) || !memcmp(header.data(), "CWAV", 4))
            return header;
    }
    if (n <= 0 || n > 64 * 1024 * 1024)
        throw std::runtime_error("Music file is empty or exceeds the 64 MB safety limit.");
    std::vector<uint8_t> b(static_cast<size_t>(n));
    f.seekg(0);
    if (!f.read(reinterpret_cast<char *>(b.data()), n))
        throw std::runtime_error("Incomplete file read.");
    return b;
}
std::unique_ptr<IMusicBackend> makeBackend(const std::vector<uint8_t> &b,
                                           const std::filesystem::path &path) {
    switch (identify(b)) {
    case Format::Bcstm:
    case Format::Bcwav:
        return makeStreamBackend(path);
    case Format::Usf:
    case Format::Gsf:
    case Format::TwoSf:
        return makePsfBackend(path, identify(b));
    case Format::Nsf:
        return std::make_unique<NSFBackend>(b);
    case Format::Nsfe:
        return std::make_unique<NSFEBackend>(b);
    case Format::Spc:
        return std::make_unique<SPCBackend>(b);
    case Format::Gbs:
        return std::make_unique<GBSBackend>(b);
    }
    throw std::runtime_error("Unsupported format.");
}
} // namespace ngmv
