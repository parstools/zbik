#pragma once

#include <cstddef>
#include <vector>

#include "DynamicBitset.h"
#include "grammar/Identifiers.h"

namespace zbik {

class TerminalSet {
public:
    using size_type = std::size_t;

    explicit TerminalSet(size_type terminalCount);

    [[nodiscard]] size_type size() const noexcept;
    [[nodiscard]] size_type count() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    bool add(TerminalId terminal);
    bool remove(TerminalId terminal);
    [[nodiscard]] bool contains(TerminalId terminal) const;
    [[nodiscard]] std::vector<TerminalId> values() const;

    void clear();

    bool unionWith(const TerminalSet &other);

    bool operator==(const TerminalSet &) const = default;

    TerminalSet &operator|=(const TerminalSet &other);
    TerminalSet &operator&=(const TerminalSet &other);
    TerminalSet &operator^=(const TerminalSet &other);

    friend TerminalSet operator|(TerminalSet lhs, const TerminalSet &rhs);
    friend TerminalSet operator&(TerminalSet lhs, const TerminalSet &rhs);
    friend TerminalSet operator^(TerminalSet lhs, const TerminalSet &rhs);
private:
    DynamicBitset bits_;
};

} // namespace zbik
