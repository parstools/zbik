find_package(GTest REQUIRED)

add_executable(zbik_tests
        AmbiguitySearchTest.cpp
        BoundedDerivationGeneratorTest.cpp
        ByteLexerTest.cpp
        ContextualLexerFixturesTest.cpp
        Utf8LexerTest.cpp
        CorpusLabelerTest.cpp
        CompressedParseTableTest.cpp
        EbnfConflictDiagnosticsTest.cpp
        EbnfGrammarTest.cpp
        EbnfRealGrammarTest.cpp
        EbnfToBnfTest.cpp
        GrammarTest.cpp
        GrammarAnalysisTest.cpp
        GrammarBuilderTest.cpp
        GrammarCorpusReaderTest.cpp
        IdentifiersTest.cpp
        NullableFirst1Test.cpp
        FirstKTest.cpp
        Follow1Test.cpp
        FollowKTest.cpp
        DerivationTreeTest.cpp
        ActionTest.cpp
        ItemTest.cpp
        LALRkDfaTest.cpp
        SelectiveLRkMergerTest.cpp
        LRkClosureTest.cpp
        LRkGotoTest.cpp
        LRkDfaTest.cpp
        LRLanguageOracleTest.cpp
        LRMachineTest.cpp
        ParseTableTest.cpp
        RegexDfaMatcherTest.cpp
        RegexDfaTest.cpp
        RegexNfaMatcherTest.cpp
        RegexNfaTest.cpp
        RegexParserTest.cpp
        LookaheadSymbolsTest.cpp
        WordSetKTest.cpp
        YieldLengthTest.cpp
        LRGrammarViewTest.cpp
)

target_link_libraries(zbik_tests
        PRIVATE
        zbik_core
        GTest::gtest_main
)

target_compile_definitions(zbik_tests
        PRIVATE
        ZBIK_TEST_RESOURCE_DIR="${PROJECT_SOURCE_DIR}/res"
)

zbik_enable_warnings(zbik_tests)

include(GoogleTest)
gtest_discover_tests(zbik_tests)
