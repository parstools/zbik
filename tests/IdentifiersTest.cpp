#include <gtest/gtest.h>

#include <type_traits>
#include <unordered_set>

#include "grammar/Identifiers.h"

static_assert(!std::is_same_v<zbik::TerminalId, zbik::NonterminalId>);
static_assert(!std::is_same_v<zbik::TerminalId, zbik::RuleId>);
static_assert(!std::is_convertible_v<zbik::TerminalId, zbik::NonterminalId>);

TEST(IdentifiersTest, ProvideValueOrdering) {
    EXPECT_LT(zbik::TerminalId{1}, zbik::TerminalId{2});
    EXPECT_EQ(zbik::NonterminalId{3}, zbik::NonterminalId{3});
    EXPECT_NE(zbik::RuleId{4}, zbik::RuleId{5});
}

TEST(IdentifiersTest, WorkAsHashKeys) {
    std::unordered_set<zbik::TerminalId> terminalIds;
    std::unordered_set<zbik::NonterminalId> nonterminalIds;
    std::unordered_set<zbik::RuleId> ruleIds;
    std::unordered_set<zbik::SymbolRef> symbols;

    terminalIds.insert(zbik::TerminalId{7});
    terminalIds.insert(zbik::TerminalId{7});
    terminalIds.insert(zbik::TerminalId{8});
    nonterminalIds.insert(zbik::NonterminalId{7});
    ruleIds.insert(zbik::RuleId{7});
    symbols.insert(zbik::TerminalId{7});
    symbols.insert(zbik::NonterminalId{7});

    EXPECT_EQ(terminalIds.size(), 2U);
    EXPECT_TRUE(terminalIds.contains(zbik::TerminalId{7}));
    EXPECT_TRUE(nonterminalIds.contains(zbik::NonterminalId{7}));
    EXPECT_TRUE(ruleIds.contains(zbik::RuleId{7}));
    EXPECT_EQ(symbols.size(), 2U);
}

TEST(IdentifiersTest, SymbolRefPreservesSymbolKind) {
    const zbik::SymbolRef terminal = zbik::TerminalId{1};
    const zbik::SymbolRef nonterminal = zbik::NonterminalId{1};

    EXPECT_TRUE(std::holds_alternative<zbik::TerminalId>(terminal));
    EXPECT_TRUE(std::holds_alternative<zbik::NonterminalId>(nonterminal));
    EXPECT_NE(terminal, nonterminal);
}
