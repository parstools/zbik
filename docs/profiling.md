# Żbik — performance measurements

This document records results that justify optimization decisions.
Raw `perf.data` files and generated corpus files remain outside Git;
measurement conditions, key figures, and conclusions are recorded here.

## Baseline before lookahead word optimization — 2026-09-19

### Conditions

- Compiler: GCC 15.2.0.
- Corpus: `res/grammars.dat`, 69 grammars.
- Parameters: `--max-lr=6 --report`.
- Correctness: 0 unexpected errors, 277 tests passing in `RelWithDebInfo`.
- Timing: `Release` build, one warm-up run and five measured runs.
- Function profile: `RelWithDebInfo` build, `cycles:u`, 999 Hz, DWARF stacks.
- The profile contained about 1000 samples and 10.59 billion cycles, with no lost samples.

Profiling command (run from the repository root):

```bash
perf record \
  -o /tmp/zbik-perf.data \
  -e cycles:u \
  -F 999 \
  -g --call-graph dwarf \
  -- ./cmake-build-relwithdebinfo/zbik \
  res/grammars.dat \
  /tmp/zbik-perf-result.dat \
  --max-lr=6 \
  --report=/tmp/zbik-perf-report.txt
```

The text report was generated with:

```bash
perf report \
  -i /tmp/zbik-perf.data \
  --stdio \
  --sort overhead,symbol \
  > /tmp/zbik-perf-functions.txt
```

The raw data occupied about 16 MB and the text report about 1.6 MB.

### Whole-corpus time in Release

The warm-up run took 2.100 s. The next five runs were:

| Run | Total time |
|---:|---:|
| 1 | 2.053 s |
| 2 | 2.053 s |
| 3 | 2.135 s |
| 4 | 2.043 s |
| 5 | 2.153 s |

The median is 2.053 s, with a range of 2.043–2.153 s. For grammar 66,
the median time of the `LR(6)` stage alone is 1.779 s. This single synthetic
grammar therefore dominates the whole-corpus time.

### Most expensive symbols according to `perf`

| Symbol or operation | Self |
|---|---:|
| `LookaheadWord::LookaheadWord(vector<LookaheadSymbol>)` | 42.33% |
| `WordSetK::add` | 9.44% |
| `concatenateTruncated` | 9.23% |
| `malloc` | 8.82% |
| `free` | 4.76% |
| `Item` constructor | 3.10% |
| Inserting `Item` into `std::set` | 2.49% |

The cost attributed to the `LookaheadWord` constructor is split almost
equally between LR(k) automaton construction, about 21.5%, and `ParseTable`
construction, about 20.6%. The stacks pass through `FirstKAnalysis::first`
and `concatenateTruncated`. The separate `malloc` and `free` costs confirm
that frequent creation of short symbol vectors is a real optimization
candidate. ACTION tries, hashing, and state interning do not dominate this profile.

### Conclusion and decision

A natural candidate is `SmallWord`: a short word stored inline, with a
dynamic allocation fallback for larger `k`. The number of grammar terminals
does not determine the length of an individual word; `k` bounds it.
A large alphabet can, however, increase the number of distinct words.

This representation is not being implemented yet. A fixed inline buffer
enlarges every `LookaheadWord` and every `Item` containing one. That could
hurt large realistic LR(1) automata despite benefiting the current LR(6)
grammar. Word representation, FIRST(k) caching, tries, and state interning
optimizations remain deferred until measurements after stage 8.5 on:

- `cmm`;
- `cminus`;
- `CDeclaratorSubset`;
- later target grammars with more than a hundred terminals.

After adding these grammars, repeat the same measurement before and after
each change. An optimization is accepted only if it preserves dumps,
conflicts, and the accepted language, and gives a repeatable improvement
without materially degrading LR(1).

Percentages below about 1% should not be interpreted strongly with only
about 1000 samples. The dominant 42.33% cost is large enough to retain
`SmallWord` as the first candidate for the next profiling round.
