// Implementation of the OpenBoardView utils subset declared in shim/utils.h.
// Mirrors upstream src/openboardview/utils.cpp without the SDL logging calls.
#include "utils.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iterator>

std::vector<char> file_as_buffer(const filesystem::path &filepath, std::string &error_msg)
{
	std::vector<char> data;

	if (!filesystem::is_regular_file(filepath)) {
		error_msg = "Not a regular file";
		return data;
	}

	std::ifstream file;
	file.open(filepath, std::ios::in | std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		error_msg = std::strerror(errno);
		return data;
	}

	file.seekg(0, std::ios_base::end);
	std::streampos sz = file.tellg();
	ENSURE(sz >= 0, error_msg);
	data.reserve(static_cast<size_t>(sz));
	file.seekg(0, std::ios_base::beg);
	data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	file.close();

	return data;
}

bool check_fileext(const filesystem::path &filepath, const std::string fileext)
{
	std::string ext{filepath.extension().string()};
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
	return ext == fileext;
}

bool find_str_in_buf(const std::string str, const std::vector<char> &buf)
{
	return std::search(buf.begin(), buf.end(), str.begin(), str.end()) != buf.end();
}

bool compare_string_insensitive(const std::string &str1, const std::string &str2)
{
	return str1.size() == str2.size() &&
	       std::equal(str2.begin(), str2.end(), str1.begin(), [](char a, char b) {
		       return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	       });
}
