<p align="center"><img src="docs/images/zbik-logo-cropped.jpeg" alt="Żbik — wildcat and LR parser illustration" width="200"></p>

# zbik

A C++20 library for grammar analysis and deterministic LR(k)/LALR(k) parser construction.

The name **Zbik** comes from the Polish **żbik**, meaning **wildcat**.

Żbik provides algorithms for exploring context-free grammars, constructing
parse tables, and recognizing token sequences. The `zbik_core` library can
be embedded in other C++ projects; the `zbik` command-line tool analyzes
and classifies a corpus of BNF grammars.

Features include:

- **LR parsing:** canonical LR(k), LALR(k), direct LALR(k) construction,
  SLR(1), conflict diagnostics, and a stack-based parser runtime.
- **Grammar analysis:** NULLABLE, FIRST(k), FOLLOW(k), productivity,
  reachability, recursion and cycle detection, and minimum/maximum yield lengths.
- **Sequence generation:** enumerate terminal sequences and their derivation
  trees up to a chosen length bound, including distinct trees for the same sequence.
- **Ambiguity search:** find two different derivation trees for one sequence
  within a bounded search; absence of a witness does not prove unambiguity.
- **EBNF support:** convert a restricted programmatic EBNF model to BNF
  while preserving the origins of helper productions for diagnostics.
- **Parse-table compression:** compact ACTION/GOTO storage and DSL/JSON export.
- **Lexical analysis:** regular-expression parsing, NFA/DFA construction,
  byte and UTF-8 lexers, and parser-controlled lexer classes.

## Build

Requires CMake 4.0, a C++20 compiler, ICU (`uc`), and GoogleTest for tests.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

To use the library from another CMake project:

```cmake
add_subdirectory(path/to/zbik)
target_link_libraries(my_parser PRIVATE zbik_core)
```

## Command-line tool

```sh
build/zbik --help
build/zbik res/grammars.dat /tmp/grammars-result.dat --max-lr=3
```

The CLI classifies grammars; it does not generate parser source code.

## Documentation

- [LR development and architecture](docs/lr-development.md)
- [Selective LR(k) merging](docs/selective-merging.md)
- [EBNF regression grammars](docs/ebnf-regressions.md)
- [Parser-controlled lexer classes](docs/lexer-classes.md)
- [Compressed-table format](grammar/README.md)
- [Contextual lexer examples](grammar/contextual-lexer/README.md)
- [Performance measurements](docs/profiling.md)

Polish versions are available alongside these documents with the `.pl.md` suffix.

Project page: https://parstools.github.io/zbik/
