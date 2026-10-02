#pragma once

#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "grammar/Grammar.h"

namespace zbik {

class DerivationTree {
public:
    using Child = std::variant<TerminalId, std::unique_ptr<DerivationTree>>;

    [[nodiscard]] static Child terminal(TerminalId terminal);
    [[nodiscard]] static Child subtree(DerivationTree tree);

    DerivationTree(const Grammar &grammar, RuleId rule, std::vector<Child> children);
    DerivationTree(const DerivationTree &other);
    DerivationTree(DerivationTree &&other) noexcept = default;
    DerivationTree &operator=(const DerivationTree &other);
    DerivationTree &operator=(DerivationTree &&other) noexcept = default;
    ~DerivationTree() = default;

    [[nodiscard]] RuleId rule() const noexcept;
    [[nodiscard]] NonterminalId rootNonterminal(const Grammar &grammar) const;
    [[nodiscard]] std::span<const Child> children() const noexcept;
    [[nodiscard]] std::vector<TerminalId> terminals() const;
    [[nodiscard]] std::string structuralFingerprint() const;
    [[nodiscard]] std::string dump(const Grammar &grammar) const;

    [[nodiscard]] bool operator==(const DerivationTree &other) const;

    void swap(DerivationTree &other) noexcept;

private:
    static std::vector<Child> copyChildren(std::span<const Child> children);
    static void validate(const Grammar &grammar, const DerivationTree &tree);
    void appendTerminals(std::vector<TerminalId> &result) const;
    void appendFingerprint(std::string &result) const;
    void appendDump(const Grammar &grammar, std::string &result) const;

    RuleId rule_;
    std::vector<Child> children_;
};

} // namespace zbik
