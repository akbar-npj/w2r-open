// Minimal drop-in replacement for the utf8-cpp `utf8valid()` used by OpenBoardView's
// FileFormats/BRDFileBase.cpp. OpenBoardView ships utf8-cpp as a git submodule; to avoid
// pulling a submodule we provide the single function it actually needs.
//
// Contract (matches utf8-cpp): returns nullptr when `s` is valid UTF-8, otherwise a pointer
// to the first offending byte.
#pragma once

#include <cstdint>

inline const char *utf8valid(const char *s)
{
	const unsigned char *p = reinterpret_cast<const unsigned char *>(s);
	while (*p) {
		unsigned char c = *p;
		int n;
		uint32_t cp;
		if (c < 0x80) {
			++p;
			continue;
		} else if ((c & 0xE0) == 0xC0) {
			n = 1;
			cp = c & 0x1Fu;
		} else if ((c & 0xF0) == 0xE0) {
			n = 2;
			cp = c & 0x0Fu;
		} else if ((c & 0xF8) == 0xF0) {
			n = 3;
			cp = c & 0x07u;
		} else {
			return reinterpret_cast<const char *>(p);
		}

		for (int i = 0; i < n; ++i) {
			++p;
			if ((*p & 0xC0) != 0x80) return reinterpret_cast<const char *>(p);
			cp = (cp << 6) | (*p & 0x3Fu);
		}
		++p;

		// Reject overlong encodings, UTF-16 surrogates and out-of-range code points.
		if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return reinterpret_cast<const char *>(p);
		if (n == 1 && cp < 0x80) return reinterpret_cast<const char *>(p);
		if (n == 2 && cp < 0x800) return reinterpret_cast<const char *>(p);
		if (n == 3 && cp < 0x10000) return reinterpret_cast<const char *>(p);
	}
	return nullptr;
}
