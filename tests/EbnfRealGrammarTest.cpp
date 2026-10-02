#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "EbnfRealGrammarFixtures.h"
#include "ebnf/EbnfConflictDiagnostics.h"
#include "ebnf/EbnfToBnf.h"
#include "first/FirstK.h"
#include "first/FollowK.h"
#include "generator/BoundedDerivationGenerator.h"
#include "lr/CompressedParseTable.h"
#include "lr/DirectLALRkDfa.h"
#include "lr/LALRkDfa.h"
#include "lr/LRMachine.h"
#include "lr/LRkDfa.h"
#include "lr/ParseTable.h"
#include "lr/SelectiveLRkMerger.h"

namespace {

std::vector<zbik::TerminalId> tokens(const zbik::Grammar &grammar, std::initializer_list<std::string_view> names) {
    std::vector<zbik::TerminalId> result;
    result.reserve(names.size());
    for (const std::string_view name: names) {
        result.push_back(grammar.findTerminal(name).value());
    }
    return result;
}

std::set<std::vector<std::string>> generatedWords(const zbik::Grammar &grammar, std::size_t maxLength) {
    std::set<std::vector<std::string>> result;
    zbik::BoundedDerivationGenerator generator(grammar, maxLength);
    while (generator.next()) {
        std::vector<std::string> word;
        for (const zbik::TerminalId terminal: generator.currentTree().terminals()) {
            word.push_back(grammar.terminalName(terminal));
        }
        result.insert(std::move(word));
    }
    return result;
}

} // namespace

TEST(EbnfRealGrammarTest, CmmIsCanonicalLr1AndLalr1) {
    const zbik::EbnfGrammarSpec specification = zbik::test::cmmEbnfGrammar();
    const zbik::EbnfConversionResult conversion =
            zbik::EbnfToBnfConverter{}.convertWithOrigins(specification, zbik::test::cmmTerminals());
    const zbik::Grammar &grammar = conversion.grammar();
    const zbik::FirstKAnalysis first(grammar, 1);
    const zbik::FollowKAnalysis follow(grammar, first);
    const zbik::LRkDfa canonical(grammar, first);
    const zbik::ParseTable canonicalTable(canonical);
    const zbik::LALRkDfa lalr(canonical);
    const zbik::ParseTable lalrTable(lalr);

    EXPECT_FALSE(canonicalTable.hasConflicts());
    EXPECT_FALSE(lalrTable.hasConflicts());
    EXPECT_EQ(conversion.origins().size(), grammar.ruleCount());

    const zbik::TableStorageStats canonicalStorage = zbik::CompressedParseTable(canonicalTable).statistics();
    const zbik::TableStorageStats lalrStorage = zbik::CompressedParseTable(lalrTable).statistics();
    std::size_t firstWords = 0;
    std::size_t followWords = 0;
    for (std::size_t index = 0; index < grammar.nonterminalCount(); ++index) {
        const zbik::NonterminalId id{static_cast<std::uint32_t>(index)};
        firstWords += first.first(id).size();
        followWords += follow.follow(id).size();
    }

    EXPECT_EQ(grammar.ruleCount(), 45U);
    EXPECT_EQ(grammar.nonterminalCount(), 26U);
    EXPECT_EQ(grammar.terminalCount(), 19U);
    EXPECT_EQ(firstWords, 64U);
    EXPECT_EQ(followWords, 131U);
    EXPECT_EQ(canonical.statistics(), (zbik::LRkDfaStats{135, 1630, 289}));
    EXPECT_EQ(canonicalStorage.uncompressedBytes, 17616U);
    EXPECT_EQ(canonicalStorage.compressedBytes, 8352U);
    EXPECT_EQ(lalr.statistics(), (zbik::LRkDfaStats{66, 1002, 141}));
    EXPECT_EQ(lalrStorage.uncompressedBytes, 10376U);
    EXPECT_EQ(lalrStorage.compressedBytes, 4864U);

    const zbik::LRMachine canonicalMachine(canonicalTable);
    const zbik::LRMachine lalrMachine(lalrTable);
    const std::vector<std::vector<zbik::TerminalId>> valid{
            tokens(grammar, {"INT", "ID", "SEMI"}),
            tokens(grammar, {"INT", "ID", "ASSIGN", "NUMBER", "SEMI"}),
            tokens(grammar, {"ID", "ASSIGN", "NUMBER", "SEMI"}),
            tokens(grammar, {"ID", "LPAREN", "RPAREN", "SEMI"}),
            tokens(grammar, {"WHILE", "LPAREN", "ID", "LT", "NUMBER", "RPAREN", "ID", "ASSIGN", "NUMBER", "SEMI"}),
            tokens(grammar, {"LBRACE", "RBRACE"}),
    };
    for (const auto &input: valid) {
        EXPECT_TRUE(canonicalMachine.parse(input).accepted);
        EXPECT_TRUE(lalrMachine.parse(input).accepted);
    }

    const std::vector<std::vector<zbik::TerminalId>> invalid{
            {},
            tokens(grammar, {"INT", "ID"}),
            tokens(grammar, {"ID", "LPAREN", "SEMI"}),
    };
    for (const auto &input: invalid) {
        EXPECT_FALSE(canonicalMachine.parse(input).accepted);
        EXPECT_FALSE(lalrMachine.parse(input).accepted);
    }
}

TEST(EbnfRealGrammarTest, CminusExposesDanglingElseAndClosedOpenRemovesIt) {
    const zbik::EbnfGrammarSpec originalSpecification = zbik::test::cminusEbnfGrammar();
    const zbik::EbnfConversionResult original =
            zbik::EbnfToBnfConverter{}.convertWithOrigins(originalSpecification, zbik::test::cminusTerminals());
    const zbik::LRkDfa originalLr1(original.grammar(), 1);
    const zbik::ParseTable originalTable1(originalLr1);
    ASSERT_EQ(originalTable1.conflicts().size(), 2U);
    EXPECT_EQ(originalLr1.statistics(), (zbik::LRkDfaStats{256, 5976, 729}));
    const std::string prefixConflict =
            zbik::dumpEbnfConflict(originalSpecification, original, originalLr1, originalTable1.conflicts()[0]);
    const std::string danglingElseConflict =
            zbik::dumpEbnfConflict(originalSpecification, original, originalLr1, originalTable1.conflicts()[1]);
    EXPECT_NE(prefixConflict.find("'returnType', alternative 0"), std::string::npos);
    EXPECT_NE(prefixConflict.find("'declarator', alternative 0"), std::string::npos);
    EXPECT_NE(danglingElseConflict.find("'ifStatement', alternative 0"), std::string::npos);
    EXPECT_NE(danglingElseConflict.find("'ifStatement', alternative 1"), std::string::npos);

    const zbik::LRkDfa originalLr2(original.grammar(), 2);
    const zbik::ParseTable originalTable2(originalLr2);
    EXPECT_EQ(originalLr2.statistics(), (zbik::LRkDfaStats{738, 81179, 2103}));
    ASSERT_EQ(originalTable2.conflicts().size(), 18U);
    for (const zbik::Conflict &conflict: originalTable2.conflicts()) {
        const std::string dump = zbik::dumpEbnfConflict(originalSpecification, original, originalLr2, conflict);
        EXPECT_EQ(dump.find("'returnType'"), std::string::npos);
        EXPECT_NE(dump.find("'ifStatement', alternative 0"), std::string::npos);
        EXPECT_NE(dump.find("'ifStatement', alternative 1"), std::string::npos);
    }

    const zbik::EbnfGrammarSpec resolvedSpecification = zbik::test::cminusClosedOpenEbnfGrammar();
    const zbik::EbnfConversionResult resolved =
            zbik::EbnfToBnfConverter{}.convertWithOrigins(resolvedSpecification, zbik::test::cminusTerminals());
    const zbik::LRkDfa canonical1(resolved.grammar(), 1);
    const zbik::ParseTable canonicalTable1(canonical1);
    ASSERT_EQ(canonicalTable1.conflicts().size(), 1U);
    const std::string resolvedLr1Conflict =
            zbik::dumpEbnfConflict(resolvedSpecification, resolved, canonical1, canonicalTable1.conflicts().front());
    EXPECT_NE(resolvedLr1Conflict.find("'returnType', alternative 0"), std::string::npos);
    EXPECT_NE(resolvedLr1Conflict.find("'declarator', alternative 0"), std::string::npos);

    const zbik::LRkDfa canonical(resolved.grammar(), 2);
    const zbik::ParseTable canonicalTable(canonical);
    const zbik::LALRkDfa lalr(canonical);
    const zbik::ParseTable lalrTable(lalr);
    const zbik::DirectLALRkDfa directLalr(resolved.grammar(), 2);
    const zbik::ParseTable directLalrTable(directLalr);
    const auto selective = zbik::SelectiveLRkMerger::merge(canonical);
    const zbik::ParseTable selectiveTable(selective.graph);
    EXPECT_FALSE(canonicalTable.hasConflicts());
    EXPECT_FALSE(lalrTable.hasConflicts());
    EXPECT_FALSE(directLalrTable.hasConflicts());
    EXPECT_FALSE(selectiveTable.hasConflicts());
    EXPECT_EQ(canonical.statistics(), (zbik::LRkDfaStats{765, 93220, 2240}));
    EXPECT_EQ(lalr.statistics(), (zbik::LRkDfaStats{139, 18779, 337}));
    EXPECT_EQ(directLalr.statistics(), lalr.statistics());
    EXPECT_TRUE(std::ranges::equal(lalr.states(), directLalr.states()));
    EXPECT_EQ(directLalrTable.dump(), lalrTable.dump());
    EXPECT_EQ(selective.graph.states().size(), 139U);
    EXPECT_TRUE(selective.statistics.usedLalr);
    EXPECT_EQ(selective.statistics.lalrConflicts, 0U);
    EXPECT_EQ(selective.statistics.attempts, 0U);
    const zbik::FirstKAnalysis resolvedFirst(resolved.grammar(), 2);
    const zbik::FollowKAnalysis resolvedFollow(resolved.grammar(), resolvedFirst);
    std::size_t resolvedFirstWords = 0;
    std::size_t resolvedFollowWords = 0;
    for (std::size_t index = 0; index < resolved.grammar().nonterminalCount(); ++index) {
        const zbik::NonterminalId id{static_cast<std::uint32_t>(index)};
        resolvedFirstWords += resolvedFirst.first(id).size();
        resolvedFollowWords += resolvedFollow.follow(id).size();
    }
    const auto canonicalStorage = zbik::CompressedParseTable(canonicalTable).statistics();
    const auto lalrStorage = zbik::CompressedParseTable(lalrTable).statistics();
    EXPECT_EQ(resolved.grammar().ruleCount(), 91U);
    EXPECT_EQ(resolved.grammar().nonterminalCount(), 49U);
    EXPECT_EQ(resolved.grammar().terminalCount(), 34U);
    EXPECT_EQ(resolvedFirstWords, 459U);
    EXPECT_EQ(resolvedFollowWords, 1811U);
    EXPECT_EQ(canonicalStorage.uncompressedBytes, 618212U);
    EXPECT_EQ(canonicalStorage.compressedBytes, 100460U);
    EXPECT_EQ(lalrStorage.uncompressedBytes, 143964U);
    EXPECT_EQ(lalrStorage.compressedBytes, 26452U);

    const zbik::LRMachine canonicalMachine(canonicalTable);
    const zbik::LRMachine lalrMachine(lalrTable);
    const std::vector<std::vector<zbik::TerminalId>> valid{
            tokens(resolved.grammar(), {"INT", "ID", "SEMI"}),
            tokens(resolved.grammar(), {"VOID",   "ID",     "LPAREN", "RPAREN", "LBRACE", "IF",   "LPAREN", "ID",
                                        "LT",     "ID",     "RPAREN", "IF",     "LPAREN", "ID",   "LT",     "ID",
                                        "RPAREN", "RETURN", "SEMI",   "ELSE",   "RETURN", "SEMI", "RBRACE"}),
            tokens(resolved.grammar(), {"VOID",   "ID",     "LPAREN", "RPAREN", "LBRACE", "WHILE", "LPAREN", "ID",
                                        "LT",     "ID",     "RPAREN", "IF",     "LPAREN", "ID",    "LT",     "ID",
                                        "RPAREN", "RETURN", "SEMI",   "ELSE",   "RETURN", "SEMI",  "RBRACE"}),
    };
    for (const auto &input: valid) {
        EXPECT_TRUE(canonicalMachine.parse(input).accepted);
        EXPECT_TRUE(lalrMachine.parse(input).accepted);
    }

    const auto unmatchedElse = tokens(resolved.grammar(),
                                      {"VOID", "ID", "LPAREN", "RPAREN", "LBRACE", "ELSE", "RETURN", "SEMI", "RBRACE"});
    EXPECT_FALSE(canonicalMachine.parse(unmatchedElse).accepted);
    EXPECT_FALSE(lalrMachine.parse(unmatchedElse).accepted);
}

TEST(EbnfRealGrammarTest, ClosedOpenPreservesTheBoundedDanglingElseLanguage) {
    const zbik::EbnfGrammarSpec ambiguous{
            zbik::EbnfRuleSpec{"statement",
                               {
                                       zbik::EbnfAlternative{zbik::EbnfElement{"OTHER"}},
                                       zbik::EbnfAlternative{zbik::EbnfElement{"IF"}, zbik::EbnfElement{"statement"}},
                                       zbik::EbnfAlternative{zbik::EbnfElement{"IF"}, zbik::EbnfElement{"statement"},
                                                             zbik::EbnfElement{"ELSE"}, zbik::EbnfElement{"statement"}},
                               }},
    };
    const zbik::EbnfGrammarSpec closedOpen{
            zbik::EbnfRuleSpec{"statement",
                               {
                                       zbik::EbnfAlternative{zbik::EbnfElement{"closedStatement"}},
                                       zbik::EbnfAlternative{zbik::EbnfElement{"openStatement"}},
                               }},
            zbik::EbnfRuleSpec{
                    "closedStatement",
                    {
                            zbik::EbnfAlternative{zbik::EbnfElement{"OTHER"}},
                            zbik::EbnfAlternative{zbik::EbnfElement{"IF"}, zbik::EbnfElement{"closedStatement"},
                                                  zbik::EbnfElement{"ELSE"}, zbik::EbnfElement{"closedStatement"}},
                    }},
            zbik::EbnfRuleSpec{
                    "openStatement",
                    {
                            zbik::EbnfAlternative{zbik::EbnfElement{"IF"}, zbik::EbnfElement{"statement"}},
                            zbik::EbnfAlternative{zbik::EbnfElement{"IF"}, zbik::EbnfElement{"closedStatement"},
                                                  zbik::EbnfElement{"ELSE"}, zbik::EbnfElement{"openStatement"}},
                    }},
    };
    const zbik::Grammar ambiguousGrammar = zbik::EbnfToBnfConverter{}.convert(ambiguous, {"IF", "ELSE", "OTHER"});
    const zbik::Grammar closedOpenGrammar = zbik::EbnfToBnfConverter{}.convert(closedOpen, {"IF", "ELSE", "OTHER"});

    EXPECT_EQ(generatedWords(ambiguousGrammar, 9), generatedWords(closedOpenGrammar, 9));
}

TEST(EbnfRealGrammarTest, UnifiedCDeclaratorIsLr1WhileEarlyClassificationConflicts) {
    const zbik::EbnfGrammarSpec legacySpecification = zbik::test::cDeclaratorLegacyEbnfGrammar();
    const zbik::EbnfConversionResult legacy = zbik::EbnfToBnfConverter{}.convertWithOrigins(
            legacySpecification, zbik::test::cDeclaratorSubsetTerminals());
    for (const std::size_t k: {1U, 2U, 3U}) {
        const zbik::FirstKAnalysis first(legacy.grammar(), k);
        const auto probe = zbik::LRkDfa::buildUntilFirstConflict(legacy.grammar(), first);
        EXPECT_TRUE(probe.conflict);
        EXPECT_EQ(probe.completedStates, 3U);
        EXPECT_EQ(probe.discoveredStates, 19U);
    }

    const zbik::EbnfGrammarSpec specification = zbik::test::cDeclaratorSubsetEbnfGrammar();
    const zbik::EbnfConversionResult conversion =
            zbik::EbnfToBnfConverter{}.convertWithOrigins(specification, zbik::test::cDeclaratorSubsetTerminals());
    const zbik::Grammar &grammar = conversion.grammar();
    const zbik::LRkDfa completeLr1(conversion.grammar(), 1);
    const zbik::ParseTable completeTable1(completeLr1);
    const zbik::LALRkDfa lalr1(completeLr1);
    const zbik::ParseTable lalrTable1(lalr1);
    const zbik::FirstKAnalysis first1(grammar, 1);
    const zbik::FollowKAnalysis follow1(grammar, first1);
    std::size_t firstWords = 0;
    std::size_t followWords = 0;
    for (std::size_t index = 0; index < grammar.nonterminalCount(); ++index) {
        const zbik::NonterminalId id{static_cast<std::uint32_t>(index)};
        firstWords += first1.first(id).size();
        followWords += follow1.follow(id).size();
    }
    const auto canonicalStorage = zbik::CompressedParseTable(completeTable1).statistics();
    const auto lalrStorage = zbik::CompressedParseTable(lalrTable1).statistics();

    EXPECT_EQ(grammar.ruleCount(), 59U);
    EXPECT_EQ(grammar.nonterminalCount(), 36U);
    EXPECT_EQ(grammar.terminalCount(), 18U);
    EXPECT_EQ(firstWords, 136U);
    EXPECT_EQ(followWords, 172U);
    EXPECT_FALSE(completeTable1.hasConflicts());
    EXPECT_EQ(completeLr1.statistics(), (zbik::LRkDfaStats{129, 1434, 229}));
    EXPECT_EQ(canonicalStorage.uncompressedBytes, 18220U);
    EXPECT_EQ(canonicalStorage.compressedBytes, 8376U);
    EXPECT_FALSE(lalrTable1.hasConflicts());
    EXPECT_EQ(lalr1.statistics(), (zbik::LRkDfaStats{77, 1024, 138}));
    EXPECT_EQ(lalrStorage.uncompressedBytes, 12684U);
    EXPECT_EQ(lalrStorage.compressedBytes, 5728U);

    const zbik::LRMachine canonicalMachine(completeTable1);
    const zbik::LRMachine lalrMachine(lalrTable1);
    const std::vector<std::vector<zbik::TerminalId>> valid{
            tokens(grammar, {"'int'", "Identifier", "';'"}),
            tokens(grammar, {"'int'", "Identifier", "'('", "')'", "';'"}),
            tokens(grammar, {"'int'", "'*'", "Identifier", "'('", "')'", "';'"}),
            tokens(grammar, {"'int'", "'('", "'*'", "Identifier", "')'", "'('", "')'", "';'"}),
            tokens(grammar, {"'int'", "Identifier", "'['", "DecimalConstant", "']'", "';'"}),
            tokens(grammar, {"'int'", "'('", "'*'", "Identifier", "'['", "DecimalConstant", "']'", "')'", "'('",
                             "'int'", "Identifier", "')'", "';'"}),
            tokens(grammar, {"'int'", "Identifier", "','", "'*'", "Identifier", "','", "Identifier", "'('", "')'",
                             "','", "'('", "'*'", "Identifier", "')'", "'('", "')'", "';'"}),
            tokens(grammar, {"'int'", "Identifier", "'='", "DecimalConstant", "','", "Identifier", "'='",
                             "DecimalConstant", "';'"}),
            tokens(grammar, {"'int'",      "Identifier", "'('",      "'int'",      "Identifier", "','",    "'char'",
                             "Identifier", "','",        "'double'", "Identifier", "','",        "'void'", "'*'",
                             "Identifier", "','",        "'int'",    "Identifier", "')'",        "'{'",    "'}'"}),
            tokens(grammar, {"'int'", "Identifier", "'('", "Identifier", "','", "Identifier", "')'", "'int'",
                             "Identifier", "';'", "'char'", "Identifier", "';'", "'{'", "'}'"}),
    };
    for (const auto &input: valid) {
        EXPECT_TRUE(canonicalMachine.parse(input).accepted);
        EXPECT_TRUE(lalrMachine.parse(input).accepted);
    }

    // These forms are syntactically declarators; a later semantic pass must reject them.
    const std::vector<std::vector<zbik::TerminalId>> semanticErrors{
            tokens(grammar, {"'int'", "Identifier", "'('", "')'", "'='", "DecimalConstant", "';'"}),
            tokens(grammar, {"'int'", "Identifier", "'('", "')'", "'['", "DecimalConstant", "']'", "';'"}),
    };
    for (const auto &input: semanticErrors) {
        EXPECT_TRUE(canonicalMachine.parse(input).accepted);
        EXPECT_TRUE(lalrMachine.parse(input).accepted);
    }

    const std::vector<std::vector<zbik::TerminalId>> syntaxErrors{
            tokens(grammar, {"'int'", "Identifier"}),
            tokens(grammar, {"'int'", "'('", "'*'", "Identifier", "'('", "')'", "';'"}),
    };
    for (const auto &input: syntaxErrors) {
        EXPECT_FALSE(canonicalMachine.parse(input).accepted);
        EXPECT_FALSE(lalrMachine.parse(input).accepted);
    }
}
