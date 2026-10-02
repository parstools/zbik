cmake_minimum_required(VERSION 4.0)

project(zbik VERSION 1.0.0 LANGUAGES CXX)

find_package(ICU REQUIRED COMPONENTS uc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(zbik_enable_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror=return-type
        )
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    endif()
endfunction()

add_library(zbik_core
        src/corpus/CorpusLabeler.cpp
        src/ebnf/EbnfGrammar.cpp
        src/ebnf/EbnfConflictDiagnostics.cpp
        src/ebnf/EbnfToBnf.cpp
        src/generator/AmbiguitySearch.cpp
        src/generator/BoundedDerivationGenerator.cpp
        src/generator/DerivationTree.cpp
        src/generator/LRLanguageOracle.cpp
        src/grammar/Rule.cpp
        src/grammar/Grammar.cpp
        src/grammar/GrammarAnalysis.cpp
        src/grammar/GrammarBuilder.cpp
        src/grammar/GrammarCorpusReader.cpp
        src/grammar/GrammarSyntax.cpp
        src/grammar/YieldLength.cpp
        src/lexer/ByteLexer.cpp
        src/lexer/LexerAutomaton.cpp
        src/lexer/Utf8Lexer.cpp
        src/lr/Action.cpp
        src/lr/CompressedParseTable.cpp
        src/lr/DirectLALRkDfa.cpp
        src/lr/Item.cpp
        src/lr/LALRkDfa.cpp
        src/lr/SelectiveLRkMerger.cpp
        src/lr/LRGrammarView.cpp
        src/lr/LRMachine.cpp
        src/lr/LexerContextPlan.cpp
        src/lr/ContextualLRMachine.cpp
        src/lr/ScopedLexerGrammar.cpp
        src/lr/LRkClosure.cpp
        src/lr/LRkGoto.cpp
        src/lr/LRkDfa.cpp
        src/lr/LRkDfaDiagnostics.cpp
        src/lr/ParseTable.cpp
        src/regex/RegexAst.cpp
        src/regex/RegexDfa.cpp
        src/regex/RegexDfaMatcher.cpp
        src/regex/RegexDfaMinimizer.cpp
        src/regex/RegexNfa.cpp
        src/regex/RegexNfaMatcher.cpp
        src/regex/RegexParser.cpp
        src/regex/UnicodeProperties.cpp
        src/first/First1.cpp
        src/first/FirstK.cpp
        src/first/Follow1.cpp
        src/first/FollowK.cpp
        src/first/LookaheadSymbols.cpp
        src/first/LookaheadWord.cpp
        src/first/Nullable.cpp
        src/first/TerminalSet.cpp
        src/first/WordSetK.cpp
        src/util/util.cpp
)

target_include_directories(zbik_core
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(zbik_core PUBLIC ICU::uc)

zbik_enable_warnings(zbik_core)

add_executable(zbik src/main.cpp)
target_link_libraries(zbik PRIVATE zbik_core)
target_compile_definitions(zbik PRIVATE
        ZBIK_RESOURCE_DIR="${PROJECT_SOURCE_DIR}/res"
        ZBIK_BUILD_CONFIGURATION="$<CONFIG>"
)
zbik_enable_warnings(zbik)

include(CTest)

if(BUILD_TESTING)
    add_test(NAME zbik_help COMMAND zbik --help)
    set_tests_properties(zbik_help PROPERTIES
            PASS_REGULAR_EXPRESSION "Zbik - analyse and classify")
    add_subdirectory(tests)
endif()
