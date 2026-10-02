#include "CompressedParseTable.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace zbik {
namespace {

// Packed format: 64-bit counts, offsets, state/rule IDs and action tags;
// 32-bit terminal IDs (including EOF). Each ACTION entry stores its word length.
std::size_t entryBytes(const LookaheadWord &word) { return 24 + 4 * word.size(); }

std::string quote(std::string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    std::ostringstream out;
    out << '"';
    for (const unsigned char character: text) {
        switch (character) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (character < 0x20U) {
                    out << "\\u00" << hex[character >> 4U] << hex[character & 0x0fU];
                } else {
                    out << static_cast<char>(character);
                }
                break;
        }
    }
    out << '"';
    return out.str();
}

std::string dumpWord(
        const LookaheadWord &word,
        const std::vector<std::string> &terminalNames,
        std::string_view eofRepresentation) {
    std::ostringstream out;
    out << '[';
    for (std::size_t i = 0; i < word.size(); ++i) {
        if (i != 0) out << ", ";
        const LookaheadSymbol &symbol = word.symbols()[i];
        if (isEndOfInput(symbol)) {
            out << eofRepresentation;
        } else {
            out << quote(terminalNames.at(
                    toIndex(std::get<TerminalId>(symbol))));
        }
    }
    out << ']';
    return out.str();
}

std::string dumpJsonAction(const Action &action) {
    return std::visit([](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Shift>) {
            return "{\"kind\": \"shift\", \"state\": "
                    + std::to_string(value.target.value) + '}';
        } else if constexpr (std::is_same_v<T, Reduce>) {
            return "{\"kind\": \"reduce\", \"rule\": "
                    + std::to_string(value.rule.value) + '}';
        } else {
            return "{\"kind\": \"accept\"}";
        }
    }, action);
}

std::string dumpDslAction(const Action &action) {
    return std::visit([](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Shift>) {
            return "shift " + std::to_string(value.target.value);
        } else if constexpr (std::is_same_v<T, Reduce>) {
            return "reduce " + std::to_string(value.rule.value);
        } else {
            return "accept";
        }
    }, action);
}

const char *parserName(ParseTableKind kind) {
    switch (kind) {
        case ParseTableKind::LR: return "LR";
        case ParseTableKind::LALR: return "LALR";
        case ParseTableKind::SLR: return "SLR";
    }
    throw std::logic_error("unknown parse table kind");
}

template<class T>
std::size_t intern(std::vector<T> &pool, T value) {
    const auto found = std::find(pool.begin(), pool.end(), value);
    if (found != pool.end()) return static_cast<std::size_t>(found - pool.begin());
    pool.push_back(std::move(value));
    return pool.size() - 1;
}

} // namespace

CompressedParseTable::CompressedParseTable(const ParseTable &table)
    : kind_(table.kind()), start_(table.start()), k_(table.maxLength()),
      terminals_(table.grammar().terminalCount()),
      nonterminals_(table.grammar().nonterminalCount()) {
    if (table.hasConflicts()) {
        throw std::invalid_argument("compression requires a conflict-free table");
    }
    terminalNames_.reserve(terminals_);
    for (std::size_t i = 0; i < terminals_; ++i) {
        terminalNames_.push_back(table.grammar().terminalName(
                TerminalId{static_cast<std::uint32_t>(i)}));
    }
    nonterminalNames_.reserve(nonterminals_);
    for (std::size_t i = 0; i < nonterminals_; ++i) {
        nonterminalNames_.push_back(table.grammar().nonterminalName(
                NonterminalId{static_cast<std::uint32_t>(i)}));
    }
    // Common header: k, start state, terminal count, state count.
    stats_.uncompressedBytes = 32;
    stats_.compressedBytes = 32 + 16 * table.stateCount();
    for (const ActionRow &source: table.actionRows()) {
        Row row;
        std::map<Action, std::size_t> savings;
        stats_.uncompressedBytes += 8; // Count; absent keys imply error.
        for (const auto &[word, cell]: source) {
            if (cell.empty()) continue;
            const Action action = cell.actions().front();
            row.exceptions.emplace(word, action);
            stats_.uncompressedBytes += entryBytes(word);
            if (std::holds_alternative<Reduce>(action)) savings[action] += entryBytes(word);
        }
        std::size_t best = 0;
        for (const auto &[action, bytes]: savings) {
            if (bytes > best) { best = bytes; row.fallback = action; }
        }
        if (row.fallback) {
            std::erase_if(row.exceptions, [&](const auto &entry) {
                return entry.second == *row.fallback;
            });
        }
        rowIds_.push_back(intern(rows_, std::move(row)));
    }
    for (const GotoRow &row: table.gotoRows()) {
        stats_.uncompressedBytes += 8 + 12 * row.size();
        gotoIds_.push_back(intern(gotos_, row));
    }
    for (const Row &row: rows_) {
        stats_.compressedBytes += 24;
        for (const auto &[word, action]: row.exceptions) {
            static_cast<void>(action);
            stats_.compressedBytes += entryBytes(word);
        }
    }
    for (const GotoRow &row: gotos_) stats_.compressedBytes += 8 + 12 * row.size();
    stats_.uniqueActionRows = rows_.size();
    stats_.uniqueGotoRows = gotos_.size();
}

std::optional<Action> CompressedParseTable::action(
        StateId state, const LookaheadWord &word) const {
    const Row &selected = rows_.at(rowIds_.at(toIndex(state)));
    if (word.empty() || word.size() > k_) return std::nullopt;
    if (word.size() < k_ && !isEndOfInput(word.symbols().back())) return std::nullopt;
    for (const auto &symbol: word.symbols()) {
        if (const auto terminal = std::get_if<TerminalId>(&symbol);
            terminal && toIndex(*terminal) >= terminals_) return std::nullopt;
    }
    const auto found = selected.exceptions.find(word);
    return found == selected.exceptions.end() ? selected.fallback : found->second;
}

std::optional<StateId> CompressedParseTable::goTo(StateId state, NonterminalId symbol) const {
    const GotoRow &row = gotos_.at(gotoIds_.at(toIndex(state)));
    const auto found = row.find(symbol);
    return found == row.end() ? std::nullopt : std::optional{found->second};
}

std::string CompressedParseTable::dumpJson() const {
    std::ostringstream out;
    std::string parser = parserName(kind_);
    if (kind_ != ParseTableKind::SLR) {
        parser += '(' + std::to_string(k_) + ')';
    }
    out << "{\n"
        << "  \"parser\": " << quote(parser) << ",\n"
        << "  \"startState\": " << start_.value << ",\n"
        << "  \"actionRows\": [\n";

    for (std::size_t id = 0; id < rows_.size(); ++id) {
        const Row &row = rows_[id];
        out << "    {\n"
            << "      \"entries\": [";
        if (!row.exceptions.empty()) out << '\n';
        std::size_t entry = 0;
        for (const auto &[word, action]: row.exceptions) {
            out << "        {\"lookahead\": "
                << dumpWord(word, terminalNames_, "{\"eof\": true}")
                << ", \"action\": " << dumpJsonAction(action) << '}';
            if (++entry != row.exceptions.size()) out << ',';
            out << '\n';
        }
        if (!row.exceptions.empty()) out << "      ";
        out << "],\n"
            << "      \"default\": "
            << (row.fallback ? dumpJsonAction(*row.fallback)
                             : "{\"kind\": \"error\"}")
            << "\n    }";
        if (id + 1 != rows_.size()) out << ',';
        out << '\n';
    }
    out << "  ],\n"
        << "  \"actionStateRows\": [";
    for (std::size_t state = 0; state < rowIds_.size(); ++state) {
        if (state != 0) out << ", ";
        out << rowIds_[state];
    }
    out << "],\n"
        << "  \"gotoRows\": [\n";

    for (std::size_t id = 0; id < gotos_.size(); ++id) {
        out << "    [";
        if (!gotos_[id].empty()) out << '\n';
        std::size_t entry = 0;
        for (const auto &[nonterminal, target]: gotos_[id]) {
            out << "      {\"nonterminal\": "
                << quote(nonterminalNames_.at(toIndex(nonterminal)))
                << ", \"state\": " << target.value << '}';
            if (++entry != gotos_[id].size()) out << ',';
            out << '\n';
        }
        if (!gotos_[id].empty()) out << "    ";
        out << ']';
        if (id + 1 != gotos_.size()) out << ',';
        out << '\n';
    }
    out << "  ],\n"
        << "  \"gotoStateRows\": [";

    for (std::size_t state = 0; state < rowIds_.size(); ++state) {
        if (state != 0) out << ", ";
        out << gotoIds_[state];
    }
    out << "]\n}\n";
    return out.str();
}

std::string CompressedParseTable::dumpDsl() const {
    std::string parser = parserName(kind_);
    if (kind_ != ParseTableKind::SLR) {
        parser += '(' + std::to_string(k_) + ')';
    }

    std::ostringstream out;
    out << "compressed-table " << quote(parser) << " {\n"
        << "  start-state " << start_.value << ";\n\n";

    for (std::size_t id = 0; id < rows_.size(); ++id) {
        const Row &row = rows_[id];
        out << "  action-row " << id << " {\n";
        for (const auto &[word, action]: row.exceptions) {
            out << "    " << dumpWord(word, terminalNames_, "EOF") << " => "
                << dumpDslAction(action) << ";\n";
        }
        out << "    any => "
            << (row.fallback ? dumpDslAction(*row.fallback) : "error")
            << ";\n"
            << "  }\n";
    }

    out << "\n  action-state-rows [";
    for (std::size_t state = 0; state < rowIds_.size(); ++state) {
        if (state != 0) out << ", ";
        out << rowIds_[state];
    }
    out << "];\n\n";

    for (std::size_t id = 0; id < gotos_.size(); ++id) {
        out << "  goto-row " << id << " {\n";
        for (const auto &[nonterminal, target]: gotos_[id]) {
            out << "    " << quote(nonterminalNames_.at(toIndex(nonterminal)))
                << " => " << target.value << ";\n";
        }
        out << "  }\n";
    }

    out << "\n  goto-state-rows [";
    for (std::size_t state = 0; state < gotoIds_.size(); ++state) {
        if (state != 0) out << ", ";
        out << gotoIds_[state];
    }
    out << "];\n}\n";
    return out.str();
}

} // namespace zbik
