#include <gtest/gtest.h>

#include <stdexcept>
#include <unordered_set>
#include <vector>

#include "lr/Action.h"

TEST(ActionTest, RepresentsTypedShiftReduceAndAcceptActions) {
    const zbik::Action shift = zbik::Shift{zbik::StateId{7}};
    const zbik::Action reduce = zbik::Reduce{zbik::RuleId{3}};
    const zbik::Action accept = zbik::Accept{};

    EXPECT_EQ(std::get<zbik::Shift>(shift).target, (zbik::StateId{7}));
    EXPECT_EQ(std::get<zbik::Reduce>(reduce).rule, (zbik::RuleId{3}));
    EXPECT_TRUE(std::holds_alternative<zbik::Accept>(accept));
    EXPECT_EQ(zbik::dumpAction(shift), "shift 7");
    EXPECT_EQ(zbik::dumpAction(reduce), "reduce R3");
    EXPECT_EQ(zbik::dumpAction(accept), "accept");

    const std::unordered_set<zbik::StateId> states{{2}, {7}, {2}};
    EXPECT_EQ(states.size(), 2U);
}

TEST(ActionCellTest, RetainsEveryDistinctActionInDeterministicOrder) {
    zbik::ActionCell cell;
    EXPECT_TRUE(cell.empty());
    EXPECT_TRUE(cell.add(zbik::Reduce{{3}}));
    EXPECT_TRUE(cell.add(zbik::Shift{{8}}));
    EXPECT_TRUE(cell.add(zbik::Accept{}));
    EXPECT_TRUE(cell.add(zbik::Reduce{{1}}));
    EXPECT_FALSE(cell.add(zbik::Reduce{{3}}));

    EXPECT_EQ(cell.size(), 4U);
    EXPECT_TRUE(cell.hasConflict());
    EXPECT_TRUE(cell.contains(zbik::Shift{{8}}));
    EXPECT_TRUE(cell.contains(zbik::Reduce{{1}}));
    EXPECT_TRUE(cell.contains(zbik::Reduce{{3}}));
    EXPECT_TRUE(cell.contains(zbik::Accept{}));
    EXPECT_EQ(cell.actions()[0], (zbik::Action{zbik::Shift{{8}}}));
    EXPECT_EQ(cell.actions()[1], (zbik::Action{zbik::Reduce{{1}}}));
    EXPECT_EQ(cell.actions()[2], (zbik::Action{zbik::Reduce{{3}}}));
    EXPECT_EQ(cell.actions()[3], (zbik::Action{zbik::Accept{}}));
}

TEST(ActionCellTest, DuplicateActionsDoNotCreateAConflict) {
    const zbik::ActionCell cell{
            zbik::Shift{{4}}, zbik::Shift{{4}}, zbik::Shift{{4}},
    };
    EXPECT_EQ(cell.size(), 1U);
    EXPECT_FALSE(cell.hasConflict());
}

TEST(ConflictTest, ReportsShiftReduceWithStateLookaheadAndRule) {
    const zbik::ActionCell cell{zbik::Reduce{{2}}, zbik::Shift{{7}}};
    const zbik::Conflict conflict(
            zbik::StateId{3},
            zbik::LookaheadWord{{zbik::TerminalId{1}, zbik::endOfInput}},
            cell);

    EXPECT_EQ(conflict.state(), (zbik::StateId{3}));
    EXPECT_EQ(conflict.lookahead(),
              (zbik::LookaheadWord{{zbik::TerminalId{1}, zbik::endOfInput}}));
    EXPECT_TRUE(conflict.shiftReduce());
    EXPECT_FALSE(conflict.reduceReduce());
    EXPECT_FALSE(conflict.shiftAccept());
    EXPECT_FALSE(conflict.reduceAccept());
    EXPECT_EQ(conflict.reductionRules(), (std::vector{zbik::RuleId{2}}));
    EXPECT_EQ(conflict.dump(),
              "conflict shift/reduce in state 3 on [t1 $]: {shift 7, reduce R2}");
}

TEST(ConflictTest, ReportsEveryApplicableConflictKindAndReductionRule) {
    const zbik::ActionCell cell{
            zbik::Reduce{{3}}, zbik::Accept{}, zbik::Shift{{9}}, zbik::Reduce{{1}},
    };
    const zbik::Conflict conflict(
            zbik::StateId{5}, zbik::LookaheadWord{{zbik::endOfInput}}, cell);

    EXPECT_TRUE(conflict.shiftReduce());
    EXPECT_TRUE(conflict.reduceReduce());
    EXPECT_TRUE(conflict.shiftAccept());
    EXPECT_TRUE(conflict.reduceAccept());
    EXPECT_EQ(conflict.reductionRules(),
              (std::vector{zbik::RuleId{1}, zbik::RuleId{3}}));
    EXPECT_EQ(conflict.dump(),
              "conflict shift/reduce, reduce/reduce, shift/accept, reduce/accept "
              "in state 5 on [$]: {shift 9, reduce R1, reduce R3, accept}");
}

TEST(ConflictTest, RejectsEpsilonLookaheadAndNonconflictingCells) {
    const zbik::ActionCell one{zbik::Accept{}};
    const zbik::ActionCell many{zbik::Accept{}, zbik::Reduce{{0}}};

    EXPECT_THROW(
            static_cast<void>(zbik::Conflict(
                    zbik::StateId{0}, zbik::LookaheadWord{{zbik::endOfInput}}, one)),
            std::invalid_argument);
    EXPECT_THROW(
            static_cast<void>(zbik::Conflict(
                    zbik::StateId{0}, zbik::LookaheadWord{}, many)),
            std::invalid_argument);
}
