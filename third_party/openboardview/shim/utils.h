// Shrink-wrapped replacement for OpenBoardView's src/openboardview/utils.h.
//
// The original header pulls in SDL2 (for SDL_LogError) and a ghc::filesystem fallback.
// Our vendored parsers only need a handful of helpers, so we re-declare exactly those
// and drop the SDL dependency entirely. Behaviour is identical to upstream utils.cpp.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace filesystem = std::filesystem;

// Verify predicate X, if false write error to ERROR_MSG and run ACTION.
// Upstream also logs via SDL_LogError; we intentionally omit the log call.
#define ENSURE_OR_FAIL(X, ERROR_MSG, ACTION)                                                       \
	if (!(X)) {                                                                                    \
		ERROR_MSG = std::string(__FILE__) + ":" + std::to_string(__LINE__) +                       \
		            ": Assertion `" #X "' failed.";                                                \
		ACTION;                                                                                    \
	}
// Same but no ACTION
#define ENSURE(X, ERROR_MSG) ENSURE_OR_FAIL(X, ERROR_MSG, )

// Loads an entire file in to memory
std::vector<char> file_as_buffer(const filesystem::path &filepath, std::string &error_msg);

// Extract extension from filename and check against given fileext (lowercase)
bool check_fileext(const filesystem::path &filepath, const std::string fileext);

// Returns true if the given str was found in buf
bool find_str_in_buf(const std::string str, const std::vector<char> &buf);

// Case insensitive comparison of std::string
bool compare_string_insensitive(const std::string &str1, const std::string &str2);
