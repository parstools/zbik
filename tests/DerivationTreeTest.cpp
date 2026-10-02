#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "generator/DerivationTree.h"
#include "grammar/GrammarBuilder.h"

namespace {

zbik::DerivationTree epsilonTree(
        const zbik::Grammar &grammar, zbik::RuleId rule) {
    return {grammar, rule, {}};
}

zbik::DerivationTree terminalTree(
        const zbik::Grammar &grammar, zbik::RuleId rule,
        zbik::TerminalId terminal) {
    std::vector<zbik::DerivationTree::Child> children;
    children.push_back(zbik::DerivationTree::terminal(terminal));
    return {grammar, rule, std::move(children)};
}

zbik::DerivationTree unaryTree(
        const zbik::Grammar &grammar, zbik::RuleId rule,
        zbik::DerivationTree child) {
    std::vector<zbik::DerivationTree::Child> children;
    children.push_back(zbik::DerivationTree::subtree(std::move(child)));
    return {grammar, rule, std::move(children)};
}

} // namespace

TEST(DerivationTreeTest, ReturnsYieldFingerprintAndDiagnosticDump) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A A", "A -> x y", "A ->",
    });
    std::vector<zbik::DerivationTree::Child> leftChildren;
    leftChildren.push_back(zbik::DerivationTree::terminal(zbik::TerminalId{0}));
    leftChildren.push_back(zbik::DerivationTree::terminal(zbik::TerminalId{1}));
    zbik::DerivationTree left(grammar, zbik::RuleId{1}, std::move(leftChildren));
    auto right = epsilonTree(grammar, zbik::RuleId{2});
    std::vector<zbik::DerivationTree::Child> children;
    children.push_back(zbik::DerivationTree::subtree(std::move(left)));
    children.push_back(zbik::DerivationTree::subtree(std::move(right)));
    const zbik::DerivationTree tree(grammar, zbik::RuleId{0}, std::move(children));

    EXPECT_EQ(tree.rule(), zbik::RuleId{0});
    EXPECT_EQ(tree.rootNonterminal(grammar), grammar.start());
    EXPECT_EQ(tree.terminals(),
              (std::vector{zbik::TerminalId{0}, zbik::TerminalId{1}}));
    EXPECT_EQ(tree.structuralFingerprint(), "R0[R1[T0;T1;]R2[]]");
    EXPECT_EQ(tree.dump(grammar), R"(S#R0(A#R1("x","y"),A#R2()))");
}

TEST(DerivationTreeTest, DistinguishesTreesForDuplicateProductions) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A", "A -> x", "A -> x",
    });
    const auto first = unaryTree(
            grammar, zbik::RuleId{0},
            terminalTree(grammar, zbik::RuleId{1}, zbik::TerminalId{0}));
    const auto second = unaryTree(
            grammar, zbik::RuleId{0},
            terminalTree(grammar, zbik::RuleId{2}, zbik::TerminalId{0}));

    EXPECT_EQ(first.terminals(), second.terminals());
    EXPECT_NE(first, second);
    EXPECT_NE(first.structuralFingerprint(), second.structuralFingerprint());
    EXPECT_NE(first.dump(grammar), second.dump(grammar));
}

TEST(DerivationTreeTest, HasDeepCopyValueSemantics) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> A", "A -> token"});
    const auto original = unaryTree(
            grammar, zbik::RuleId{0},
            terminalTree(grammar, zbik::RuleId{1}, zbik::TerminalId{0}));
    zbik::DerivationTree copy(original);

    EXPECT_EQ(copy, original);
    const auto &originalChild = std::get<std::unique_ptr<zbik::DerivationTree>>(
            original.children().front());
    const auto &copyChild = std::get<std::unique_ptr<zbik::DerivationTree>>(
            copy.children().front());
    EXPECT_NE(originalChild.get(), copyChild.get());

    auto assigned = terminalTree(grammar, zbik::RuleId{1}, zbik::TerminalId{0});
    assigned = original;
    EXPECT_EQ(assigned, original);
}

TEST(DerivationTreeTest, ValidatesChildrenAgainstTheSelectedRule) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A", "A -> x", "B -> y",
    });

    EXPECT_THROW(
            zbik::DerivationTree(grammar, zbik::RuleId{0}, {}),
            std::invalid_argument);

    std::vector<zbik::DerivationTree::Child> terminalInsteadOfSubtree;
    terminalInsteadOfSubtree.push_back(
            zbik::DerivationTree::terminal(zbik::TerminalId{0}));
    EXPECT_THROW(
            zbik::DerivationTree(
                    grammar, zbik::RuleId{0}, std::move(terminalInsteadOfSubtree)),
            std::invalid_argument);

    std::vector<zbik::DerivationTree::Child> wrongSubtree;
    wrongSubtree.push_back(zbik::DerivationTree::subtree(
            terminalTree(grammar, zbik::RuleId{2}, zbik::TerminalId{1})));
    EXPECT_THROW(
            zbik::DerivationTree(grammar, zbik::RuleId{0}, std::move(wrongSubtree)),
            std::invalid_argument);

    std::vector<zbik::DerivationTree::Child> nullSubtree;
    nullSubtree.emplace_back(std::unique_ptr<zbik::DerivationTree>{});
    EXPECT_THROW(
            zbik::DerivationTree(grammar, zbik::RuleId{0}, std::move(nullSubtree)),
            std::invalid_argument);

    EXPECT_THROW(
            terminalTree(grammar, zbik::RuleId{1}, zbik::TerminalId{1}),
            std::invalid_argument);
}

TEST(DerivationTreeTest, EscapesTerminalNamesInDiagnosticDump) {
    const auto grammar = zbik::GrammarBuilder{}.build({R"(S -> a"b\c)"});
    const auto tree = terminalTree(grammar, zbik::RuleId{0}, zbik::TerminalId{0});

    EXPECT_EQ(tree.dump(grammar), R"(S#R0("a\"b\\c"))");
}
