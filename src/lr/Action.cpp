#include "Action.h"

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace zbik {

std::string dumpAction(const Action &action) {
    return std::visit([](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Shift>) {
            return "shift " + std::to_string(value.target.value);
        } else if constexpr (std::is_same_v<T, Reduce>) {
            return "reduce R" + std::to_string(value.rule.value);
        } else {
            return "accept";
        }
    }, action);
}

ActionCell::ActionCell(std::initializer_list<Action> actions) {
    for (const auto &action: actions) {
        static_cast<void>(add(action));
    }
}

bool ActionCell::add(Action action) {
    const auto position = std::ranges::lower_bound(actions_, action);
    if (position != actions_.end() && *position == action) {
        return false;
    }
    actions_.insert(position, std::move(action));
    return true;
}

bool ActionCell::empty() const noexcept {
    return actions_.empty();
}

std::size_t ActionCell::size() const noexcept {
    return actions_.size();
}

bool ActionCell::hasConflict() const noexcept {
    return actions_.size() > 1;
}

bool ActionCell::contains(const Action &action) const {
    return std::ranges::binary_search(actions_, action);
}

std::span<const Action> ActionCell::actions() const noexcept {
    return actions_;
}

Conflict::Conflict(StateId state, LookaheadWord lookahead, const ActionCell &cell)
    : state_(state), lookahead_(std::move(lookahead)),
      actions_(cell.actions().begin(), cell.actions().end()) {
    if (lookahead_.empty()) {
        throw std::invalid_argument("a conflict lookahead must not be epsilon");
    }
    if (!cell.hasConflict()) {
        throw std::invalid_argument("a conflict needs at least two distinct actions");
    }
}

StateId Conflict::state() const noexcept {
    return state_;
}

const LookaheadWord &Conflict::lookahead() const noexcept {
    return lookahead_;
}

std::span<const Action> Conflict::actions() const noexcept {
    return actions_;
}

std::size_t Conflict::shiftCount() const noexcept {
    return std::ranges::count_if(actions_, [](const auto &action) {
        return std::holds_alternative<Shift>(action);
    });
}

std::size_t Conflict::reductionCount() const noexcept {
    return std::ranges::count_if(actions_, [](const auto &action) {
        return std::holds_alternative<Reduce>(action);
    });
}

std::size_t Conflict::acceptCount() const noexcept {
    return std::ranges::count_if(actions_, [](const auto &action) {
        return std::holds_alternative<Accept>(action);
    });
}

bool Conflict::shiftReduce() const noexcept {
    return shiftCount() != 0 && reductionCount() != 0;
}

bool Conflict::reduceReduce() const noexcept {
    return reductionCount() > 1;
}

bool Conflict::shiftAccept() const noexcept {
    return shiftCount() != 0 && acceptCount() != 0;
}

bool Conflict::reduceAccept() const noexcept {
    return reductionCount() != 0 && acceptCount() != 0;
}

std::vector<RuleId> Conflict::reductionRules() const {
    std::vector<RuleId> result;
    for (const auto &action: actions_) {
        if (const auto reduction = std::get_if<Reduce>(&action)) {
            result.push_back(reduction->rule);
        }
    }
    return result;
}

std::string Conflict::dump() const {
    std::vector<std::string> kinds;
    if (shiftReduce()) {
        kinds.emplace_back("shift/reduce");
    }
    if (reduceReduce()) {
        kinds.emplace_back("reduce/reduce");
    }
    if (shiftAccept()) {
        kinds.emplace_back("shift/accept");
    }
    if (reduceAccept()) {
        kinds.emplace_back("reduce/accept");
    }
    if (kinds.empty()) {
        kinds.emplace_back("multiple-action");
    }

    std::string result = "conflict ";
    for (std::size_t i = 0; i < kinds.size(); ++i) {
        if (i != 0) {
            result += ", ";
        }
        result += kinds[i];
    }
    result += " in state " + std::to_string(state_.value);
    result += " on " + lookahead_.dump() + ": {";
    for (std::size_t i = 0; i < actions_.size(); ++i) {
        if (i != 0) {
            result += ", ";
        }
        result += dumpAction(actions_[i]);
    }
    result += '}';
    return result;
}

} // namespace zbik
