#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include "first/WordSetK.h"

namespace {

zbik::LookaheadWord word(std::initializer_list<zbik::LookaheadSymbol> symbols) {
    return zbik::LookaheadWord{symbols};
}

struct ConstantWordHash {
    std::size_t operator()(const zbik::LookaheadWord &) const noexcept {
        return 0;
    }
};

} // namespace

TEST(LookaheadWordTest, RepresentsEpsilonAsEmptyWord) {
    const zbik::LookaheadWord epsilon;

    EXPECT_TRUE(epsilon.empty());
    EXPECT_EQ(epsilon.size(), 0U);
    EXPECT_FALSE(epsilon.endsWithEndOfInput());
    EXPECT_EQ(epsilon.dump(), "[]");
}

TEST(LookaheadWordTest, EnforcesEndOfInputPosition) {
    const zbik::LookaheadWord terminalThenEnd = word({zbik::TerminalId{0}, zbik::endOfInput});

    EXPECT_TRUE(terminalThenEnd.endsWithEndOfInput());
    EXPECT_EQ(terminalThenEnd.dump(), "[t0 $]");
    EXPECT_THROW(static_cast<void>(word({zbik::endOfInput, zbik::TerminalId{0}})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(word({zbik::endOfInput, zbik::endOfInput})), std::invalid_argument);
}

TEST(WordSetKTest, StoresOnlyDistinctWordsInStructuralOrder) {
    zbik::WordSetK set(2);

    EXPECT_TRUE(set.add(word({zbik::TerminalId{1}})));
    EXPECT_TRUE(set.add(word({})));
    EXPECT_TRUE(set.add(word({zbik::TerminalId{0}, zbik::TerminalId{1}})));
    EXPECT_TRUE(set.add(word({zbik::endOfInput})));
    EXPECT_TRUE(set.add(word({zbik::TerminalId{0}})));
    EXPECT_FALSE(set.add(word({zbik::TerminalId{1}})));

    EXPECT_EQ(set.size(), 5U);
    EXPECT_EQ(set.dump(), "{[] [t0] [t0 t1] [t1] [$]}");
}

TEST(WordSetKTest, EnforcesMaximumWordLength) {
    zbik::WordSetK set(2);

    EXPECT_TRUE(set.add(word({zbik::TerminalId{0}, zbik::endOfInput})));
    EXPECT_THROW(set.add(word({zbik::TerminalId{0}, zbik::TerminalId{1}, zbik::endOfInput})), std::invalid_argument);

    zbik::WordSetK zeroLookahead(0);
    EXPECT_TRUE(zeroLookahead.add(word({})));
    EXPECT_THROW(zeroLookahead.add(word({zbik::TerminalId{0}})), std::invalid_argument);
}

TEST(WordSetKTest, ComputesUnionAndReportsWhetherItChanged) {
    zbik::WordSetK left(2, {word({}), word({zbik::TerminalId{0}})});
    const zbik::WordSetK right(2, {word({zbik::TerminalId{0}}), word({zbik::TerminalId{1}, zbik::endOfInput})});

    EXPECT_TRUE(left.unionWith(right));
    EXPECT_EQ(left.dump(), "{[] [t0] [t1 $]}");
    EXPECT_FALSE(left.unionWith(right));

    const zbik::WordSetK incompatible(1);
    EXPECT_THROW(left.unionWith(incompatible), std::invalid_argument);
}

TEST(WordSetKTest, SupportsStructuralComparison) {
    const zbik::WordSetK first(2, {word({zbik::TerminalId{0}})});
    const zbik::WordSetK same(2, {word({zbik::TerminalId{0}})});
    const zbik::WordSetK differentWord(2, {word({zbik::TerminalId{1}})});
    const zbik::WordSetK differentLimit(3, {word({zbik::TerminalId{0}})});

    EXPECT_EQ(first, same);
    EXPECT_NE(first, differentWord);
    EXPECT_NE(first, differentLimit);
    EXPECT_LT(first, differentWord);
}

TEST(WordSetKTest, WordIdentityDoesNotDependOnHashUniqueness) {
    const std::unordered_set<zbik::LookaheadWord, ConstantWordHash> words{
            word({}),
            word({zbik::TerminalId{0}}),
            word({zbik::TerminalId{1}}),
            word({zbik::endOfInput}),
            word({zbik::TerminalId{0}}),
    };

    EXPECT_EQ(words.size(), 4U);
    EXPECT_TRUE(words.contains(word({})));
    EXPECT_TRUE(words.contains(word({zbik::endOfInput})));
}

TEST(WordSetKTest, ConcatenatesLanguagesAndTruncatesAtLengthLimit) {
    const zbik::WordSetK left(3, {
            word({}),
            word({zbik::TerminalId{0}}),
            word({zbik::TerminalId{1}, zbik::TerminalId{0}}),
    });
    const zbik::WordSetK right(3, {
            word({zbik::TerminalId{2}, zbik::TerminalId{0}, zbik::endOfInput}),
            word({zbik::TerminalId{3}, zbik::endOfInput}),
    });

    const zbik::WordSetK result = zbik::concatenateTruncated(left, right);

    EXPECT_EQ(result.dump(),
              "{[t0 t2 t0] [t0 t3 $] [t1 t0 t2] [t1 t0 t3] [t2 t0 $] [t3 $]}");
}

TEST(WordSetKTest, TreatsEpsilonAsConcatenationIdentity) {
    const zbik::WordSetK epsilon(3, {word({})});
    const zbik::WordSetK language(3, {
            word({}),
            word({zbik::TerminalId{0}}),
            word({zbik::TerminalId{1}, zbik::endOfInput}),
    });

    EXPECT_EQ(zbik::concatenateTruncated(epsilon, language), language);
    EXPECT_EQ(zbik::concatenateTruncated(language, epsilon), language);
}

TEST(WordSetKTest, StopsAtEndOfInputOrAtAnAlreadyFullLeftWord) {
    const zbik::WordSetK left(2, {
            word({zbik::TerminalId{0}, zbik::TerminalId{1}}),
            word({zbik::TerminalId{2}, zbik::endOfInput}),
    });
    const zbik::WordSetK right(2, {
            word({zbik::TerminalId{3}}),
            word({zbik::TerminalId{4}, zbik::endOfInput}),
    });

    EXPECT_EQ(zbik::concatenateTruncated(left, right).dump(), "{[t0 t1] [t2 $]}");
}

TEST(WordSetKTest, KeepsEmptyLanguageAbsorbing) {
    const zbik::WordSetK empty(2);
    const zbik::WordSetK language(2, {
            word({}),
            word({zbik::TerminalId{0}}),
    });

    EXPECT_TRUE(zbik::concatenateTruncated(empty, language).empty());
    EXPECT_TRUE(zbik::concatenateTruncated(language, empty).empty());
}

TEST(WordSetKTest, KeepsEmptyLanguageAbsorbingForCompletedLeftWords) {
    const zbik::WordSetK empty(2);
    const zbik::WordSetK completed(2, {
            word({zbik::TerminalId{0}, zbik::TerminalId{1}}),
            word({zbik::TerminalId{2}, zbik::endOfInput}),
    });

    EXPECT_TRUE(zbik::concatenateTruncated(completed, empty).empty());
}

TEST(WordSetKTest, DeduplicatesEqualConcatenationResults) {
    const zbik::WordSetK left(1, {
            word({}),
            word({zbik::TerminalId{0}}),
    });
    const zbik::WordSetK right(1, {
            word({zbik::TerminalId{0}}),
            word({zbik::TerminalId{1}}),
    });

    EXPECT_EQ(zbik::concatenateTruncated(left, right).dump(), "{[t0] [t1]}");
}

TEST(WordSetKTest, DefinesZeroLookaheadAndRejectsMismatchedLimits) {
    const zbik::WordSetK left(0, {word({})});
    const zbik::WordSetK right(0, {word({})});

    EXPECT_EQ(zbik::concatenateTruncated(left, right).dump(), "{[]}");

    const zbik::WordSetK incompatible(1, {word({})});
    EXPECT_THROW(
            static_cast<void>(zbik::concatenateTruncated(left, incompatible)),
            std::invalid_argument);
}

TEST(WordSetKTest, AllocatesForExistingWordsInsteadOfTheMaximumLength) {
    constexpr std::size_t hugeLimit = std::numeric_limits<std::size_t>::max();
    const zbik::WordSetK left(hugeLimit, {word({zbik::TerminalId{0}})});
    const zbik::WordSetK right(hugeLimit, {word({zbik::TerminalId{1}})});

    EXPECT_EQ(zbik::concatenateTruncated(left, right).dump(), "{[t0 t1]}");
}
