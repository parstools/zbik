#include "WordSetK.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace zbik {

WordSetK::WordSetK(std::size_t maxLength) : maxLength_(maxLength) {
}

WordSetK::WordSetK(std::size_t maxLength, std::vector<LookaheadWord> words) : WordSetK(maxLength) {
    for (LookaheadWord &word: words) {
        add(std::move(word));
    }
}

std::size_t WordSetK::maxLength() const noexcept {
    return maxLength_;
}

std::size_t WordSetK::size() const noexcept {
    return words_.size();
}

bool WordSetK::empty() const noexcept {
    return words_.empty();
}

std::span<const LookaheadWord> WordSetK::words() const noexcept {
    return words_;
}

bool WordSetK::contains(const LookaheadWord &word) const {
    validate(word);
    return std::binary_search(words_.begin(), words_.end(), word);
}

std::string WordSetK::dump() const {
    std::string result = "{";
    for (std::size_t i = 0; i < words_.size(); ++i) {
        if (i != 0) {
            result += ' ';
        }
        result += words_[i].dump();
    }
    result += '}';
    return result;
}

bool WordSetK::add(LookaheadWord word) {
    validate(word);
    const auto position = std::lower_bound(words_.begin(), words_.end(), word);
    if (position != words_.end() && *position == word) {
        return false;
    }
    words_.insert(position, std::move(word));
    return true;
}

void WordSetK::clear() noexcept {
    words_.clear();
}

bool WordSetK::unionWith(const WordSetK &other) {
    requireCompatible(other);
    std::vector<LookaheadWord> result;
    result.reserve(words_.size() + other.words_.size());
    std::set_union(words_.begin(), words_.end(), other.words_.begin(), other.words_.end(), std::back_inserter(result));
    const bool changed = result.size() != words_.size();
    words_ = std::move(result);
    return changed;
}

WordSetK &WordSetK::operator|=(const WordSetK &other) {
    unionWith(other);
    return *this;
}

WordSetK operator|(WordSetK lhs, const WordSetK &rhs) {
    lhs |= rhs;
    return lhs;
}

WordSetK concatenateTruncated(const WordSetK &lhs, const WordSetK &rhs) {
    if (lhs.maxLength() != rhs.maxLength()) {
        throw std::invalid_argument("WordSetK length limit mismatch");
    }

    const std::size_t maxLength = lhs.maxLength();
    WordSetK result(maxLength);
    for (const LookaheadWord &left: lhs.words()) {
        for (const LookaheadWord &right: rhs.words()) {
            std::vector<LookaheadSymbol> symbols;
            std::size_t reserveSize = left.size();
            if (reserveSize < maxLength) {
                reserveSize += std::min(right.size(), maxLength - reserveSize);
            }
            symbols.reserve(reserveSize);

            for (const LookaheadSymbol &symbol: left.symbols()) {
                if (symbols.size() == maxLength) {
                    break;
                }
                symbols.push_back(symbol);
            }

            if (symbols.size() < maxLength && !left.endsWithEndOfInput()) {
                for (const LookaheadSymbol &symbol: right.symbols()) {
                    if (symbols.size() == maxLength) {
                        break;
                    }
                    symbols.push_back(symbol);
                    if (isEndOfInput(symbol)) {
                        break;
                    }
                }
            }

            result.add(LookaheadWord(std::move(symbols)));
        }
    }
    return result;
}

void WordSetK::validate(const LookaheadWord &word) const {
    if (word.size() > maxLength_) {
        throw std::invalid_argument("lookahead word exceeds the WordSetK length limit");
    }
}

void WordSetK::requireCompatible(const WordSetK &other) const {
    if (maxLength_ != other.maxLength_) {
        throw std::invalid_argument("WordSetK length limit mismatch");
    }
}

} // namespace zbik
