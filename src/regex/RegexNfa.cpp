#include "regex/RegexNfa.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace zbik {
namespace {

struct Fragment {
    NfaStateId start;
    NfaStateId accept;
};

class ThompsonBuilder {
public:
    RegexNfa build(const RegexAst &expression) {
        const Fragment root = buildFragment(expression);
        return RegexNfa(std::move(states_), root.start, root.accept);
    }
private:
    Fragment buildFragment(const RegexAst &expression) {
        switch (expression.kind()) {
            case RegexAst::Kind::Epsilon:
                return buildEpsilon();
            case RegexAst::Kind::ByteClass:
                return buildByteClass(expression.bytes());
            case RegexAst::Kind::Concatenation:
                return buildConcatenation(expression.elements());
            case RegexAst::Kind::Alternation:
                return buildAlternation(expression.alternatives());
            case RegexAst::Kind::Repetition:
                return buildRepetition(expression.repeated(), expression.quantifier());
        }
        throw std::logic_error("Unknown regex AST kind");
    }

    Fragment buildEpsilon() {
        const NfaStateId start = addState();
        const NfaStateId accept = addState();
        addEpsilon(start, accept);
        return {start, accept};
    }

    Fragment buildByteClass(const ByteClass &bytes) {
        const NfaStateId start = addState();
        const NfaStateId accept = addState();
        states_[toIndex(start)].byteTransitions.push_back({bytes, accept});
        return {start, accept};
    }

    Fragment buildConcatenation(std::span<const RegexAst> elements) {
        if (elements.empty())
            return buildEpsilon();

        Fragment result = buildFragment(elements.front());
        for (const RegexAst &element: elements.subspan(1)) {
            const Fragment next = buildFragment(element);
            addEpsilon(result.accept, next.start);
            result.accept = next.accept;
        }
        return result;
    }

    Fragment buildAlternation(std::span<const RegexAst> alternatives) {
        const NfaStateId start = addState();
        states_[toIndex(start)].orderedDecision = true;
        std::vector<Fragment> branches;
        branches.reserve(alternatives.size());
        for (const RegexAst &alternative: alternatives) {
            branches.push_back(buildFragment(alternative));
        }
        const NfaStateId accept = addState();
        for (const Fragment branch: branches) {
            addEpsilon(start, branch.start);
            addEpsilon(branch.accept, accept);
        }
        return {start, accept};
    }

    Fragment buildRepetition(const RegexAst &expression, RegexQuantifier quantifier) {
        const NfaStateId start = addState();
        const Fragment inner = buildFragment(expression);
        const NfaStateId accept = addState();

        switch (quantifier) {
            case RegexQuantifier::ZeroOrMore:
                markDecision(start);
                markDecision(inner.accept);
                addEpsilon(start, inner.start);
                addEpsilon(start, accept);
                addEpsilon(inner.accept, inner.start);
                addEpsilon(inner.accept, accept);
                break;
            case RegexQuantifier::OneOrMore:
                addEpsilon(start, inner.start);
                markDecision(inner.accept);
                addEpsilon(inner.accept, inner.start);
                addEpsilon(inner.accept, accept);
                break;
            case RegexQuantifier::ZeroOrOne:
                markDecision(start);
                addEpsilon(start, inner.start);
                addEpsilon(start, accept);
                addEpsilon(inner.accept, accept);
                break;
            case RegexQuantifier::LazyZeroOrMore:
                markDecision(start, true);
                markDecision(inner.accept, true);
                addEpsilon(start, accept);
                addEpsilon(start, inner.start);
                addEpsilon(inner.accept, accept);
                addEpsilon(inner.accept, inner.start);
                break;
            case RegexQuantifier::LazyOneOrMore:
                addEpsilon(start, inner.start);
                markDecision(inner.accept, true);
                addEpsilon(inner.accept, accept);
                addEpsilon(inner.accept, inner.start);
                break;
        }
        return {start, accept};
    }

    NfaStateId addState() {
        if (states_.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("NFA has too many states");
        }
        const auto id = NfaStateId{static_cast<std::uint32_t>(states_.size())};
        states_.emplace_back();
        return id;
    }

    void addEpsilon(NfaStateId source, NfaStateId target) {
        states_[toIndex(source)].epsilonTransitions.push_back(target);
    }

    void markDecision(NfaStateId state, bool activatesPriority = false) {
        states_[toIndex(state)].orderedDecision = true;
        states_[toIndex(state)].activatesPriority = activatesPriority;
    }

    std::vector<NfaState> states_;
};

} // namespace

RegexNfa::RegexNfa(std::vector<NfaState> states, NfaStateId startState, NfaStateId acceptingState) :
    states_(std::move(states)), startState_(startState), acceptingState_(acceptingState) {
    if (states_.empty()) {
        throw std::invalid_argument("An NFA must have at least one state");
    }
    const auto validate = [this](NfaStateId id) {
        if (toIndex(id) >= states_.size()) {
            throw std::invalid_argument("NFA transition refers to an unknown state");
        }
    };
    validate(startState_);
    validate(acceptingState_);
    for (const NfaState &state: states_) {
        hasPrioritizedDecisions_ = hasPrioritizedDecisions_ || state.activatesPriority;
        for (const NfaStateId target: state.epsilonTransitions)
            validate(target);
        for (const NfaByteTransition &transition: state.byteTransitions) {
            validate(transition.target);
        }
    }
}

RegexNfa RegexNfa::fromRegex(const RegexAst &expression) {
    return ThompsonBuilder{}.build(expression);
}

NfaStateId RegexNfa::startState() const noexcept {
    return startState_;
}

NfaStateId RegexNfa::acceptingState() const noexcept {
    return acceptingState_;
}

std::span<const NfaState> RegexNfa::states() const noexcept {
    return states_;
}

const NfaState &RegexNfa::state(NfaStateId id) const {
    if (toIndex(id) >= states_.size()) {
        throw std::out_of_range("Unknown NFA state");
    }
    return states_[toIndex(id)];
}

bool RegexNfa::hasPrioritizedDecisions() const noexcept {
    return hasPrioritizedDecisions_;
}

std::vector<NfaStateId> RegexNfa::epsilonClosure(std::span<const NfaStateId> seeds) const {
    std::vector<bool> visited(states_.size());
    std::vector<NfaStateId> pending;
    pending.reserve(seeds.size());
    for (const NfaStateId seed: seeds) {
        if (toIndex(seed) >= states_.size()) {
            throw std::out_of_range("Unknown NFA state in epsilon closure");
        }
        if (!visited[toIndex(seed)]) {
            visited[toIndex(seed)] = true;
            pending.push_back(seed);
        }
    }

    while (!pending.empty()) {
        const NfaStateId current = pending.back();
        pending.pop_back();
        for (const NfaStateId target: state(current).epsilonTransitions) {
            if (!visited[toIndex(target)]) {
                visited[toIndex(target)] = true;
                pending.push_back(target);
            }
        }
    }

    std::vector<NfaStateId> result;
    for (std::size_t index = 0; index < visited.size(); ++index) {
        if (visited[index]) {
            result.push_back(NfaStateId{static_cast<std::uint32_t>(index)});
        }
    }
    return result;
}

std::vector<NfaStateId> RegexNfa::epsilonClosure(std::initializer_list<NfaStateId> seeds) const {
    return epsilonClosure(std::span<const NfaStateId>(seeds.begin(), seeds.size()));
}

} // namespace zbik
