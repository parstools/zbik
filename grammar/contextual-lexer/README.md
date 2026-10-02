# Small grammars with a lexer and composable classes

Executable definitions are in
[ContextualLexerFixturesTest.cpp](../../tests/ContextualLexerFixturesTest.cpp).
Each contains parser productions, lexer regexes, class assignments, and
inputs with expected token sequences. These are not merely terminal streams
from a `.data` corpus.

The examples now test both an independent reference for class selection
and the actual `zbik_core` API: lexer-rule masks, class declarations on
nonterminals, scope specialization, and lookahead requests through
`ContextualLRMachine`. Tests compare tokens, text, and offsets for both
paths and both byte and UTF-8 lexers, with LR(1) and LR(2).
[Public API and its limitations](../../docs/lexer-classes.md).

## Class representation

`READ` and `SHR` are two independent class components. The example names
them after the tokens whose rules they enable. `IDENT` and `GT` belong to
the base. A class is a mask of enabled components; four combinations are tested:

| Mask | Additionally active rules | Recognition of `read>>` |
|---|---|---|
| `0` | None | `IDENT GT GT` |
| `READ` | The word `read` | `READ GT GT` |
| `SHR` | The symbol `>>` | `IDENT SHR` |
| `READ \| SHR` | Both | `READ SHR` |

No separate automaton is created for each mask. The reference compiles
each regex once into a `LexerAutomaton`, filters rules by mask, and chooses
the longest active match; the earlier rule wins a tie. `READ` precedes
`IDENT`; `reader` remains a single `IDENT`. The bit-63 test checks that a
large component index does not cause enumeration of all subsets.

The reference handles ASCII and the greedy regexes used here. Its matching
cost depends on rule count and match lengths, not `2^n` component combinations.
The target shared automaton needs a separate design: it must retain
candidates/activation conditions so disabling a higher-priority rule permits
another rule or a shorter token to be selected.

## 1. `SHR` versus `GT GT`

```text
S     -> A ruleA | C ruleB
ruleA -> SHR B
ruleB -> GT GT D

A='a', C='c', B='b', D='d'
SHR='>>' [requires SHR]
GT='>'
WS=[ \t\n]+ [skip]

after A: SHR
after C: 0
```

Examples: `a>>b`, `c>>d`, `c > > d`. Tests check token kinds, text, byte
offsets, and LR(1)/LR(2) acceptance.

An additional test demonstrates a specific LR(2) issue: initial lookahead
for `a>>b` must contain `[A SHR]`. Using the initial class for the second
token as well produces `[A GT]`, absent from the initial state's ACTION.

## 2. `READ` versus `IDENT`

```text
S     -> AT ruleA | HASH ruleB
ruleA -> READ END | WRITE END
ruleB -> IDENT END

AT='@', HASH='#', END=';'
READ='read' [requires READ]
WRITE='write' [requires READ]
IDENT=[a-z]+
WS=[ \t\n]+ [skip]

after AT: READ
after HASH: 0
```

Examples: `@read;`, `#read;`, `#reader;`, `@write;`, `#write;`.
Word recognition depends on the class, while token length for `read` remains unchanged.

## 3. Both mechanisms together

```text
S       -> ZERO plain | ONE keyword | TWO shift | THREE both
plain   -> IDENT GT GT END
keyword -> READ GT GT END
shift   -> IDENT SHR END
both    -> READ SHR END

ZERO='0', ONE='1', TWO='2', THREE='3', END=';'
READ='read' [requires READ]
IDENT=[a-z]+
SHR='>>' [requires SHR]
GT='>'
WS=[ \t\n]+ [skip]

after ZERO: 0
after ONE: READ
after TWO: SHR
after THREE: READ | SHR
```

Inputs `0read>>;`, `1read>>;`, `2read>>;`, and `3read>>;` must be accepted
with different token streams. In LR(2), `read` is already the second token
of initial lookahead before the first parser action executes.

## 4. Deliberately incorrect class assignment

Use the grammar from section 3 but assign `READ | SHR` to all four branches.
Longest matching and keyword priority reject the first three valid inputs.
Only `THREE` still works. The test reproduces an error that future class
validation should prevent; it demonstrates the effects of incorrect
assignment. Additional API tests reject contradictory class requirements
and unavailable tokens before parsing.

The expected target diagnostic should identify the branch/class declaration,
LR context, lookahead prefix, and a specific collision, such as `READ`
instead of `IDENT` for `read`, or `SHR` instead of `GT GT` for `>>`.

## Running from the repository root

```sh
cmake --build build --target zbik_tests -j 4
build/tests/zbik_tests --gtest_filter='ContextualLexerFixturesTest.*'
```
