#pragma once

#include <algorithm>
#include <stdexcept>
#include "lr/ParseTable.h"

namespace selective_test {

struct Run {
    bool accepted;
    std::size_t offset;
    std::vector<zbik::RuleId> reductions;
};

// A bounded diagnostic interpreter. Tracks grammar symbols independently of
// state IDs to catch invalid reduction handles as well as nontermination.
inline Run run(const zbik::ParseTable &table,
               const std::vector<zbik::TerminalId> &input) {
    using namespace zbik;
    std::vector<StateId> states{table.start()};
    std::vector<SymbolRef> symbols;
    std::vector<RuleId> reductions;
    std::size_t offset = 0;
    for (std::size_t step = 0; step < 100000; ++step) {
        std::vector<LookaheadSymbol> window;
        for (std::size_t j = offset; j < input.size() && window.size() < table.maxLength(); ++j) {
            window.push_back(input[j]);
        }
        if (window.size() < table.maxLength()) window.push_back(endOfInput);
        const auto &cell = table.actions(states.back(), LookaheadWord(std::move(window)));
        if (cell.empty()) return {false, offset, std::move(reductions)};
        if (cell.hasConflict()) throw std::runtime_error("conflict during test parse");
        const Action &action = cell.actions().front();
        if (const auto *shift = std::get_if<Shift>(&action)) {
            if (offset >= input.size()) throw std::runtime_error("shift past EOF");
            symbols.push_back(input[offset++]);
            states.push_back(shift->target);
        } else if (const auto *reduce = std::get_if<Reduce>(&action)) {
            const Rule &rule = table.grammar().rule(reduce->rule);
            if (symbols.size() < rule.size() ||
                !std::equal(rule.rhs().begin(), rule.rhs().end(),
                            symbols.end() - rule.size())) {
                throw std::runtime_error("invalid reduction handle");
            }
            symbols.resize(symbols.size() - rule.size());
            states.resize(states.size() - rule.size());
            const auto target = table.goTo(states.back(), rule.lhs());
            if (!target) throw std::runtime_error("missing GOTO");
            symbols.push_back(rule.lhs());
            states.push_back(*target);
            reductions.push_back(reduce->rule);
        } else {
            if (offset != input.size() || symbols.size() != 1 ||
                symbols.front() != SymbolRef{table.grammar().start()}) {
                throw std::runtime_error("invalid accept configuration");
            }
            return {true, offset, std::move(reductions)};
        }
    }
    throw std::runtime_error("test parse exceeded step budget");
}
}
