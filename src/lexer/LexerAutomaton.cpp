#include "lexer/LexerAutomaton.h"

#include <algorithm>
#include <map>
#include <utility>

#include "regex/RegexNfa.h"
#include "regex/RegexParser.h"

namespace zbik {
namespace {

struct CombinedTransition {
    CodePointClass codePoints;
    std::size_t target;
};

struct CombinedState {
    std::vector<std::size_t> epsilonTransitions;
    std::vector<CombinedTransition> transitions;
    std::optional<std::size_t> acceptingRule;
    std::optional<std::size_t> owner;
};

using StateSubset = std::vector<std::size_t>;

auto epsilonClosure(const std::vector<CombinedState> &states, const std::vector<std::size_t> &seeds)
        -> std::vector<std::size_t> {
    std::vector<bool> visited(states.size());
    std::vector<std::size_t> pending;
    for (const std::size_t seed: seeds) {
        if (!visited[seed]) {
            visited[seed] = true;
            pending.push_back(seed);
        }
    }
    while (!pending.empty()) {
        const std::size_t current = pending.back();
        pending.pop_back();
        for (const std::size_t target: states[current].epsilonTransitions) {
            if (!visited[target]) {
                visited[target] = true;
                pending.push_back(target);
            }
        }
    }
    std::vector<std::size_t> result;
    for (std::size_t state = 0; state < visited.size(); ++state) {
        if (visited[state])
            result.push_back(state);
    }
    return result;
}

auto rulesInSubset(const std::vector<CombinedState> &states, const StateSubset &subset, bool accepting)
        -> std::vector<std::size_t> {
    std::vector<std::size_t> result;
    for (const auto state : subset) {
        const auto rule = accepting ? states[state].acceptingRule : states[state].owner;
        if (rule) result.push_back(*rule);
    }
    std::ranges::sort(result);
    result.erase(std::ranges::unique(result).begin(), result.end());
    return result;
}

auto parseExpressions(std::span<const LexerRule> rules, std::optional<CodePoint> maximumCodePoint)
        -> std::vector<RegexAst> {
    std::vector<RegexAst> result;
    result.reserve(rules.size());
    for (std::size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex) {
        try {
            result.push_back(
                    RegexParser(maximumCodePoint.value_or(maxUnicodeCodePoint)).parse(rules[ruleIndex].pattern));
        } catch (const RegexParseError &error) {
            throw LexerBuildError(ruleIndex, error.what());
        }
    }
    return result;
}

auto buildCombinedNfa(std::span<const RegexAst> expressions, std::optional<CodePoint> maximumCodePoint)
        -> std::vector<CombinedState> {
    std::vector<CombinedState> combined(1);
    for (std::size_t ruleIndex = 0; ruleIndex < expressions.size(); ++ruleIndex) {
        const RegexNfa nfa = RegexNfa::fromRegex(expressions[ruleIndex]);
        const std::vector<NfaStateId> initial = nfa.epsilonClosure({nfa.startState()});
        if (std::ranges::binary_search(initial, nfa.acceptingState())) {
            throw LexerBuildError(ruleIndex, "token regex must not accept the empty string");
        }

        const std::size_t offset = combined.size();
        combined.resize(offset + nfa.states().size());
        combined.front().epsilonTransitions.push_back(offset + toIndex(nfa.startState()));
        for (std::size_t state = 0; state < nfa.states().size(); ++state) {
            const NfaState &source = nfa.states()[state];
            CombinedState &target = combined[offset + state];
            target.owner = ruleIndex;
            for (const NfaStateId epsilonTarget: source.epsilonTransitions) {
                target.epsilonTransitions.push_back(offset + toIndex(epsilonTarget));
            }
            for (const NfaByteTransition &transition: source.byteTransitions) {
                if (maximumCodePoint && !transition.bytes.ranges().empty() &&
                    transition.bytes.ranges().back().last > *maximumCodePoint) {
                    throw LexerBuildError(ruleIndex, "lexer rule contains a code point outside its alphabet");
                }
                target.transitions.push_back({transition.bytes, offset + toIndex(transition.target)});
            }
        }
        combined[offset + toIndex(nfa.acceptingState())].acceptingRule = ruleIndex;
    }
    return combined;
}

} // namespace

LexerAutomaton::LexerAutomaton(std::span<const LexerRule> rules, std::optional<CodePoint> maximumCodePoint) :
    LexerAutomaton(parseExpressions(rules, maximumCodePoint), maximumCodePoint) {
    for (std::size_t i = 0; i < rules.size(); ++i) classes_[i] = rules[i].requiredClasses;
}

LexerAutomaton::LexerAutomaton(std::span<const RegexAst> expressions, std::optional<CodePoint> maximumCodePoint,
                               std::span<const LexerClassMask> classes) {
    if (!classes.empty() && classes.size() != expressions.size())
        throw LexerBuildError(std::nullopt, "lexer class and expression counts differ");
    classes_.assign(expressions.size(), 0);
    if (!classes.empty()) classes_.assign(classes.begin(), classes.end());
    if (expressions.empty()) {
        throw LexerBuildError(std::nullopt, "at least one rule is required");
    }
    const std::vector<CombinedState> nfa = buildCombinedNfa(expressions, maximumCodePoint);
    std::map<StateSubset, std::size_t> stateIds;
    std::vector<StateSubset> subsets;
    const auto intern = [&](StateSubset subset) -> std::size_t {
        const auto [it, inserted] = stateIds.try_emplace(subset, states_.size());
        if (inserted) {
            subsets.push_back(subset);
            states_.push_back({rulesInSubset(nfa, subset, true), rulesInSubset(nfa, subset, false), {}});
        }
        return it->second;
    };

    intern(epsilonClosure(nfa, {0}));
    for (std::size_t state = 0; state < states_.size(); ++state) {
        std::vector<CodePoint> boundaries;
        for (const std::size_t nfaState: subsets[state]) {
            for (const CombinedTransition &transition: nfa[nfaState].transitions) {
                for (const CodePointRange range: transition.codePoints.ranges()) {
                    boundaries.push_back(range.first);
                    boundaries.push_back(range.last + 1U);
                }
            }
        }
        std::ranges::sort(boundaries);
        boundaries.erase(std::ranges::unique(boundaries).begin(), boundaries.end());

        std::map<std::size_t, std::vector<CodePointRange>> rangesByTarget;
        for (std::size_t boundary = 0; boundary + 1 < boundaries.size(); ++boundary) {
            const CodePoint first = boundaries[boundary];
            const CodePoint last = boundaries[boundary + 1] - 1U;
            std::vector<std::size_t> targets;
            for (const std::size_t nfaState: subsets[state]) {
                for (const CombinedTransition &transition: nfa[nfaState].transitions) {
                    if (transition.codePoints.contains(first)) {
                        targets.push_back(transition.target);
                    }
                }
            }
            if (targets.empty())
                continue;
            std::ranges::sort(targets);
            targets.erase(std::ranges::unique(targets).begin(), targets.end());
            const std::size_t target = intern(epsilonClosure(nfa, targets));
            auto &ranges = rangesByTarget[target];
            if (!ranges.empty() && ranges.back().last + 1U == first) {
                ranges.back().last = last;
            } else {
                ranges.push_back({first, last});
            }
        }
        for (auto &[target, ranges]: rangesByTarget) {
            states_[state].transitions.push_back({CodePointClass(std::move(ranges)), target});
        }
    }
}

std::size_t LexerAutomaton::stateCount() const noexcept {
    return states_.size();
}

std::size_t LexerAutomaton::transitionRangeCount() const noexcept {
    std::size_t result = 0;
    for (const State &state: states_) {
        for (const LexerAutomatonTransition &transition: state.transitions) {
            result += transition.codePoints.ranges().size();
        }
    }
    return result;
}

std::optional<std::size_t> LexerAutomaton::acceptingRule(std::size_t state) const {
    const auto &rules = states_.at(state).acceptingRules;
    return rules.empty() ? std::nullopt : std::optional<std::size_t>{rules.front()};
}

std::optional<std::size_t> LexerAutomaton::acceptingRule(std::size_t state, LexerClassMask active) const {
    for (const auto rule : states_.at(state).acceptingRules)
        if (lexerClassEnabled(classes_[rule], active)) return rule;
    return std::nullopt;
}

bool LexerAutomaton::canMatch(std::size_t state, LexerClassMask active) const {
    for (const auto rule : states_.at(state).participatingRules)
        if (lexerClassEnabled(classes_[rule], active)) return true;
    return false;
}

std::vector<LexerClassMask> LexerAutomaton::classDependencies() const {
    auto result = classes_;
    for (const auto &state : states_) {
        LexerClassMask competing = 0;
        for (const auto rule : state.participatingRules) competing |= classes_[rule];
        for (const auto rule : state.acceptingRules) result[rule] |= competing;
    }
    return result;
}

std::optional<std::size_t> LexerAutomaton::nextState(std::size_t state, CodePoint codePoint) const {
    for (const LexerAutomatonTransition &transition: states_.at(state).transitions) {
        if (transition.codePoints.contains(codePoint))
            return transition.target;
    }
    return std::nullopt;
}

std::span<const LexerAutomatonTransition> LexerAutomaton::transitions(std::size_t state) const {
    return states_.at(state).transitions;
}

} // namespace zbik
