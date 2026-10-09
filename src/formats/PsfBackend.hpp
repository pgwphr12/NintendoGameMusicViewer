#pragma once
#include "core/IMusicBackend.hpp"
namespace ngmv {
std::unique_ptr<IMusicBackend> makePsfBackend(const std::filesystem::path &, Format);
}
