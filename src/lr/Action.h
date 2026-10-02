#pragma once

#include <compare>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "Identifiers.h"
#include "first/LookaheadWord.h"
#include "grammar/Identifiers.h"

namespace zbik {

struct Shift {
    StateId target;
    auto operator<=>(const Shift &) const = default;
};

struct Reduce {
    RuleId rule;
    auto operator<=>(const Reduce &) const = default;
};

struct Accept {
    auto operator<=>(const Accept &) const = default;
};

using Action = std::variant<Shift, Reduce, Accept>;

[[nodiscard]] std::string dumpAction(const Action &action);

// A deterministic, duplicate-free set. Multiple actions are retained as a conflict.
class ActionCell {
public:
    ActionCell() = default;
    ActionCell(std::initializer_list<Action> actions);

    [[nodiscard]] bool add(Action action);
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool hasConflict() const noexcept;
    [[nodiscard]] bool contains(const Action &action) const;
    [[nodiscard]] std::span<const Action> actions() const noexcept;

    bool operator==(const ActionCell &) const = default;

private:
    std::vector<Action> actions_;
};

class Conflict {
public:
    Conflict(StateId state, LookaheadWord lookahead, const ActionCell &cell);

    [[nodiscard]] StateId state() const noexcept;
    [[nodiscard]] const LookaheadWord &lookahead() const noexcept;
    [[nodiscard]] std::span<const Action> actions() const noexcept;
    [[nodiscard]] bool shiftReduce() const noexcept;
    [[nodiscard]] bool reduceReduce() const noexcept;
    [[nodiscard]] bool shiftAccept() const noexcept;
    [[nodiscard]] bool reduceAccept() const noexcept;
    [[nodiscard]] std::vector<RuleId> reductionRules() const;
    [[nodiscard]] std::string dump() const;

    bool operator==(const Conflict &) const = default;

private:
    [[nodiscard]] std::size_t shiftCount() const noexcept;
    [[nodiscard]] std::size_t reductionCount() const noexcept;
    [[nodiscard]] std::size_t acceptCount() const noexcept;

    StateId state_;
    LookaheadWord lookahead_;
    std::vector<Action> actions_;
};

} // namespace zbik
