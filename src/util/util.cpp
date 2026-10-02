#include "util.h"

#include <fstream>

namespace zbik {
std::string trimRight(std::string_view sv) {
    const std::string toRemove = " \t\r\n";

    while (!sv.empty() && toRemove.find(sv.back()) != std::string::npos) {
        sv.remove_suffix(1);
    }
    return std::string(sv);
}

std::string trimLeft(std::string_view sv) {
    const std::string toRemove = " \t";

    while (!sv.empty() && toRemove.find(sv.front()) != std::string::npos) {
        sv.remove_prefix(1);
    }
    return std::string(sv);
}

std::string trim(const std::string &s) {
    return trimLeft(trimRight(s));
}

std::vector<std::string> readAllLines(const std::filesystem::path &path) {
    const auto full = std::filesystem::absolute(path);
    std::ifstream in(full);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + full.string());
    }

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(in, line)) {
        lines.push_back(trimRight(line));
    }

    return lines;
}

std::vector<std::string> split(const std::string &s, char sep) {
    std::vector<std::string> out;
    std::string token;

    for (char c: s) {
        if (c == sep) {
            if (!token.empty()) {
                out.push_back(token);
                token.clear();
            }
        } else {
            token.push_back(c);
        }
    }

    if (!token.empty())
        out.push_back(token);

    return out;
}

} // namespace zbik
