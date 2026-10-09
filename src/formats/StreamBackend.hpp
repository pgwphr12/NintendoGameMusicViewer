#pragma once
#include "core/IMusicBackend.hpp"
namespace ngmv {
std::unique_ptr<IMusicBackend> makeStreamBackend(const std::filesystem::path &);
}
