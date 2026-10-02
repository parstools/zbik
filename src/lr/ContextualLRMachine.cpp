#include "lr/ContextualLRMachine.h"

#include <utility>

namespace zbik {
namespace {

template<class Lexer>
auto parseSource(const ParseTable &table, const LexerContextPlan &plan,
                 std::span<const TerminalId> origins,
                 const Lexer &lexer, std::string_view source, bool capture) -> ContextualParseResult {
    ContextualParseResult result;
    std::vector<StateId> stack{table.start()};
    std::size_t committed = 0;
    std::vector<LexedToken> buffered;
    // This first runtime rebuilds unconsumed lookahead and verifies its stability.
    // A reduction must not silently change tokens that justified that reduction.
    for (std::size_t steps = 0; steps < 1000000; ++steps) {
        const auto state = stack.back();
        std::vector<LookaheadSymbol> prefix;
        std::vector<LexedToken> preview;
        std::size_t cursor = committed;
        while (prefix.size() < table.maxLength()) {
            const auto mask = plan.activeMask(state, prefix);
            if (!mask) break;
            if (capture) result.requests.push_back({state, LookaheadWord{prefix}, *mask, cursor});
            const auto token = lexer.next(source, cursor, *mask);
            if (!token) { prefix.emplace_back(endOfInput); break; }
            const auto terminal = plan.terminalFor(state, prefix, token->terminal, origins);
            if (!terminal) {
                result.error = "unexpected token '" + token->text + "' at byte " +
                    std::to_string(token->offset) + " in state " + std::to_string(state.value) +
                    " after " + LookaheadWord{prefix}.dump();
                return result;
            }
            prefix.emplace_back(*terminal);
            preview.push_back(*token);
        }
        for (std::size_t i = 0; i < buffered.size() && i < preview.size(); ++i) {
            if (buffered[i] != preview[i]) {
                result.error = "lexer context changes buffered token at byte " +
                    std::to_string(buffered[i].offset) + " in state " + std::to_string(state.value);
                return result;
            }
        }
        const LookaheadWord word{prefix};
        const auto &cell = table.actions(state, word);
        if (cell.empty()) {
            result.error = "syntax error at byte " + std::to_string(committed) +
                           " in state " + std::to_string(state.value) + " on " + word.dump();
            return result;
        }
        const auto &action = cell.actions().front();
        if (const auto shift = std::get_if<Shift>(&action)) {
            if (preview.empty()) throw std::logic_error("contextual parser attempted to shift EOF");
            result.tokens.push_back(preview.front());
            committed = preview.front().offset + preview.front().text.size();
            preview.erase(preview.begin());
            buffered = std::move(preview);
            stack.push_back(shift->target);
        } else if (const auto reduce = std::get_if<Reduce>(&action)) {
            const auto &rule = table.grammar().rule(reduce->rule);
            if (rule.size() >= stack.size()) throw std::logic_error("contextual LR stack underflow");
            stack.resize(stack.size() - rule.size());
            const auto target = table.goTo(stack.back(), rule.lhs());
            if (!target) throw std::logic_error("missing contextual LR GOTO");
            stack.push_back(*target);
            buffered = std::move(preview);
        } else {
            if (prefix.empty() || !isEndOfInput(prefix.front()) || cursor != source.size())
                throw std::logic_error("contextual LR accepted before EOF");
            result.accepted = true;
            return result;
        }
    }
    result.error = "contextual parse step limit exceeded";
    return result;
}

} // namespace

ContextualLRMachine::ContextualLRMachine(const ParseTable &table,
                                       std::span<const LexerClassRequirement> requirements,
                                       std::span<const TerminalId> sourceTerminals)
    : table_(table), plan_(table, requirements), sourceTerminals_(sourceTerminals.begin(), sourceTerminals.end()) {
    if (sourceTerminals_.empty()) {
        for (std::size_t i = 0; i < table.grammar().terminalCount(); ++i)
            sourceTerminals_.push_back(TerminalId{static_cast<std::uint32_t>(i)});
    }
    if (sourceTerminals_.size() != table.grammar().terminalCount())
        throw std::invalid_argument("scoped terminal map size differs from LR alphabet");
}

ContextualParseResult ContextualLRMachine::parse(const ByteLexer &lexer, std::string_view input,
                                                bool captureRequests) const {
    validateLexer(lexer.rules());
    return parseSource(table_, plan_, sourceTerminals_, lexer, input, captureRequests);
}

ContextualParseResult ContextualLRMachine::parse(const Utf8Lexer &lexer, std::string_view input,
                                                bool captureRequests) const {
    validateLexer(lexer.rules());
    return parseSource(table_, plan_, sourceTerminals_, lexer, input, captureRequests);
}

void ContextualLRMachine::validateLexer(std::span<const LexerRule> rules) const {
    plan_.validateRules(rules, sourceTerminals_);
}

} // namespace zbik
