# Żbik — pomiary wydajności

Ten dokument przechowuje wyniki, które uzasadniają decyzje optymalizacyjne.
Surowe pliki `perf.data` i generowane pliki korpusu pozostają poza Git; tutaj
zapisujemy warunki pomiaru, najważniejsze liczby i wnioski.

## Pomiar bazowy przed optymalizacją słów lookaheadu — 2026-09-19

### Warunki

- kompilator: GCC 15.2.0;
- korpus: `res/grammars.dat`, 69 gramatyk;
- parametry: `--max-lr=6 --report`;
- poprawność: 0 nieoczekiwanych błędów, 277 testów przechodzących w
  `RelWithDebInfo`;
- pomiar czasu: build `Release`, jeden przebieg rozgrzewkowy i pięć
  mierzonych przebiegów;
- profil funkcji: build `RelWithDebInfo`, `cycles:u`, częstotliwość 999 Hz,
  stosy DWARF;
- profil zawierał około 1000 próbek, około 10,59 miliarda cykli i nie utracił
  żadnej próbki.

Polecenie profilujące:

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

Raport tekstowy utworzono poleceniem:

```bash
perf report \
  -i /tmp/zbik-perf.data \
  --stdio \
  --sort overhead,symbol \
  > /tmp/zbik-perf-functions.txt
```

Surowe dane miały około 16 MB, a raport tekstowy około 1,6 MB.

### Czas całego korpusu w Release

Pierwszy przebieg rozgrzewkowy trwał 2,100 s. Pięć kolejnych przebiegów:

| Przebieg | Czas całkowity |
|---:|---:|
| 1 | 2,053 s |
| 2 | 2,053 s |
| 3 | 2,135 s |
| 4 | 2,043 s |
| 5 | 2,153 s |

Mediana wynosi 2,053 s, a zakres 2,043–2,153 s. Dla gramatyki 66 mediana
samego etapu `LR(6)` wynosi 1,779 s. Ta jedna sztuczna gramatyka dominuje
więc czas całego korpusu.

### Najdroższe symbole według `perf`

| Symbol lub operacja | Self |
|---|---:|
| `LookaheadWord::LookaheadWord(vector<LookaheadSymbol>)` | 42,33% |
| `WordSetK::add` | 9,44% |
| `concatenateTruncated` | 9,23% |
| `malloc` | 8,82% |
| `free` | 4,76% |
| konstruktor `Item` | 3,10% |
| wstawianie `Item` do `std::set` | 2,49% |

Koszt przypisany konstruktorowi `LookaheadWord` dzieli się prawie równo
między budowę automatu LR(k), około 21,5%, oraz budowę `ParseTable`, około
20,6%. Stosy prowadzą przez `FirstKAnalysis::first` i
`concatenateTruncated`. Osobne koszty `malloc` i `free` potwierdzają, że
częste tworzenie krótkich wektorów symboli jest rzeczywistym kandydatem do
optymalizacji. Trie ACTION, hashowanie i internowanie stanów nie dominują w
tym profilu.

### Wniosek i decyzja

Naturalnym kandydatem jest `SmallWord`: krótkie słowo przechowywane inline,
z możliwością przejścia na pamięć dynamiczną dla większego `k`. Liczba
terminali gramatyki nie wyznacza długości pojedynczego słowa; tę ogranicza
`k`. Duży alfabet może jednak zwiększyć liczbę różnych słów.

Nie wdrażamy jeszcze tej reprezentacji. Stały bufor inline powiększa każdy
`LookaheadWord` i każdy zawierający go `Item`, co może zaszkodzić dużym
realistycznym automatom LR(1), mimo zysku dla obecnej gramatyki LR(6).
Optymalizacje reprezentacji słów, cache FIRST(k), trie i internowania stanów
pozostają odroczone do pomiarów po etapie 8.5 na gramatykach:

- `cmm`;
- `cminus`;
- `CDeclaratorSubset`;
- późniejsze gramatyki docelowe mające ponad setkę terminali.

Po ich dodaniu należy powtórzyć identyczny pomiar przed i po każdej zmianie.
Optymalizacja jest akceptowana tylko wtedy, gdy zachowuje dumpy, konflikty i
akceptowany język oraz daje powtarzalny zysk bez istotnego pogorszenia LR(1).

Procentów poniżej około 1% nie należy interpretować stanowczo przy około
1000 próbek. Dominujący koszt 42,33% jest wystarczająco duży, by zachować
`SmallWord` jako pierwszego kandydata do następnego profilowania.
