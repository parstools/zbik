#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <variant>

namespace zbik {

struct TerminalId {
    std::uint32_t value;

    auto operator<=>(const TerminalId &) const = default;
};

struct NonterminalId {
    std::uint32_t value;

    auto operator<=>(const NonterminalId &) const = default;
};

struct RuleId {
    std::uint32_t value;

    auto operator<=>(const RuleId &) const = default;
};

using SymbolRef = std::variant<TerminalId, NonterminalId>;

constexpr std::size_t toIndex(TerminalId id) noexcept {
    return id.value;
}

constexpr std::size_t toIndex(NonterminalId id) noexcept {
    return id.value;
}

constexpr std::size_t toIndex(RuleId id) noexcept {
    return id.value;
}

} // namespace zbik

template<>
struct std::hash<zbik::TerminalId> {
    std::size_t operator()(zbik::TerminalId id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};

template<>
struct std::hash<zbik::NonterminalId> {
    std::size_t operator()(zbik::NonterminalId id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};

template<>
struct std::hash<zbik::RuleId> {
    std::size_t operator()(zbik::RuleId id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};
