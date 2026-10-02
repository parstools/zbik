#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <climits>
#include <bit>         // std::popcount (C++20)
#include <stdexcept>
#include <string>

namespace zbik {

class DynamicBitset {
public:
    using size_type  = std::size_t;
    using block_type = std::uint64_t;

    static constexpr size_type bits_per_block = 64;

    explicit DynamicBitset(size_type num_bits = 0)
        : num_bits_(num_bits),
          blocks_(blocks_for_bits(num_bits), 0ULL) {}

    size_type size() const noexcept {
        return num_bits_;
    }

    bool empty() const noexcept {
        return num_bits_ == 0;
    }

    void resize(size_type num_bits, bool value = false) {
        num_bits_ = num_bits;
        blocks_.resize(blocks_for_bits(num_bits),
                       value ? ~0ULL : 0ULL);
        trim_excess_bits();
    }

    void clear() noexcept {
        blocks_.assign(blocks_.size(), 0ULL);
    }

    // --- single bit operations  ---

    void set(size_type pos, bool value = true) {
        check_index(pos);
        const size_type bi  = block_index(pos);
        const size_type bit = bit_index(pos);
        const block_type mask = block_type(1) << bit;

        if (value)
            blocks_[bi] |= mask;
        else
            blocks_[bi] &= ~mask;
    }

    void reset(size_type pos) {
        set(pos, false);
    }

    void flip(size_type pos) {
        check_index(pos);
        const size_type bi  = block_index(pos);
        const size_type bit = bit_index(pos);
        const block_type mask = block_type(1) << bit;
        blocks_[bi] ^= mask;
    }

    bool test(size_type pos) const {
        check_index(pos);
        const size_type bi  = block_index(pos);
        const size_type bit = bit_index(pos);
        const block_type mask = block_type(1) << bit;
        return (blocks_[bi] & mask) != 0;
    }

    bool operator[](size_type pos) const {
        return test(pos);
    }

    bool set_and_changed(size_type pos, bool value = true) {
        check_index(pos);
        const size_type bi  = block_index(pos);
        const size_type bit = bit_index(pos);
        const block_type mask = block_type(1) << bit;

        block_type before = blocks_[bi];

        if (value)
            blocks_[bi] |= mask;
        else
            blocks_[bi] &= ~mask;

        return blocks_[bi] != before;
    }

    // --- global operations ---

    bool any() const noexcept {
        for (block_type b : blocks_) {
            if (b != 0ULL)
                return true;
        }
        return false;
    }

    bool none() const noexcept {
        return !any();
    }

    std::size_t count() const noexcept {
        std::size_t c = 0;
        for (block_type b : blocks_) {
            c += std::popcount(b);
        }
        return c;
    }

    bool operator==(const DynamicBitset &) const = default;

    // --- bitwise operations on entire masks ---

    DynamicBitset& operator&=(const DynamicBitset& other) {
        check_compatible(other);
        for (size_type i = 0; i < blocks_.size(); ++i)
            blocks_[i] &= other.blocks_[i];
        return *this;
    }

    DynamicBitset& operator|=(const DynamicBitset& other) {
        check_compatible(other);
        for (size_type i = 0; i < blocks_.size(); ++i)
            blocks_[i] |= other.blocks_[i];
        return *this;
    }

    DynamicBitset& operator^=(const DynamicBitset& other) {
        check_compatible(other);
        for (size_type i = 0; i < blocks_.size(); ++i)
            blocks_[i] ^= other.blocks_[i];
        return *this;
    }

    bool or_assign_and_changed(const DynamicBitset& other) {
        check_compatible(other);

        bool changed = false;

        for (size_type i = 0; i < blocks_.size(); ++i) {
            block_type before = blocks_[i];
            block_type after  = before | other.blocks_[i];
            blocks_[i] = after;
            if (after != before)
                changed = true;
        }

        return changed;
    }

    friend DynamicBitset operator&(DynamicBitset a, const DynamicBitset& b) {
        a &= b;
        return a;
    }

    friend DynamicBitset operator|(DynamicBitset a, const DynamicBitset& b) {
        a |= b;
        return a;
    }

    friend DynamicBitset operator^(DynamicBitset a, const DynamicBitset& b) {
        a ^= b;
        return a;
    }

    // --- debug / export as string ---

    std::string to_string() const {
        std::string s;
        s.reserve(num_bits_);
        for (size_type i = num_bits_; i > 0; --i)
            s.push_back(test(i - 1) ? '1' : '0');
        return s;
    }

private:
    size_type num_bits_;
    std::vector<block_type> blocks_;

    static constexpr size_type blocks_for_bits(size_type bits) noexcept {
        return bits == 0 ? 0
             : (bits + bits_per_block - 1) / bits_per_block;
    }

    static constexpr size_type block_index(size_type bit) noexcept {
        return bit / bits_per_block;
    }

    static constexpr size_type bit_index(size_type bit) noexcept {
        return bit % bits_per_block;
    }

    void check_index(size_type pos) const {
        if (pos >= num_bits_)
            throw std::out_of_range("DynamicBitset index out of range");
    }

    void check_compatible(const DynamicBitset& other) const {
        if (num_bits_ != other.num_bits_)
            throw std::invalid_argument("DynamicBitset size mismatch");
    }

    void trim_excess_bits() noexcept {
        if (num_bits_ == 0 || blocks_.empty())
            return;

        const size_type used = num_bits_ % bits_per_block;
        if (used == 0)
            return;

        const block_type mask = (block_type(1) << used) - 1;
        blocks_.back() &= mask;
    }
};

} // namespace zbik
