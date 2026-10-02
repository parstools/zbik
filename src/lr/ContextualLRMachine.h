#pragma once

#include "lexer/ByteLexer.h"
#include "lexer/Utf8Lexer.h"
#include "lr/LexerContextPlan.h"

namespace zbik {

struct ContextualLexRequest {
    StateId state;
    LookaheadWord prefix;
    LexerClassMask active;
    std::size_t byteOffset;
};

struct ContextualParseResult {
    bool accepted{};
    std::vector<LexedToken> tokens;
    std::string error;
    std::vector<ContextualLexRequest> requests;
};

class ContextualLRMachine {
public:
    ContextualLRMachine(const ParseTable &table, std::span<const LexerClassRequirement> requirements,
                        std::span<const TerminalId> sourceTerminals = {});
    ContextualLRMachine(ParseTable &&, std::span<const LexerClassRequirement>,
                        std::span<const TerminalId> = {}) = delete;
    [[nodiscard]] const LexerContextPlan &plan() const noexcept { return plan_; }
    void validateLexer(std::span<const LexerRule> rules) const;
    [[nodiscard]] ContextualParseResult parse(const ByteLexer &lexer, std::string_view input,
                                               bool captureRequests = false) const;
    [[nodiscard]] ContextualParseResult parse(const Utf8Lexer &lexer, std::string_view input,
                                               bool captureRequests = false) const;
private:
    const ParseTable &table_;
    LexerContextPlan plan_;
    std::vector<TerminalId> sourceTerminals_;
};

} // namespace zbik
