#include <gtest/gtest.h>

#include <string>

#include "ebnf/EbnfConflictDiagnostics.h"
#include "ebnf/EbnfToBnf.h"
#include "lr/LRkDfa.h"
#include "lr/ParseTable.h"

TEST(EbnfConflictDiagnosticsTest, MapsReduceAndShiftRulesBackToEbnfAlternatives) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S",
                               {
                                       zbik::EbnfAlternative{zbik::EbnfElement{"A"}, zbik::EbnfElement{"x"}},
                                       zbik::EbnfAlternative{zbik::EbnfElement{"x"}},
                               }},
            zbik::EbnfRuleSpec{"A", {zbik::EbnfAlternative{}}},
    };
    const zbik::EbnfConversionResult conversion = zbik::EbnfToBnfConverter{}.convertWithOrigins(specification, {"x"});
    const zbik::LRkDfa dfa(conversion.grammar(), 1);
    const zbik::ParseTable table(dfa);

    ASSERT_EQ(table.conflicts().size(), 1U);
    const std::string dump = zbik::dumpEbnfConflict(specification, conversion, dfa, table.conflicts().front());

    EXPECT_NE(dump.find("shift/reduce"), std::string::npos);
    EXPECT_NE(dump.find("reductions:\n  R2 -> rule 1 'A', alternative 0"), std::string::npos);
    EXPECT_NE(dump.find("shift items:\n  R1 -> rule 0 'S', alternative 1"), std::string::npos);
}

TEST(EbnfConflictDiagnosticsTest, ListsEverySourceAlternativeInAReduceReduceConflict) {
    const zbik::EbnfGrammarSpec specification{
            zbik::EbnfRuleSpec{"S",
                               {
                                       zbik::EbnfAlternative{zbik::EbnfElement{"A"}},
                                       zbik::EbnfAlternative{zbik::EbnfElement{"B"}},
                               }},
            zbik::EbnfRuleSpec{"A", {zbik::EbnfAlternative{zbik::EbnfElement{"x"}}}},
            zbik::EbnfRuleSpec{"B", {zbik::EbnfAlternative{zbik::EbnfElement{"x"}}}},
    };
    const zbik::EbnfConversionResult conversion = zbik::EbnfToBnfConverter{}.convertWithOrigins(specification, {"x"});
    const zbik::LRkDfa dfa(conversion.grammar(), 1);
    const zbik::ParseTable table(dfa);

    ASSERT_EQ(table.conflicts().size(), 1U);
    const std::string dump = zbik::dumpEbnfConflict(specification, conversion, dfa, table.conflicts().front());

    EXPECT_NE(dump.find("reduce/reduce"), std::string::npos);
    EXPECT_NE(dump.find("R2 -> rule 1 'A', alternative 0"), std::string::npos);
    EXPECT_NE(dump.find("R3 -> rule 2 'B', alternative 0"), std::string::npos);
}
