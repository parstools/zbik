# API for lexer classes controlled by parser rules

The first implementation is in `zbik_core`. Classes use a `uint64_t` mask.
A lexer is not created for every bit combination: a single shared DFA
retains all accepting rules in each state and their class dependencies.

## Lexer rules

The third field of `LexerRule` is `requiredClasses`. The default value `0`
means an always-active rule; a multibit mask requires all specified bits.
Example definition fragment, with terminal IDs already declared:

```cpp
constexpr zbik::LexerClassMask SHIFT = 1;
constexpr zbik::LexerClassMask WORDS = 2;
zbik::ByteLexer lexer({
    {SHR, "'>>'", SHIFT},
    {GT, "'>'"},
    {READ, "'read'", WORDS},
    {WRITE, "'write'", WORDS},
    {IDENT, "[a-z]+"},
    {std::nullopt, "[ \\t\\n]+"},
});
```

`ByteLexer` and `Utf8Lexer` provide `next(source, byteOffset, activeMask)`.
The method advances the offset past the token, skips active rules without
a terminal, and returns `nullopt` at EOF. `tokenize(source, activeMask)`
retains the batch interface; omitting the mask means `0`. Rules without
classes behave as before. The longest active match wins, with rule order
breaking ties. Disabling `READ` does not prevent an `IDENT` match;
disabling `SHR` leaves the shorter `GT` available.

## Parser rules and inheritance

`ParserLexerClass` assigns `enabled` and `disabled` masks to a nonterminal.
They apply to all its alternatives and descendants until overridden by
a nested declaration:

```cpp
std::vector<zbik::ParserLexerClass> classes{
    {*grammar.findNonterminal("expression"), SHIFT, 0},
    {*grammar.findNonterminal("type"), 0, SHIFT},
};
auto scoped = zbik::scopeLexerClasses(grammar, classes, lexer);
zbik::ParseTable table(zbik::LRkDfa(scoped.grammar, 2));
zbik::ContextualLRMachine parser(table, scoped.requirements, scoped.sourceTerminals);
parser.validateLexer(lexer.rules());
auto result = parser.parse(lexer, source, true);
```

A rule inherits a mask and computes `(inherited | enabled) & ~disabled`.
On return, the caller's context applies again. Enabling and disabling the
same bit in a declaration is an error. The optional `initial` argument
of `scopeLexerClasses` sets the initial mask. Thus `SHIFT` can be enabled
globally and disabled in `type`, or enabled only in `expression`.

If `GT` should also be initially inactive, its rule can receive a separate
bit. Vist expressions normally need both `GT` and `SHR`, since `GT` is also
used in comparisons such as `a > b`.

## Building LR(k) context

`scopeLexerClasses` specializes reachable `(nonterminal, mask)` pairs.
The return to the enclosing mask is encoded in the grammar structure,
without lexer `push`/`pop` actions. Recursion returns to an already-created
pair. All masks are not enumerated, but the number of actually reachable
pairs may still be large. The default limit is 4096; exceeding it produces
an error. The result maps terminals, nonterminals, and productions to their originals.

The overload that accepts a lexer analyzes the shared DFA. For each terminal,
it determines the bits that can affect a match or extend it. For example,
`SHR` affects `GT` but not `LT`. Only these relevant bits distinguish terminal
variants in the LR grammar. Without a lexer argument, the API uses
conservative full scope masks.

`LexerContextPlan` builds prefix trees for ACTION words. A node represents
a `(state, lookahead prefix so far)` pair. Requirements of the next tokens
determine the active mask. Inconsistent requirements produce a
`LexerContextError` containing the state, prefix, contradictory-bit mask,
and terminal names. Lexer validation also detects an expected token with
no active lexical rule.

`ContextualLRMachine` requests tokens without tokenizing the whole file
first. The second LR(2) lookahead token receives a mask dependent on the
first, before the parser's first shift. The current version recognizes
unconsumed lookahead again after actions and checks it against the buffer.
It does not allow a context change to silently change the token that
justified an earlier reduction. The result contains only consumed tokens,
a syntax error or success, and an optional trace of lexer requests.
Lexical errors retain their existing exception types.

## Verified cases and current scope limits

- Masks for `SHR`, the shared `READ`/`WRITE` class, and their combinations, LR(1)/LR(2).
- Nested enables and disables, an initial class, recursive types, an empty
  production, and return to the expression class after a type closes.
- UTF-8, byte offsets, bit 63, a longer `IDENT` despite an active keyword,
  and selection of a shorter token after disabling `SHR`.
- Contradictory declarations, incompatible contexts for the second
  lookahead token, failure to enable a required class, and the reachable-specialization limit.

The implementation does not yet cover `.ag` syntax, class export to an
artifact, or the Rust runtime. Diagnostics do not yet generate a textual
regex-collision witness or positions in `.ag`; mappings to originals are
available to the frontend. Regex dependency analysis is conservative and
may require finer context separation than an approach using multiple
tokenizations. Matching and generation costs need measurements on larger
grammars before optimization.

Standalone lexers also support masks for `skip`. The variant controlled
by parser-rule scopes currently requires unconditional `skip` rules and
rejects others: ownership of skipped characters at scope boundaries and
EOF needs a separate contract. An XML example and named channels are the
next stage. For lazy regexes, incremental `Utf8Lexer::next` decodes the
remaining input; optimizing this path also remains to be done.
