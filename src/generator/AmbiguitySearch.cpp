#include "AmbiguitySearch.h"

#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace zbik {
namespace {

struct TerminalWordHash {
    std::size_t operator()(const std::vector<TerminalId> &word) const noexcept {
        std::size_t result = word.size();
        for (const TerminalId terminal: word) {
            const std::size_t value = std::hash<TerminalId>{}(terminal);
            result ^= value + 0x9e3779b9U + (result << 6U) + (result >> 2U);
        }
        return result;
    }
};

const char *orderName(DerivationOrder order) {
    switch (order) {
        case DerivationOrder::RuleId: return "rule-id";
        case DerivationOrder::StableShuffle: return "stable-shuffle";
    }
    throw std::logic_error("unknown derivation order");
}

void appendQuoted(std::string &result, const std::string &text) {
    result += '"';
    for (const char character: text) {
        if (character == '"' || character == '\\') result += '\\';
        result += character;
    }
    result += '"';
}

std::string dumpWord(const Grammar &grammar, const std::vector<TerminalId> &word) {
    std::string result = "[";
    for (std::size_t i = 0; i < word.size(); ++i) {
        if (i != 0) result += ' ';
        appendQuoted(result, grammar.terminalName(word[i]));
    }
    result += ']';
    return result;
}

} // namespace

AmbiguitySearchResult::AmbiguitySearchResult(
        AmbiguitySearchStatus status, std::size_t examinedTrees,
        AmbiguitySearchOptions options, DerivationOrder effectiveOrder,
        std::optional<AmbiguityWitness> witness)
    : status_(status), examinedTrees_(examinedTrees), options_(std::move(options)),
      effectiveOrder_(effectiveOrder), witness_(std::move(witness)) {
    if ((status_ == AmbiguitySearchStatus::WitnessFound) != witness_.has_value()) {
        throw std::invalid_argument("ambiguity witness does not match search status");
    }
}

AmbiguitySearchStatus AmbiguitySearchResult::status() const noexcept {
    return status_;
}

std::size_t AmbiguitySearchResult::examinedTrees() const noexcept {
    return examinedTrees_;
}

std::size_t AmbiguitySearchResult::maxLength() const noexcept {
    return options_.maxLength;
}

std::optional<std::size_t> AmbiguitySearchResult::treeLimit() const noexcept {
    return options_.treeLimit;
}

DerivationOrder AmbiguitySearchResult::order() const noexcept {
    return effectiveOrder_;
}

std::uint32_t AmbiguitySearchResult::seed() const noexcept {
    return options_.seed;
}

const std::optional<AmbiguityWitness> &AmbiguitySearchResult::witness() const noexcept {
    return witness_;
}

std::string AmbiguitySearchResult::dump(const Grammar &grammar) const {
    std::string result;
    switch (status_) {
        case AmbiguitySearchStatus::WitnessFound:
            result = "witness-found";
            break;
        case AmbiguitySearchStatus::Exhausted:
            result = "exhausted";
            break;
        case AmbiguitySearchStatus::Inconclusive:
            result = "inconclusive";
            break;
    }
    result += ": maxLen=" + std::to_string(options_.maxLength);
    result += " examined=" + std::to_string(examinedTrees_);
    result += " order=";
    result += orderName(effectiveOrder_);
    if (options_.treeLimit) {
        result += " limit=" + std::to_string(*options_.treeLimit);
    }
    if (effectiveOrder_ == DerivationOrder::StableShuffle) {
        result += " seed=" + std::to_string(options_.seed);
    }
    if (witness_) {
        result += " word=" + dumpWord(grammar, witness_->word);
        result += " first=" + witness_->first.dump(grammar);
        result += " second=" + witness_->second.dump(grammar);
    } else if (status_ == AmbiguitySearchStatus::Exhausted) {
        result += "; no ambiguity witness exists within maxLen";
    } else {
        result += "; tree limit reached before exhaustion";
    }
    return result;
}

AmbiguitySearchResult findAmbiguity(
        const Grammar &grammar, AmbiguitySearchOptions options) {
    const DerivationOrder order = options.order.value_or(
            options.treeLimit ? DerivationOrder::StableShuffle
                              : DerivationOrder::RuleId);
    BoundedDerivationGenerator generator(
            grammar, options.maxLength, order, options.seed);
    std::unordered_map<
            std::vector<TerminalId>, DerivationTree, TerminalWordHash> firstTrees;
    std::size_t examined = 0;

    while (!options.treeLimit || examined < *options.treeLimit) {
        if (!generator.next()) {
            return {AmbiguitySearchStatus::Exhausted, examined,
                    std::move(options), order, std::nullopt};
        }
        ++examined;
        const DerivationTree &tree = generator.currentTree();
        std::vector<TerminalId> word = tree.terminals();
        const auto found = firstTrees.find(word);
        if (found != firstTrees.end()) {
            if (found->second != tree) {
                AmbiguityWitness witness{
                        std::move(word), found->second, tree,
                };
                return {AmbiguitySearchStatus::WitnessFound, examined,
                        std::move(options), order, std::move(witness)};
            }
        } else {
            firstTrees.emplace(std::move(word), tree);
        }
    }

    const AmbiguitySearchStatus status = generator.next()
            ? AmbiguitySearchStatus::Inconclusive
            : AmbiguitySearchStatus::Exhausted;
    return {status, examined, std::move(options), order, std::nullopt};
}

} // namespace zbik
