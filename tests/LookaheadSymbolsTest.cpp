#include <gtest/gtest.h>

#include <type_traits>
#include <unordered_set>
#include <vector>

#include "first/LookaheadSymbols.h"
#include "grammar/GrammarBuilder.h"

static_assert(std::is_constructible_v<zbik::LookaheadSymbol, zbik::EndOfInput>);

TEST(LookaheadSymbolsTest, RepresentsEpsilonAsEmptySequence) {
    const std::vector<zbik::LookaheadSymbol> emptySequence;

    EXPECT_TRUE(emptySequence.empty());
    EXPECT_TRUE(zbik::isValidLookaheadSequence(emptySequence));
}

TEST(LookaheadSymbolsTest, AllowsEndOfInputOnlyAtSequenceEnd) {
    const zbik::LookaheadSymbol token = zbik::TerminalId{0};
    const zbik::LookaheadSymbol eof = zbik::endOfInput;

    EXPECT_TRUE(zbik::isValidLookaheadSequence(std::vector<zbik::LookaheadSymbol>{token}));
    EXPECT_TRUE(zbik::isValidLookaheadSequence(std::vector<zbik::LookaheadSymbol>{eof}));
    EXPECT_TRUE(zbik::isValidLookaheadSequence(std::vector<zbik::LookaheadSymbol>{token, eof}));
    EXPECT_FALSE(zbik::isValidLookaheadSequence(std::vector<zbik::LookaheadSymbol>{eof, token}));
    EXPECT_FALSE(zbik::isValidLookaheadSequence(std::vector<zbik::LookaheadSymbol>{eof, eof}));
}

TEST(LookaheadSymbolsTest, DistinguishesUserTerminalNamedEofFromEndMarker) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> EOF"});
    const zbik::TerminalId userEof = grammar.findTerminal("EOF").value();
    const zbik::LookaheadSymbol userToken = userEof;
    const zbik::LookaheadSymbol internalEof = zbik::endOfInput;

    EXPECT_NE(userToken, internalEof);
    EXPECT_FALSE(zbik::isEndOfInput(userToken));
    EXPECT_TRUE(zbik::isEndOfInput(internalEof));
    EXPECT_EQ(grammar.terminalName(userEof), "EOF");
}

TEST(LookaheadSymbolsTest, SupportsHashingInternalMarkers) {
    const std::unordered_set<zbik::LookaheadSymbol> symbols{
            zbik::TerminalId{0},
            zbik::endOfInput,
    };

    EXPECT_EQ(symbols.size(), 2U);
    EXPECT_TRUE(symbols.contains(zbik::endOfInput));
}
