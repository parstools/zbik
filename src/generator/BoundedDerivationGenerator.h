#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>

#include "DerivationTree.h"
#include "grammar/GrammarAnalysis.h"
#include "grammar/YieldLength.h"

namespace zbik {

enum class DerivationOrder { RuleId, StableShuffle };

class GeneratorGrammarError final : public std::runtime_error {
public:
    explicit GeneratorGrammarError(GrammarCycleComponent cycle);

    [[nodiscard]] const GrammarCycleComponent &cycle() const noexcept;

private:
    GrammarCycleComponent cycle_;
};

class BoundedDerivationGenerator {
public:
    BoundedDerivationGenerator(
            const Grammar &grammar, std::size_t maxLength,
            DerivationOrder order = DerivationOrder::RuleId,
            std::uint32_t seed = 0);
    ~BoundedDerivationGenerator();

    BoundedDerivationGenerator(const BoundedDerivationGenerator &) = delete;
    BoundedDerivationGenerator &operator=(const BoundedDerivationGenerator &) = delete;
    BoundedDerivationGenerator(BoundedDerivationGenerator &&) = delete;
    BoundedDerivationGenerator &operator=(BoundedDerivationGenerator &&) = delete;

    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] std::size_t maxLength() const noexcept;
    [[nodiscard]] DerivationOrder order() const noexcept;
    [[nodiscard]] std::uint32_t seed() const noexcept;
    [[nodiscard]] bool next();
    [[nodiscard]] bool hasCurrent() const noexcept;
    [[nodiscard]] bool exhausted() const noexcept;
    [[nodiscard]] const DerivationTree &currentTree() const;

private:
    class Enumerator;

    const Grammar &grammar_;
    std::size_t maxLength_;
    MinYieldLength minima_;
    GrammarAnalysis analysis_;
    DerivationOrder order_;
    std::uint32_t seed_;
    std::mt19937 random_;
    std::unique_ptr<Enumerator> root_;
    std::optional<DerivationTree> current_;
    bool exhausted_ = false;
};

} // namespace zbik
