# Importing compressed tables

`CompressedTable.g4` describes the current output of
`CompressedParseTable::dumpDsl()`. Its entry rule is `document`, which
requires end of input. The grammar has no target-language-dependent actions.
The first importer can use ANTLR4; after bootstrapping Agas, this syntax
will be moved to `.ag`.

Example C++ parser generation (run from the repository root, with output outside sources):

```bash
antlr4 -Dlanguage=Cpp -visitor -no-listener -o /tmp/zbik-dsl-parser \
  grammar/CompressedTable.g4
```

The generated parser recognizes structure only. The importer should then:

- Reject the document after any lexical or syntax error, even if ANTLR recovers and continues parsing.
- Decode strings according to JSON rules and check the parser name:
  `SLR` means k=1; `LR(k)` and `LALR(k)` require positive k.
- Check numeric ranges before conversion to C++ types.
- Check row-number uniqueness and existence of every reference.
- Check equal lengths of the two state mappings, the initial-state range,
  and shift/GOTO targets; an index in a mapping is a state number.
- Check symbols and rule numbers against the separately supplied grammar
  (rules are numbered from zero; table export does not contain productions).
- Check for duplicate keys, lookahead lengths, and EOF position:
  a shorter word must end at EOF; accept applies only to EOF.

The grammar requires exactly one default action at the end of each ACTION
row. Empty GOTO rows are allowed; a missing entry means no transition.

Unquoted `EOF` means end of input, while `"EOF"` and `"$"` are ordinary
terminals. Examples: `[EOF]`, `["i", EOF]`, `["EOF", EOF]`. The importer
distinguishes these cases through the `lookaheadSymbol` alternative before
decoding strings. In JSON export, EOF is represented by `{"eof": true}`,
while a terminal remains a string such as `"EOF"` or `"$"`. Default reductions
may also delay error detection; the importer must reproduce this semantics.

Adding the `.g4` grammar does not connect ANTLR to the build or Żbik's `main`.
