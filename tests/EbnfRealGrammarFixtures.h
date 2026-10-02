#pragma once

#include <string>
#include <vector>

#include "ebnf/EbnfGrammar.h"

namespace zbik::test {

inline EbnfElement optional(std::string symbol) {
    return EbnfElement{std::move(symbol), Repetition::Optional};
}

inline EbnfElement zeroOrMore(std::string symbol) {
    return EbnfElement{std::move(symbol), Repetition::ZeroOrMore};
}

inline EbnfElement oneOrMore(std::string symbol) {
    return EbnfElement{std::move(symbol), Repetition::OneOrMore};
}

inline EbnfGrammarSpec cmmEbnfGrammar() {
    return EbnfGrammarSpec{
            EbnfRuleSpec{"program", {EbnfAlternative{oneOrMore("declaration")}}},
            EbnfRuleSpec{"declaration",
                         {
                                 EbnfAlternative{EbnfElement{"varDeclaration"}},
                                 EbnfAlternative{EbnfElement{"varInitialization"}},
                                 EbnfAlternative{EbnfElement{"statement"}},
                         }},
            EbnfRuleSpec{"declarator", {EbnfAlternative{EbnfElement{"ID"}}}},
            EbnfRuleSpec{"varDeclaration",
                         {EbnfAlternative{
                                 EbnfElement{"typeSpecifier"},
                                 EbnfElement{"declarator"},
                                 EbnfElement{"SEMI"},
                         }}},
            EbnfRuleSpec{"typeSpecifier", {EbnfAlternative{EbnfElement{"INT"}}}},
            EbnfRuleSpec{"compoundStatement",
                         {EbnfAlternative{
                                 EbnfElement{"LBRACE"},
                                 zeroOrMore("declaration"),
                                 EbnfElement{"RBRACE"},
                         }}},
            EbnfRuleSpec{"varInitialization",
                         {EbnfAlternative{
                                 EbnfElement{"typeSpecifier"},
                                 EbnfElement{"declarator"},
                                 EbnfElement{"ASSIGN"},
                                 EbnfElement{"additiveExpression"},
                                 EbnfElement{"SEMI"},
                         }}},
            EbnfRuleSpec{"relop",
                         {
                                 EbnfAlternative{EbnfElement{"LE"}},
                                 EbnfAlternative{EbnfElement{"LT"}},
                                 EbnfAlternative{EbnfElement{"GT"}},
                                 EbnfAlternative{EbnfElement{"GE"}},
                                 EbnfAlternative{EbnfElement{"EQ"}},
                                 EbnfAlternative{EbnfElement{"NEQ"}},
                         }},
            EbnfRuleSpec{"booleanExpression",
                         {EbnfAlternative{
                                 EbnfElement{"additiveExpression"},
                                 EbnfElement{"relop"},
                                 EbnfElement{"additiveExpression"},
                         }}},
            EbnfRuleSpec{"addop",
                         {
                                 EbnfAlternative{EbnfElement{"PLUS"}},
                                 EbnfAlternative{EbnfElement{"MINUS"}},
                         }},
            EbnfRuleSpec{"additiveExpression",
                         {EbnfAlternative{
                                 EbnfElement{"factor"},
                                 zeroOrMore("additiveOperation"),
                         }}},
            EbnfRuleSpec{"additiveOperation",
                         {EbnfAlternative{
                                 EbnfElement{"addop"},
                                 EbnfElement{"factor"},
                         }}},
            EbnfRuleSpec{"factor",
                         {
                                 EbnfAlternative{EbnfElement{"varUsage"}},
                                 EbnfAlternative{EbnfElement{"call"}},
                                 EbnfAlternative{EbnfElement{"NUMBER"}},
                                 EbnfAlternative{EbnfElement{"MINUS"}, EbnfElement{"factor"}},
                         }},
            EbnfRuleSpec{"varUsage", {EbnfAlternative{EbnfElement{"ID"}}}},
            EbnfRuleSpec{"call",
                         {EbnfAlternative{
                                 EbnfElement{"ID"},
                                 EbnfElement{"LPAREN"},
                                 optional("argList"),
                                 EbnfElement{"RPAREN"},
                         }}},
            EbnfRuleSpec{"argList",
                         {EbnfAlternative{
                                 EbnfElement{"argument"},
                                 zeroOrMore("argumentTail"),
                         }}},
            EbnfRuleSpec{"argumentTail", {EbnfAlternative{EbnfElement{"COMMA"}, EbnfElement{"argument"}}}},
            EbnfRuleSpec{"argument", {EbnfAlternative{EbnfElement{"additiveExpression"}}}},
            EbnfRuleSpec{"statement",
                         {
                                 EbnfAlternative{EbnfElement{"whileStatement"}},
                                 EbnfAlternative{EbnfElement{"assignStatement"}, EbnfElement{"SEMI"}},
                                 EbnfAlternative{EbnfElement{"call"}, EbnfElement{"SEMI"}},
                                 EbnfAlternative{EbnfElement{"compoundStatement"}},
                         }},
            EbnfRuleSpec{"whileStatement",
                         {EbnfAlternative{
                                 EbnfElement{"WHILE"},
                                 EbnfElement{"LPAREN"},
                                 EbnfElement{"booleanExpression"},
                                 EbnfElement{"RPAREN"},
                                 EbnfElement{"statement"},
                         }}},
            EbnfRuleSpec{"assignStatement",
                         {EbnfAlternative{
                                 EbnfElement{"varUsage"},
                                 EbnfElement{"ASSIGN"},
                                 EbnfElement{"additiveExpression"},
                         }}},
    };
}

inline std::vector<std::string> cmmTerminals() {
    return {
            "PLUS", "MINUS", "COMMA",  "LPAREN", "RPAREN", "LBRACE", "RBRACE", "SEMI", "LT",     "LE",
            "GT",   "GE",    "ASSIGN", "EQ",     "NEQ",    "INT",    "WHILE",  "ID",   "NUMBER",
    };
}

inline std::vector<EbnfRuleSpec> cminusCommonRules() {
    return {
            EbnfRuleSpec{"program", {EbnfAlternative{oneOrMore("declaration")}}},
            EbnfRuleSpec{"declaration",
                         {
                                 EbnfAlternative{EbnfElement{"varDeclaration"}},
                                 EbnfAlternative{EbnfElement{"funDeclaration"}},
                         }},
            EbnfRuleSpec{"declarator", {EbnfAlternative{EbnfElement{"ID"}}}},
            EbnfRuleSpec{"varDeclaration",
                         {EbnfAlternative{
                                 EbnfElement{"typeSpecifier"},
                                 EbnfElement{"declarator"},
                                 EbnfElement{"SEMI"},
                         }}},
            EbnfRuleSpec{"funDeclaration",
                         {EbnfAlternative{
                                 EbnfElement{"returnType"},
                                 EbnfElement{"ID"},
                                 EbnfElement{"LPAREN"},
                                 optional("formalParameters"),
                                 EbnfElement{"RPAREN"},
                                 EbnfElement{"compoundStatement"},
                         }}},
            EbnfRuleSpec{"returnType",
                         {
                                 EbnfAlternative{EbnfElement{"typeSpecifier"}},
                                 EbnfAlternative{EbnfElement{"VOID"}},
                         }},
            EbnfRuleSpec{"typeSpecifier",
                         {
                                 EbnfAlternative{EbnfElement{"INT"}},
                                 EbnfAlternative{EbnfElement{"STRING"}},
                         }},
            EbnfRuleSpec{"formalParameters",
                         {EbnfAlternative{
                                 EbnfElement{"formalParameter"},
                                 zeroOrMore("formalParameterTail"),
                         }}},
            EbnfRuleSpec{"formalParameterTail",
                         {EbnfAlternative{EbnfElement{"COMMA"}, EbnfElement{"formalParameter"}}}},
            EbnfRuleSpec{"formalParameter", {EbnfAlternative{EbnfElement{"typeSpecifier"}, EbnfElement{"declarator"}}}},
            EbnfRuleSpec{"compoundStatement",
                         {EbnfAlternative{
                                 EbnfElement{"LBRACE"},
                                 zeroOrMore("compoundItem"),
                                 EbnfElement{"RBRACE"},
                         }}},
            EbnfRuleSpec{"compoundItem",
                         {
                                 EbnfAlternative{EbnfElement{"localDeclaration"}},
                                 EbnfAlternative{EbnfElement{"statement"}},
                         }},
            EbnfRuleSpec{"localDeclaration",
                         {
                                 EbnfAlternative{EbnfElement{"varDeclaration"}},
                                 EbnfAlternative{EbnfElement{"varInitialization"}},
                         }},
            EbnfRuleSpec{"varInitialization",
                         {EbnfAlternative{
                                 EbnfElement{"typeSpecifier"},
                                 EbnfElement{"declarator"},
                                 EbnfElement{"ASSIGN"},
                                 EbnfElement{"additiveExpression"},
                                 EbnfElement{"SEMI"},
                         }}},
            EbnfRuleSpec{"relop",
                         {
                                 EbnfAlternative{EbnfElement{"LE"}},
                                 EbnfAlternative{EbnfElement{"LT"}},
                                 EbnfAlternative{EbnfElement{"GT"}},
                                 EbnfAlternative{EbnfElement{"GE"}},
                                 EbnfAlternative{EbnfElement{"EQ"}},
                                 EbnfAlternative{EbnfElement{"NEQ"}},
                         }},
            EbnfRuleSpec{"booleanExpression",
                         {EbnfAlternative{
                                 EbnfElement{"additiveExpression"},
                                 EbnfElement{"relop"},
                                 EbnfElement{"additiveExpression"},
                         }}},
            EbnfRuleSpec{"addop",
                         {
                                 EbnfAlternative{EbnfElement{"PLUS"}},
                                 EbnfAlternative{EbnfElement{"MINUS"}},
                         }},
            EbnfRuleSpec{"additiveExpression", {EbnfAlternative{EbnfElement{"term"}, zeroOrMore("additiveOperation")}}},
            EbnfRuleSpec{"additiveOperation", {EbnfAlternative{EbnfElement{"addop"}, EbnfElement{"term"}}}},
            EbnfRuleSpec{"mulop",
                         {
                                 EbnfAlternative{EbnfElement{"MUL"}},
                                 EbnfAlternative{EbnfElement{"DIV"}},
                         }},
            EbnfRuleSpec{"term", {EbnfAlternative{EbnfElement{"factor"}, zeroOrMore("multiplicativeOperation")}}},
            EbnfRuleSpec{"multiplicativeOperation", {EbnfAlternative{EbnfElement{"mulop"}, EbnfElement{"factor"}}}},
            EbnfRuleSpec{"factor",
                         {
                                 EbnfAlternative{EbnfElement{"LPAREN"}, EbnfElement{"additiveExpression"},
                                                 EbnfElement{"RPAREN"}},
                                 EbnfAlternative{EbnfElement{"varUsage"}},
                                 EbnfAlternative{EbnfElement{"call"}},
                                 EbnfAlternative{EbnfElement{"NUMBER"}},
                                 EbnfAlternative{EbnfElement{"STRING_LITERAL"}},
                                 EbnfAlternative{EbnfElement{"MINUS"}, EbnfElement{"factor"}},
                         }},
            EbnfRuleSpec{"varUsage", {EbnfAlternative{EbnfElement{"ID"}}}},
            EbnfRuleSpec{"call",
                         {EbnfAlternative{EbnfElement{"ID"}, EbnfElement{"LPAREN"}, optional("argList"),
                                          EbnfElement{"RPAREN"}}}},
            EbnfRuleSpec{"argList", {EbnfAlternative{EbnfElement{"argument"}, zeroOrMore("argumentTail")}}},
            EbnfRuleSpec{"argumentTail", {EbnfAlternative{EbnfElement{"COMMA"}, EbnfElement{"argument"}}}},
            EbnfRuleSpec{"argument", {EbnfAlternative{EbnfElement{"additiveExpression"}}}},
    };
}

inline void appendCminusStatementTail(std::vector<EbnfRuleSpec> &rules) {
    rules.push_back(EbnfRuleSpec{"assignop",
                                 {
                                         EbnfAlternative{EbnfElement{"ASSIGN"}},
                                         EbnfAlternative{EbnfElement{"ASSIGN_PLUS"}},
                                         EbnfAlternative{EbnfElement{"ASSIGN_MINUS"}},
                                         EbnfAlternative{EbnfElement{"ASSIGN_MUL"}},
                                         EbnfAlternative{EbnfElement{"ASSIGN_DIV"}},
                                 }});
    rules.push_back(EbnfRuleSpec{"assignStatement",
                                 {
                                         EbnfAlternative{EbnfElement{"varUsage"}, EbnfElement{"assignop"},
                                                         EbnfElement{"additiveExpression"}},
                                         EbnfAlternative{EbnfElement{"varUsage"}, EbnfElement{"INC"}},
                                         EbnfAlternative{EbnfElement{"varUsage"}, EbnfElement{"DEC"}},
                                 }});
    rules.push_back(EbnfRuleSpec{
            "returnStatement",
            {EbnfAlternative{EbnfElement{"RETURN"}, optional("additiveExpression"), EbnfElement{"SEMI"}}}});
}

inline EbnfGrammarSpec cminusEbnfGrammar() {
    std::vector<EbnfRuleSpec> rules = cminusCommonRules();
    rules.push_back(EbnfRuleSpec{"statement",
                                 {
                                         EbnfAlternative{EbnfElement{"whileStatement"}},
                                         EbnfAlternative{EbnfElement{"forStatement"}},
                                         EbnfAlternative{EbnfElement{"ifStatement"}},
                                         EbnfAlternative{EbnfElement{"assignStatement"}, EbnfElement{"SEMI"}},
                                         EbnfAlternative{EbnfElement{"call"}, EbnfElement{"SEMI"}},
                                         EbnfAlternative{EbnfElement{"compoundStatement"}},
                                         EbnfAlternative{EbnfElement{"returnStatement"}},
                                 }});
    rules.push_back(
            EbnfRuleSpec{"whileStatement",
                         {EbnfAlternative{EbnfElement{"WHILE"}, EbnfElement{"LPAREN"}, EbnfElement{"booleanExpression"},
                                          EbnfElement{"RPAREN"}, EbnfElement{"statement"}}}});
    rules.push_back(EbnfRuleSpec{"forStatement",
                                 {EbnfAlternative{
                                         EbnfElement{"FOR"},
                                         EbnfElement{"LPAREN"},
                                         EbnfElement{"varInitialization"},
                                         EbnfElement{"booleanExpression"},
                                         EbnfElement{"SEMI"},
                                         EbnfElement{"assignStatement"},
                                         EbnfElement{"RPAREN"},
                                         EbnfElement{"statement"},
                                 }}});
    rules.push_back(EbnfRuleSpec{
            "ifStatement",
            {
                    EbnfAlternative{EbnfElement{"IF"}, EbnfElement{"LPAREN"}, EbnfElement{"booleanExpression"},
                                    EbnfElement{"RPAREN"}, EbnfElement{"statement"}},
                    EbnfAlternative{EbnfElement{"IF"}, EbnfElement{"LPAREN"}, EbnfElement{"booleanExpression"},
                                    EbnfElement{"RPAREN"}, EbnfElement{"statement"}, EbnfElement{"ELSE"},
                                    EbnfElement{"statement"}},
            }});
    appendCminusStatementTail(rules);
    return EbnfGrammarSpec{std::move(rules)};
}

inline EbnfGrammarSpec cminusClosedOpenEbnfGrammar() {
    std::vector<EbnfRuleSpec> rules = cminusCommonRules();
    rules.push_back(EbnfRuleSpec{"statement",
                                 {
                                         EbnfAlternative{EbnfElement{"closedStatement"}},
                                         EbnfAlternative{EbnfElement{"openStatement"}},
                                 }});
    rules.push_back(EbnfRuleSpec{"closedStatement",
                                 {
                                         EbnfAlternative{EbnfElement{"closedWhileStatement"}},
                                         EbnfAlternative{EbnfElement{"closedForStatement"}},
                                         EbnfAlternative{EbnfElement{"closedIfStatement"}},
                                         EbnfAlternative{EbnfElement{"assignStatement"}, EbnfElement{"SEMI"}},
                                         EbnfAlternative{EbnfElement{"call"}, EbnfElement{"SEMI"}},
                                         EbnfAlternative{EbnfElement{"compoundStatement"}},
                                         EbnfAlternative{EbnfElement{"returnStatement"}},
                                 }});
    rules.push_back(EbnfRuleSpec{"openStatement",
                                 {
                                         EbnfAlternative{EbnfElement{"openWhileStatement"}},
                                         EbnfAlternative{EbnfElement{"openForStatement"}},
                                         EbnfAlternative{EbnfElement{"openIfStatement"}},
                                 }});
    rules.push_back(
            EbnfRuleSpec{"closedWhileStatement",
                         {EbnfAlternative{EbnfElement{"WHILE"}, EbnfElement{"LPAREN"}, EbnfElement{"booleanExpression"},
                                          EbnfElement{"RPAREN"}, EbnfElement{"closedStatement"}}}});
    rules.push_back(
            EbnfRuleSpec{"openWhileStatement",
                         {EbnfAlternative{EbnfElement{"WHILE"}, EbnfElement{"LPAREN"}, EbnfElement{"booleanExpression"},
                                          EbnfElement{"RPAREN"}, EbnfElement{"openStatement"}}}});
    rules.push_back(EbnfRuleSpec{"closedForStatement",
                                 {EbnfAlternative{
                                         EbnfElement{"FOR"},
                                         EbnfElement{"LPAREN"},
                                         EbnfElement{"varInitialization"},
                                         EbnfElement{"booleanExpression"},
                                         EbnfElement{"SEMI"},
                                         EbnfElement{"assignStatement"},
                                         EbnfElement{"RPAREN"},
                                         EbnfElement{"closedStatement"},
                                 }}});
    rules.push_back(EbnfRuleSpec{"openForStatement",
                                 {EbnfAlternative{
                                         EbnfElement{"FOR"},
                                         EbnfElement{"LPAREN"},
                                         EbnfElement{"varInitialization"},
                                         EbnfElement{"booleanExpression"},
                                         EbnfElement{"SEMI"},
                                         EbnfElement{"assignStatement"},
                                         EbnfElement{"RPAREN"},
                                         EbnfElement{"openStatement"},
                                 }}});
    rules.push_back(
            EbnfRuleSpec{"closedIfStatement",
                         {EbnfAlternative{EbnfElement{"IF"}, EbnfElement{"LPAREN"}, EbnfElement{"booleanExpression"},
                                          EbnfElement{"RPAREN"}, EbnfElement{"closedStatement"}, EbnfElement{"ELSE"},
                                          EbnfElement{"closedStatement"}}}});
    rules.push_back(EbnfRuleSpec{
            "openIfStatement",
            {
                    EbnfAlternative{EbnfElement{"IF"}, EbnfElement{"LPAREN"}, EbnfElement{"booleanExpression"},
                                    EbnfElement{"RPAREN"}, EbnfElement{"statement"}},
                    EbnfAlternative{EbnfElement{"IF"}, EbnfElement{"LPAREN"}, EbnfElement{"booleanExpression"},
                                    EbnfElement{"RPAREN"}, EbnfElement{"closedStatement"}, EbnfElement{"ELSE"},
                                    EbnfElement{"openStatement"}},
            }});
    appendCminusStatementTail(rules);
    return EbnfGrammarSpec{std::move(rules)};
}

inline std::vector<std::string> cminusTerminals() {
    return {
            "PLUS",       "MINUS", "MUL", "DIV",    "COMMA", "LPAREN", "RPAREN",         "LBRACE",       "RBRACE",
            "SEMI",       "LT",    "LE",  "GT",     "GE",    "ASSIGN", "ASSIGN_PLUS",    "ASSIGN_MINUS", "ASSIGN_MUL",
            "ASSIGN_DIV", "INC",   "DEC", "EQ",     "NEQ",   "INT",    "STRING",         "VOID",         "IF",
            "ELSE",       "WHILE", "FOR", "RETURN", "ID",    "NUMBER", "STRING_LITERAL",
    };
}

inline EbnfGrammarSpec cDeclaratorLegacyEbnfGrammar() {
    return EbnfGrammarSpec{
            EbnfRuleSpec{"compilationUnit", {EbnfAlternative{zeroOrMore("external_declaration")}}},
            EbnfRuleSpec{"external_declaration",
                         {
                                 EbnfAlternative{EbnfElement{"functionDefinition"}},
                                 EbnfAlternative{EbnfElement{"varFuncDeclaration"}},
                                 EbnfAlternative{EbnfElement{"declaration"}},
                         }},
            EbnfRuleSpec{"declaration",
                         {
                                 EbnfAlternative{EbnfElement{"varDeclaration"}},
                                 EbnfAlternative{EbnfElement{"typeDefinition"}},
                                 EbnfAlternative{EbnfElement{"';'"}},
                         }},
            EbnfRuleSpec{"typeOrDecl", {EbnfAlternative{EbnfElement{"type"}}}},
            EbnfRuleSpec{"functionDefinition",
                         {EbnfAlternative{optional("type"), EbnfElement{"functionDeclarator"},
                                          optional("parametersKandRlist"), EbnfElement{"compoundStatement"}}}},
            EbnfRuleSpec{"type",
                         {
                                 EbnfAlternative{EbnfElement{"'int'"}},
                                 EbnfAlternative{EbnfElement{"'char'"}},
                                 EbnfAlternative{EbnfElement{"'double'"}},
                                 EbnfAlternative{EbnfElement{"'void'"}},
                                 EbnfAlternative{EbnfElement{"'struct'"}, EbnfElement{"Identifier"}},
                                 EbnfAlternative{EbnfElement{"Identifier"}},
                         }},
            EbnfRuleSpec{"typeModifier", {EbnfAlternative{EbnfElement{"'*'"}}}},
            EbnfRuleSpec{"parameterOrTypeList",
                         {EbnfAlternative{EbnfElement{"parameterOrType"}, zeroOrMore("parameterOrTypeListGroup1")}}},
            EbnfRuleSpec{"parameterOrTypeListGroup1",
                         {EbnfAlternative{EbnfElement{"','"}, EbnfElement{"parameterOrType"}}}},
            EbnfRuleSpec{"parametersKandRlist", {EbnfAlternative{oneOrMore("parametersKandRlistGroup1")}}},
            EbnfRuleSpec{"parametersKandRlistGroup1",
                         {EbnfAlternative{EbnfElement{"parametersKandR"}, EbnfElement{"';'"}}}},
            EbnfRuleSpec{"parametersKandR",
                         {EbnfAlternative{EbnfElement{"typeOrDecl"}, EbnfElement{"parametersKandRGroup1"},
                                          zeroOrMore("parametersKandRGroup2")}}},
            EbnfRuleSpec{"parametersKandRGroup1",
                         {
                                 EbnfAlternative{EbnfElement{"variableDeclarator"}},
                                 EbnfAlternative{EbnfElement{"functionDeclarator"}},
                         }},
            EbnfRuleSpec{"parametersKandRGroup3",
                         {
                                 EbnfAlternative{EbnfElement{"variableDeclarator"}},
                                 EbnfAlternative{EbnfElement{"functionDeclarator"}},
                         }},
            EbnfRuleSpec{"parametersKandRGroup2",
                         {EbnfAlternative{EbnfElement{"','"}, EbnfElement{"parametersKandRGroup3"}}}},
            EbnfRuleSpec{"parameterOrType",
                         {EbnfAlternative{EbnfElement{"typeOrDecl"}, EbnfElement{"parameterOrTypeGroup1"}}}},
            EbnfRuleSpec{"parameterOrTypeGroup1",
                         {
                                 EbnfAlternative{EbnfElement{"variableDeclarator"}},
                                 EbnfAlternative{EbnfElement{"variableDeclaratorPlace"}},
                                 EbnfAlternative{EbnfElement{"functionDeclarator"}},
                                 EbnfAlternative{EbnfElement{"functionDeclaratorPlace"}},
                         }},
            EbnfRuleSpec{"compoundStatement", {EbnfAlternative{EbnfElement{"'{'"}, EbnfElement{"'}'"}}}},
            EbnfRuleSpec{"varFuncDeclaration",
                         {EbnfAlternative{EbnfElement{"type"}, EbnfElement{"varFuncList"}, EbnfElement{"';'"}}}},
            EbnfRuleSpec{"varDeclaration",
                         {EbnfAlternative{EbnfElement{"type"}, EbnfElement{"varList"}, EbnfElement{"';'"}}}},
            EbnfRuleSpec{"varFuncList",
                         {EbnfAlternative{EbnfElement{"commonDeclarator"}, zeroOrMore("varFuncListGroup1")}}},
            EbnfRuleSpec{"varFuncListGroup1", {EbnfAlternative{EbnfElement{"','"}, EbnfElement{"commonDeclarator"}}}},
            EbnfRuleSpec{"varList",
                         {EbnfAlternative{EbnfElement{"variableDeclaratorWithInit"}, zeroOrMore("varListGroup1")}}},
            EbnfRuleSpec{"varListGroup1",
                         {EbnfAlternative{EbnfElement{"','"}, EbnfElement{"variableDeclaratorWithInit"}}}},
            EbnfRuleSpec{
                    "variableDeclaratorWithInit",
                    {EbnfAlternative{EbnfElement{"variableDeclarator"}, optional("variableDeclaratorWithInitGroup1")}}},
            EbnfRuleSpec{"variableDeclaratorWithInitGroup1",
                         {EbnfAlternative{EbnfElement{"'='"}, EbnfElement{"initializer"}}}},
            EbnfRuleSpec{"commonDeclarator",
                         {
                                 EbnfAlternative{EbnfElement{"variableDeclaratorWithInit"}},
                                 EbnfAlternative{EbnfElement{"functionDeclarator"}},
                         }},
            EbnfRuleSpec{"variableDeclarator",
                         {
                                 EbnfAlternative{EbnfElement{"name"}},
                                 EbnfAlternative{EbnfElement{"variablePtrDeclarator"}},
                         }},
            EbnfRuleSpec{"variablePtrDeclarator",
                         {
                                 EbnfAlternative{EbnfElement{"ptrname"}},
                                 EbnfAlternative{zeroOrMore("typeModifier"), EbnfElement{"variableSubDeclarator"}},
                         }},
            EbnfRuleSpec{"variableSubDeclarator",
                         {EbnfAlternative{EbnfElement{"'('"}, EbnfElement{"variablePtrDeclarator"}, EbnfElement{"')'"},
                                          EbnfElement{"functionParameters"}}}},
            EbnfRuleSpec{"variableDeclaratorPlace",
                         {
                                 EbnfAlternative{EbnfElement{"namePlace"}},
                                 EbnfAlternative{EbnfElement{"variablePtrDeclaratorPlace"}},
                         }},
            EbnfRuleSpec{"variablePtrDeclaratorPlace",
                         {
                                 EbnfAlternative{EbnfElement{"ptrnamePlace"}},
                                 EbnfAlternative{zeroOrMore("typeModifier"), EbnfElement{"variableSubDeclaratorPlace"}},
                         }},
            EbnfRuleSpec{"variableSubDeclaratorPlace",
                         {EbnfAlternative{EbnfElement{"'('"}, EbnfElement{"variablePtrDeclaratorPlace"},
                                          EbnfElement{"')'"}, EbnfElement{"functionParameters"}}}},
            EbnfRuleSpec{"functionDeclarator",
                         {
                                 EbnfAlternative{EbnfElement{"functionSubDeclarator"}},
                                 EbnfAlternative{oneOrMore("typeModifier"), EbnfElement{"functionDeclarator"}},
                                 EbnfAlternative{EbnfElement{"'('"}, EbnfElement{"functionDeclarator"},
                                                 EbnfElement{"')'"}, EbnfElement{"functionDeclaratorGroup1"}},
                         }},
            EbnfRuleSpec{"functionDeclaratorGroup1",
                         {
                                 EbnfAlternative{EbnfElement{"functionParameters"}},
                                 EbnfAlternative{EbnfElement{"array"}},
                         }},
            EbnfRuleSpec{
                    "functionSubDeclarator",
                    {
                            EbnfAlternative{EbnfElement{"'('"}, EbnfElement{"functionDeclarator"}, EbnfElement{"')'"}},
                            EbnfAlternative{EbnfElement{"name"}, EbnfElement{"functionParameters"}},
                    }},
            EbnfRuleSpec{"functionDeclaratorPlace",
                         {
                                 EbnfAlternative{EbnfElement{"functionSubDeclaratorPlace"}},
                                 EbnfAlternative{oneOrMore("typeModifier"), EbnfElement{"functionDeclaratorPlace"}},
                                 EbnfAlternative{EbnfElement{"'('"}, EbnfElement{"functionDeclaratorPlace"},
                                                 EbnfElement{"')'"}, EbnfElement{"functionDeclaratorPlaceGroup1"}},
                         }},
            EbnfRuleSpec{"functionDeclaratorPlaceGroup1",
                         {
                                 EbnfAlternative{EbnfElement{"functionParameters"}},
                                 EbnfAlternative{EbnfElement{"array"}},
                         }},
            EbnfRuleSpec{
                    "functionSubDeclaratorPlace",
                    {
                            EbnfAlternative{EbnfElement{"'('"}, EbnfElement{"functionDeclarator"}, EbnfElement{"')'"}},
                            EbnfAlternative{EbnfElement{"namePlace"}, EbnfElement{"functionParameters"}},
                    }},
            EbnfRuleSpec{"name",
                         {
                                 EbnfAlternative{EbnfElement{"'('"}, EbnfElement{"name"}, EbnfElement{"')'"}},
                                 EbnfAlternative{EbnfElement{"Identifier"}},
                         }},
            EbnfRuleSpec{"arrname", {EbnfAlternative{EbnfElement{"name"}, EbnfElement{"array"}}}},
            EbnfRuleSpec{"ptrname",
                         {
                                 EbnfAlternative{oneOrMore("typeModifier"), EbnfElement{"name"}},
                                 EbnfAlternative{zeroOrMore("typeModifier"), EbnfElement{"arrname"}},
                         }},
            EbnfRuleSpec{"namePlace",
                         {
                                 EbnfAlternative{EbnfElement{"'('"}, EbnfElement{"namePlace"}, EbnfElement{"')'"}},
                                 EbnfAlternative{},
                         }},
            EbnfRuleSpec{"arrnamePlace",
                         {
                                 EbnfAlternative{EbnfElement{"namePlace"}, EbnfElement{"array"}},
                                 EbnfAlternative{EbnfElement{"'('"}, EbnfElement{"ptrnamePlace"}, EbnfElement{"')'"},
                                                 EbnfElement{"array"}},
                         }},
            EbnfRuleSpec{"ptrnamePlace",
                         {
                                 EbnfAlternative{EbnfElement{"typeModifier"}, zeroOrMore("typeModifier"),
                                                 EbnfElement{"namePlace"}},
                                 EbnfAlternative{zeroOrMore("typeModifier"), EbnfElement{"arrnamePlace"}},
                                 EbnfAlternative{zeroOrMore("typeModifier"), EbnfElement{"'('"},
                                                 EbnfElement{"ptrnamePlace"}, EbnfElement{"')'"}},
                         }},
            EbnfRuleSpec{"initializer", {EbnfAlternative{EbnfElement{"atom"}}}},
            EbnfRuleSpec{"surroundedVariableName",
                         {
                                 EbnfAlternative{EbnfElement{"'('"}, EbnfElement{"surroundedVariableName"},
                                                 EbnfElement{"')'"}},
                                 EbnfAlternative{EbnfElement{"typeModifier"}, EbnfElement{"surroundedVariableName"}},
                                 EbnfAlternative{EbnfElement{"'('"}, EbnfElement{"variableName"}, EbnfElement{"')'"}},
                         }},
            EbnfRuleSpec{"variableName",
                         {
                                 EbnfAlternative{EbnfElement{"typeModifier"}, EbnfElement{"variableName"}},
                                 EbnfAlternative{EbnfElement{"variableName"}, EbnfElement{"array"}},
                                 EbnfAlternative{EbnfElement{"Identifier"}},
                         }},
            EbnfRuleSpec{"functionParameters",
                         {EbnfAlternative{EbnfElement{"'('"}, optional("parameterOrTypeList"), EbnfElement{"')'"}}}},
            EbnfRuleSpec{"array", {EbnfAlternative{EbnfElement{"'['"}, optional("atom"), EbnfElement{"']'"}}}},
            EbnfRuleSpec{"atom",
                         {
                                 EbnfAlternative{EbnfElement{"Identifier"}},
                                 EbnfAlternative{EbnfElement{"literal"}},
                         }},
            EbnfRuleSpec{"literal", {EbnfAlternative{EbnfElement{"integerConstant"}}}},
            EbnfRuleSpec{"typeDefinition",
                         {EbnfAlternative{EbnfElement{"'typedef'"}, EbnfElement{"typeOrDecl"},
                                          EbnfElement{"commonDeclarator"}, zeroOrMore("typeDefinitionGroup1"),
                                          EbnfElement{"';'"}}}},
            EbnfRuleSpec{"typeDefinitionGroup1",
                         {EbnfAlternative{EbnfElement{"','"}, EbnfElement{"commonDeclarator"}}}},
            EbnfRuleSpec{"integerConstant", {EbnfAlternative{EbnfElement{"DecimalConstant"}}}},
    };
}

inline std::vector<std::string> cDeclaratorSubsetTerminals() {
    return {
            "';'", "'int'", "'char'", "'double'", "'void'", "'struct'", "Identifier", "'*'",       "','",
            "'{'", "'}'",   "'='",    "'('",      "')'",    "'['",      "']'",        "'typedef'", "DecimalConstant",
    };
}

inline EbnfGrammarSpec cDeclaratorSubsetEbnfGrammar() {
    return EbnfGrammarSpec{
            EbnfRuleSpec{"compilationUnit", {EbnfAlternative{zeroOrMore("externalDeclaration")}}},
            EbnfRuleSpec{"externalDeclaration",
                         {
                                 EbnfAlternative{EbnfElement{"functionDefinition"}},
                                 EbnfAlternative{EbnfElement{"declaration"}},
                         }},
            EbnfRuleSpec{"declaration",
                         {
                                 EbnfAlternative{EbnfElement{"initDeclaration"}},
                                 EbnfAlternative{EbnfElement{"typeDefinition"}},
                                 EbnfAlternative{EbnfElement{"';'"}},
                         }},
            EbnfRuleSpec{"functionDefinition",
                         {EbnfAlternative{EbnfElement{"type"}, EbnfElement{"declarator"},
                                          optional("parametersKandRList"), EbnfElement{"compoundStatement"}}}},
            EbnfRuleSpec{"initDeclaration",
                         {EbnfAlternative{EbnfElement{"type"}, EbnfElement{"initDeclaratorList"}, EbnfElement{"';'"}}}},
            EbnfRuleSpec{"type",
                         {
                                 EbnfAlternative{EbnfElement{"'int'"}},
                                 EbnfAlternative{EbnfElement{"'char'"}},
                                 EbnfAlternative{EbnfElement{"'double'"}},
                                 EbnfAlternative{EbnfElement{"'void'"}},
                                 EbnfAlternative{EbnfElement{"'struct'"}, EbnfElement{"Identifier"}},
                                 EbnfAlternative{EbnfElement{"Identifier"}},
                         }},
            EbnfRuleSpec{"initDeclaratorList",
                         {EbnfAlternative{EbnfElement{"initDeclarator"}, zeroOrMore("initDeclaratorListGroup1")}}},
            EbnfRuleSpec{"initDeclaratorListGroup1",
                         {EbnfAlternative{EbnfElement{"','"}, EbnfElement{"initDeclarator"}}}},
            EbnfRuleSpec{"initDeclarator",
                         {EbnfAlternative{EbnfElement{"declarator"}, optional("initDeclaratorGroup1")}}},
            EbnfRuleSpec{"initDeclaratorGroup1", {EbnfAlternative{EbnfElement{"'='"}, EbnfElement{"initializer"}}}},
            EbnfRuleSpec{"declarator", {EbnfAlternative{optional("pointer"), EbnfElement{"directDeclarator"}}}},
            EbnfRuleSpec{"pointer", {EbnfAlternative{oneOrMore("'*'")}}},
            EbnfRuleSpec{"directDeclarator",
                         {
                                 EbnfAlternative{EbnfElement{"Identifier"}},
                                 EbnfAlternative{EbnfElement{"'('"}, EbnfElement{"declarator"}, EbnfElement{"')'"}},
                                 EbnfAlternative{EbnfElement{"directDeclarator"}, EbnfElement{"functionParameters"}},
                                 EbnfAlternative{EbnfElement{"directDeclarator"}, EbnfElement{"array"}},
                         }},
            EbnfRuleSpec{"functionParameters",
                         {EbnfAlternative{EbnfElement{"'('"}, optional("parameterOrTypeList"), EbnfElement{"')'"}}}},
            EbnfRuleSpec{"parameterOrTypeList",
                         {EbnfAlternative{EbnfElement{"parameterOrType"}, zeroOrMore("parameterOrTypeListGroup1")}}},
            EbnfRuleSpec{"parameterOrTypeListGroup1",
                         {EbnfAlternative{EbnfElement{"','"}, EbnfElement{"parameterOrType"}}}},
            EbnfRuleSpec{"parameterOrType", {EbnfAlternative{EbnfElement{"type"}, optional("declarator")}}},
            EbnfRuleSpec{"parametersKandRList", {EbnfAlternative{oneOrMore("parametersKandR")}}},
            EbnfRuleSpec{"parametersKandR",
                         {EbnfAlternative{EbnfElement{"type"}, EbnfElement{"initDeclaratorList"}, EbnfElement{"';'"}}}},
            EbnfRuleSpec{"compoundStatement", {EbnfAlternative{EbnfElement{"'{'"}, EbnfElement{"'}'"}}}},
            EbnfRuleSpec{"array", {EbnfAlternative{EbnfElement{"'['"}, optional("atom"), EbnfElement{"']'"}}}},
            EbnfRuleSpec{"initializer", {EbnfAlternative{EbnfElement{"atom"}}}},
            EbnfRuleSpec{"atom",
                         {
                                 EbnfAlternative{EbnfElement{"Identifier"}},
                                 EbnfAlternative{EbnfElement{"integerConstant"}},
                         }},
            EbnfRuleSpec{"integerConstant", {EbnfAlternative{EbnfElement{"DecimalConstant"}}}},
            EbnfRuleSpec{"typeDefinition",
                         {EbnfAlternative{EbnfElement{"'typedef'"}, EbnfElement{"type"},
                                          EbnfElement{"initDeclaratorList"}, EbnfElement{"';'"}}}},
    };
}

} // namespace zbik::test
