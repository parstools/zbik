#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace zbik {

using GrammarSource = std::vector<std::string>;

class GrammarCorpusReader {
public:
    [[nodiscard]] static std::vector<GrammarSource> read(const std::filesystem::path &path);
    [[nodiscard]] static std::vector<GrammarSource> split(const std::vector<std::string> &lines);
};

} // namespace zbik
