#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "BoundedDerivationGenerator.h"

namespace zbik {

enum class AmbiguitySearchStatus { WitnessFound, Exhausted, Inconclusive };

struct AmbiguitySearchOptions {
    std::size_t maxLength;
    std::optional<std::size_t> treeLimit = std::nullopt;
    std::optional<DerivationOrder> order = std::nullopt;
    std::uint32_t seed = 0;
};

struct AmbiguityWitness {
    std::vector<TerminalId> word;
    DerivationTree first;
    DerivationTree second;
};

class AmbiguitySearchResult {
public:
    AmbiguitySearchResult(
            AmbiguitySearchStatus status, std::size_t examinedTrees,
            AmbiguitySearchOptions options, DerivationOrder effectiveOrder,
            std::optional<AmbiguityWitness> witness);

    [[nodiscard]] AmbiguitySearchStatus status() const noexcept;
    [[nodiscard]] std::size_t examinedTrees() const noexcept;
    [[nodiscard]] std::size_t maxLength() const noexcept;
    [[nodiscard]] std::optional<std::size_t> treeLimit() const noexcept;
    [[nodiscard]] DerivationOrder order() const noexcept;
    [[nodiscard]] std::uint32_t seed() const noexcept;
    [[nodiscard]] const std::optional<AmbiguityWitness> &witness() const noexcept;
    [[nodiscard]] std::string dump(const Grammar &grammar) const;

private:
    AmbiguitySearchStatus status_;
    std::size_t examinedTrees_;
    AmbiguitySearchOptions options_;
    DerivationOrder effectiveOrder_;
    std::optional<AmbiguityWitness> witness_;
};

[[nodiscard]] AmbiguitySearchResult findAmbiguity(
        const Grammar &grammar, AmbiguitySearchOptions options);

} // namespace zbik
