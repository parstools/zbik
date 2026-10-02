#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "grammar/GrammarBuilder.h"
#include "lexer/LexerAutomaton.h"
#include "lr/LRMachine.h"
#include "lr/ContextualLRMachine.h"
#include "lr/ScopedLexerGrammar.h"

namespace {

using Features = std::uint64_t;
constexpr Features readFeature = 1U;
constexpr Features shiftFeature = 2U;

struct TokenRule {
    std::string name;
    std::string pattern;
    Features required{};
};

struct Sample {
    std::string input;
    Features bodyClass;
    std::vector<std::string> tokens;
};

struct Scope {
    std::string name;
    Features enabled{};
    Features disabled{};
};

struct Fixture {
    std::vector<std::string> productions;
    std::vector<TokenRule> lexer;
    std::vector<Sample> samples;
    std::vector<Scope> scopes;
};

auto shiftFixture() -> Fixture {
    return {
        {"S -> A ruleA", "S -> C ruleB", "ruleA -> SHR B", "ruleB -> GT GT D"},
        {{"A", "'a'"}, {"C", "'c'"}, {"B", "'b'"}, {"D", "'d'"},
         {"SHR", "'>>'", shiftFeature}, {"GT", "'>'"}, {"", "[ \\t\\n]+"}},
        {{"a>>b", shiftFeature, {"A", "SHR", "B"}},
         {"c>>d", 0, {"C", "GT", "GT", "D"}},
         {"c > > d", 0, {"C", "GT", "GT", "D"}}},
        {{"ruleA", shiftFeature, 0}, {"ruleB", 0, shiftFeature}},
    };
}

auto keywordFixture() -> Fixture {
    return {
        {"S -> AT ruleA", "S -> HASH ruleB", "ruleA -> READ END", "ruleA -> WRITE END", "ruleB -> IDENT END"},
        {{"AT", "'@'"}, {"HASH", "'#'"}, {"READ", "'read'", readFeature},
         {"WRITE", "'write'", readFeature}, {"IDENT", "[a-z]+"}, {"END", "';'"}, {"", "[ \\t\\n]+"}},
        {{"@read;", readFeature, {"AT", "READ", "END"}},
         {"#read;", 0, {"HASH", "IDENT", "END"}},
         {"#reader;", 0, {"HASH", "IDENT", "END"}},
         {"@write;", readFeature, {"AT", "WRITE", "END"}},
         {"#write;", 0, {"HASH", "IDENT", "END"}}},
        {{"ruleA", readFeature, 0}, {"ruleB", 0, readFeature}},
    };
}

auto combinedFixture() -> Fixture {
    return {
        {"S -> ZERO plain", "S -> ONE keyword", "S -> TWO shift", "S -> THREE both",
         "plain -> IDENT GT GT END", "keyword -> READ GT GT END",
         "shift -> IDENT SHR END", "both -> READ SHR END"},
        {{"ZERO", "'0'"}, {"ONE", "'1'"}, {"TWO", "'2'"}, {"THREE", "'3'"},
         {"READ", "'read'", readFeature}, {"IDENT", "[a-z]+"},
         {"SHR", "'>>'", shiftFeature}, {"GT", "'>'"},
         {"END", "';'"}, {"", "[ \\t\\n]+"}},
        {{"0read>>;", 0, {"ZERO", "IDENT", "GT", "GT", "END"}},
         {"1read>>;", readFeature, {"ONE", "READ", "GT", "GT", "END"}},
         {"2read>>;", shiftFeature, {"TWO", "IDENT", "SHR", "END"}},
         {"3read>>;", readFeature | shiftFeature, {"THREE", "READ", "SHR", "END"}}},
        {{"plain", 0, readFeature | shiftFeature}, {"keyword", readFeature, shiftFeature},
         {"shift", shiftFeature, readFeature}, {"both", readFeature | shiftFeature, 0}},
    };
}

// An ASCII, eager-regex oracle for the fixtures, not the production runtime.
// Compile each rule once; no lexer or DFA is built per feature combination.
class FixtureLexer {
public:
    FixtureLexer(const zbik::Grammar &grammar, const std::vector<TokenRule> &rules) {
        for (const auto &rule : rules) {
            std::optional<zbik::TerminalId> terminal;
            if (!rule.name.empty()) {
                terminal = grammar.findTerminal(rule.name);
                if (!terminal)
                    throw std::runtime_error("unknown fixture terminal: " + rule.name);
            }
            const std::array<zbik::LexerRule, 1> definition{{{terminal, rule.pattern}}};
            compiled_.push_back({terminal, rule.required, zbik::LexerAutomaton(definition)});
        }
    }

    auto compiledRuleCount() const -> std::size_t { return compiled_.size(); }

    auto next(std::string_view source, std::size_t &offset, Features active) const
            -> std::optional<zbik::LexedToken> {
        while (offset < source.size()) {
            const Compiled *best = nullptr;
            std::size_t length = 0;
            for (const auto &rule : compiled_) {
                if ((active & rule.required) != rule.required)
                    continue;
                std::size_t state = 0;
                for (std::size_t cursor = offset; cursor < source.size(); ++cursor) {
                    auto target = rule.dfa.nextState(state, static_cast<unsigned char>(source[cursor]));
                    if (!target)
                        break;
                    state = *target;
                    if (rule.dfa.acceptingRule(state) && cursor + 1 - offset > length) {
                        best = &rule;
                        length = cursor + 1 - offset;
                    }
                }
            }
            if (!best)
                throw std::runtime_error("fixture lexer: no match at byte " + std::to_string(offset));
            const auto begin = offset;
            offset += length;
            if (best->terminal)
                return zbik::LexedToken{*best->terminal, begin, std::string(source.substr(begin, length))};
        }
        return std::nullopt;
    }

private:
    struct Compiled {
        std::optional<zbik::TerminalId> terminal;
        Features required;
        zbik::LexerAutomaton dfa;
    };
    std::vector<Compiled> compiled_;
};

// The selector-to-class association is authored in Sample.bodyClass.
// This gives the future parser-directed implementation an independent oracle.
auto tokenize(const FixtureLexer &lexer, const Sample &sample) -> zbik::LexResult {
    zbik::LexResult result;
    std::size_t offset = 0;
    while (auto token = lexer.next(sample.input, offset,
                                  result.tokens.empty() ? 0 : sample.bodyClass)) {
        result.terminalIds.push_back(token->terminal);
        result.tokens.push_back(*token);
    }
    return result;
}

void verifyFixture(const Fixture &fixture, std::size_t k) {
    const auto grammar = zbik::GrammarBuilder{}.build(fixture.productions);
    const FixtureLexer lexer(grammar, fixture.lexer);
    const zbik::ParseTable table(zbik::LRkDfa(grammar, k));
    ASSERT_FALSE(table.hasConflicts());
    const zbik::LRMachine parser(table);
    std::vector<zbik::ParserLexerClass> declarations;
    for (const auto &scope : fixture.scopes)
        declarations.push_back({*grammar.findNonterminal(scope.name), scope.enabled, scope.disabled});
    std::vector<zbik::LexerRule> rules;
    for (const auto &rule : fixture.lexer)
        rules.push_back({rule.name.empty() ? std::nullopt : grammar.findTerminal(rule.name),
                         rule.pattern, rule.required});
    const zbik::ByteLexer byteLexer(rules);
    const zbik::Utf8Lexer utf8Lexer(rules);
    const auto scoped = zbik::scopeLexerClasses(grammar, declarations, byteLexer);
    const zbik::ParseTable scopedTable(zbik::LRkDfa(scoped.grammar, k));
    const zbik::ContextualLRMachine contextual(scopedTable, scoped.requirements, scoped.sourceTerminals);
    for (const auto &sample : fixture.samples) {
        SCOPED_TRACE(sample.input);
        const auto lexed = tokenize(lexer, sample);
        std::vector<std::string> names;
        for (const auto &token : lexed.tokens) {
            names.push_back(grammar.terminalName(token.terminal));
            EXPECT_EQ(sample.input.substr(token.offset, token.text.size()), token.text);
        }
        EXPECT_EQ(names, sample.tokens);
        const auto actual = contextual.parse(byteLexer, sample.input, true);
        ASSERT_TRUE(actual.accepted) << actual.error;
        EXPECT_EQ(actual.tokens, lexed.tokens);
        const auto unicode = contextual.parse(utf8Lexer, sample.input);
        ASSERT_TRUE(unicode.accepted) << unicode.error;
        EXPECT_EQ(unicode.tokens, lexed.tokens);
        if (k == 2) {
            ASSERT_GE(actual.requests.size(), 2U);
            EXPECT_EQ(actual.requests[1].state, scopedTable.start());
            EXPECT_EQ(actual.requests[1].prefix.size(), 1U);
            const auto relevant = sample.tokens[1] == "SHR" || sample.tokens[1] == "GT"
                ? shiftFeature : readFeature;
            EXPECT_EQ(actual.requests[1].active, sample.bodyClass & relevant);
        }
        const auto parsed = parser.parse(lexed.terminalIds, true);
        ASSERT_TRUE(parsed.accepted);
        ASSERT_FALSE(parsed.trace.empty());
        ASSERT_EQ(parsed.trace.front().lookahead.size(), k);
        if (k == 2) {
            // The second token already uses the body's class before any shift.
            EXPECT_EQ(parsed.trace.front().stack.size(), 1U);
            EXPECT_EQ(parsed.trace.front().lookahead.symbols()[1],
                      zbik::LookaheadSymbol{lexed.terminalIds[1]});
        }
    }
    EXPECT_EQ(lexer.compiledRuleCount(), fixture.lexer.size());
}

TEST(ContextualLexerFixturesTest, ShiftBoundariesLr1) { verifyFixture(shiftFixture(), 1); }
TEST(ContextualLexerFixturesTest, ShiftBoundariesLr2) { verifyFixture(shiftFixture(), 2); }
TEST(ContextualLexerFixturesTest, ReadKeywordLr1) { verifyFixture(keywordFixture(), 1); }
TEST(ContextualLexerFixturesTest, ReadKeywordLr2) { verifyFixture(keywordFixture(), 2); }
TEST(ContextualLexerFixturesTest, CombinedFeaturesLr1) { verifyFixture(combinedFixture(), 1); }
TEST(ContextualLexerFixturesTest, CombinedFeaturesLr2) { verifyFixture(combinedFixture(), 2); }

TEST(ContextualLexerFixturesTest, AUniversalClassLosesThreeValidBranches) {
    const auto fixture = combinedFixture();
    const auto grammar = zbik::GrammarBuilder{}.build(fixture.productions);
    const FixtureLexer lexer(grammar, fixture.lexer);
    for (const std::size_t k : {1U, 2U}) {
        const zbik::ParseTable table(zbik::LRkDfa(grammar, k));
        ASSERT_FALSE(table.hasConflicts());
        const zbik::LRMachine parser(table);
        for (auto sample : fixture.samples) {
            SCOPED_TRACE(sample.input);
            const auto wanted = sample.bodyClass;
            sample.bodyClass = readFeature | shiftFeature;
            const auto lexed = tokenize(lexer, sample);
            EXPECT_EQ(parser.parse(lexed.terminalIds).accepted,
                      wanted == (readFeature | shiftFeature));
        }
    }
}

TEST(ContextualLexerFixturesTest, SecondLookaheadCannotUseTheInitialClass) {
    const auto fixture = shiftFixture();
    const auto grammar = zbik::GrammarBuilder{}.build(fixture.productions);
    const FixtureLexer lexer(grammar, fixture.lexer);
    const zbik::ParseTable table(zbik::LRkDfa(grammar, 2));
    const auto &sample = fixture.samples.front();
    std::size_t offset = 0;
    const auto first = lexer.next(sample.input, offset, 0);
    const auto second = lexer.next(sample.input, offset, 0);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(grammar.terminalName(second->terminal), "GT");
    EXPECT_TRUE(table.actions(table.start(), zbik::LookaheadWord{
        first->terminal, second->terminal}).empty());
    offset = first->offset + first->text.size();
    const auto corrected = lexer.next(sample.input, offset, shiftFeature);
    ASSERT_TRUE(corrected);
    EXPECT_EQ(corrected->text, ">>");
    EXPECT_EQ(corrected->offset, 1U);
    EXPECT_FALSE(table.actions(table.start(), zbik::LookaheadWord{
        first->terminal, corrected->terminal}).empty());
}

TEST(ContextualLexerFixturesTest, FeatureBitsDoNotCreateAPowersetOfLexers) {
    auto fixture = keywordFixture();
    fixture.lexer[2].required |= Features{1} << 63;
    const auto grammar = zbik::GrammarBuilder{}.build(fixture.productions);
    const FixtureLexer lexer(grammar, fixture.lexer);
    std::size_t offset = 0;
    EXPECT_EQ(lexer.next("read", offset, readFeature)->terminal,
              grammar.findTerminal("IDENT"));
    offset = 0;
    EXPECT_EQ(lexer.next("read", offset, readFeature | (Features{1} << 63))->terminal,
              grammar.findTerminal("READ"));
    offset = 0;
    EXPECT_EQ(lexer.next("reader", offset, ~Features{0})->terminal,
              grammar.findTerminal("IDENT"));
    EXPECT_EQ(lexer.compiledRuleCount(), fixture.lexer.size());
}

TEST(ContextualLexerFixturesTest, NestedParserRuleDisablesAndRestoresClasses) {
    const auto grammar = zbik::GrammarBuilder{}.build({
        "S -> outer END", "outer -> A SHR inner SHR READ WRITE D",
        "inner -> L GT GT IDENT R",
    });
    constexpr Features gtFeature = 4;
    constexpr Features all = readFeature | shiftFeature | gtFeature;
    const auto terminal = [&](std::string_view name) { return *grammar.findTerminal(name); };
    const zbik::Utf8Lexer lexer({
        {terminal("A"), "'a'"}, {terminal("D"), "'d'"},
        {terminal("SHR"), "'>>'", shiftFeature}, {terminal("GT"), "'>'", gtFeature},
        {terminal("READ"), "'read'", readFeature}, {terminal("WRITE"), "'write'", readFeature},
        {terminal("IDENT"), "[a-z]+"}, {terminal("L"), "'['"},
        {terminal("R"), "']'"}, {terminal("END"), "';'"}, {std::nullopt, "[ ]+"},
    });
    const std::string source = "a>>[>>read]>>read write d;";
    for (const auto initial : {Features{0}, all}) {
        const std::array<zbik::ParserLexerClass, 2> declarations{{
            {*grammar.findNonterminal("outer"), initial ? 0 : all, 0},
            {*grammar.findNonterminal("inner"), 0, readFeature | shiftFeature},
        }};
        const auto scoped = zbik::scopeLexerClasses(grammar, declarations, lexer, initial);
        for (const std::size_t k : {1U, 2U}) {
            const zbik::ParseTable table(zbik::LRkDfa(scoped.grammar, k));
            const zbik::ContextualLRMachine parser(table, scoped.requirements, scoped.sourceTerminals);
            const auto result = parser.parse(lexer, source, true);
            ASSERT_TRUE(result.accepted) << result.error;
            std::vector<std::string> names;
            for (const auto &token : result.tokens) names.push_back(grammar.terminalName(token.terminal));
            EXPECT_EQ(names, (std::vector<std::string>{
                "A", "SHR", "L", "GT", "GT", "IDENT", "R", "SHR", "READ", "WRITE", "D", "END"}));
        }
    }
    std::size_t offset = 0;
    EXPECT_THROW((void)lexer.next(">>", offset, 0), zbik::Utf8LexerError);
}

TEST(ContextualLexerFixturesTest, RejectsIncompatibleClassesAtSecondLookaheadBeforeParsing) {
    const auto grammar = zbik::GrammarBuilder{}.build({
        "S -> X a", "S -> X b", "a -> SHR END", "b -> GT GT END",
    });
    const std::array<zbik::ParserLexerClass, 2> declarations{{
        {*grammar.findNonterminal("a"), shiftFeature, 0},
        {*grammar.findNonterminal("b"), 0, shiftFeature},
    }};
    const auto scoped = zbik::scopeLexerClasses(grammar, declarations);
    const zbik::ParseTable table(zbik::LRkDfa(scoped.grammar, 2));
    ASSERT_FALSE(table.hasConflicts());
    try {
        const zbik::LexerContextPlan plan(table, scoped.requirements);
        FAIL() << "Expected incompatible lexer scopes";
    } catch (const zbik::LexerContextError &error) {
        EXPECT_EQ(error.state(), table.start());
        EXPECT_EQ(error.prefix().size(), 1U);
        EXPECT_EQ(error.conflictingBits(), shiftFeature);
        EXPECT_NE(std::string(error.what()).find("SHR"), std::string::npos);
        EXPECT_NE(std::string(error.what()).find("GT"), std::string::npos);
    }
}

TEST(ContextualLexerFixturesTest, RejectsContradictoryDeclarationsAndBoundsReachableContexts) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> child", "child -> X"});
    const std::array<zbik::ParserLexerClass, 1> declarations{{{grammar.start(), 1, 1}}};
    EXPECT_THROW((void)zbik::scopeLexerClasses(grammar, declarations), std::invalid_argument);
    EXPECT_THROW((void)zbik::scopeLexerClasses(grammar, {}, 0, 1), std::length_error);
    const zbik::ByteLexer lexer({{*grammar.findTerminal("X"), "'x'"}, {std::nullopt, "[ ]+", 1}});
    EXPECT_THROW((void)zbik::scopeLexerClasses(grammar, {}, lexer), std::invalid_argument);
}

TEST(ContextualLexerFixturesTest, DetectsMissingEnableDeclarationBeforeReadingInput) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> SHR"});
    const auto scoped = zbik::scopeLexerClasses(grammar, {});
    const zbik::ParseTable table(zbik::LRkDfa(scoped.grammar, 2));
    const zbik::ContextualLRMachine parser(table, scoped.requirements, scoped.sourceTerminals);
    const zbik::ByteLexer lexer({{*grammar.findTerminal("SHR"), "'>>'", shiftFeature}});
    try {
        parser.validateLexer(lexer.rules());
        FAIL() << "Expected a disabled-token diagnostic";
    } catch (const zbik::LexerContextError &error) {
        EXPECT_EQ(error.conflictingBits(), shiftFeature);
        EXPECT_NE(std::string(error.what()).find("SHR"), std::string::npos);
    }
}

TEST(ContextualLexerFixturesTest, RecursiveTypeScopeAndNullableRuleRestoreExpressionClasses) {
    const auto grammar = zbik::GrammarBuilder{}.build({
        "S -> type optional SHR END", "type -> IDENT", "type -> IDENT LT type GT", "optional ->",
    });
    const std::array<zbik::ParserLexerClass, 3> declarations{{
        {grammar.start(), shiftFeature, 0},
        {*grammar.findNonterminal("type"), 0, shiftFeature},
        {*grammar.findNonterminal("optional"), readFeature, shiftFeature},
    }};
    const auto terminal = [&](std::string_view name) { return *grammar.findTerminal(name); };
    const zbik::ByteLexer lexer({
        {terminal("SHR"), "'>>'", shiftFeature}, {terminal("GT"), "'>'"},
        {terminal("LT"), "'<'"}, {terminal("IDENT"), "[a-z]+"}, {terminal("END"), "';'"},
    });
    const auto scoped = zbik::scopeLexerClasses(grammar, declarations, lexer);
    for (const std::size_t k : {1U, 2U}) {
        const zbik::ParseTable table(zbik::LRkDfa(scoped.grammar, k));
        const zbik::ContextualLRMachine parser(table, scoped.requirements, scoped.sourceTerminals);
        const auto result = parser.parse(lexer, "outer<inner<int>>>>;");
        ASSERT_TRUE(result.accepted) << result.error;
        std::vector<std::string> names;
        for (const auto &token : result.tokens) names.push_back(grammar.terminalName(token.terminal));
        EXPECT_EQ(names, (std::vector<std::string>{"IDENT", "LT", "IDENT", "LT", "IDENT", "GT", "GT", "SHR", "END"}));
    }
}

} // namespace
