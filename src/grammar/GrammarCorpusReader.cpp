#include "GrammarCorpusReader.h"

#include <utility>

#include "GrammarSyntax.h"
#include "util/util.h"

namespace zbik {
namespace {

bool isSeparator(const std::string &line) {
    const std::string text = trim(line);
    return grammar_syntax::isBlankLine(line) || text == "---" || text.starts_with("===");
}

} // namespace

std::vector<GrammarSource> GrammarCorpusReader::read(const std::filesystem::path &path) {
    return split(readAllLines(path));
}

std::vector<GrammarSource> GrammarCorpusReader::split(const std::vector<std::string> &lines) {
    std::vector<GrammarSource> grammars;
    GrammarSource current;
    bool hasRule = false;

    const auto finishGrammar = [&]() {
        if (hasRule) {
            grammars.push_back(std::move(current));
        }
        current.clear();
        hasRule = false;
    };

    for (const std::string &line: lines) {
        if (isSeparator(line)) {
            finishGrammar();
            continue;
        }

        current.push_back(line);
        if (!grammar_syntax::isCommentLine(line)) {
            hasRule = true;
        }
    }
    finishGrammar();

    return grammars;
}

} // namespace zbik
