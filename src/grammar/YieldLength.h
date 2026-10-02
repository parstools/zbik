#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "Grammar.h"

namespace zbik {

class MinYield {
public:
    enum class Kind { Finite, ExceedsSizeT, NoYield };

    [[nodiscard]] static MinYield finite(std::size_t value) noexcept;
    [[nodiscard]] static MinYield exceedsSizeT() noexcept;
    [[nodiscard]] static MinYield noYield() noexcept;

    [[nodiscard]] Kind kind() const noexcept;
    [[nodiscard]] bool isFinite() const noexcept;
    [[nodiscard]] bool exceeds() const noexcept;
    [[nodiscard]] bool hasYield() const noexcept;
    [[nodiscard]] std::size_t value() const;

    bool operator==(const MinYield &) const = default;

private:
    MinYield(Kind kind, std::size_t value) noexcept;

    Kind kind_;
    std::size_t value_;
};

class MaxYield {
public:
    enum class Kind { NoYield, Finite, ExceedsSizeT, Unbounded };

    [[nodiscard]] static MaxYield noYield() noexcept;
    [[nodiscard]] static MaxYield finite(std::size_t value) noexcept;
    [[nodiscard]] static MaxYield exceedsSizeT() noexcept;
    [[nodiscard]] static MaxYield unbounded() noexcept;

    [[nodiscard]] Kind kind() const noexcept;
    [[nodiscard]] bool isFinite() const noexcept;
    [[nodiscard]] bool exceeds() const noexcept;
    [[nodiscard]] bool hasYield() const noexcept;
    [[nodiscard]] bool isUnbounded() const noexcept;
    [[nodiscard]] std::size_t value() const;

    bool operator==(const MaxYield &) const = default;

private:
    MaxYield(Kind kind, std::size_t value) noexcept;

    Kind kind_;
    std::size_t value_;
};

class MinYieldLength {
public:
    explicit MinYieldLength(const Grammar &grammar);

    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] MinYield minimum(TerminalId terminal) const;
    [[nodiscard]] MinYield minimum(NonterminalId nonterminal) const;
    [[nodiscard]] MinYield minimum(const SymbolRef &symbol) const;
    [[nodiscard]] MinYield minimum(RuleId rule) const;
    [[nodiscard]] bool checkMinLen() const noexcept;
    [[nodiscard]] std::span<const NonterminalId> noYieldNonterminals() const noexcept;
    [[nodiscard]] std::span<const RuleId> noYieldRules() const noexcept;
    [[nodiscard]] std::span<const NonterminalId> exceededNonterminals() const noexcept;
    [[nodiscard]] std::span<const RuleId> exceededRules() const noexcept;

private:
    const Grammar &grammar_;
    std::vector<MinYield> nonterminalMinima_;
    std::vector<MinYield> ruleMinima_;
    std::vector<NonterminalId> noYieldNonterminals_;
    std::vector<RuleId> noYieldRules_;
    std::vector<NonterminalId> exceededNonterminals_;
    std::vector<RuleId> exceededRules_;
};

class MaxYieldLength {
public:
    explicit MaxYieldLength(const Grammar &grammar);

    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] MaxYield maximum(TerminalId terminal) const;
    [[nodiscard]] MaxYield maximum(NonterminalId nonterminal) const;
    [[nodiscard]] MaxYield maximum(const SymbolRef &symbol) const;
    [[nodiscard]] MaxYield maximum(RuleId rule) const;

private:
    const Grammar &grammar_;
    std::vector<MaxYield> nonterminalMaxima_;
    std::vector<MaxYield> ruleMaxima_;
};

} // namespace zbik
