#include <chrono>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "corpus/CorpusLabeler.h"
#include "util/util.h"

namespace {

double seconds(std::chrono::steady_clock::duration duration) {
    return std::chrono::duration<double>(duration).count();
}

std::size_t parseMaxLr(std::string_view value) {
    std::size_t result = 0;
    const auto [end, error] =
            std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size() || result == 0) {
        throw std::invalid_argument("--max-lr requires a positive integer");
    }
    return result;
}

void writeLines(
        const std::filesystem::path &path,
        const std::vector<std::string> &lines) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot write file: " + std::filesystem::absolute(path).string());
    }
    for (const std::string &line: lines) output << line << '\n';
    if (!output) {
        throw std::runtime_error("failed to write file: " + std::filesystem::absolute(path).string());
    }
}

void writeText(const std::filesystem::path &path, const std::string &text) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot write file: " + std::filesystem::absolute(path).string());
    }
    output << text;
    if (!output) {
        throw std::runtime_error("failed to write file: " + std::filesystem::absolute(path).string());
    }
}

void printHelp(std::ostream &output) {
    output << R"(Zbik - analyse and classify a corpus of context-free grammars

Usage:
  zbik [input.dat [output.dat]] [--max-lr=N] [--report[=report.txt]]
  zbik --help

Arguments:
  input.dat             Grammar corpus. Defaults to res/grammars.dat.
  output.dat            Corpus with computed labels. Defaults to
                        res/grammars_result.dat.

Options:
  --max-lr=N            Try canonical LR(k) for k=1..N. Default: 6.
  --max-lr N            Equivalent form of --max-lr=N.
  --report              Write a detailed report to res/grammars_report.txt.
  --report=FILE         Write the detailed report to FILE.
  -h, --help            Show this help and exit.

The input contains one production per line (A -> symbols). An empty right-hand
side denotes epsilon. Blank lines, --- and ===... separate grammars; a line
whose first non-whitespace character is ';' is a comment. The report mode
builds complete automata and collects detailed timings and sizes, so it can be
much slower than ordinary classification.
)";
}

} // namespace

int main(int argc, char **argv) {
    try {
        const std::filesystem::path resources{ZBIK_RESOURCE_DIR};
        std::vector<std::filesystem::path> positional;
        std::optional<std::filesystem::path> report;
        std::optional<std::size_t> maxLr;
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument{argv[index]};
            if (argument == "--help" || argument == "-h") {
                printHelp(std::cout);
                return 0;
            } else if (argument == "--report") {
                if (report) throw std::invalid_argument("--report specified more than once");
                report = resources / "grammars_report.txt";
            } else if (argument.starts_with("--report=")) {
                if (report) throw std::invalid_argument("--report specified more than once");
                const std::string_view path = argument.substr(std::string_view{"--report="}.size());
                if (path.empty()) throw std::invalid_argument("--report path must not be empty");
                report = std::filesystem::path{path};
            } else if (argument == "--max-lr") {
                if (maxLr) throw std::invalid_argument("--max-lr specified more than once");
                if (++index == argc) {
                    throw std::invalid_argument("--max-lr requires a positive integer");
                }
                maxLr = parseMaxLr(argv[index]);
            } else if (argument.starts_with("--max-lr=")) {
                if (maxLr) throw std::invalid_argument("--max-lr specified more than once");
                maxLr = parseMaxLr(
                        argument.substr(std::string_view{"--max-lr="}.size()));
            } else if (argument.starts_with("--")) {
                throw std::invalid_argument("unknown option: " + std::string{argument});
            } else {
                positional.emplace_back(argument);
            }
        }
        if (positional.size() > 2) {
            std::cerr << "usage: zbik [input.dat [output.dat]] [--max-lr=N] "
                         "[--report[=report.txt]]\n";
            return 2;
        }

        const std::filesystem::path input = !positional.empty()
                ? positional[0]
                : resources / "grammars.dat";
        const std::filesystem::path output = positional.size() >= 2
                ? positional[1]
                : resources / "grammars_result.dat";
        const auto absoluteInput = std::filesystem::absolute(input).lexically_normal();
        const auto absoluteOutput = std::filesystem::absolute(output).lexically_normal();
        if (absoluteInput == absoluteOutput) {
            throw std::invalid_argument("input and output paths must be different");
        }
        if (report) {
            const auto absoluteReport = std::filesystem::absolute(*report).lexically_normal();
            if (absoluteInput == absoluteReport || absoluteOutput == absoluteReport) {
                throw std::invalid_argument("report path must differ from input and output paths");
            }
        }

        zbik::CorpusLabelOptions options;
        if (maxLr) options.maxLookahead = *maxLr;
        options.collectDetailedReport = report.has_value();
        options.stopAfterFirstConflict = !report.has_value();
        std::size_t current = 0;
        const auto result = zbik::labelGrammarCorpus(
                zbik::readAllLines(input), options,
                [&](std::size_t index, std::string_view stage) {
            if (index != current) {
                if (current != 0) std::cout << '\n';
                current = index;
                std::cout << index << ':';
            }
            std::cout << ' ' << stage << std::flush;
        });
        if (current != 0) std::cout << '\n';

        for (const auto &grammar: result.grammars) {
            if (grammar.error) {
                std::cerr << grammar.index << ": unexpected failure: "
                          << *grammar.error << '\n';
                continue;
            }
            std::cout << grammar.index << ":";
            for (const std::string &label: grammar.labels) std::cout << ' ' << label;
            if (grammar.ambiguityDiagnostic) {
                std::cout << " ambiguity={" << *grammar.ambiguityDiagnostic << '}';
            }
            for (const auto &timing: grammar.timings) {
                std::cout << ' ' << timing.stage << '=' << std::fixed
                          << std::setprecision(3) << seconds(timing.elapsed) << 's';
            }
            std::cout << '\n';
        }

        writeLines(output, result.lines);
        if (report) {
            writeText(*report, zbik::formatCorpusReport(
                    result, options, ZBIK_BUILD_CONFIGURATION));
        }
        std::cout << result.grammars.size() << " grammars written to " << output
                  << " in " << std::fixed << std::setprecision(3)
                  << seconds(result.elapsed) << "s; unexpected failures: "
                  << result.unexpectedFailureCount() << '\n';
        if (report) std::cout << "report written to " << *report << '\n';
        return result.unexpectedFailureCount() == 0 ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << "zbik: " << error.what() << '\n';
        return 1;
    }
}
