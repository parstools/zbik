#pragma once

#include <initializer_list>
#include <span>
#include <string>
#include <vector>

namespace zbik {

enum class Repetition {
    One,
    Optional,
    ZeroOrMore,
    OneOrMore,
};

class EbnfElement {
public:
    explicit EbnfElement(std::string symbol, Repetition repetition = Repetition::One);

    [[nodiscard]] const std::string &symbol() const noexcept;
    [[nodiscard]] Repetition repetition() const noexcept;

    bool operator==(const EbnfElement &) const = default;
private:
    std::string symbol_;
    Repetition repetition_;
};

class EbnfAlternative {
public:
    EbnfAlternative() = default;
    explicit EbnfAlternative(std::vector<EbnfElement> elements);
    EbnfAlternative(std::initializer_list<EbnfElement> elements);

    [[nodiscard]] std::span<const EbnfElement> elements() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    bool operator==(const EbnfAlternative &) const = default;
private:
    std::vector<EbnfElement> elements_;
};

class EbnfRuleSpec {
public:
    EbnfRuleSpec(std::string lhs, std::vector<EbnfAlternative> alternatives);
    EbnfRuleSpec(std::string lhs, std::initializer_list<EbnfAlternative> alternatives);

    [[nodiscard]] const std::string &lhs() const noexcept;
    [[nodiscard]] std::span<const EbnfAlternative> alternatives() const noexcept;

    bool operator==(const EbnfRuleSpec &) const = default;
private:
    std::string lhs_;
    std::vector<EbnfAlternative> alternatives_;
};

class EbnfGrammarSpec {
public:
    explicit EbnfGrammarSpec(std::vector<EbnfRuleSpec> rules);
    EbnfGrammarSpec(std::initializer_list<EbnfRuleSpec> rules);

    [[nodiscard]] std::span<const EbnfRuleSpec> rules() const noexcept;
    [[nodiscard]] const std::string &startSymbol() const noexcept;
private:
    std::vector<EbnfRuleSpec> rules_;
};

} // namespace zbik
