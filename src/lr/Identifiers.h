#pragma once

#include <compare>
#include <cstddef>
#include <functional>

namespace zbik {

struct StateId {
    std::size_t value;

    auto operator<=>(const StateId &) const = default;
};

constexpr std::size_t toIndex(StateId id) noexcept {
    return id.value;
}

} // namespace zbik

template<>
struct std::hash<zbik::StateId> {
    std::size_t operator()(zbik::StateId id) const noexcept {
        return std::hash<std::size_t>{}(id.value);
    }
};
