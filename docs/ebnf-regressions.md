# EBNF tests on grammars resembling real languages

## Purpose

This document describes tests intended to answer a practical question:
does Agas's restricted EBNF, after conversion to BNF, yield useful
deterministic LR grammars before Żbik gains GLR?

The experiment is not intended to prove that all programming languages
are LR(k), or that failure for several `k` values implies infinite lookahead.
It should distinguish:

- a conflict caused by an ambiguous grammar;
- a conflict caused by a particular description of a language that can be
  reformulated equivalently;
- a common prefix difficult for LL(k) but natural for LR(1);
- a contextual dependency that increasing `k` cannot resolve;
- a case genuinely requiring multiple GLR interpretations to remain available.

The language of token sequences is studied separately from AST shape.
Two grammars may accept the same language but produce different trees.
Every reformulation must therefore report both effects.

## Input material and levels of realism

Tests should be added gradually. The following paths refer to resources
in the separate Agas project:

1. **Cmm:** `agas-cpp/grammars/examples/cmm.ag` and its source
   `agas-cpp/grammars/examples/cmm.g4`. This is a small grammar with declarations,
   initialization, calls, assignment, a loop, and expressions.
2. **CMinus:** `agas-cpp/grammars/examples/cminus.ag` and
   `agas-cpp/grammars/examples/cminus.g4`. It adds functions, `if/else`,
   `for`, `return`, multiplicative operators, and more assignment forms.
3. **C90 declarators:** first translate
   `agas-cpp/grammars/c/CDeclaratorSubset.g4` as mechanically as possible into
   `agas-cpp/grammars/c/CDeclaratorSubset.ag`, then map it to the EBNF model.
   The grammar covers prototypes, function definitions, function pointers,
   arrays, declarator lists sharing one type, and K&R syntax.
4. **Isolated `typedef` context:** a microgrammar demonstrating why
   `Identifier`/`TYPE_NAME` classification must not be confused with LR lookahead.
5. Only after the above tests pass should the fuller
   `agas-cpp/grammars/c/C.ag` or `agas-cpp/grammars/c/C90.ag` be used as
   stress tests. They are not prerequisites for starting the Agas bootstrap.

In stage 8.5, the parser portion of `.ag` is manually mapped into a
programmatic EBNF model. `node`, `inline`, AST fields, channels, and lexer
rules do not enter the parser-language model. The final `EOF` in an `.ag`
file means Żbik's built-in end of input, not an ordinary `TerminalId`.

## Common investigation protocol

For every grammar version:

1. Build a programmatic EBNF specification.
2. Convert it through stage 8.2 into an ordinary `Grammar`, retaining the
   origins of all helper rules.
3. Run nullable, trap, minimum-length, FIRST, and FOLLOW analyses.
4. Build canonical LR for `k=1`, `k=2`, and `k=3`, stopping at the first
   success but allowing full diagnostics for a failed `k`.
5. Check LALR(k) for the first successful `k`, distinguishing conflicts
   introduced by merging from canonical LR(k) conflicts.
6. Run the parser on named valid and invalid programs.
7. Compare the language with the generator for all short sequences within
   a feasible bound, retaining every counterexample found.
8. Record state, item, transition, and conflict counts, table sizes, and
   the time of each stage.

A conflict report must contain the state, complete items with lookahead,
all actions in the ACTION cell, and EBNF rule origins. For a reduce action,
identify the reduced `RuleId`. For a shift, list every state item that
justifies shifting that terminal. Map every participating `RuleId` through
`EbnfRuleOrigin` to the `.ag` rule name, alternative number, element position,
and quantifier; a BNF helper must not obscure the author's rule. Merely
reporting “shift/reduce” is insufficient to determine the cause.

Conflict freedom is a property of a particular grammar and table construction.
Failure to find an ambiguity witness up to length `n` is not proof of
unambiguity. Likewise, a conflict for `k <= 3` does not prove that no finite
`k` exists.

## `dangling else`

### Source of ambiguity

The simplified notation:

```text
statement
    : IF '(' condition ')' statement
    | IF '(' condition ')' statement ELSE statement
    | otherStatement
    ;
```

has two trees for the sequence corresponding to:

```c
if (a)
    if (b)
        first;
    else
        second;
```

The `else` may belong to the inner or outer `if`. Increasing `k` does not
remove grammar ambiguity: both derivations have the same complete token
sequence. GLR can retain both trees, but cannot itself define the language's semantics.

A typical language rule binds `else` to the nearest unmatched `if`.
Agas should provide two explicit ways to obtain this semantics.

### Unambiguous `closed`/`open` grammar

The test version should separate statements that can no longer capture an
external `else` from statements containing an unmatched `if`:

```text
statement
    : closedStatement
    | openStatement
    ;

closedStatement
    : simpleStatement
    | compoundStatement
    | IF '(' condition ')' closedStatement ELSE closedStatement
    | WHILE '(' condition ')' closedStatement
    | FOR '(' forControl ')' closedStatement
    ;

openStatement
    : IF '(' condition ')' statement
    | IF '(' condition ')' closedStatement ELSE openStatement
    | WHILE '(' condition ')' openStatement
    | FOR '(' forControl ')' openStatement
    ;
```

Propagation through `while` and `for` matters. Separating just the two `if`
rules is insufficient if another statement's body can end in an open `if`.
`compoundStatement` is externally closed because `else` cannot cross a brace.

Technical rules may be `inline`, but an AST test should verify that omitting
these nodes yields the same target tree as the “nearest `if`” policy.
The original and `closed/open` token-sequence languages should be identical;
the number of allowed trees changes.

### Explicit conflict resolution

Rewriting the grammar is a valid oracle but can be verbose. Agas should
later gain a declaration resolving a specific conflict. The following
notation illustrates the contract, not approved `.ag` syntax:

```text
resolve danglingElse {
    terminal = ELSE;
    choose = shift;
    over = reduce ifStatement#IfStatement;
    expect = 1;
}
```

Meaning:

- The declaration applies only to a conflict on `ELSE`.
- The competing reduction must be the specified short-`if` alternative.
- `shift` is selected, binding `else` to the nearest `if`.
- Exactly one matching conflict must exist after table construction.
- Zero, two, or a different kind of conflict means a stale declaration and
  a generator error, rather than silent application of a general priority.

Do not introduce a global “always choose shift” rule. Diagnostics for the
resolved table must retain the original conflicting cell, the selected
action, and the policy name. `expect` does not choose an action; it only
guards the expected conflict count.

Such a declaration is not simply named `ambig`, since a table conflict is
not proof of ambiguity. It may result from insufficient `k` or LALR merging.
The name `resolve` describes the actual operation.

### Acceptance tests for `dangling else`

- Original `cminus` produces the expected shift/reduce conflict, and the
  generator finds two trees for as short a witness as possible.
- The `closed/open` version has no such conflict in canonical LR(1).
- Bounded comparison of the original and rewritten languages finds no
  token-sequence differences.
- Explicit `resolve` chooses the same tree as the `closed/open` version.
- Nesting through `while`, `for`, and blocks checks the proper scope of `else`.
- Changing or removing the specified production produces a stale-`resolve`
  error rather than resolving a different conflict.
- Future GLR without a policy retains both trees, while a language compiler
  with a nearest-`if` rule does not leave selection to chance.

Stage 8.5 cannot be completed with only a conflict report for original
`cminus`. The `closed/open` version must be an executable deterministic
path and pass the named parser examples. The original form remains a
diagnostic case and input for future GLR. The `CDeclaratorSubset` test is
independent, since it contains neither `if` nor `else` statements.

## Function prototype and definition with a common prefix

An LL(k) problem may look like this:

```text
externalDeclaration
    : declarationSpecifiers declarator ';'
    | declarationSpecifiers declarator compoundStatement
    ;
```

`declarator` can be arbitrarily long. An LL parser must choose an alternative
before reading it, so no small fixed `k` need suffice. An LR parser does not
make this decision at the beginning. After shifting the common prefix, it
sees `;` for a prototype/declaration or `{` starting the definition body.
Therefore this case should first be checked as LR(1), without automatically
increasing `k` or rewriting the grammar.

An equivalent variant, useful if the full grammar still conflicts, is:

```text
externalDeclaration
    : declarationSpecifiers declarator externalDeclarationSuffix
    ;

inline externalDeclarationSuffix
    : ';'
    | compoundStatement
    ;
```

This is not left factoring required by the LR algorithm, but it may simplify
the grammar and diagnostics. `inline` prevents an extra AST node being imposed.

The test should include increasingly nested declarators, prototypes,
definitions, parameter lists, and several declarations with the same start.
For the direct and suffix versions, compare:

- the smallest successful `k` and conflicts;
- all short token sequences;
- trees after omitting the `inline` node;
- automaton and table sizes, so the reorganization is not judged solely by
  “passes/fails”.

If LR(1) accepts the direct version, this is an important positive result:
it demonstrates LR's advantage over the former LL(k), rather than a need
for LR(2) or LR(3).

## `typedef` is a contextual problem

A separate case is code such as:

```c
T * x;
```

If the lexer returns `T` as an ordinary `Identifier`, its meaning depends
on whether an earlier declaration in the current scope introduced `T`
through `typedef`. The same notation may resemble a declaration or a
multiplication expression. This is not a common-prefix problem that LR
can resolve by reading more tokens. The required information is in the
symbol table and may originate arbitrarily far away.

Three architectures should be investigated separately:

1. **Lexical classification:** the lexer, or a layer between lexer and parser,
   returns `TYPE_NAME` instead of `Identifier`, using the current symbol table
   and scopes. This is the classic efficient solution, but creates a
   controlled parser–semantics–lexer information loop.
2. **Contextual token provider:** the raw lexer remains independent and
   the parser asks a contextual layer to classify an identifier. This better
   preserves module separation, but its contract must specify when scope
   updates occur and how error recovery behaves.
3. **GLR and semantic filtering:** the parser retains both interpretations,
   and the symbol table rejects the impossible one. This is the most general
   approach, but may cost more and complicates deferred semantic actions.

Increasing `k` replaces none of these methods. The `typedef` microtest
should deliberately start with one `Identifier` token and demonstrate a
conflict or two interpretations, then separate `Identifier`/`TYPE_NAME`
and confirm a conflict-free grammar. The result should not block Cmm or
CMinus, which have no contextual type names.

## Proposed implementation order in Agas

1. Complete the EBNF model, converter, and rule origins from 8.1–8.4.
2. Run the Cmm test without any conflict-resolution declarations.
3. Run the raw CMinus test and obtain an exact `dangling else` witness.
4. Add the `closed/open` version as an oracle for the unambiguous language.
5. Translate `CDeclaratorSubset.g4` to `CDeclaratorSubset.ag`, compare their
   structure, and run the full LR(1..3) protocol on C90 samples.
6. Design, then add, `resolve` syntax to `Ag.g4` and `Ag.ag`.
7. Test the common prototype/definition prefix first in LR(1), then in the
   variant with an `inline` suffix.
8. Treat `typedef` as a separate context-integration experiment, rather than
   a reason to increase default lookahead.
9. Start GLR only with a list of conflicts deliberately left unresolved.

## Result criterion

The experiment supports a deterministic Agas bootstrap if Cmm and the
unambiguous CMinus variant have conflict-free tables for small `k`, and
parser execution agrees with the bounded oracle. The ambiguous `dangling
else` notation is not required to become LR(k) by increasing `k`.

If function prototypes and definitions work in LR(1), retain that case
as a test demonstrating the difference from LL(k). If they require
reformulation, the report must identify the specific conflict and justify
the suffix version's equivalence.

Agas remains useful even if some languages require an explicit conflict
policy, contextual token classification, or GLR. A negative result would
be the absence of a clear, stable path for simple unambiguous grammars
after EBNF conversion, rather than merely the fact that an arbitrary
user-written grammar can be ambiguous.
