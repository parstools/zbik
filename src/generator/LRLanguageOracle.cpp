#include "LRLanguageOracle.h"

#include <cstdint>
#include <set>
#include <stdexcept>
#include <utility>

#include "BoundedDerivationGenerator.h"
#include "lr/LRMachine.h"

namespace zbik {
namespace {

bool sameGrammar(const Grammar &left, const Grammar &right) {
    if (left.terminalCount() != right.terminalCount()
            || left.nonterminalCount() != right.nonterminalCount()
            || left.ruleCount() != right.ruleCount()
            || left.start() != right.start()) {
        return false;
    }
    for (std::size_t i = 0; i < left.terminalCount(); ++i) {
        const TerminalId id{static_cast<std::uint32_t>(i)};
        if (left.terminalName(id) != right.terminalName(id)) return false;
    }
    for (std::size_t i = 0; i < left.nonterminalCount(); ++i) {
        const NonterminalId id{static_cast<std::uint32_t>(i)};
        if (left.nonterminalName(id) != right.nonterminalName(id)) return false;
    }
    for (std::size_t i = 0; i < left.ruleCount(); ++i) {
        const RuleId id{static_cast<std::uint32_t>(i)};
        if (left.rule(id).lhs() != right.rule(id).lhs()
                || left.rule(id).rhs() != right.rule(id).rhs()) {
            return false;
        }
    }
    return true;
}

template<typename Consumer>
void forEachWord(const Grammar &grammar, std::size_t maxLength, Consumer consume) {
    consume(std::vector<TerminalId>{});
    const std::size_t alphabetSize = grammar.terminalCount();
    if (alphabetSize == 0 || maxLength == 0) return;

    for (std::size_t length = 1;; ++length) {
        std::vector<TerminalId> word(length, TerminalId{0});
        while (true) {
            consume(word);
            std::size_t position = length;
            while (position > 0) {
                --position;
                const std::size_t next = toIndex(word[position]) + 1;
                if (next < alphabetSize) {
                    word[position] = TerminalId{static_cast<std::uint32_t>(next)};
                    break;
                }
                word[position] = TerminalId{0};
            }
            if (position == 0 && word[0] == TerminalId{0}) break;
        }
        if (length == maxLength) break;
    }
}

} // namespace

bool LRLanguageOracleResult::matches() const noexcept {
    return mismatches.empty();
}

LRLanguageOracleResult compareGeneratedLanguage(
        const Grammar &grammar, const ParseTable &table,
        std::size_t maxWordLength) {
    if (table.hasConflicts()) {
        throw std::invalid_argument("LR language oracle requires a conflict-free table");
    }
    if (!sameGrammar(grammar, table.grammar())) {
        throw std::invalid_argument("LR language oracle requires the same grammar");
    }

    std::set<std::vector<TerminalId>> generatedWords;
    std::size_t generatedTrees = 0;
    BoundedDerivationGenerator generator(grammar, maxWordLength);
    while (generator.next()) {
        ++generatedTrees;
        generatedWords.insert(generator.currentTree().terminals());
    }

    const LRMachine machine(table);
    std::vector<LRLanguageMismatch> mismatches;
    std::size_t testedWords = 0;
    forEachWord(grammar, maxWordLength, [&](const std::vector<TerminalId> &word) {
        ++testedWords;
        const bool generated = generatedWords.contains(word);
        const LRParseResult parsed = machine.parse(word);
        if (generated == parsed.accepted) return;
        mismatches.push_back({
                word,
                generated,
                parsed.accepted,
                parsed.error ? parsed.error->message : std::string{},
        });
    });

    return {
            generatedTrees,
            generatedWords.size(),
            testedWords,
            std::move(mismatches),
    };
}

} // namespace zbik
