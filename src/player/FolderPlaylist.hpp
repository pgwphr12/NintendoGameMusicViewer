#pragma once
#include "core/Models.hpp"
#include <filesystem>
namespace ngmv {
struct FolderPlaylist {
    std::vector<std::filesystem::path> files;
    size_t current = 0;
    static FolderPlaylist scan(const std::filesystem::path &file, Format format);
    bool active() const {
        return !files.empty();
    }
};
} // namespace ngmv
