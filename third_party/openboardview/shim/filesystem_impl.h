// Replacement for OpenBoardView's src/openboardview/filesystem_impl.h.
// We always build with C++17 std::filesystem, so the ghc::filesystem fallback is dropped.
#pragma once

#include <filesystem>
#include <fstream>

namespace filesystem = std::filesystem;
using ifstream = std::ifstream;
using ofstream = std::ofstream;
using fstream = std::fstream;
