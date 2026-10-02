#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "ebnf/EbnfToBnf.h"
#include "first/FirstK.h"
#include "generator/BoundedDerivationGenerator.h"
#include "grammar/GrammarBuilder.h"
#include "lr/LRkDfa.h"
#include "lr/ParseTable.h"

namespace {

using Word = std::vector<std::string>;
using Language = std::set<Word>;

Language concatenate(const Language &left, const Language &right, std::size_t maxLength) {
    Language result;
    for (const Word &prefix: left) {
        for (const Word &suffix: right) {
            if (prefix.size() + suffix.size() > maxLength) {
                continue;
            }
            Word word = prefix;
            word.insert(word.end(), suffix.begin(), suffix.end());
            result.insert(std::move(word));
        }
    }
    return result;
}

Language kleeneStar(const Language &base, std::size_t maxLength) {
    Language result{Word{}};
    for (;;) {
        Language expanded = result;
        const Language appended = concatenate(result, base, maxLength);
        expanded.insert(appended.begin(), appended.end());
        if (expanded == result) {
            return result;
        }
        result = std::move(expanded);
    }
}

Language applyRepetition(const Language &base, zbik::Repetition repetition, std::size_t maxLength) {
    switch (repetition) {
        case zbik::Repetition::One:
            return base;
        case zbik::Repetition::Optional: {
            Language result = base;
            result.insert(Word{});
            return result;
        }
        case zbik::Repetition::ZeroOrMore:
            return kleeneStar(base, maxLength);
        case zbik::Repetition::OneOrMore:
            return concatenate(base, kleeneStar(base, maxLength), maxLength);
    }
    throw std::logic_error("unknown repetition in test oracle");
}

Language directEbnfLanguage(const zbik::EbnfGrammarSpec &specification, std::size_t maxLength) {
    std::map<std::string, Language> languages;
    for (const zbik::EbnfRuleSpec &rule: specification.rules()) {
        languages.emplace(rule.lhs(), Language{});
    }

    bool changed;
    do {
        changed = false;
        for (const zbik::EbnfRuleSpec &rule: specification.rules()) {
            Language additions;
            for (const zbik::EbnfAlternative &alternative: rule.alternatives()) {
                Language words{Word{}};
                for (const zbik::EbnfElement &element: alternative.elements()) {
                    const auto nonterminal = languages.find(element.symbol());
                    const Language base =
                            nonterminal == languages.end() ? Language{Word{element.symbol()}} : nonterminal->second;
                    words = concatenate(words, applyRepetition(base, element.repetition(), maxLength), maxLength);
                }
                additions.insert(words.begin(), words.end());
            }
            Language &target = languages.at(rule.lhs());
            const std::size_t oldSize = target.size();
            target.insert(additions.begin(), additions.end());
            changed = changed || target.size() != oldSize;
        }
    } while (changed);
    return languages.at(specification.startSymbol());
}

Language generatedBnfLanguage(const zbik::Grammar &grammar, std::size_t maxLength) {
    Language result;
    zbik::BoundedDerivationGenerator generator(grammar, maxLength);
    std::size_t generatedTrees = 0;
    while (generator.next()) {
        if (generatedTrees++ == 10000U) {
            throw std::runtime_error("generated BNF oracle exceeded its safety limit");
        }
        Word word;
        for (const zbik::TerminalId terminal: generator.currentTree().terminals()) {
            word.push_back(grammar.terminalName(terminal));
        }
        result.insert(std::move(word));
    }
    return result;
}

} // namespace

TEST(EbnfToBnfTest, ExpandsEveryQuantifierWithLeftRecursion) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S",
                               {
                                       zbik::EbnfAlternative{
                                               zbik::EbnfElement{"a", zbik::Repetition::Optional},
                                               zbik::EbnfElement{"b", zbik::Repetition::ZeroOrMore},
                                               zbik::EbnfElement{"c", zbik::Repetition::OneOrMore},
                                       },
                               }},
    };

    const zbik::Grammar grammar = zbik::EbnfToBnfConverter{}.convert(specification);

    EXPECT_EQ(grammar.dump(), "start: N0 S\n"
                              "nonterminals:\n"
                              "  N0: S\n"
                              "  N1: S__ebnf_0_0\n"
                              "  N2: S__ebnf_0_1\n"
                              "  N3: S__ebnf_0_2\n"
                              "terminals:\n"
                              "  T0: a\n"
                              "  T1: b\n"
                              "  T2: c\n"
                              "rules:\n"
                              "  R0: S -> S__ebnf_0_0 S__ebnf_0_1 S__ebnf_0_2\n"
                              "  R1: S__ebnf_0_0 -> a\n"
                              "  R2: S__ebnf_0_0 -> <epsilon>\n"
                              "  R3: S__ebnf_0_1 -> S__ebnf_0_1 b\n"
                              "  R4: S__ebnf_0_1 -> <epsilon>\n"
                              "  R5: S__ebnf_0_2 -> S__ebnf_0_2 c\n"
                              "  R6: S__ebnf_0_2 -> c\n");
}

TEST(EbnfToBnfTest, ExpandsOptionalToPresentAndEmptyProductions) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S", {zbik::EbnfAlternative{zbik::EbnfElement{"x", zbik::Repetition::Optional}}}},
    };

    const zbik::Grammar grammar = zbik::EbnfToBnfConverter{}.convert(specification);

    EXPECT_EQ(grammar.dump(), "start: N0 S\n"
                              "nonterminals:\n"
                              "  N0: S\n"
                              "  N1: S__ebnf_0_0\n"
                              "terminals:\n"
                              "  T0: x\n"
                              "rules:\n"
                              "  R0: S -> S__ebnf_0_0\n"
                              "  R1: S__ebnf_0_0 -> x\n"
                              "  R2: S__ebnf_0_0 -> <epsilon>\n");
}

TEST(EbnfToBnfTest, ExpandsZeroOrMoreWithLeftRecursionAndAnEmptyBase) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S", {zbik::EbnfAlternative{zbik::EbnfElement{"x", zbik::Repetition::ZeroOrMore}}}},
    };

    const zbik::Grammar grammar = zbik::EbnfToBnfConverter{}.convert(specification);

    EXPECT_EQ(grammar.dump(), "start: N0 S\n"
                              "nonterminals:\n"
                              "  N0: S\n"
                              "  N1: S__ebnf_0_0\n"
                              "terminals:\n"
                              "  T0: x\n"
                              "rules:\n"
                              "  R0: S -> S__ebnf_0_0\n"
                              "  R1: S__ebnf_0_0 -> S__ebnf_0_0 x\n"
                              "  R2: S__ebnf_0_0 -> <epsilon>\n");
}

TEST(EbnfToBnfTest, ExpandsOneOrMoreWithLeftRecursionAndOneSymbolBase) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S", {zbik::EbnfAlternative{zbik::EbnfElement{"x", zbik::Repetition::OneOrMore}}}},
    };

    const zbik::Grammar grammar = zbik::EbnfToBnfConverter{}.convert(specification);

    EXPECT_EQ(grammar.dump(), "start: N0 S\n"
                              "nonterminals:\n"
                              "  N0: S\n"
                              "  N1: S__ebnf_0_0\n"
                              "terminals:\n"
                              "  T0: x\n"
                              "rules:\n"
                              "  R0: S -> S__ebnf_0_0\n"
                              "  R1: S__ebnf_0_0 -> S__ebnf_0_0 x\n"
                              "  R2: S__ebnf_0_0 -> x\n");
}

TEST(EbnfToBnfTest, PreservesAlternativesEpsilonAndDuplicateProductions) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S",
                               {
                                       zbik::EbnfAlternative{zbik::EbnfElement{"token"}},
                                       zbik::EbnfAlternative{},
                                       zbik::EbnfAlternative{zbik::EbnfElement{"token"}},
                               }},
    };

    const zbik::Grammar grammar = zbik::EbnfToBnfConverter{}.convert(specification);

    ASSERT_EQ(grammar.ruleCount(), 3U);
    EXPECT_EQ(grammar.dump(), "start: N0 S\n"
                              "nonterminals:\n"
                              "  N0: S\n"
                              "terminals:\n"
                              "  T0: token\n"
                              "rules:\n"
                              "  R0: S -> token\n"
                              "  R1: S -> <epsilon>\n"
                              "  R2: S -> token\n");
}

TEST(EbnfToBnfTest, GivesEveryOccurrenceItsOwnHelper) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S",
                               {
                                       zbik::EbnfAlternative{
                                               zbik::EbnfElement{"item", zbik::Repetition::Optional},
                                               zbik::EbnfElement{"item", zbik::Repetition::Optional},
                                       },
                                       zbik::EbnfAlternative{
                                               zbik::EbnfElement{"item", zbik::Repetition::Optional},
                                       },
                               }},
    };

    const zbik::Grammar grammar = zbik::EbnfToBnfConverter{}.convert(specification);

    EXPECT_TRUE(grammar.findNonterminal("S__ebnf_0_0"));
    EXPECT_TRUE(grammar.findNonterminal("S__ebnf_0_1"));
    EXPECT_TRUE(grammar.findNonterminal("S__ebnf_1_0"));
    EXPECT_EQ(grammar.nonterminalCount(), 4U);
    EXPECT_EQ(grammar.ruleCount(), 8U);
}

TEST(EbnfToBnfTest, AvoidsNamesUsedByUserSymbolsDeterministically) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S",
                               {
                                       zbik::EbnfAlternative{
                                               zbik::EbnfElement{"item", zbik::Repetition::Optional},
                                               zbik::EbnfElement{"S__ebnf_0_0"},
                                       },
                               }},
            zbik::EbnfRuleSpec{"S__ebnf_0_0",
                               {
                                       zbik::EbnfAlternative{zbik::EbnfElement{"token"}},
                               }},
    };

    const zbik::Grammar first = zbik::EbnfToBnfConverter{}.convert(specification);
    const zbik::Grammar second = zbik::EbnfToBnfConverter{}.convert(specification);

    EXPECT_TRUE(first.findNonterminal("S__ebnf_0_0_2"));
    EXPECT_EQ(first.dump(), second.dump());
}

TEST(EbnfToBnfTest, MapsEveryGeneratedRuleToItsSource) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S",
                               {
                                       zbik::EbnfAlternative{
                                               zbik::EbnfElement{"prefix"},
                                               zbik::EbnfElement{"item", zbik::Repetition::Optional},
                                       },
                                       zbik::EbnfAlternative{zbik::EbnfElement{"other"}},
                               }},
    };

    const zbik::EbnfConversionResult result = zbik::EbnfToBnfConverter{}.convertWithOrigins(specification);

    ASSERT_EQ(result.grammar().ruleCount(), 4U);
    ASSERT_EQ(result.origins().size(), 4U);
    EXPECT_EQ(result.origin(zbik::RuleId{0}), (zbik::EbnfRuleOrigin{
                                                      zbik::RuleId{0},
                                                      0,
                                                      0,
                                                      std::nullopt,
                                                      zbik::Repetition::One,
                                                      zbik::EbnfGeneratedRuleRole::SourceAlternative,
                                                      std::nullopt,
                                              }));
    EXPECT_EQ(result.origin(zbik::RuleId{1}).alternativeIndex, 1U);
    EXPECT_EQ(result.origin(zbik::RuleId{2}), (zbik::EbnfRuleOrigin{
                                                      zbik::RuleId{2},
                                                      0,
                                                      0,
                                                      1,
                                                      zbik::Repetition::Optional,
                                                      zbik::EbnfGeneratedRuleRole::OptionalPresent,
                                                      "S__ebnf_0_1",
                                              }));
    EXPECT_EQ(result.origin(zbik::RuleId{3}).role, zbik::EbnfGeneratedRuleRole::OptionalEmpty);
    EXPECT_EQ(result.origin(zbik::RuleId{3}).helperName, "S__ebnf_0_1");
}

TEST(EbnfToBnfTest, PreservesDistinctOriginsForDuplicateAlternatives) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S",
                               {
                                       zbik::EbnfAlternative{zbik::EbnfElement{"token"}},
                                       zbik::EbnfAlternative{zbik::EbnfElement{"token"}},
                               }},
    };

    const zbik::EbnfConversionResult result = zbik::EbnfToBnfConverter{}.convertWithOrigins(specification);

    ASSERT_EQ(result.grammar().ruleCount(), 2U);
    EXPECT_NE(result.grammar().rule(zbik::RuleId{0}).id(), result.grammar().rule(zbik::RuleId{1}).id());
    EXPECT_EQ(result.origin(zbik::RuleId{0}).alternativeIndex, 0U);
    EXPECT_EQ(result.origin(zbik::RuleId{1}).alternativeIndex, 1U);
}

TEST(EbnfToBnfTest, RecordsTheAllocatedNameAfterAHelperCollision) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S",
                               {
                                       zbik::EbnfAlternative{
                                               zbik::EbnfElement{"item", zbik::Repetition::OneOrMore},
                                               zbik::EbnfElement{"S__ebnf_0_0"},
                                       },
                               }},
            zbik::EbnfRuleSpec{"S__ebnf_0_0", {zbik::EbnfAlternative{}}},
    };

    const zbik::EbnfConversionResult result = zbik::EbnfToBnfConverter{}.convertWithOrigins(specification);

    ASSERT_EQ(result.origins().size(), 4U);
    EXPECT_EQ(result.origin(zbik::RuleId{2}).helperName, "S__ebnf_0_0_2");
    EXPECT_EQ(result.origin(zbik::RuleId{2}).elementIndex, 0U);
    EXPECT_EQ(result.origin(zbik::RuleId{2}).repetition, zbik::Repetition::OneOrMore);
    EXPECT_EQ(result.origin(zbik::RuleId{2}).role, zbik::EbnfGeneratedRuleRole::RepetitionRecursive);
    EXPECT_EQ(result.origin(zbik::RuleId{3}).role, zbik::EbnfGeneratedRuleRole::RepetitionBase);
}

TEST(EbnfToBnfTest, UsesGrammarBuilderDiagnosticsWithAnExplicitAlphabet) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S", {zbik::EbnfAlternative{zbik::EbnfElement{"typo"}}}},
    };

    try {
        static_cast<void>(zbik::EbnfToBnfConverter{}.convertWithOrigins(specification, {"known"}));
        FAIL() << "Expected GrammarBuildError";
    } catch (const zbik::GrammarBuildError &error) {
        EXPECT_EQ(error.line(), 1U);
        EXPECT_NE(std::string{error.what()}.find("unknown symbol 'typo'"), std::string::npos);
    }

    EXPECT_THROW(static_cast<void>(zbik::EbnfToBnfConverter{}.convert(specification, {""})), zbik::GrammarBuildError);
}

TEST(EbnfToBnfTest, RejectsStarAndPlusAppliedToAnIndirectlyNullableSymbol) {
    for (const zbik::Repetition repetition: {zbik::Repetition::ZeroOrMore, zbik::Repetition::OneOrMore}) {
        const zbik::EbnfGrammarSpec specification{
                zbik::EbnfRuleSpec{"S", {zbik::EbnfAlternative{zbik::EbnfElement{"A", repetition}}}},
                zbik::EbnfRuleSpec{"A", {zbik::EbnfAlternative{zbik::EbnfElement{"B"}}}},
                zbik::EbnfRuleSpec{"B", {zbik::EbnfAlternative{}}},
        };

        try {
            static_cast<void>(zbik::EbnfToBnfConverter{}.convert(specification));
            FAIL() << "Expected EbnfConversionError";
        } catch (const zbik::EbnfConversionError &error) {
            EXPECT_EQ(error.sourceRuleIndex(), 0U);
            EXPECT_EQ(error.alternativeIndex(), 0U);
            EXPECT_EQ(error.elementIndex(), 0U);
            EXPECT_NE(std::string{error.what()}.find("requires non-nullable symbol 'A'"), std::string::npos);
        }
    }
}

TEST(EbnfToBnfTest, DetectsNullabilityIntroducedByAnotherQuantifier) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S", {zbik::EbnfAlternative{zbik::EbnfElement{"A", zbik::Repetition::OneOrMore}}}},
            zbik::EbnfRuleSpec{"A", {zbik::EbnfAlternative{zbik::EbnfElement{"token", zbik::Repetition::Optional}}}},
    };

    EXPECT_THROW(static_cast<void>(zbik::EbnfToBnfConverter{}.convert(specification)), zbik::EbnfConversionError);
}

TEST(EbnfToBnfTest, AllowsOptionalAppliedToANullableSymbol) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S", {zbik::EbnfAlternative{zbik::EbnfElement{"A", zbik::Repetition::Optional}}}},
            zbik::EbnfRuleSpec{"A", {zbik::EbnfAlternative{}}},
    };

    const zbik::Grammar grammar = zbik::EbnfToBnfConverter{}.convert(specification);

    EXPECT_EQ(grammar.ruleCount(), 4U);
}

TEST(EbnfToBnfTest, KeepsDumpIdsAndOriginsStableBetweenConversions) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S",
                               {zbik::EbnfAlternative{
                                       zbik::EbnfElement{"a", zbik::Repetition::Optional},
                                       zbik::EbnfElement{"plain"},
                                       zbik::EbnfElement{"b", zbik::Repetition::OneOrMore},
                               }}},
    };

    const zbik::EbnfConversionResult first = zbik::EbnfToBnfConverter{}.convertWithOrigins(specification);
    const zbik::EbnfConversionResult second = zbik::EbnfToBnfConverter{}.convertWithOrigins(specification);

    EXPECT_EQ(first.grammar().dump(), second.grammar().dump());
    EXPECT_TRUE(std::ranges::equal(first.origins(), second.origins()));
    ASSERT_EQ(first.origins().size(), first.grammar().ruleCount());
    for (std::size_t index = 0; index < first.origins().size(); ++index) {
        EXPECT_EQ(first.origins()[index].generatedRule.value, index);
    }
}

TEST(EbnfToBnfTest, PreservesTheBoundedLanguageOfTheEbnfSpecification) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S",
                               {zbik::EbnfAlternative{
                                       zbik::EbnfElement{"a", zbik::Repetition::Optional},
                                       zbik::EbnfElement{"b", zbik::Repetition::ZeroOrMore},
                                       zbik::EbnfElement{"c", zbik::Repetition::OneOrMore},
                               }}},
    };
    constexpr std::size_t maxLength = 4;
    const zbik::Grammar grammar = zbik::EbnfToBnfConverter{}.convert(specification);

    EXPECT_EQ(generatedBnfLanguage(grammar, maxLength), directEbnfLanguage(specification, maxLength));
}

TEST(EbnfToBnfTest, FeedsTheConvertedGrammarDirectlyIntoFirstAndLr) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S",
                               {zbik::EbnfAlternative{
                                       zbik::EbnfElement{"a", zbik::Repetition::Optional},
                                       zbik::EbnfElement{"b", zbik::Repetition::ZeroOrMore},
                                       zbik::EbnfElement{"c", zbik::Repetition::OneOrMore},
                               }}},
    };
    const zbik::Grammar grammar = zbik::EbnfToBnfConverter{}.convert(specification);
    const zbik::FirstKAnalysis first(grammar, 2);
    const zbik::LRkDfa dfa(grammar, first);
    const zbik::ParseTable table(dfa);

    EXPECT_FALSE(first.first(grammar.start()).empty());
    EXPECT_GT(dfa.states().size(), 1U);
    EXPECT_FALSE(table.hasConflicts());
}
