#include <stdexcept>

#include "TerminalSet.h"

namespace zbik {

TerminalSet::TerminalSet(size_type terminalCount) : bits_(terminalCount) {}

TerminalSet::size_type TerminalSet::size() const noexcept {
    return bits_.size();
}

TerminalSet::size_type TerminalSet::count() const noexcept {
    return bits_.count();
}

bool TerminalSet::empty() const noexcept {
    return bits_.none();
}

bool TerminalSet::add(TerminalId terminal) {
    return bits_.set_and_changed(toIndex(terminal), true);
}

bool TerminalSet::remove(TerminalId terminal) {
    return bits_.set_and_changed(toIndex(terminal), false);
}

bool TerminalSet::contains(TerminalId terminal) const {
    return bits_.test(toIndex(terminal));
}

std::vector<TerminalId> TerminalSet::values() const {
    std::vector<TerminalId> result;
    result.reserve(count());
    for (size_type i = 0; i < size(); ++i) {
        const TerminalId terminal{static_cast<std::uint32_t>(i)};
        if (contains(terminal)) {
            result.push_back(terminal);
        }
    }
    return result;
}

void TerminalSet::clear() {
    bits_.clear();
}

bool TerminalSet::unionWith(const TerminalSet &other) {
    return bits_.or_assign_and_changed(other.bits_);
}

TerminalSet &TerminalSet::operator|=(const TerminalSet &other) {
    if (bits_.size() != other.bits_.size()) {
        throw std::invalid_argument("TokenSet::operator|=: size mismatch");
    }

    bits_ |= other.bits_;
    return *this;
}

TerminalSet &TerminalSet::operator&=(const TerminalSet &other) {
    if (bits_.size() != other.bits_.size()) {
        throw std::invalid_argument("TokenSet::operator&=: size mismatch");
    }

    bits_ &= other.bits_;
    return *this;
}

TerminalSet &TerminalSet::operator^=(const TerminalSet &other) {
    if (bits_.size() != other.bits_.size()) {
        throw std::invalid_argument("TokenSet::operator^=: size mismatch");
    }

    bits_ ^= other.bits_;
    return *this;
}

TerminalSet operator|(TerminalSet lhs, const TerminalSet &rhs) {
    lhs |= rhs;
    return lhs;
}

TerminalSet operator&(TerminalSet lhs, const TerminalSet &rhs) {
    lhs &= rhs;
    return lhs;
}

TerminalSet operator^(TerminalSet lhs, const TerminalSet &rhs) {
    lhs ^= rhs;
    return lhs;
}

} // namespace zbik
