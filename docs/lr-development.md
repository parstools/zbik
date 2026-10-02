# Żbik: grammar, LR(k), and lexical analysis

## Purpose

Żbik is a second approach to the C++ code in `zubr/z_javy`: it should retain
the required functionality with a simpler model of grammar, clear ownership,
and fewer interdependent mechanisms. `z_javy` is an earlier translation of
the Java project `zubr-kit`. Żbik's target scope is building **DFAs of LR(k)
states**, parse tables, and executing **LR(k)** parsers. The project will
not implement LL or grammar transformations for LL. It will retain
FIRST/FOLLOW as grammar analyses, and the bounded word and derivation-tree
generator known from `testAmbig`.

The direct reference is the current C++ code in `zubr/z_javy`, synchronized
with `zubr-kit` revision `47ded90` of 2026-09-17. Working algorithms, tests,
and data should be ported from there. The original Java remains upstream
and an additional oracle when translation uncertainty needs clarification.
Do not copy `z_javy` class by class: it retains much of the older project's
architecture, also built for LL(k), which is unnecessarily heavy for Żbik.

## Żbik's historical starting point

The description and comparison below document the skeleton before stages
0–2.4. The current `Grammar` stores values and typed IDs, has `GrammarBuilder`
and `LRGrammarView`, and supports NULLABLE, FIRST(1), FOLLOW(1), and `WordSetK`.
The current implementation status is maintained in the project's change history.

The most important change from `z_javy` is already visible in the data model:

- `Grammar` owns rules, terminals, and nonterminals through `std::unique_ptr`.
- `Rule` and rule right-hand sides use raw pointers only as non-owning references.
- Terminals and nonterminals receive stable, dense identifiers.
- The grammar is read in two passes: all left-hand sides, then right-hand-side symbols.
- Rules have one global vector, while nonterminals keep only views of their rules.
- `DynamicBitset`, `TerminalSet`, and `SetContainer` are merely an initial
  FIRST/FOLLOW(1) sketch, not an initial LL parser. They can be removed or
  replaced when proper FIRST(k) for LR(k) is implemented.

This direction is sound. In `z_javy`, `Grammar`, `Rule`, `Symbol`, and
`Nonterminal` still contain ported grammar-transformation, length, cycle,
name-generation, generation, and LL-preparation logic. In Żbik, the model
should primarily be an immutable grammar description, with algorithms in separate modules.

## Comparing grammar models

| Area | C++ `z_javy` (`zubr-kit` port) | Current Żbik | Target direction |
|---|---|---|---|
| Ownership | C++ objects connected by pointers and mutated by algorithms | `unique_ptr` in `Grammar`, pointers as views | Retain Żbik's model |
| Symbol identity | Index found in a vector, object identity often used | Dense terminal or nonterminal `id` | Use identifiers rather than addresses in algorithms |
| Rule identity | Object identity; global number created by the LR parser | No explicit `RuleId` | Assign each rule a stable `RuleId` during grammar construction |
| Start rule | `addStartNt()` creates hidden `S′ -> S` and mutates the grammar | No augmented rule | Create it in `LRGrammarView` without mutating the user grammar |
| Mutability | Recursion elimination, factoring, adding symbols | Practically no changes after construction | Formally freeze the grammar after building |
| FIRST/FOLLOW | Shared extensive LL(k) infrastructure | Unfinished bitset FIRST/FOLLOW(1) | Build a separate `FirstFollowK` module; LR(k) depends on FIRST(k), not FOLLOW(k) |
| Hashing | Mixture of identity and custom 32/64-bit hashes | No LR hash model | Hash small values: `RuleId`, dot position, and lookahead word |
| Duplicate rules | Retained; must cause reduce/reduce conflicts | Global vector retains them too | Do not deduplicate rules |

### What should not be ported from `z_javy`

Without LL, Żbik does not need:

- left-recursion elimination;
- grammar factoring;
- LL tables or LL production-selection algorithms; FIRST/FOLLOW remain valuable grammar analyses;
- mutable `minLen` and `maxLen` fields in symbols; compute required minimum
  lengths in a separate analysis for the generator;
- graph transformations for LL left-recursion elimination and factoring;
  retain graph analyses of cycles, productivity, and length;
- the three-layer `TokenSet` (`BUILD`, `DONE`, `EOF`) as the public foundation of the whole project;
- LL classification or `[LL(k)]` labels.

Left recursion is natural for LR and must not be removed. An LR parser
should operate on the user's original grammar.

### What must be retained from `z_javy`

Reproduce behavior rather than class layout:

- the augmented start production and acceptance only at EOF;
- production identity, including two textually identical rules;
- closure and `GOTO` for LR items;
- canonical state collections compared by full contents rather than hash alone;
- exact LR(k) lookahead words ending after `k` terminals or earlier at EOF;
- all competing actions in an ACTION cell, without arbitrary conflict resolution;
- a sparse ACTION table without constructing the whole alphabet raised to power `k`;
- merging equal-core states for LALR(k), if LALR(k) remains in scope;
- a deterministic recognizer for testing generated tables.

## Problems in Żbik's historical skeleton

This list describes the state before reconstruction, not current defects.
Items 1–9 were covered by stages 0–2.4; corpus synchronization should be verified separately.

The project compiles but has no tests (`ctest` reports “No tests were found”).
Its foundations must be fixed before implementing LR.

1. `SetContainer` does not initialize `firstSets` or `followSets`. The first
   call to `makeFirstSets1()` accesses an empty vector. This code does not
   mean LL has been started; it is only a set-computation sketch with no tests or LL table.
2. `TerminalSet::unionWith()` does not transfer the end-of-input marker,
   although bitwise operators account for it. The contract is inconsistent.
3. `SetContainer` remains a port of FIRST/FOLLOW concepts for LL(1), even
   though LL is outside the target project.
4. Types are split between the `base`, `set`, and global namespaces.
   `Grammar` and `Rule` are in `base`, while `Symbol`, `Terminal`,
   `Nonterminal`, `SetContainer`, and `DynamicBitset` partly remain global.
5. `isTterminal` has a typo and encodes symbol kind as a Boolean.
   `SymbolKind` or separate typed identifiers would be clearer.
6. `Rule` lacks a stable identifier. A rule's address is stable but should
   not be the format used by tables, diagnostics, or serialization.
7. Lookup by name and identifier, and an explicit start-symbol designation, are missing.
8. The constructor accepts only rule lines. Comments, empty blocks, and
   `grammars.dat` handling must be separated from parsing a single grammar.
9. `CMakeLists.txt` lists `src/main.cpp` twice, builds everything as one
   executable, and creates no library to which tests can link.
10. The corpus `zbik/res/grammars.dat` has 492 lines, whereas current
    `zubr/z_javy/res/grammars.dat` has 500. `z_javy` gained a grammar requiring
    LALR(2) and classification labels from upstream. Żbik should synchronize
    LR examples but may ignore or remove LL labels.

## Proposed target model

### Identifiers and immutable grammar

Small value types are useful:

```cpp
struct TerminalId { uint32_t value; };
struct NonterminalId { uint32_t value; };
struct RuleId { uint32_t value; };
```

`SymbolRef` can be a `SymbolKind + uint32_t` pair or
`std::variant<TerminalId, NonterminalId>`. A rule's right-hand side should
be `std::vector<SymbolRef>`. Once building is complete, all identifiers and vectors remain fixed.

`Terminal` and `Nonterminal` may remain thin name-and-ID descriptors, but
polymorphism provides no benefit here. Ultimately, one terminal-name store,
one nonterminal-name store, and rules using `SymbolRef` will be simpler.

`GrammarBuilder` should parse text and validate it; the resulting `Grammar`
should be immutable. Validation should cover:

- a nonempty set of nonterminals;
- an existing start symbol;
- no empty names;
- a valid arrow and tokenization;
- preservation of empty rules;
- preservation of duplicate productions;
- identifier and range consistency.

### Augmented grammar view

The LR algorithm needs `S′ -> S` but should not append it to the user grammar.
`LRGrammarView` can expose a synthetic `RuleId = ruleCount()` and synthetic
`NonterminalId = nonterminalCount()`. Consequently:

- building several parsers for one grammar mutates nothing;
- FIRST(k) caches remain valid;
- rule numbering in reduce actions is stable;
- rule 0 can be reserved for the augmented production only in the table layer.

### FIRST(k) and FOLLOW(k) without LL infrastructure

LR(k) needs FIRST(k), but not FOLLOW or LL tables. FOLLOW(k) is nevertheless
worth retaining as an independent analysis, testing aid, and possible SLR
foundation. Both algorithms can share the key abstraction:

```text
LookaheadWord = a TerminalId/EOF sequence of length 0..k
WordSetK      = a sparse, ordered set of only words that actually exist
```

Required operations:

- set union reporting whether anything changed;
- language concatenation truncated to `k`;
- FIRST(k) of a terminal, epsilon, a right-hand side, and a rule suffix;
- appending inherited lookahead: `FIRST_k(βu)`;
- suffix-FIRST caching keyed by `(RuleId, dotPosition, k)` and tied to a
  specific immutable grammar; caching complete `FIRST_k(βu)` also requires
  `u` and must not retain stale results from an ongoing fixed-point computation.

There is no need to copy `Tier` and `Trie` immediately. “Sparse” means not
allocating a table of size `terminal_count^k`. Store only words actually
produced. A proposed first implementation:

```cpp
struct LookaheadWord {
    // The empty vector represents epsilon.
    // EOF is a distinct symbol and may occur only at the end.
    std::vector<LookaheadSymbol> tokens;

    auto operator<=>(const LookaheadWord&) const = default;
};

class WordSetK {
    uint32_t k_;
    std::vector<LookaheadWord> words_; // Sorted and duplicate-free.
};
```

`words_` is equivalent to a `flat_set`: union is a linear merge of two sorted
vectors, automatically removing duplicates. Concatenation iterates only
over pairs of existing words, joins them, truncates at `k` tokens or EOF,
then sorts and deduplicates. For sets `A` and `B`, cost depends on
`|A| * |B|`, not the whole possible alphabet.

Example for `k=3`:

```text
A = { [], [id], [( id] }
B = { [+ id EOF], [) EOF] }

truncate_3(A · B) = {
    [+ id EOF], [) EOF],
    [id + id], [id ) EOF],
    [( id +], [( id )]
}
```

The empty word is needed during FIRST(k) fixed-point computation but must
not be an ACTION key or the lookahead of a completed item. A completed
lookahead word has length `k` or ends earlier at EOF.

For typically small `k`, a separate `std::vector` per word may cost more
than the contents. After correctness is established, a custom `SmallWord`
can store, for example, 3–4 terminals inline and longer words in an arena.
Interning is another useful optimization:

```text
WordArena: LookaheadWord -> WordId
WordSetK:  sorted vector<WordId>
```

Interning reduces lookahead copying between items, but should not be a
prerequisite for the first version. A trie makes most sense in the final
ACTION table, where shared word prefixes really save memory. FIRST(k)
and ACTION need not use the same representation.

EOF should be a separate type or reserved ID, not an ordinary user terminal.
The word invariant: EOF occurs only at the end; a word has length `k` or
ends earlier at EOF.

## DFA and the LR(k) layer

Proposed module division:

```text
src/grammar/
    Grammar, GrammarBuilder, identifiers
src/first/
    LookaheadWord, WordSetK, FirstFollowK
src/lr/
    Item, State, LRkDfa
    Action, Conflict, ParseTable
    LRk, optionally LALRk
src/runtime/
    LRMachine
src/generator/
    MinYieldLength, DerivationGenerator, AmbiguitySearch
src/regex/
    RegexAst, RegexParser, ThompsonBuilder
src/automata/
    RegexNfa, RegexDfa, DfaMinimizer
src/lexer/
    Lexer, Token
```

### Item

One type suffices for every `k >= 1`:

```cpp
struct Item {
    RuleId rule;
    uint32_t dot;
    LookaheadWord lookahead;
};
```

An item's core is only `(rule, dot)`. Full canonical-state equality includes
lookahead. `RuleId`, rather than content, distinguishes productions, so
identical rules can still cause a reduce/reduce conflict.

### Closure, GOTO, and the LR-state DFA

For `[A -> α · B β, u]`, closure adds `[B -> · γ, v]` for every `v` in
`FIRST_k(βu)`. The algorithm should be an iterative fixed point.
`GOTO(I, X)` advances the dot in all items with symbol `X`, then computes closure.

The canonical collection is a deterministic finite automaton:

```text
DFA node      = a closed set of LR(k) items
DFA alphabet  = grammar terminals and nonterminals
transition I--X = GOTO(I, X)
initial state = closure({[S′ -> ·S, EOF]})
```

The code should have an explicit `LRkDfa` type independent of the ACTION/GOTO
table. Items, transitions, reachability, state count, graph export, and
LALR(k) core merging can then be tested separately. A subsequent layer
translates the DFA into a parse table.

The LR parser itself is not an ordinary DFA. ACTION/GOTO execution uses
a state stack, so the runtime is a deterministic pushdown automaton. Separate:

```text
LRkDfa       — a finite graph of canonical item sets;
ParseTable   — ACTION/GOTO derived from this graph;
LRMachine    — a pushdown automaton executing shift/reduce/accept.
```

Intern states by their full item sets. A hash only finds candidates; equality
must always be checked, since a hash collision must never lose a state.

### Different treatment of hashes

Following its historical implementation, `z_javy` builds separate 32- and
64-bit hashes and combines item-set hashes using XOR. Żbik should treat
a hash exclusively as an auxiliary index, never as object identity.

Rules:

- Items are equal only when `RuleId`, dot position, and full lookahead are equal.
- States are equal only when their canonically sorted items are equal.
- A state-interning map may be `unordered_map<StateKey, StateId, StateHasher>`:
  the map itself calls `operator==` after collisions.
- Alternatively use `unordered_map<Hash, vector<StateId>>` and always
  search each bucket structurally.
- Never use `unordered_set<size_t>` containing only hashes: collisions remove valid states.
- Do not compose a state hash using XOR alone: XOR is commutative but
  easily cancels repeated components and gives poor distribution.
- Sort `StateKey` items first, then hash them sequentially using a 64- or 128-bit algorithm.
- `std::hash` is a process detail, unsuitable as a persistent identifier or file format.
- A persistent grammar fingerprint, if needed for caching, should be
  computed separately from names and ordered productions.
- Cache hashes only for immutable objects.

Tests should inject a constant-valued hasher. The DFA and table must remain
identical; only performance may degrade. This is the simplest demonstration
that the code does not confuse hashes with identity.

### Table and conflicts

Each row has:

- `GOTO: NonterminalId -> StateId`;
- `ACTION: LookaheadWord -> set<Action>`.

The action set matters: construction must not pick the first action or
automatically prefer shift. `Conflict` should retain the state, lookahead
word, and all actions, and distinguish at least shift/reduce and reduce/reduce.

For `k > 1`, the first terminal alone does not correctly describe shift.
Shift applies to words `FIRST_k(aβu)` from item `[A -> α · a β, u]`.
This subtle but crucial condition exists in `z_javy`.

### `LRMachine`: executing the table with a stack

A minimal runtime should accept terminal names or IDs, append EOF, and
execute shift/reduce/accept. If the table conflicts, the recognizer should
refuse deterministic parsing rather than choose an arbitrary action.
Initially, building a syntax tree is unnecessary; `accepted/rejected`
and an optional reduction trace suffice.

A single reduction shows the difference between a state graph and a machine.
Suppose the stack is `[q0, q5]` and the table says `reduce A -> a`.
The machine removes `q5`, reads exposed `q0`, checks the static transition
`GOTO(q0, A) = q2`, and pushes `q2`, producing `[q0, q2]`.
`LRkDfa` contains `q0 --A--> q2` but neither stores nor executes a stack.
The same DFA state can therefore occur on many different machine stacks.

```text
LRkDfa:     Grammar -> item-set graph -> ParseTable
LRMachine:  ParseTable + stack + input/lookahead -> shift/reduce/accept
```

The first object is finite and static. The second has potentially infinitely
many configurations because its stack can grow without a fixed bound.

### LALR(k)

The simplest and safest LALR(k) implementation is:

1. Build canonical LR(k).
2. Group states by `(RuleId, dot)` core.
3. Union full items and their lookaheads.
4. Remap transitions.
5. Rebuild ACTION/GOTO and report conflicts introduced by merging.

Do not initially implement direct lookahead propagation from LR(0).
Merging is easier to verify; optimization can be added later.

## Word and derivation-tree generator

### Grammar analyses to port

Cycle and length analyses in `z_javy` are not exclusively LL infrastructure.
Port their functionality into separate analyses of immutable `Grammar`,
with results indexed by `NonterminalId` and `RuleId`:

| Source in `z_javy/src/grammar` | Żbik counterpart |
|---|---|
| `computeMinLen()`, `checkMinLen()` | `MinYieldLength`, symbol/rule productivity, and diagnostics |
| `computeNonNullableCount()` | Number of nonnullable RHS occurrences, based on `Nullable` |
| `Cycle`, `Cycles`, `detectCycles()` | Cycles without terminal progress, with witnesses using `RuleId` and RHS position |
| `RecurCycle`, `RecurCycles`, `detectRecursion()` | Left-recursion analysis, also through nullable prefixes, without eliminating it for LR |
| `computeMaxLen()`, `Symbol::maxLen` | Maximum derived word length, distinguishing no words, finite maximum, and unboundedness |

`Cycle` and `RecurCycle` retain witnesses; the meaning of the analysis follows
from the graphs built in `Grammar.cpp`, not class names alone. Old
`detectRecursion()` examines the first RHS symbol; the new analysis should
account for the whole nullable prefix. In the no-progress cycle graph,
an `A -> B` edge through a specific occurrence of `B` requires all other
symbols in that production to derive epsilon. For left recursion in
`A -> α B β`, nullable `α` suffices regardless of `β`; for a no-progress
cycle, both `α` and `β` must be nullable.

In each graph, report every cyclic SCC and one witness per component.
A singleton SCC requires a self-edge. A witness is a closed sequence of
`(RuleId, zero-based RHS position)` pairs; positions distinguish repeated
symbols within one production, while `RuleId` distinguishes textually
identical productions. The result must be deterministic for a fixed grammar
and IDs. A witness does not replace the set of all cyclic rules.
Enumerating all cycles using Johnson's algorithm should be optional and bounded.

This analysis was moved in the roadmap to item 3.6, before tables and the
generator. Tests should cover `CycleTest.cpp` and the five grammars from
original `zubr-kit/src/main/resources/trapGrammars.dat`. That file also
contains historical expected FIRST/FOLLOW tables after `---` separators,
so only the grammar parts of blocks should be copied into Żbik's resource.

An important distinction: `S -> S | a` is a syntactically valid productive
CFG, but has a no-progress cycle and infinitely many trees for `a`, so it
does not meet the basic generator's requirements. Do not confuse this
with a builder error or prohibit ordinary left recursion `S -> S a | a`.
Report productivity, structural reachability, and usefulness in a complete
terminal derivation separately, without mutating input. Determine usefulness
from a productive start using only productive productions. In
`S -> A Dead | b`, `A -> A | a`, `Dead -> Dead`, `A` is reachable and
productive but participates in no complete derivation from `S`.
A no-progress cycle in the useful portion implies infinitely many trees
for some start word; a structural cycle alone is insufficient.
`S -> S` has no finite tree.

The generator policy uses these results and prunes unproductive expansions.
Canonical LR(k) construction need not prohibit cycles: item deduplication
ensures closure terminates. The detailed contract and required tests,
including comparison of productivity with nonempty `FIRST₀`, are described
in item 3.6 of separate implementation documentation. Minimum and maximum
lengths remain in 5.1.

Old `computeMaxLen()` marks dependency-graph cycles as infinity. Do not port
this as an exact algorithm: `S -> S | a` has maximum 1, `S -> S | ε` maximum
0, `S -> S a | a` unbounded length, and `S -> S` derives no word.
Analyze productivity and cycles that can increase length in a productive
context. Do not use one `int` value for unknown results, empty language,
infinity, and overflow.

### Enumeration and limits

The generator already available in C++ `z_javy`, used by `testAmbig`, is too
valuable to remove. Given an upper length bound, it expands derivation trees,
returns terminal sequences and tree representations, and can thereby find
two different derivations of one word. In Żbik it should be a separate
client of immutable `Grammar`, rather than part of the symbol classes.

Retain Java's iterator model: `Generator(grammar, maxLen, ruleOrder)` exposes
successive derivations through `next()`. `maxLen` defines the window in
which the generator tries successive productions. A child's budget already
accounts for minimum lengths reserved for later symbols of the current
production. Results may have any length from zero to `maxLen`; do not
introduce a separate `generateExact(n)`. Clients interested in an exact
length filter generated results. This particularly matters for grammars
whose derivations are all shorter than `n`: the generator should not perform
a full fruitless search merely to satisfy a client filter.

Important properties:

- Return both `vector<TerminalId>` and a tree or unambiguous derivation representation.
- Do not deduplicate solely by terminal word: that would erase ambiguity evidence.
- Rule-expansion order must be deterministic; random shuffling can later
  be an optional search strategy.
- Each symbol's minimum derived word length belongs in separate
  `MinYieldLength`, not mutable `Grammar` fields.
- The generator may return a word repeatedly with different trees;
  `testAmbig` detects ambiguity upon finding two different witnesses.
- Accept only grammars meeting analysis 3.6's conditions; in particular,
  do not attempt to enumerate useful no-progress cycles as “trap grammars”.
- Rejection identifies the specific violated condition, rather than
  masking it with a depth limit or global configuration deduplication.
- For a supported grammar, `exhausted` means all derivations of length at
  most `maxLen` are exhausted; `witness-found` means ambiguity search ended early.
- `testAmbig(n)` can provide an ambiguity witness, but lack of a witness
  up to length `n` does not prove the grammar unambiguous.

`AmbiguitySearch` may keep a `word -> first tree` map. A second structurally
different tree for the same word ends the search and returns both witnesses.
Compare `RuleId` and tree structure rather than formatted text.

The generator has two additional testing uses:

1. It supplies short words for `LRMachine` comparison; absence of a word
   proves rejection only after complete search, and tests must also cover
   inputs outside the language.
2. It permits empirical FIRST/FOLLOW checks at small lengths without
   replacing the proper fixed-point algorithm.

Port the ideas and behavior of C++ `z_javy`'s `Generator`, `Node`, and
`RuleOrder` for supported grammars, together with existing non-trap tests.
Rewrite ownership using IDs and values or `unique_ptr`; do not copy the
raw-pointer tree, tie the generator to mutable symbol fields, or replace
validation 3.6 with internal cycle masking.

## Regular-expression automata

Żbik may also adopt the `regex` and `lex` portions of C++ `z_javy`, but this
is a second independent development track. Do not mix LR and regex automata:

| Automaton | State | Alphabet | Execution |
|---|---|---|---|
| `LRkDfa` | Set of LR(k) items | Grammar terminals and nonterminals | Used to build a table |
| `LRMachine` | Table state plus state stack | Input tokens | Deterministic pushdown automaton |
| `RegexNfa` | Thompson node | Characters/classes plus epsilon | Multiple possible states |
| `RegexDfa` | Set of NFA states | Characters/classes | Ordinary lexer DFA |

`z_javy` already has:

- a simple regex-to-AST parser: alternation, concatenation, literals,
  `*`, `+`, `?`, and groups;
- basic `NFA`/`NFAState` with epsilon transitions;
- a simple NFA comparator and regex parser tests.

It lacks the complete production pipeline: AST → Thompson NFA → subset
construction DFA → optional minimization → matcher. The current port also
uses raw pointers in the AST, does not free the whole regex tree, and gives
`NFA` a manual destructor with default copy operations, risking double free.
Port syntax, tests, and general division, but not the ownership model.

Target:

```text
RegexParser -> RegexAst (unique_ptr or value variant)
            -> ThompsonBuilder -> RegexNfa
            -> SubsetConstruction -> RegexDfa
            -> optionally DfaMinimizer
            -> Lexer/Matcher
```

NFA and DFA states should have dense `StateId`s; transitions store IDs,
not pointers. Subset construction interns sorted `NfaStateId` sets.
The same hashing rules apply as for LR: the hash finds a bucket; the full
set determines equality.

Define the regex alphabet before implementation. The current AST stores
a literal as `std::string`, while the NFA stores a transition as `char`,
making the API inconsistent. Reasonable choices are:

- Unicode code points or bytes for a conventional text lexer;
- `CharacterClassId` for ranges and character classes;
- `LiteralId`/token for regexes at grammar-symbol level.

A single generic automaton class for everything is not worthwhile.
Only small infrastructure pieces should be shared: `StateId`, DOT export,
ID-based transitions, reachability, and interning rules. `LRkDfa` and
`RegexDfa` have different state meanings, alphabets, and construction methods.

After basic LR(k) is complete, the separate regex path can be:

1. An ownership-safe `RegexAst` and tests ported from `z_javy`.
2. Thompson NFA with epsilon closure.
3. DFA through subset construction.
4. Matcher and equivalence tests on generated short words.
5. DFA minimization and lexer integration only after correctness is established.

## Implementation roadmap

The execution plan and status of each item are maintained in the project's change history.

## Minimum set of tests to port

Port existing C++ test behavior from `z_javy`, rewritten against the new API:

1. Classic expression grammars for LR(0), conflicting LR(0), and valid
   SLR/LR(1); even if SLR is not an API, the example still tests conflicts.
2. An assignment grammar that is not SLR(1) but is LALR(1).
3. `S -> C C`, `C -> c C | d`: 10 LR(1) states, 7 after LALR merging.
4. An LR(1) grammar that produces two reduce/reduce conflicts after merging.
5. A grammar requiring two lookahead tokens, including `a EOF` versus `a a`.
6. A grammar genuinely requiring LR(3).
7. A nullable start and nullable chain.
8. Left recursion and nullable cycles; the algorithm must reach a fixed point.
9. Two textually identical productions; their reduce/reduce conflict must remain.
10. Artificially colliding state hashes; equality must protect the collection.
11. A large alphabet and `k=6`; the ACTION node count must remain small.
12. Exhaustive comparison of all short inputs against a simple oracle.

Reference-table tests are especially valuable: they reveal wrong reduction
numbering, lost transitions, and incorrect lookaheads that `accepts()` alone may miss.

## Decisions that should not be postponed

- **Scope:** canonical LR(k) is mandatory; decide whether LALR(k) also belongs
  in the first release. This document assumes it does.
- **FIRST/FOLLOW without LL:** implement separate grammar analyses, not LL
  tables. SLR may later be a small FOLLOW(1)-based addition, but is not a
  roadmap milestone. LR(1) and LR(k) do not depend on FOLLOW.
- **DFA as a separate layer:** state collection and transitions cannot be
  hidden side effects of table construction.
- **Pushdown automaton:** LR runtime must be a separate `LRMachine`, not
  a method attached to the state graph.
- **Separate regex:** `RegexNfa/RegexDfa` may share graph tools with LR,
  but not the state model or alphabet.
- **Immutability:** no parser construction should modify `Grammar`.
- **Identity:** identical productions retain different `RuleId`s.
- **Conflicts:** the table stores all actions; precedence policies may be
  added only as a separate layer.
- **Performance:** begin with a correct simple `WordSetK` and sparse ACTION
  map, then profile. ACTION tries are a later representation once comparative
  tests protect semantics.
- **Data format:** comments and labels in `grammars.dat` are not rule syntax.
  The corpus reader separates them from `GrammarBuilder`.

## Shortest sensible path

The lowest-risk order is:

```text
immutable Grammar
  -> nullable, FIRST(1), FOLLOW(1)
  -> FIRST(k), FOLLOW(k) as sets of short words
  -> productivity, reachability, no-progress cycles, and left recursion
  -> canonical LR(k)-state DFA
  -> ACTION/GOTO and conflicts
  -> stack-based LRMachine, including k > 1
  -> LALR(k) through core merging
  -> generator with length bound n and testAmbig
  -> optimizations and corpus classifier
  -> programmatic restricted EBNF -> BNF
  -> GLR on conflicting LR(1) tables
  -> IELR(1), then experimental IELR(k) with canonical LR(k) fallback

next lexical-analysis track:
RegexAst -> RegexNfa -> RegexDfa -> minimization -> Lexer -> LRMachine
```

This develops Żbik's original idea: a small typed core instead of copying
the entire earlier port. `z_javy` remains the primary source of working
code, behavior, and test cases, but not a project-structure template.
Java `zubr-kit` remains upstream, useful for verifying the translation and later updates.
