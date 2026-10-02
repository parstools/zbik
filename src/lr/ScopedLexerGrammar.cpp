#include "lr/ScopedLexerGrammar.h"

#include <map>
#include <utility>

#include "grammar/GrammarBuilder.h"

namespace zbik {
namespace {

ScopedLexerGrammar buildScopes(const Grammar &source,
                                    std::span<const ParserLexerClass> declarations,
                                    LexerClassMask initial, std::size_t maxContexts,
                                    std::span<const LexerClassMask> dependencies) {
    std::vector<ParserLexerClass> classes(source.nonterminalCount());
    LexerClassMask used = initial;
    for (const auto &declaration : declarations) {
        if (toIndex(declaration.nonterminal) >= classes.size())
            throw std::invalid_argument("lexer scope names an unknown parser nonterminal");
        auto &entry = classes[toIndex(declaration.nonterminal)];
        entry.enabled |= declaration.enabled;
        entry.disabled |= declaration.disabled;
        if (entry.enabled & entry.disabled)
            throw std::invalid_argument("parser rule " + source.nonterminalName(declaration.nonterminal) +
                                        " enables and disables the same lexer class");
        used |= entry.enabled | entry.disabled;
    }
    using Key = std::pair<NonterminalId, LexerClassMask>;
    std::map<Key, std::size_t> interned;
    std::vector<Key> contexts;
    std::vector<std::string> names;
    const auto intern = [&](NonterminalId nonterminal, LexerClassMask inherited) -> std::size_t {
        const auto &entry = classes[toIndex(nonterminal)];
        const auto effective = (inherited | entry.enabled) & ~entry.disabled;
        const Key key{nonterminal, effective};
        if (const auto found = interned.find(key); found != interned.end()) return found->second;
        if (contexts.size() >= maxContexts)
            throw std::length_error("reachable lexer scope limit exceeded at " + source.nonterminalName(nonterminal));
        const auto index = contexts.size();
        contexts.push_back(key);
        names.push_back("N" + std::to_string(index) + "_" + source.nonterminalName(nonterminal));
        interned.emplace(key, index);
        return index;
    };
    intern(source.start(), initial);
    std::map<std::pair<TerminalId, LexerClassMask>, std::size_t> terminalIds;
    std::vector<std::string> terminalNames;
    std::vector<TerminalId> sourceTerminals;
    std::vector<LexerClassRequirement> requirements;
    std::vector<std::string> productions;
    std::vector<RuleId> sourceRules;
    std::vector<NonterminalId> sourceNonterminals;
    for (std::size_t context = 0; context < contexts.size(); ++context) {
        const auto [nonterminal, active] = contexts[context];
        sourceNonterminals.push_back(nonterminal);
        for (const auto ruleId : source.rulesFor(nonterminal)) {
            std::string production = names[context] + " ->";
            for (const auto &symbol : source.rule(ruleId).rhs()) {
                if (const auto child = std::get_if<NonterminalId>(&symbol)) {
                    const auto index = intern(*child, active);
                    production += ' ' + names[index];
                } else {
                    const auto terminal = std::get<TerminalId>(symbol);
                    const auto relevant = dependencies.empty() ? used : dependencies[toIndex(terminal)];
                    const auto projected = active & relevant;
                    const auto [it, inserted] = terminalIds.emplace(std::make_pair(terminal, projected), terminalNames.size());
                    if (inserted) {
                        terminalNames.push_back("T" + std::to_string(it->second) + "_" + source.terminalName(terminal));
                        sourceTerminals.push_back(terminal);
                        requirements.push_back({TerminalId{static_cast<std::uint32_t>(it->second)}, projected, relevant & ~active});
                    }
                    production += ' ' + terminalNames[it->second];
                }
            }
            productions.push_back(std::move(production));
            sourceRules.push_back(ruleId);
        }
    }
    return {GrammarBuilder{}.build(productions, terminalNames), std::move(sourceTerminals),
            std::move(sourceRules), std::move(sourceNonterminals), std::move(requirements)};
}

template<class Lexer>
auto buildWithLexer(const Grammar &source, std::span<const ParserLexerClass> declarations,
                    const Lexer &lexer, LexerClassMask initial, std::size_t maxContexts) -> ScopedLexerGrammar {
    const auto ruleDependencies = lexer.automaton().classDependencies();
    std::vector<LexerClassMask> dependencies(source.terminalCount());
    for (std::size_t i = 0; i < lexer.rules().size(); ++i) {
        if (!lexer.rules()[i].terminal && lexer.rules()[i].requiredClasses)
            throw std::invalid_argument("parser-directed lexer currently requires unconditional skipped rules");
        if (const auto terminal = lexer.rules()[i].terminal) {
            if (toIndex(*terminal) >= dependencies.size())
                throw std::invalid_argument("lexer rule names a terminal outside the source grammar");
            dependencies[toIndex(*terminal)] |= ruleDependencies[i];
        }
    }
    return buildScopes(source, declarations, initial, maxContexts, dependencies);
}

} // namespace

ScopedLexerGrammar scopeLexerClasses(const Grammar &source, std::span<const ParserLexerClass> declarations,
                                    LexerClassMask initial, std::size_t maxContexts) {
    return buildScopes(source, declarations, initial, maxContexts, {});
}

ScopedLexerGrammar scopeLexerClasses(const Grammar &source, std::span<const ParserLexerClass> declarations,
                                    const ByteLexer &lexer, LexerClassMask initial, std::size_t maxContexts) {
    return buildWithLexer(source, declarations, lexer, initial, maxContexts);
}

ScopedLexerGrammar scopeLexerClasses(const Grammar &source, std::span<const ParserLexerClass> declarations,
                                    const Utf8Lexer &lexer, LexerClassMask initial, std::size_t maxContexts) {
    return buildWithLexer(source, declarations, lexer, initial, maxContexts);
}

} // namespace zbik
