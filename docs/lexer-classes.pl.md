# API klas lexera sterowanych regułami parsera

Pierwsza implementacja jest w `zbik_core`. Klasy mają maskę `uint64_t`.
Nie powstaje lexer dla każdej kombinacji bitów: jeden wspólny DFA zachowuje
wszystkie reguły akceptujące w stanie i ich zależności od klas.

## Reguły lexera

Trzecie pole `LexerRule` to `requiredClasses`. Domyślne `0` oznacza zawsze
aktywną regułę; przy masce wielobitowej wymagane są wszystkie wskazane bity.
Przykład fragmentu definicji, przy wcześniej zadeklarowanych ID terminali:

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

`ByteLexer` i `Utf8Lexer` udostępniają `next(source, byteOffset, activeMask)`.
Metoda przesuwa offset za token, pomija aktywne reguły bez terminala i
zwraca `nullopt` przy EOF. `tokenize(source, activeMask)` zachowuje interfejs
wsadowy; brak maski oznacza `0`. Reguły bez klas działają jak poprzednio.
Najdłuższe aktywne dopasowanie wygrywa, a przy remisie decyduje kolejność
reguł. Wyłączenie `READ` nie usuwa możliwości dopasowania `IDENT`; wyłączenie
`SHR` pozostawia krótsze `GT`.

## Reguły parsera i dziedziczenie

`ParserLexerClass` przypisuje nieterminalowi maski `enabled` i `disabled`.
Dotyczy to wszystkich jego alternatyw i potomków, do chwili przesłonięcia
przez deklarację zagnieżdżoną:

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

Reguła dziedziczy maskę, po czym oblicza
`(inherited | enabled) & ~disabled`. Po powrocie obowiązuje kontekst
wywołującego. Sprzeczne włączenie i wyłączenie tego samego bitu w deklaracji
jest błędem. Opcjonalny argument `initial` funkcji `scopeLexerClasses`
ustala maskę początkową. Można więc włączyć `SHIFT` globalnie i wyłączyć
go w `type`, albo włączać go wyłącznie w `expression`.

Jeśli `GT` też ma być początkowo nieaktywny, można nadać jego regule osobny
bit. Wyrażenia Vist potrzebują zwykle zarówno `GT`, jak i `SHR`, ponieważ
`GT` służy również do porównania `a > b`.

## Budowanie kontekstu LR(k)

`scopeLexerClasses` specjalizuje osiągalne pary `(nieterminal, maska)`.
Powrót do maski otaczającej jest zapisany w strukturze gramatyki, bez akcji
`push`/`pop` lexera. Rekurencja wraca do już utworzonej pary.
Nie enumerujemy wszystkich masek; liczba faktycznie osiągalnych par może
jednak być duża. Domyślny limit wynosi 4096 i jego przekroczenie daje błąd.
Wynik zawiera mapowania terminali, nieterminali i produkcji na oryginały.

Przeciążenie przyjmujące lexer analizuje wspólny DFA. Dla każdego terminala
wyznacza bity, które mogą wpływać na dopasowanie lub jego przedłużenie.
Na przykład `SHR` wpływa na `GT`, ale nie na `LT`. Tylko te istotne bity
różnicują warianty terminala w gramatyce LR. Bez argumentu lexera API stosuje
konserwatywne, pełne maski zakresów.

`LexerContextPlan` buduje drzewa prefiksów słów ACTION. Węzeł odpowiada
parze `(stan, dotychczasowy prefiks podglądu)`. Wymagania następnych tokenów
wyznaczają aktywną maskę. Sprzeczność wymagań daje `LexerContextError` ze
stanem, prefiksem, maską sprzecznych bitów i nazwami terminali. Walidacja
lexera wykrywa też oczekiwany token bez aktywnej reguły leksykalnej.

`ContextualLRMachine` pobiera tokeny bez wcześniejszego tokenizowania całego
pliku. Drugi token podglądu LR(2) dostaje maskę zależną od pierwszego, przed
pierwszym przesunięciem parsera. Obecna wersja ponownie rozpoznaje niezużyty
podgląd po akcjach i sprawdza zgodność z buforem. Nie pozwala, by zmiana
kontekstu po cichu zmieniła token uzasadniający wcześniejszą redukcję.
Wynik zawiera tylko zużyte tokeny, błąd składni lub sukces oraz opcjonalny
ślad żądań lexera. Błędy leksykalne mają dotychczasowe typy wyjątków.

## Sprawdzone przypadki i granice obecnego zakresu

- Maski dla `SHR`, wspólnej klasy `READ`/`WRITE` i ich kombinacji, LR(1)/LR(2).
- Zagnieżdżone włączenia i wyłączenia, klasa początkowa, rekurencyjne typy,
  produkcja pusta oraz powrót do klasy wyrażenia po zamknięciu typu.
- UTF-8, offsety bajtowe, bit 63, dłuższy `IDENT` mimo aktywnego słowa
  kluczowego i wybór krótszego tokenu po wyłączeniu `SHR`.
- Sprzeczne deklaracje, niezgodne konteksty drugiego tokenu podglądu,
  brak włączenia wymaganej klasy oraz limit osiągalnych specjalizacji.

Implementacja nie obejmuje jeszcze składni `.ag`, eksportu klas do
artefaktu ani runtime Rust. Diagnostyka nie generuje jeszcze tekstowego
świadka kolizji regexów ani pozycji w `.ag`; mapowania do oryginałów są
dostępne dla frontendu. Analiza zależności regexów jest konserwatywna i
może wymagać lepszego rozdzielenia kontekstów niż rozwiązanie z wieloma
tokenizacjami. Koszt dopasowania i generowania wymaga pomiarów na większych
gramatykach przed optymalizacją.

Samodzielne lexery obsługują maski także przy `skip`. Wariant sterowany
zakresem reguł parsera wymaga obecnie bezwarunkowych reguł `skip` i odrzuca
inne: własność pomijanych znaków na granicy zakresów i przy EOF wymaga
osobnego kontraktu. Przykład XML i nazwane kanały będą kolejnym etapem.
Przy leniwych regexach inkrementalny `Utf8Lexer::next` dekoduje pozostałe
wejście; optymalizacja tej ścieżki także pozostaje do wykonania.
