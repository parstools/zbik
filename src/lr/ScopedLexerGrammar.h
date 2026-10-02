#pragma once

#include "lr/LexerContextPlan.h"
#include "lexer/ByteLexer.h"
#include "lexer/Utf8Lexer.h"

namespace zbik {

struct ParserLexerClass {
    NonterminalId nonterminal;
    LexerClassMask enabled{};
    LexerClassMask disabled{};
};

struct ScopedLexerGrammar {
    Grammar grammar;
    // Indexed by scoped terminal/rule/nonterminal ID, for diagnostics and AST reductions.
    std::vector<TerminalId> sourceTerminals;
    std::vector<RuleId> sourceRules;
    std::vector<NonterminalId> sourceNonterminals;
    std::vector<LexerClassRequirement> requirements;
};

// Specialize only nonterminal/mask pairs reachable from the start symbol.
// Nested declarations shadow the caller without runtime push/pop actions.
[[nodiscard]] ScopedLexerGrammar scopeLexerClasses(
    const Grammar &source, std::span<const ParserLexerClass> declarations,
    LexerClassMask initial = 0, std::size_t maxContexts = 4096);

// Lexer-aware overloads project scope masks onto bits relevant to each token.
[[nodiscard]] ScopedLexerGrammar scopeLexerClasses(
    const Grammar &source, std::span<const ParserLexerClass> declarations, const ByteLexer &lexer,
    LexerClassMask initial = 0, std::size_t maxContexts = 4096);
[[nodiscard]] ScopedLexerGrammar scopeLexerClasses(
    const Grammar &source, std::span<const ParserLexerClass> declarations, const Utf8Lexer &lexer,
    LexerClassMask initial = 0, std::size_t maxContexts = 4096);

} // namespace zbik
