#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace zbik {
std::string trimRight(std::string_view sv);
std::string trimLeft(std::string_view sv);
std::string trim(const std::string &s);
std::vector<std::string> readAllLines(const std::filesystem::path &path);
std::vector<std::string> split(const std::string &s, char sep);
} // namespace zbik
