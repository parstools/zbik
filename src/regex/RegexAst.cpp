#include "regex/RegexAst.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <variant>

namespace zbik {
namespace {

struct EpsilonNode {
    friend bool operator==(const EpsilonNode &, const EpsilonNode &) = default;
};
struct ConcatenationNode {
    std::vector<RegexAst> elements;

    friend bool operator==(const ConcatenationNode &, const ConcatenationNode &) = default;
};
struct AlternationNode {
    std::vector<RegexAst> alternatives;

    friend bool operator==(const AlternationNode &, const AlternationNode &) = default;
};
struct RepetitionNode {
    RegexAst expression;
    RegexQuantifier quantifier;

    friend bool operator==(const RepetitionNode &, const RepetitionNode &) = default;
};

} // namespace

struct RegexAst::Node {
    std::variant<EpsilonNode, CodePointClass, ConcatenationNode,
                 AlternationNode, RepetitionNode> value;
};

CodePointClass::CodePointClass(std::vector<CodePointRange> ranges) {
    std::vector<CodePointRange> scalarRanges;
    for (const CodePointRange range: ranges) {
        if (range.first > range.last) {
            throw std::invalid_argument("A code-point range must be ascending");
        }
        if (range.last > maxUnicodeCodePoint) {
            throw std::invalid_argument("A code-point range exceeds Unicode");
        }
        if (range.first < firstHighSurrogate && range.last >= firstHighSurrogate) {
            scalarRanges.push_back({range.first, firstHighSurrogate - 1});
        }
        if (range.last > lastLowSurrogate && range.first <= lastLowSurrogate) {
            scalarRanges.push_back({lastLowSurrogate + 1, range.last});
        }
        if (range.last < firstHighSurrogate || range.first > lastLowSurrogate) {
            scalarRanges.push_back(range);
        }
    }
    std::ranges::sort(scalarRanges, {}, &CodePointRange::first);
    for (const CodePointRange range: scalarRanges) {
        if (ranges_.empty()
            || range.first > ranges_.back().last + 1U) {
            ranges_.push_back(range);
        } else if (range.last > ranges_.back().last) {
            ranges_.back().last = range.last;
        }
    }
}

const std::vector<CodePointRange> &CodePointClass::ranges() const noexcept {
    return ranges_;
}

bool CodePointClass::contains(CodePoint codePoint) const noexcept {
    return std::ranges::any_of(ranges_, [codePoint](CodePointRange range) {
        return range.first <= codePoint && codePoint <= range.last;
    });
}

bool CodePointClass::isByteClass() const noexcept {
    return ranges_.empty() || ranges_.back().last <= 0xFF;
}

RegexAst::RegexAst() : node_(std::make_unique<Node>(Node{EpsilonNode{}})) {}
RegexAst::~RegexAst() = default;

RegexAst::RegexAst(const RegexAst &other)
    : node_(std::make_unique<Node>(*other.node_)) {}

RegexAst::RegexAst(RegexAst &&other) noexcept = default;

RegexAst &RegexAst::operator=(const RegexAst &other) {
    if (this != &other) {
        node_ = std::make_unique<Node>(*other.node_);
    }
    return *this;
}

RegexAst &RegexAst::operator=(RegexAst &&other) noexcept = default;

RegexAst::RegexAst(std::unique_ptr<Node> node) : node_(std::move(node)) {}

RegexAst RegexAst::epsilon() {
    return RegexAst{};
}

RegexAst RegexAst::codePointClass(CodePointClass codePoints) {
    return RegexAst(std::make_unique<Node>(Node{std::move(codePoints)}));
}

RegexAst RegexAst::byteClass(ByteClass bytes) {
    return codePointClass(std::move(bytes));
}

RegexAst RegexAst::concatenate(std::vector<RegexAst> elements) {
    if (elements.empty()) return epsilon();

    std::vector<RegexAst> flattened;
    for (RegexAst &element: elements) {
        if (element.kind() == Kind::Epsilon) {
            continue;
        } else if (element.kind() == Kind::Concatenation) {
            for (const RegexAst &nested: element.elements()) {
                flattened.push_back(nested);
            }
        } else {
            flattened.push_back(std::move(element));
        }
    }
    if (flattened.empty()) return epsilon();
    if (flattened.size() == 1) return std::move(flattened.front());
    return RegexAst(std::make_unique<Node>(
            Node{ConcatenationNode{std::move(flattened)}}));
}

RegexAst RegexAst::alternate(std::vector<RegexAst> alternatives) {
    if (alternatives.size() == 1) return std::move(alternatives.front());

    std::vector<RegexAst> flattened;
    for (RegexAst &alternative: alternatives) {
        if (alternative.kind() == Kind::Alternation) {
            for (const RegexAst &nested: alternative.alternatives()) {
                flattened.push_back(nested);
            }
        } else {
            flattened.push_back(std::move(alternative));
        }
    }
    return RegexAst(std::make_unique<Node>(
            Node{AlternationNode{std::move(flattened)}}));
}

RegexAst RegexAst::repeat(RegexAst expression, RegexQuantifier quantifier) {
    return RegexAst(std::make_unique<Node>(
            Node{RepetitionNode{std::move(expression), quantifier}}));
}

RegexAst::Kind RegexAst::kind() const noexcept {
    return static_cast<Kind>(node_->value.index());
}

const CodePointClass &RegexAst::codePoints() const {
    return std::get<CodePointClass>(node_->value);
}

const ByteClass &RegexAst::bytes() const {
    return codePoints();
}

const std::vector<RegexAst> &RegexAst::elements() const {
    return std::get<ConcatenationNode>(node_->value).elements;
}

const std::vector<RegexAst> &RegexAst::alternatives() const {
    return std::get<AlternationNode>(node_->value).alternatives;
}

const RegexAst &RegexAst::repeated() const {
    return std::get<RepetitionNode>(node_->value).expression;
}

RegexQuantifier RegexAst::quantifier() const {
    return std::get<RepetitionNode>(node_->value).quantifier;
}

bool operator==(const RegexAst &left, const RegexAst &right) {
    return left.node_->value == right.node_->value;
}

} // namespace zbik
