#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "Identifiers.h"

namespace zbik {

class Rule {
public:
    Rule(RuleId id, NonterminalId lhs, std::vector<SymbolRef> rhs)
        : id_(id), lhs_(lhs), rhs_(std::move(rhs)) {}

    [[nodiscard]] RuleId id() const noexcept {
        return id_;
    }

    [[nodiscard]] NonterminalId lhs() const noexcept {
        return lhs_;
    }

    [[nodiscard]] const std::vector<SymbolRef> &rhs() const noexcept {
        return rhs_;
    }

    [[nodiscard]] const SymbolRef &symbol(std::size_t position) const {
        return rhs_.at(position);
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return rhs_.size();
    }

private:
    RuleId id_;
    NonterminalId lhs_;
    std::vector<SymbolRef> rhs_;
};

} // namespace zbik
