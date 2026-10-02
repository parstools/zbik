#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "EbnfGrammar.h"
#include "grammar/Grammar.h"

namespace zbik {

enum class EbnfGeneratedRuleRole {
    SourceAlternative,
    OptionalPresent,
    OptionalEmpty,
    RepetitionRecursive,
    RepetitionBase,
};

class EbnfConversionError final : public std::runtime_error {
public:
    EbnfConversionError(std::size_t sourceRuleIndex, std::size_t alternativeIndex, std::size_t elementIndex,
                        const std::string &message);

    [[nodiscard]] std::size_t sourceRuleIndex() const noexcept;
    [[nodiscard]] std::size_t alternativeIndex() const noexcept;
    [[nodiscard]] std::size_t elementIndex() const noexcept;
private:
    std::size_t sourceRuleIndex_;
    std::size_t alternativeIndex_;
    std::size_t elementIndex_;
};

struct EbnfRuleOrigin {
    RuleId generatedRule;
    std::size_t sourceRuleIndex;
    std::size_t alternativeIndex;
    std::optional<std::size_t> elementIndex;
    Repetition repetition;
    EbnfGeneratedRuleRole role;
    std::optional<std::string> helperName;

    bool operator==(const EbnfRuleOrigin &) const = default;
};

// Diagnostic metadata remains separate from the immutable Grammar. Entries are
// indexed by generated RuleId and preserve source alternative identity.
class EbnfConversionResult {
public:
    EbnfConversionResult(Grammar grammar, std::vector<EbnfRuleOrigin> origins);

    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] std::span<const EbnfRuleOrigin> origins() const noexcept;
    [[nodiscard]] const EbnfRuleOrigin &origin(RuleId rule) const;
    [[nodiscard]] Grammar takeGrammar() &&;
private:
    Grammar grammar_;
    std::vector<EbnfRuleOrigin> origins_;
};

class EbnfToBnfConverter {
public:
    [[nodiscard]] Grammar convert(const EbnfGrammarSpec &specification) const;
    [[nodiscard]] Grammar convert(const EbnfGrammarSpec &specification,
                                  const std::vector<std::string> &declaredTerminals) const;

    [[nodiscard]] EbnfConversionResult convertWithOrigins(const EbnfGrammarSpec &specification) const;
    [[nodiscard]] EbnfConversionResult convertWithOrigins(const EbnfGrammarSpec &specification,
                                                          const std::vector<std::string> &declaredTerminals) const;
};

} // namespace zbik
