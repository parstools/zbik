# Selective merging of canonical LR(k) states

Status: first experimental implementation and comparative tests;
the full proof and contextual diagnostics remain open. Date: 2026-09-20.

This document describes a construction using an already-built canonical
automaton. It is neither an implementation of published IELR(1) nor a
correctness proof for its generalization to IELR(k).

## 1. Purpose and scope

The goal is to reduce state count and exported Agas table size for a
conflict-free LR(k) grammar, primarily LR(2) and LR(3). The canonical
automaton remains an immutable reference and generation-time fallback.
The runtime receives one selected parser; it does not need a second
canonical table for diagnostics. The grammar, production numbers, and `k`
are unchanged.

LALR(k) merges all states with the same LR(0) core. Selective merging merges
only groups passing the checks below. In particular, decisions cannot
depend solely on conflicts in the two directly compared rows: merging
them may force successor merges, then merges of further states.

The cost of canonical LR(k) construction is still incurred. The optimization
concerns the finished automaton and artifact, not initial canonical construction.

The first version rejects conflicting input tables. It does not use default
shift, the first reduction, or precedence to manufacture apparent success.
Grammars with conflict-resolution policies are a separate extension.

## 2. Two compatibility criteria, two different guarantees

### 2.1. `ExactActions` mode

Every state in a group must have the same actions for every lookahead word
after shift targets are mapped to groups. A missing action means `error`
and also participates in comparison. Transition targets must agree.

This mode preserves parser execution step by step, modulo state numbers.
It has a simple proof below and is a good first implementation and control
case. However, it may leave every state separate: lookahead differences
that LALR normally exploits for compression often mean precisely a reduction
versus an error.

### 2.2. `CompatibleUnion` mode

Action sets are unioned. `error` is an empty set, so an error can be combined
with one action. Two different actions for the same lookahead word are
not allowed. Transition targets must still agree.

This is the direction for more effective selective merging. It may add an
action in a context where the canonical parser would already report an
error. It therefore must not inherit the stepwise-equivalence proof of `ExactActions`.

This document gives a complete construction and mechanical partition
validation algorithm for both modes. Before `CompatibleUnion` is admitted
to normal export, it also requires justification of language preservation
and behavior on invalid inputs for Żbik's particular LR(k) construction.
Passing tests and absence of conflicts do not replace this justification.

## 3. Data and notation

Input:

- a finite, complete canonical `LRkDfa` with state set `Q`;
- `k >= 1`, with the same grammar and EOF convention throughout;
- a raw, uncompressed, conflict-free `ParseTable`;
- stable symbol, state, and production order.

`core(q)` is the ordered set of `(RuleId, dot)` pairs without lookahead.
Initially use precisely the `stateCore()` convention in `src/lr/LALRkDfa.cpp`:
the core projects the entire stored item set. Do not replace it with only
the kernel in the same change without justifying representation equivalence.

`delta(q, X)` is a transition on terminal or nonterminal `X`. `P` is a
partition of canonical states, and `group(q)` is the group containing `q`.
Each state belongs to exactly one nonempty group.

Actions mapped to the partition:

```text
normalize(Shift(t), P) = Shift(group(t))
normalize(Reduce(r), P) = Reduce(r)
normalize(Accept, P) = Accept
normalize(error, P) = error
```

Compare reductions by `RuleId`, not merely RHS length or nonterminal name.
Different productions may have different AST meanings.

## 4. Full lookahead words

For `k=2`, `[a,b]`, `[a,c]`, `[a,EOF]`, and `[EOF]` are distinct keys.
Do not replace them with a set of first terminals. `EOF` is a special
end-of-input symbol, not a similarly named user terminal.

Żbik's runtime reads up to `k` terminals and appends one `EOF` if input
ends before the window is full. EOF is not repeated to length `k`.
The empty FIRST word means epsilon, not an empty input token.

`ParseTable::ActionTrie::find()` matches the whole key exactly, without
prefix matching. Checks must preserve that semantics. If prefix keys are
introduced later, compatibility checks must also detect overlapping key domains.

Do not enumerate the whole alphabet raised to power `k`. The union of
existing keys in compared rows suffices; outside it all rows have errors.

An important implementation detail: for `k>1`, `buildActionRow()` creates
shift actions through `FIRST_k` of the production remainder plus item
lookahead. Never construct them solely from the transition's first terminal.

## 5. Invariants of an accepted partition

After every successful transaction:

1. Groups are nonempty, disjoint, and cover all of `Q`.
2. All states in a group have the same `core`.
3. For every symbol, transition presence agrees; if a transition exists,
   all targets belong to one group.
4. Normalized ACTION rows satisfy the chosen compatibility mode.
5. Result-state items are exactly the union of member items.
6. The start is the group containing the canonical initial state.
7. Grammar productions and symbols are not renumbered.

A shared core should ensure agreement of transition presence, but check
it explicitly. A missing transition is incompatible with an existing
one even in `CompatibleUnion` mode.

## 6. Baseline algorithm: transactional group merging

### 6.1. Initialization

In `CompatibleUnion`, first build and check full LALR(k). If conflict-free,
adopt its core groups, validate the result, and finish without selective
merge attempts. Only conflicting LALR(k) triggers the transactional
algorithm. `maxAttempts` limits only those attempts; it does not disable
the initial LALR check. Statistics record `usedLalr`, the LALR state count,
and its conflicts. The control mode `ExactActions` does not use this
shortcut because full LALR may change empty cells.

After unsuccessful LALR, or in strict mode, start with a singleton partition:
each canonical state has its own group. Determine buckets of equal cores.
Only groups from the same bucket are merge candidates.

The first implementation may use a full working partition copy for every
attempt. This is easier to check than incremental structures. Union-find
with a rollback journal can be used later.

### 6.2. Closure of merges forced by transitions

An attempt to merge `A` and `B` includes the whole transaction:

```text
tryMerge(P, A, B, mode):
    W = working copy of P
    pending = [(A, B)]

    while pending is not empty:
        (a, b) = remove first pair
        a = current representative of a in W
        b = current representative of b in W
        if a == b: continue

        if core(a) != core(b): reject the whole attempt
        if transition domains of a and b differ: reject the whole attempt

        for each symbol X in stable order:
            append (target(a, X), target(b, X)) to pending

        merge a and b in W

    if W violates transition invariants: reject the whole attempt
    if ACTION rows are incompatible with mode: reject the whole attempt
    return W as the accepted result
```

A working group may temporarily have several targets for one symbol.
Retain those targets or the corresponding obligations in the queue.
Never overwrite one target with another and lose a forced merge.
Here `target` means a representative target with all previously created
obligations retained.

The queue may contain duplicates; current representatives allow them to
be skipped. Transition cycles do not cause infinite recursion: each actual
union reduces group count, at most `|Q|-1` times.

Check actions after transition closure. Two shifts to different canonical
states may become the same action after their targets merge. Do not
prematurely classify them as a conflict.

Rejection rolls back every transaction change, including forced merges
of distant successors. The canonical graph is never modified.

### 6.3. ACTION checks

For every working group `B`, collect its canonical-row keys. For each `w`:

- `ExactActions`: all `normalize(ACTION(q,w),W)` must be equal, including errors.
- `CompatibleUnion`: the set of distinct nonempty normalized actions may
  have at most one element.

`Reduce(r1)` and `Reduce(r2)` for different productions conflict.
So do shift/reduce, shift/accept, and reduce/accept. Do not remove accept
or replace it with a default reduction.

The simple version checks the whole partition after every attempt.
Restricting checks to affected groups is possible after adding explicit
dependencies and tests.

### 6.4. Attempt order and fixed point

Order buckets by core and groups by their smallest canonical `StateId`.
Try pairs lexicographically. After a successful transaction, restart
pair scanning on the current partition.

Stop when a complete scan produces no merge or the budget is exhausted.
Do not persist pair rejection without accounting for partition version.
Action targets are mapped relative to current groups.

The result is deterministic for a fixed order and attempt budget.
A time limit may stop the algorithm at different points; reproducible
artifacts should use work limits or a complete run.

## 7. Why successors must be checked

Suppose `p` and `q` have the same core and locally compatible actions:

```text
p --x--> r
q --x--> s

ACTION(r, [a,b]) = Reduce(R1)
ACTION(s, [a,b]) = Reduce(R2)
R1 != R2
```

After merging `p` and `q`, there must be one transition on `x`. This requires
merging `r` and `s`, which is forbidden. Thus the initial pair is rejected
too. This problem can emerge after an arbitrarily long transition chain.

An example allowed only in `CompatibleUnion`:

```text
ACTION(p, [a,b]) = Reduce(R1)    ACTION(q, [a,b]) = error
ACTION(p, [a,c]) = error         ACTION(q, [a,c]) = Reduce(R1)
```

The union contains the same reduction for both keys and creates no conflict,
but does not preserve the exact error-reporting moment.

## 8. Materializing the result and certificate

After selecting a partition:

1. Create one state per group, retaining `canonicalOrigins`.
2. Union, sort, and deduplicate full member items.
3. Remap all transitions through `canonicalToMerged`.
4. Assign stable dense identifiers, preferably BFS from the start with
   ordered transition symbols.
5. Rebuild the table using the same rules as `buildActionRow()`.
6. Check agreement with the union of normalized canonical rows.
7. Only then perform table compression and DSL/JSON export.

Rebuilding rows matters for LR(k): it checks that item merging and FIRST(k)
computation produce exactly the expected actions. A discrepancy is a
construction error, not grounds to choose the more convenient result.

The certificate consists of the partition, state map, and chosen mode.
An independent validator should check invariants without replaying attempt
order. In `ExactActions`, this certificate suffices for the proof below.
In union mode, it certifies local construction properties, not language preservation.

Never use a compressed table to make merge decisions: default reductions
may intentionally replace some empty cells.

### 8.1. One default reduction instead of many entries

After approving a merge, repeated reductions can be stored as a default
row action, as current `CompressedParseTable` does:

```text
before compression: [a,b] -> R7, [a,c] -> R7, [d,EOF] -> R7
after compression:  any -> R7
```

The second form may execute R7 in previously empty cells too. It is not
literally the same ACTION function, although this compression technique
is used to reduce tables. Shifts, accept, and other reductions remain
explicit exceptions; R7 must not overwrite them.

Merging and compression complement each other: the first reduces states,
the second entries, including repeated rows. Experimental reports cover
both stages. States with similar reductions may benefit from row
deduplication even if their cores or transitions cannot be merged.

Choosing the same default reduction alone does not justify state merging.
Compatibility still uses uncompressed ACTION, full lookahead, and successor
closure. Extending reductions into empty cells is a separate transformation:
proofs and error-delay measurements for the raw automaton do not automatically
apply to compressed-table execution.

## 9. Correctness justification and its limits

### 9.1. Proof for `ExactActions`

Relate canonical stack `[q0,...,qn]` to result stack
`[group(q0),...,group(qn)]` at the same input position.

Initially the start states satisfy the relation. Then:

- Both parsers read the same lookahead and select the same action.
- Shift consumes the same terminal and enters corresponding states.
- Reduction uses the same `RuleId`, removes equally many stack elements,
  and GOTO agreement preserves the relation after pushing a state.
- An empty production removes zero elements and uses the same argument.
- Accept and error occur at the same step.

Induction over steps gives the same result, reduction sequence, and error
position. If the canonical parser terminates, so does the result parser.
State numbers and message text containing them may differ.

### 9.2. Language theorem for `CompatibleUnion`: assumptions

Let `G` be the grammar, `C` the canonical parser, and `M` the merged parser.
`L(M)` is the set of finite inputs for which `M` terminates with accept.
This definition alone promises no termination on other inputs.

The proof route is:

```text
L(C) ⊆ L(M) ⊆ L(G)
L(C) = L(G)
therefore L(M) = L(C) = L(G)
```

Require a correct and complete canonical LR(k) construction, a conflict-free
input table, and section 5 invariants. Result ACTION is exactly the union
of canonical actions after target mapping; no actions are removed to
resolve conflicts. Reductions use original productions, and accept applies
only to the completed synthetic-start production and end of input.

An additional structural obligation is to check the item, closure, and
GOTO property in 9.4. Merely checking that all rows are deterministic cannot
replace it. The following argument justifies the design; before deployment,
its lemmas must be related to Żbik's representation.

### 9.3. Preserving valid runs: `L(C) ⊆ L(M)`

For input accepted by `C`, relate the stacks:

```text
C: [q0,        q1,        ..., qn]
M: [group(q0), group(q1), ..., group(qn)]
```

The initial states correspond. At the same input position both parsers
read the same full lookahead. Every `C` action remains in the group's row
union; absence of conflicts forces `M` to choose it.

Shift consumes the same token and preserves stack correspondence through
target mapping. Reduction uses the same `RuleId`, removes equally many
states, and GOTO agreement preserves the relation after pushing a state.
This includes empty reductions. Accept remains the same action at the
same input position.

Induction over a finite valid run proves acceptance by `M`. The reduction
sequence is also preserved, and identical AST-building rules yield the
same syntax tree. Identical empty cells are unnecessary because valid
runs never use them.

### 9.4. Structural lemma: a reduction matches stack symbols

Add an auxiliary symbol stack to the proof. Its state describes a path:

```text
B0 --X1--> B1 --X2--> ... --Xn--> Bn
```

`Bi` are groups and `Xi` grammar symbols. This stack need not physically
exist in the production runtime; a test validator can maintain it.

If the core of `Bn` contains completed item `A -> Y1 ... Ym ·`, then:

1. `n >= m`.
2. The last `m` stack symbols are exactly `Y1 ... Ym`.
3. After popping them, the proper GOTO on `A` exists for an ordinary reduction.

The first two claims follow by moving the dot backwards. Closure adds
items with the dot at the beginning. An item whose dot is beyond at least
one symbol in a transition target can therefore arise only by advancing
the corresponding source item. For each edge at the end of the stack
path, move the dot back one position and read the required RHS symbol.

Merging cannot lose this property: all states in a group have the same
item projection, and every edge originates from a canonical transition
to a group with the same target projection. Check this for Żbik's
full-item projection rather than assuming it from the name “core”.

A stack that is too short would require an item with a positive dot
position in the initial state; start construction forbids this. Moving
back over the whole RHS yields `A -> · Y1 ... Ym` in the pre-handle state.
For an ordinary production, its presence follows from closure of an active
expectation of `A`, ensuring GOTO on `A`. The synthetic production is the
exception handled by accept rather than ordinary reduction. For an empty
production, move back over zero edges; GOTO existence needs the same closure argument.

### 9.5. Soundness of every acceptance: `L(M) ⊆ L(G)`

Assign a derivation tree to each auxiliary stack symbol. The invariant
says all trees are valid in `G` and their leaves, in order, form exactly
the consumed input prefix.

Shift appends a leaf for the read terminal. Reduction `A -> Y1 ... Ym`
uses lemma 9.4: the last stack symbols really match the RHS. Replace their
trees with one tree rooted at `A`, without changing the leaf sequence.
An empty reduction creates a tree with an empty leaf sequence.

At accept, the completed synthetic-start item, stack-path property, and
EOF condition ensure a start-symbol tree for the entire input. Thus every
accepted word is derivable in the grammar.

This argument does not require additional steps to agree with the canonical
parser. On invalid input the merged parser may try further reductions,
but no accepting analysis can create a tree for a word outside `G`.
Together with 9.3 and canonical LR(k) correctness, this yields language
equality under the stated structural assumptions.

### 9.6. Termination on invalid inputs

Equality of accepted-word sets does not yet prove that `M` always terminates.
When `C` selects error, `M` may perform an extra reduction and the previous
relation between runs no longer applies.

On finite input, shifts are bounded by input length. An infinite analysis
would therefore eventually perform only reductions. Such runs must be
excluded, especially with empty productions and cycles through nullable contexts.

A first termination theorem could use a suitable condition excluding
derivation cycles without length growth. Define the condition precisely,
prove sufficiency, then check whether Żbik's existing trap analysis covers
it. Absence of a direct `A -> A` production alone is insufficient.
Running the existing cycle detector does not by itself discharge this obligation.

Lemma 9.4 should also exclude stack underflow and missing GOTO after
reduction. Tests should check these explicitly and use a step limit,
but the limit does not replace a termination proof.

### 9.7. Scope of the obtained guarantees

After justifying structural lemmas, language and tree agreement on valid
inputs holds for conflict-free LR(k), including `k=2` and `k=3`.
A separate termination proof also gives a parser deciding every finite
input. Error timing and expected-token sets may differ from the canonical parser.

This excludes grammars whose canonical conflicts were removed by precedence
or reduction-selection policies. For those parsers `L(C)` may be a proper
subset of `L(G)`, so the inclusion chain in 9.2 no longer yields
`L(M) = L(C)`. This requires additional conflict-policy compatibility analysis,
which is important in published IELR(1).

Until implementation assumptions are checked and termination is established,
`CompatibleUnion` remains experimental. Absence of conflicts and successful
bounded differential tests do not change that status.

### 9.8. Priority: reject invalid sentences and identify a useful location

The most important practical condition is absence of false acceptance:
the merged parser must accept no sentence outside the grammar's language.
Smaller tables, faster execution, or better messages do not justify
violating this condition. All valid sentences and their analyses must
still be preserved. Hanging on invalid input is not correct rejection;
termination needs the separate guarantee in 9.6.

Allowing a different error-detection location does not permit arbitrarily
imprecise diagnostics. Rejecting at file end is insufficient if a useful
position could have been identified much earlier. Likewise, do not use
file start as a substitute position when the proper range was not reconstructed.

Distinguish three concepts:

- detection moment: the parser step at which no action is available;
- input position: current token index and a window of up to `k` tokens;
- diagnosis location: the source range shown to the user with an explanation.

Extra reductions may change state and expected symbols without advancing
the input. This differs from consuming further tokens and detecting an
error only far away. For `k>1`, a later lookahead token may cause the
mismatch, so do not automatically blame the first token. When a single
token cannot be identified, the message should honestly indicate the problematic window.

The canonical parser's error position is a reference, not proof of the
author's mistake location. A missing closing parenthesis, for example,
may justify diagnosis at EOF. Also indicate its opening parenthesis if
the diagnostic model can determine it. File end or start is allowed when
substantively justified; the prohibition concerns substitute positions
and optimization-only delay without a useful diagnosis.

### 9.9. One parser and a logically relevant diagnosis

Agas's target runtime uses only the merged parser. It neither retains a
parallel canonical parser nor replays parsing with one after an error.
The canonical automaton serves the generator and tests as an oracle;
once the artifact is selected and verified, it is not an execution dependency.

The experiment compares each parser's first error on the same immutable
token stream before any recovery. Report extra reductions, extra consumed
tokens, byte ranges, and changes in expected lookahead words separately.
Measure worst corpus delay as well as average. Do not assume without proof
that delay is bounded by `k`. These measurements are research information,
not a requirement to indicate the same or an adjacent token.

The practical criterion is logical proximity: the highlighted location
must have an explained relationship to the construct that cannot be
completed correctly. It may be distant in bytes, tokens, or lines. Examples:

- EOF as detection location and an earlier opening parenthesis as a related
  range of an unclosed construct;
- a declaration-ending token and the range of its incomplete parameter list;
- the start of a construct whose continuation does not fit, with another
  indication of where continuation became impossible;
- the whole incompatible LR(k) window when blaming one token cannot be justified.

Diagnostics should distinguish detection location from context range.
Do not present a construct's start as the certain location of the author's
mistake. Identify a specific unclosed or incomplete construct supported
by diagnostic evidence. Do not automatically replace it with its parent,
then successive ancestors, until the range covers the whole file.
For a missing `)` in a parameter list, context is the corresponding `(`
or that list, not the start of the function, class, or compilation unit.
Reduction ranges should preserve the necessary local-context information;
the start of an ever-larger AST node alone is not a sufficient diagnostic
anchor. If a specific construct cannot be reliably identified, keep the
detection location rather than pointing to an ancestor. A message may
describe the mismatch and expected continuations without guessing a unique
fix. A distant indication is allowed, but shifting to EOF or file start
without explanation fails the criterion.

Context should come from token and reduced-construct ranges, stack
information, and grammar-rule metadata available to a single parser.
If diagnostic annotations or delimiter information are needed, design
them explicitly and measure their size. Do not assume state numbers or
`canonicalOrigins` alone can reconstruct meaningful context. Choosing
primary and related ranges is part of Agas's diagnostics design, not a
ready-made property of merging.

When context is insufficient for a detailed diagnosis, indicate the real
detection location and available mismatch information rather than invent
an earlier error position. Repeated useless diagnoses in tests require
better metadata or merging strategy, not a second runtime parser.

Diagnostic tests should delete, insert, and replace tokens at the beginning,
middle, and end of valid sentences; omit delimiters; mismatch the second
or third lookahead token; and include long valid suffixes after errors.
Check user-visible positions, related ranges, and justification of their
connection to the error, including EOF. Include cases where relevant
context lies many lines from detection. A separate nesting test should
ensure a missing delimiter points to the correct inner construct rather
than its parents or file start. Deployment requires language correctness
and justified useful error localization; eventual rejection alone is
insufficient to assess diagnostics.

## 10. Alternative construction for strict mode

`ExactActions` can be computed more efficiently through partition refinement:

1. Initially group all states by `core`.
2. For each state compute a signature: core, full sparse ACTION map with
   shift targets replaced by current groups, and transition map with
   targets replaced by current groups.
3. Split every group by signature.
4. Repeat until no group splits.

An absent key means error; maps with different domains are unequal.
Partitions only become finer, so computation terminates. This yields the
coarsest stable partition for the chosen strict equivalence and shared-core
constraint, not the globally smallest parser recognizing the same language.

Signature equality cannot mechanically be replaced by union compatibility.
Liberal compatibility is not transitive: A may fit B and B fit C while
A does not fit C. Hence `CompatibleUnion` uses transactions checking the
whole group and all forced merges.

## 11. Sizes, costs, and limits

Every result state is a group of input states:

```text
number of distinct cores <= number of groups <= number of canonical states
```

The lower bound corresponds to LALR(k) merging with the same core convention
and reachability handling. Reaching it is not promised, especially in `ExactActions`.

Group count never exceeds canonical state count, even during attempts.
Memory can nevertheless be larger: the reference automaton, accepted
partition, working copy, rows, and dependency queue are retained. A state
limit does not replace a memory limit.

Selective-result selection is strict: it must have at least one fewer
state than the canonical automaton. If state counts are equal, discard
the selective variant and return the original canonical automaton
(`retainedCanonical=true`), retaining attempt statistics. This also covers
attempts stopped by a limit without any reduction. Earlier selection of
conflict-free LALR(k) is a separate path.

For core buckets of sizes `m_i`, potential pairs per scan are on the order
of the sum of `m_i^2`. Every attempt may cover much of the graph. The simple
copying and full-checking version is deliberately a reference implementation;
do not advertise it as linear.

Limits cover attempts, forced unions, time, and extra memory. Interruption
rolls back the current attempt and leaves the last accepted partition.
The result reports the stop reason. If there is no benefit or result
validation fails, use the canonical automaton.

Fewer states need not mean a smaller compressed table: lookahead unions
may make rows denser. Report separately:

- states, transitions, items, ACTION entries, and GOTO entries before and after;
- raw table, compressed table, and text-export sizes;
- canonical construction, merging, and compression times;
- attempts, approvals, rejections, and peak extra memory;
- compatibility mode and whether a fixed point was reached.

Agas artifact selection should account for actual size, not just state count.

## 12. Tests before deployment

Construction tests:

- Shared core and disjoint lookahead; differences between modes.
- R/R conflict for one full word, but no conflict between `[a,b]` and `[a,c]`.
- Shifts to different targets that can be merged.
- Conflicts appearing only in a successor or after several transitions.
- Transition cycles and merging groups of more than two members.
- Full rollback after an unsuccessful attempt.
- EOF, short end-of-input windows, and empty productions.
- Deterministic results, limits, and fallback.
- Independent certificate and rebuilt-row validation.

Parser tests:

- Comparison against canonical LR(1), LR(2), and LR(3).
- An LR(1) grammar that is not LALR(1), and an LALR(1) grammar.
- Exhaustive short inputs including nonmembers, and larger seeded samples.
- Acceptance and reduction `RuleId` comparison on valid inputs.
- In `ExactActions`, also error timing and full trace agreement after state
  mapping; in union mode, separately check invalid-run termination.
- Agas and CMinus grammars requiring `k>1`, measuring artifact sizes.

Denny and Malloy's first example:

```text
S -> a A a
S -> b A b
A -> a
A -> a a
```

Use it to compare LR(1) with LR(2) and later test conflict policies.
The valid words are exactly `aaa`, `aaaa`, `bab`, and `baab`. The first
merging version rejects its conflicting LR(1) table rather than forcing
a reduction. For LR(2), check preservation of all four words.

Figures 2–4 of the paper describe ambiguous grammars. Increasing `k` does
not remove ambiguity; initially test their rejection by the merging API.
Only a conflict-policy extension can reproduce their behavior described in the paper.

## 13. Integration and work order

Proposed working component name: `SelectiveLRkMerger`, with explicit
`MergeMode`. Do not add an overload implying the result is LALR(k) or
published IELR(k).

Reuse existing `ItemCore`, `LookaheadWord`, `WordSetK`, `buildActionRow()`,
and `LALRkDfa`'s `canonicalOrigins` convention. Do not equate
`mergeConflicts()` results with a full certificate: absence of new conflicts
does not check every invariant.

Implementation order:

1. Partition model, state mapping, and independent validator.
2. `ExactActions`, table materialization, and a stepwise-equivalence test.
3. Size reporting, compression integration, and explicit fallback.
4. Experimental `CompatibleUnion`, transactions, and required justification.
5. LR(2)/LR(3) measurements and possible candidate-order improvements.
6. Separately, published IELR(1), conflict policies, and IELR(k) research.

The greedy algorithm does not guarantee optimal grouping in union mode.
Several stable candidate orders can later be tried, choosing the smallest
validated artifact. Correctness criteria remain unchanged.

## 14. Source and relationship to IELR

Joel E. Denny, Brian A. Malloy, *The IELR(1) algorithm for generating minimal
LR(1) parser tables for non-LR(1) grammars with conflict resolution*,
Science of Computer Programming 75 (2010), 943–979,
DOI: `10.1016/j.scico.2009.08.001`.

The text copy read was
`1-s2.0-S0167642309001191-main.txt` (a local download outside the repository).
Figures were read from `/tmp/ie_wybrane`; this is a temporary location,
not a permanent repository resource.

Relevant sections: 2.6 on limitations of Pager compatibility checks,
3.1 on constructing IELR from LALR, 3.5.3 on conflict-policy stability,
3.8 on suboptimal merging, and 6 on the proposed IELR(k) generalization.

Published IELR(1) does not require preserving errors in the same cells as
canonical LR(1). This document's strict mode imposes a stronger requirement.
Conversely, simple conflict-free union is not the full IELR criterion for
grammars with conflict resolution. Keep these three concepts separate
in documentation, code, and reports.

## 15. First experiment on Ag.ag

Implementation: `src/lr/SelectiveLRkMerger.{h,cpp}`. Both modes, transactional
merge closure, full unsuccessful-attempt rollback, an attempt limit,
and independent final-partition checks are available. The result owns
its graph and origin mapping without a canonical automaton copy. The
validator rebuilds ACTION from items and compares it with canonical-row unions.

After the first measurement, the target order was added: LALR(k) first,
selective merging only when LALR conflicts. Ag.ag therefore selects
conflict-free LALR(2), with `usedLalr=true` and zero attempts. Table size
results remain unchanged. The 21 transactions and timings below describe
an earlier measurement without this shortcut, not the current selection path.

The first version uses full partition copies, full compatibility checks,
and stable pair order by state number. Result numbering follows the
smallest origin-state number, not BFS. Not all proposed memory/time limits
or automatic smaller-artifact selection are implemented. Certificate
mismatch aborts the experiment with an exception. Agas's normal generator
remains canonical.

A separate program measures this without changing default export:

```bash
cmake -S agas-cpp -B agas-cpp/cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build agas-cpp/cmake-build-release -j 4
agas-cpp/cmake-build-release/agas-merge-report
```

Run these commands from a directory containing `agas-cpp`. An optional
argument selects another `.ag` file; `k` comes from its options. LALR(k)
is built and reported even when conflicting: its state count shows the
number of core groups. Conflicting tables are neither compressed nor
treated as executable deterministic parsers.

Result for current `agas-cpp/grammars/Ag.ag`, `k=2`:

| Construction | States | Items | Transitions | Raw table, B | Compressed, B | DSL, B |
|---|---:|---:|---:|---:|---:|---:|
| Canonical LR(2) | 206 | 23846 | 294 | 280156 | 33832 | 45322 |
| LALR(2) | 130 | 12283 | 184 | 146060 | 23576 | 31619 |
| ExactActions | 206 | 23846 | 294 | 280156 | 33832 | 45322 |
| CompatibleUnion | 130 | 12283 | 184 | 146060 | 23576 | 31617 |

All four tables are conflict-free. In this measurement Ag.ag is therefore
LALR(2): selective merging reaches the core lower bound but has no advantage
over LALR(2). A separate grammar with conflicting LALR tests rejection of
unsafe merges while retaining other merges.

Gains over canonical: about 36.9% of states, 47.9% of raw-table bytes,
and 30.3% after compression. Table bytes estimate packed storage according
to `TableStorageStats`, not C++ container memory. DSL size is the actual
measured string length. The experimental graph currently enters general
`ParseTable(LRkDfa)`, so the string uses family `LR`, not a separate selective
construction marker. The 2 B difference from LALR comes from the header
name, not an additional structural gain. This measures format size; it
does not enable experimental export in the normal CLI.

Example Release timings: canonical construction about 20 ms, LALR alone
about 2 ms, selective union with reference-table construction and validation
about 30 ms after canonical construction. These are individual local
measurements, not a stable benchmark. Report compression is excluded.
Liberal mode performed 21 successful transactions, some joining several
groups; strict mode rejected 122 attempts and did not reduce the automaton.

The integration test builds a parser from Ag.ag and analyzes 11 real `.ag`
files from the repository. Tokens come from the bootstrap ANTLR lexer,
requiring no further work on the custom lexer. It also performed 2200
deterministic mutations: token deletion, insertion, replacement, and input
truncation, with seed 20260920.

Both parsers rejected 1839 mutations. The others remained valid; the test
does not assume every change makes a sentence invalid. No differences
in acceptance or valid-analysis reductions were found. The largest
observed error delay was 1 consumed token, a corpus result rather than
a universal bound. Raw tables were compared; compressed-table default
reductions may affect diagnostics separately.

The test interpreter checks popped-handle symbols, GOTO, accept conditions,
and a step budget. Valid files were also checked by production `LRMachine`.
Microgrammar tests cover `k=1,2,3`, nullable cases, genuine LR(2), successor
conflicts, rollback, partial merging, attempt limits, and damaged certificates.
The full Żbik Debug suite had 319 tests and Agas Release 13 tests, all passing.
This is still neither a full proof nor the completed logical-construct
diagnostics of 9.9.

## 16. Żubr corpus examples with conflicting LALR

Source: `zubr-kit/src/main/resources/grammars.dat` in a separate checkout,
labels near lines 225 and 240 in the version read. Productions were embedded
in `ComparesSmallReferenceCorpusExamples` so the test does not depend on
a private neighboring repository. Labels were verified by automaton
construction rather than treated as a decisive oracle.

The first example is labeled LR(2), non-LALR(2) in the corpus:

```text
X -> Y
X -> b Y a
Y -> c
Y -> c a
```

| k | Canonical: states / conflicts | LALR: states / conflicts | Selective: states / conflicts | Compressed bytes: before → after |
|---|---|---|---|---|
| 1 | 10 / 1 | 8 / 1 | Input rejected | — |
| 2 | 10 / 0 | 8 / 1 | 9 / 0 | 720 → 704 |
| 3 | 10 / 0 | 8 / 1 | 9 / 0 | 772 → 756 |

This demonstrates strict inequality
`|LALR(k)| < |selective LR(k)| < |LR(k)|`. The algorithm preserves a necessary
distinction while merging another compatible pair. State reduction does
not yield proportional byte savings here, since canonical-table compression
already shares some data.

The second example is the classic LR(1), non-LALR(1) grammar:

```text
S -> a A d
S -> b B d
S -> a B e
S -> b A e
A -> c
B -> c
```

| k | Canonical: states / conflicts | LALR: states / conflicts | Selective: states / conflicts | Compressed bytes: before → after |
|---|---|---|---|---|
| 1 | 14 / 0 | 13 / 2 | 14 / 0 | 992 → 992 |
| 2 | 14 / 0 | 13 / 2 | 14 / 0 | 1096 → 1096 |
| 3 | 14 / 0 | 13 / 2 | 14 / 0 | 1192 → 1192 |

There is no gain: the only core merge introduces a conflict, so all states
are retained. This is a valid experimental outcome, not a reason to weaken
compatibility conditions.

For every conflict-free case, all words up to length 5 were compared with
the canonical parser, including valid-analysis reductions. An independent
derivation generator also compares the grammar's language with the result
table over the same bounded range. Checks cover conflict freedom, expected
state counts, and rejection of the conflicting LR(1) input table.

In `CompatibleUnion`, without budget exhaustion, conflict-free full LALR(k)
should allow all core groups to be reached: every partial action union
is contained in the final compatible union after targets are identified
appropriately. This justification excludes `ExactActions` and attempts
interrupted by a limit. When LALR conflicts, selective group count depends
on safe merges and their order; no global minimum is promised.
