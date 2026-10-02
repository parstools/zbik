#include "BoundedDerivationGenerator.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace zbik {
namespace {

std::string zeroProgressMessage(const GrammarCycleComponent &cycle) {
    std::string result =
            "bounded derivation generator does not support a useful "
            "zero-progress cycle";
    if (!cycle.nonterminals.empty()) {
        result += " containing N" + std::to_string(cycle.nonterminals.front().value);
    }
    return result;
}

std::uint64_t nextRandom64(std::mt19937 &random) {
    return (static_cast<std::uint64_t>(random()) << 32U) | random();
}

std::size_t stableBoundedRandom(std::mt19937 &random, std::size_t bound) {
    const auto range = static_cast<std::uint64_t>(bound);
    const std::uint64_t threshold = (std::uint64_t{0} - range) % range;
    std::uint64_t value;
    do {
        value = nextRandom64(random);
    } while (value < threshold);
    return static_cast<std::size_t>(value % range);
}

void stableShuffle(std::vector<RuleId> &rules, std::mt19937 &random) {
    for (std::size_t remaining = rules.size(); remaining > 1; --remaining) {
        const std::size_t selected = stableBoundedRandom(random, remaining);
        std::swap(rules[remaining - 1], rules[selected]);
    }
}

} // namespace

GeneratorGrammarError::GeneratorGrammarError(GrammarCycleComponent cycle)
    : std::runtime_error(zeroProgressMessage(cycle)),
      cycle_(std::move(cycle)) {}

const GrammarCycleComponent &GeneratorGrammarError::cycle() const noexcept {
    return cycle_;
}

class BoundedDerivationGenerator::Enumerator {
public:
    Enumerator(
            const Grammar &grammar, const MinYieldLength &minima,
            NonterminalId nonterminal, std::size_t maxLength,
            DerivationOrder order, std::mt19937 &random)
        : grammar_(grammar), minima_(minima), nonterminal_(nonterminal),
          maxLength_(maxLength), order_(order), random_(random),
          rules_(grammar.rulesFor(nonterminal)) {
        if (order_ == DerivationOrder::StableShuffle) {
            stableShuffle(rules_, random_);
        }
    }

    bool next() {
        if (exhausted_) {
            return false;
        }
        if (currentRule_ && advanceCurrentRule()) {
            return true;
        }

        currentRule_.reset();
        children_.clear();
        while (nextRulePosition_ < rules_.size()) {
            const RuleId rule = rules_[nextRulePosition_++];
            const MinYield minimum = minima_.minimum(rule);
            if (!minimum.isFinite() || minimum.value() > maxLength_) {
                continue;
            }
            if (initializeRule(rule)) {
                return true;
            }
        }
        exhausted_ = true;
        return false;
    }

    [[nodiscard]] std::size_t currentLength() const {
        if (!currentRule_) {
            throw std::logic_error("derivation enumerator has no current tree");
        }
        return currentLength_;
    }

    [[nodiscard]] DerivationTree tree() const {
        if (!currentRule_) {
            throw std::logic_error("derivation enumerator has no current tree");
        }
        std::vector<DerivationTree::Child> result;
        result.reserve(children_.size());
        for (const Child &child: children_) {
            if (const auto terminal = std::get_if<TerminalId>(&child)) {
                result.push_back(DerivationTree::terminal(*terminal));
            } else {
                result.push_back(DerivationTree::subtree(
                        std::get<std::unique_ptr<Enumerator>>(child)->tree()));
            }
        }
        return {grammar_, *currentRule_, std::move(result)};
    }

private:
    using Child = std::variant<TerminalId, std::unique_ptr<Enumerator>>;

    [[nodiscard]] std::size_t childLength(const Child &child) const {
        if (std::holds_alternative<TerminalId>(child)) {
            return 1;
        }
        return std::get<std::unique_ptr<Enumerator>>(child)->currentLength();
    }

    bool initializeRule(RuleId ruleId) {
        currentRule_ = ruleId;
        children_.clear();
        const Rule &rule = grammar_.rule(ruleId);
        suffixMinima_.assign(rule.size() + 1, 0);
        for (std::size_t i = rule.size(); i > 0; --i) {
            const MinYield minimum = minima_.minimum(rule.symbol(i - 1));
            if (!minimum.isFinite()
                    || minimum.value() > std::numeric_limits<std::size_t>::max()
                            - suffixMinima_[i]) {
                throw std::logic_error("finite rule has a non-finite minimum");
            }
            suffixMinima_[i - 1] = minimum.value() + suffixMinima_[i];
        }
        if (!initializeSuffix(0)) {
            currentRule_.reset();
            children_.clear();
            return false;
        }
        return true;
    }

    bool initializeSuffix(std::size_t start) {
        const Rule &rule = grammar_.rule(*currentRule_);
        std::size_t used = 0;
        for (std::size_t i = 0; i < start; ++i) {
            used += childLength(children_[i]);
        }
        if (used > maxLength_) {
            return false;
        }
        std::size_t remaining = maxLength_ - used;

        for (std::size_t i = start; i < rule.size(); ++i) {
            if (suffixMinima_[i + 1] > remaining) {
                return false;
            }
            const std::size_t childBudget = remaining - suffixMinima_[i + 1];
            const SymbolRef &symbol = rule.symbol(i);
            if (const auto terminal = std::get_if<TerminalId>(&symbol)) {
                if (childBudget < 1) {
                    return false;
                }
                children_.emplace_back(*terminal);
                --remaining;
                continue;
            }

            auto child = std::make_unique<Enumerator>(
                    grammar_, minima_, std::get<NonterminalId>(symbol), childBudget,
                    order_, random_);
            if (!child->next()) {
                return false;
            }
            const std::size_t length = child->currentLength();
            if (length > remaining) {
                throw std::logic_error("child derivation exceeded its budget");
            }
            remaining -= length;
            children_.emplace_back(std::move(child));
        }
        currentLength_ = maxLength_ - remaining;
        return true;
    }

    bool advanceCurrentRule() {
        for (std::size_t position = children_.size(); position > 0; --position) {
            const std::size_t index = position - 1;
            auto child = std::get_if<std::unique_ptr<Enumerator>>(&children_[index]);
            if (!child || !(*child)->next()) {
                continue;
            }
            children_.resize(index + 1);
            if (!initializeSuffix(index + 1)) {
                throw std::logic_error(
                        "minimum reservation did not produce a valid suffix");
            }
            return true;
        }
        return false;
    }

    const Grammar &grammar_;
    const MinYieldLength &minima_;
    NonterminalId nonterminal_;
    std::size_t maxLength_;
    DerivationOrder order_;
    std::mt19937 &random_;
    std::vector<RuleId> rules_;
    std::size_t nextRulePosition_ = 0;
    std::optional<RuleId> currentRule_;
    std::vector<Child> children_;
    std::vector<std::size_t> suffixMinima_;
    std::size_t currentLength_ = 0;
    bool exhausted_ = false;
};

BoundedDerivationGenerator::BoundedDerivationGenerator(
        const Grammar &grammar, std::size_t maxLength,
        DerivationOrder order, std::uint32_t seed)
    : grammar_(grammar), maxLength_(maxLength), minima_(grammar), analysis_(grammar),
      order_(order), seed_(seed), random_(seed) {
    const auto cycle = std::ranges::find_if(
            analysis_.zeroProgressCycles(), [&](const auto &component) {
        return std::ranges::any_of(component.nonterminals, [&](const auto nonterminal) {
            return analysis_.isUseful(nonterminal);
        });
    });
    if (cycle != analysis_.zeroProgressCycles().end()) {
        throw GeneratorGrammarError(*cycle);
    }
    root_ = std::make_unique<Enumerator>(
            grammar_, minima_, grammar_.start(), maxLength_, order_, random_);
}

BoundedDerivationGenerator::~BoundedDerivationGenerator() = default;

const Grammar &BoundedDerivationGenerator::grammar() const noexcept {
    return grammar_;
}

std::size_t BoundedDerivationGenerator::maxLength() const noexcept {
    return maxLength_;
}

DerivationOrder BoundedDerivationGenerator::order() const noexcept {
    return order_;
}

std::uint32_t BoundedDerivationGenerator::seed() const noexcept {
    return seed_;
}

bool BoundedDerivationGenerator::next() {
    current_.reset();
    if (exhausted_ || !root_->next()) {
        exhausted_ = true;
        return false;
    }
    current_.emplace(root_->tree());
    return true;
}

bool BoundedDerivationGenerator::hasCurrent() const noexcept {
    return current_.has_value();
}

bool BoundedDerivationGenerator::exhausted() const noexcept {
    return exhausted_;
}

const DerivationTree &BoundedDerivationGenerator::currentTree() const {
    if (!current_) {
        throw std::logic_error("bounded derivation generator has no current tree");
    }
    return *current_;
}

} // namespace zbik
