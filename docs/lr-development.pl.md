# Żbik: gramatyka, LR(k) i analiza leksykalna

## Cel dokumentu

Żbik jest drugim podejściem do kodu z C++ `zubr/z_javy`: ma zachować potrzebną funkcjonalność, ale z prostszym modelem gramatyki, jasno określoną własnością obiektów i mniejszą liczbą zależnych od siebie mechanizmów. `z_javy` jest wcześniejszym tłumaczeniem projektu Java `zubr-kit`. Docelowy zakres Żbika to budowanie **DFA stanów LR(k)**, tablic parsera i uruchamianie parserów **LR(k)**. Projekt nie będzie implementował LL ani przekształceń gramatyk na potrzeby LL. Zachowa natomiast FIRST/FOLLOW jako analizy gramatyki oraz generator ograniczonych słów i drzew wyprowadzeń znany z `testAmbig`.

Bezpośrednim punktem odniesienia jest aktualny kod C++ `zubr/z_javy`, zsynchronizowany z `zubr-kit` do rewizji `47ded90` z 2026-09-17. To z niego należy przenosić działające algorytmy, testy i dane. Oryginalna Java pozostaje upstreamem i dodatkowym oracle, gdy trzeba wyjaśnić niepewność translacji. Nie należy jednak kopiować `z_javy` klasa po klasie: zachowuje ono znaczną część architektury starszego projektu, powstałej także dla LL(k), która jest niepotrzebnie ciężka dla Żbika.

## Historyczny punkt wyjścia Żbika

Poniższy opis i tabela porównawcza dokumentują szkielet sprzed etapów 0–2.4.
Aktualny `Grammar` przechowuje wartości i typowane ID, ma `GrammarBuilder`
oraz `LRGrammarView`; działają NULLABLE, FIRST(1), FOLLOW(1) i `WordSetK`.
Aktualny stan wykonania jest utrzymywany w historii zmian projektu.

Najważniejsza zmiana względem `z_javy` jest już widoczna w modelu danych:

- `Grammar` jest właścicielem reguł, terminali i nieterminali przez `std::unique_ptr`;
- `Rule` oraz prawa strona reguły używają surowych wskaźników wyłącznie jako nieposiadających referencji;
- terminale i nieterminale otrzymują stałe, gęste identyfikatory;
- gramatyka jest czytana w dwóch przebiegach: najpierw wszystkie lewe strony, potem symbole prawych stron;
- reguły mają jeden globalny wektor, a nieterminale przechowują tylko widok własnych reguł;
- `DynamicBitset`, `TerminalSet` i `SetContainer` są jedynie rozpoczętym szkicem FIRST/FOLLOW(1), a nie rozpoczętym parserem LL. Można je usunąć lub zastąpić, gdy powstanie właściwe FIRST(k) dla LR(k).

Ten kierunek jest dobry. W `z_javy` klasy `Grammar`, `Rule`, `Symbol` i `Nonterminal` nadal zawierają przeniesioną logikę transformacji gramatyki, wyliczania długości, wykrywania cykli, generowania nazw, losowania i przygotowania LL. W Żbiku model powinien być przede wszystkim niezmiennym opisem gramatyki, a algorytmy powinny znajdować się w osobnych modułach.

## Porównanie modeli gramatyki

| Obszar | C++ `z_javy` (port `zubr-kit`) | Obecny Żbik | Kierunek docelowy |
|---|---|---|---|
| Własność | obiekty C++ połączone wskaźnikami i mutowane przez algorytmy | `unique_ptr` w `Grammar`, wskaźniki jako widoki | zachować model Żbika |
| Identyfikacja symbolu | indeks wyszukiwany w wektorze, często używana tożsamość obiektu | gęsty `id` terminala lub nieterminala | w algorytmach używać identyfikatorów, nie adresów |
| Identyfikacja reguły | tożsamość obiektu; globalny numer tworzony przez parser LR | brak jawnego `RuleId` | nadać każdej regule stały `RuleId` podczas budowy gramatyki |
| Reguła startowa | `addStartNt()` tworzy ukryte `S′ -> S` i mutuje gramatykę | brak reguły rozszerzonej | tworzyć ją w `LRGrammarView`, bez mutowania gramatyki użytkownika |
| Mutowalność | eliminacja rekurencji, faktoryzacja, dodawanie symboli | po konstrukcji praktycznie brak zmian | formalnie zamrozić gramatykę po budowie |
| FIRST/FOLLOW | wspólna rozbudowana infrastruktura LL(k) | niedokończone FIRST/FOLLOW(1) na bitsetach | zbudować osobny moduł `FirstFollowK`; LR(k) zależy od FIRST(k), lecz nie od FOLLOW(k) |
| Haszowanie | mieszanka tożsamości i własnych hashy 32/64-bitowych | brak modelu hashy LR | haszować małe wartości: `RuleId`, pozycję kropki i słowo lookahead |
| Duplikaty reguł | zachowane; muszą powodować konflikt reduce/reduce | globalny wektor także je zachowuje | nie deduplikować reguł |

### Czego z `z_javy` nie należy przenosić

Żbik bez LL nie potrzebuje:

- eliminacji rekurencji lewostronnej;
- faktoryzacji gramatyki;
- tabel LL i algorytmów wyboru produkcji LL; same FIRST/FOLLOW pozostają wartościową analizą gramatyki;
- mutowalnych pól `minLen` i `maxLen` w samych symbolach; potrzebne długości minimalne należy wyliczać w osobnej analizie dla generatora;
- transformacji grafowych służących eliminacji lewej rekurencji i faktoryzacji LL; analizy grafowe cykli, produktywności i długości należy zachować;
- trójwarstwowego `TokenSet` (`BUILD`, `DONE`, `EOF`) jako publicznego fundamentu całego projektu;
- klasyfikacji LL i etykiet `[LL(k)]`.

Rekurencja lewostronna jest naturalna dla LR i nie należy jej usuwać. Parser LR powinien pracować na oryginalnej gramatyce użytkownika.

### Co trzeba zachować z `z_javy`

Należy odtworzyć zachowanie, a nie układ klas:

- rozszerzoną produkcję startową oraz akceptację tylko na EOF;
- tożsamość produkcji, także dla dwóch tekstowo identycznych reguł;
- domknięcie i `GOTO` dla itemów LR;
- kanoniczne kolekcje stanów porównywane pełną zawartością, nie samym hashem;
- dokładne słowa wyprzedzenia LR(k), kończące się po `k` terminalach albo wcześniej na EOF;
- wszystkie konkurujące akcje w komórce ACTION, bez arbitralnego rozstrzygania konfliktu;
- rozrzedzoną tabelę ACTION, bez budowania iloczynu całego alfabetu do potęgi `k`;
- łączenie stanów o tym samym rdzeniu dla LALR(k), jeżeli LALR(k) pozostaje w zakresie projektu;
- deterministyczny recognizer do testowania wygenerowanych tablic.

## Problemy historycznego szkieletu Żbika

Lista opisuje stan przed przebudową, nie bieżące usterki. Punkty 1–9 zostały
objęte etapami 0–2.4; synchronizację korpusu należy weryfikować osobno.

Projekt się kompiluje, ale nie ma testów (`ctest` zgłasza „No tests were found”). Przed implementacją LR trzeba poprawić fundamenty.

1. `SetContainer` nie inicjalizuje `firstSets` ani `followSets`. Pierwsze wywołanie `makeFirstSets1()` odwoła się do pustego wektora. Ten kod nie oznacza, że LL zostało rozpoczęte; jest tylko szkicem obliczania zbiorów i nie ma testów ani tabeli LL.
2. `TerminalSet::unionWith()` nie przenosi znacznika końca wejścia, mimo że operatory bitowe go uwzględniają. To jest niespójny kontrakt.
3. `SetContainer` nadal jest portem koncepcji FIRST/FOLLOW dla LL(1), mimo że docelowo projekt nie ma zawierać LL.
4. Typy są podzielone pomiędzy przestrzeń `base`, przestrzeń `set` i przestrzeń globalną. `Grammar` i `Rule` są w `base`, ale `Symbol`, `Terminal`, `Nonterminal`, `SetContainer` i `DynamicBitset` częściowo pozostają globalne.
5. Pole `isTterminal` zawiera literówkę i koduje rodzaj symbolu wartością logiczną. Czytelniejszy będzie `SymbolKind` albo osobne typowane identyfikatory.
6. `Rule` nie ma stałego identyfikatora. Adres reguły jest stabilny, lecz nie powinien być formatem tabel, diagnostyki ani serializacji.
7. Brakuje metod odczytu po nazwie i identyfikatorze oraz jawnego wskazania symbolu startowego.
8. Konstruktor przyjmuje tylko linie reguł. Obsługę komentarzy, pustych bloków i pliku `grammars.dat` trzeba oddzielić od parsera pojedynczej gramatyki.
9. `CMakeLists.txt` wymienia `src/main.cpp` dwa razy, buduje wszystko jako jeden executable i nie tworzy biblioteki, do której można podłączyć testy.
10. Korpus `zbik/res/grammars.dat` ma 492 linie, a aktualny `zubr/z_javy/res/grammars.dat` 500. W `z_javy` doszła gramatyka wymagająca LALR(2) oraz etykiety klasyfikacyjne przeniesione z upstreamu. Żbik powinien zsynchronizować przykłady LR, lecz może ignorować lub usunąć etykiety LL.

## Proponowany model docelowy

### Identyfikatory i niezmienna gramatyka

Warto wprowadzić małe typy wartościowe:

```cpp
struct TerminalId { uint32_t value; };
struct NonterminalId { uint32_t value; };
struct RuleId { uint32_t value; };
```

`SymbolRef` może być parą `SymbolKind + uint32_t` albo `std::variant<TerminalId, NonterminalId>`. Prawa strona reguły powinna być `std::vector<SymbolRef>`. Po zakończeniu budowy wszystkie identyfikatory i wektory pozostają stałe.

Klasy `Terminal` i `Nonterminal` mogą zostać jako cienkie deskryptory nazwy i ID, ale polimorfizm nie daje tu korzyści. Docelowo prostszy będzie jeden magazyn nazw terminali, jeden magazyn nazw nieterminali i reguły operujące na `SymbolRef`.

`GrammarBuilder` powinien odpowiadać za parsowanie tekstu i walidację, a wynikowy `Grammar` być niezmienny. Walidacja powinna obejmować:

- niepusty zbiór nieterminali;
- istniejący symbol startowy;
- brak pustych nazw;
- poprawną strzałkę i tokenizację;
- zachowanie reguł pustych;
- zachowanie duplikatów produkcji;
- spójność identyfikatorów i zakresów.

### Widok gramatyki rozszerzonej

Algorytm LR potrzebuje `S′ -> S`, ale nie powinien dopisywać go do gramatyki użytkownika. `LRGrammarView` może udostępniać syntetyczny `RuleId = ruleCount()` i syntetyczny `NonterminalId = nonterminalCount()`. Dzięki temu:

- zbudowanie kilku parserów dla tej samej gramatyki niczego nie mutuje;
- cache FIRST(k) pozostaje ważny;
- numeracja reguł w akcjach reduce jest stabilna;
- reguła 0 może być zarezerwowana dla produkcji rozszerzonej tylko w warstwie tablicy.

### FIRST(k) i FOLLOW(k) bez infrastruktury LL

LR(k) potrzebuje FIRST(k), ale nie potrzebuje FOLLOW ani tabel LL. FOLLOW(k) warto mimo to zachować jako samodzielną analizę, pomoc w testach i ewentualną podstawę SLR. Oba algorytmy mogą współdzielić najważniejszą abstrakcję:

```text
LookaheadWord = sekwencja TerminalId/EOF o długości 0..k
WordSetK      = rozrzedzony, uporządkowany zbiór tylko rzeczywiście istniejących słów
```

Potrzebne operacje:

- suma zbiorów ze zwrotem informacji, czy coś się zmieniło;
- konkatenacja języków z obcięciem do `k`;
- FIRST(k) terminala, epsilonu, prawej strony i sufiksu reguły;
- dołączenie odziedziczonego lookaheadu: `FIRST_k(βu)`;
- cache FIRST sufiksu pod kluczem `(RuleId, dotPosition, k)`, związany z konkretną niezmienną gramatyką; cache całego `FIRST_k(βu)` wymaga także uwzględnienia `u` i nie może przechowywać nieaktualnych wyników trwającego punktu stałego.

Nie trzeba od razu kopiować `Tier` i `Trie`. „Rozrzedzony” oznacza, że nie rezerwujemy tablicy o rozmiarze `liczba_terminali^k`. Przechowujemy wyłącznie słowa, które rzeczywiście powstały. Proponowana pierwsza implementacja:

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

`words_` jest odpowiednikiem `flat_set`: suma dwóch zbiorów to liniowe scalanie dwóch posortowanych wektorów, a wynik automatycznie usuwa duplikaty. Konkatenacja iteruje tylko po parach istniejących słów, skleja je i obcina po `k` tokenach lub po EOF, po czym sortuje i usuwa duplikaty. Dla zbiorów `A` i `B` koszt zależy od `|A| * |B|`, a nie od całego możliwego alfabetu.

Przykład dla `k=3`:

```text
A = { [], [id], [( id] }
B = { [+ id EOF], [) EOF] }

truncate_3(A · B) = {
    [+ id EOF], [) EOF],
    [id + id], [id ) EOF],
    [( id +], [( id )]
}
```

Puste słowo jest potrzebne podczas punktu stałego FIRST(k), ale nie wolno go umieszczać jako klucza ACTION ani lookaheadu gotowego itemu. Gotowe słowo lookahead ma długość `k` albo kończy się EOF wcześniej.

Dla typowych małych `k` narzut osobnego `std::vector` na każde słowo może być większy niż jego treść. Po uzyskaniu poprawności można wprowadzić własny `SmallWord`, np. 3–4 terminale przechowywane inline, a dłuższe w arenie. Inną dobrą optymalizacją jest internowanie:

```text
WordArena: LookaheadWord -> WordId
WordSetK:  posortowany vector<WordId>
```

Internowanie redukuje kopiowanie lookaheadów pomiędzy itemami. Nie powinno być jednak warunkiem pierwszej wersji. Trie ma największy sens w gotowej tabeli ACTION, gdzie wspólne prefiksy wielu słów rzeczywiście oszczędzają pamięć. Reprezentacja FIRST(k) i reprezentacja ACTION nie muszą być takie same.

EOF powinien być osobnym typem lub zarezerwowanym ID, a nie zwykłym terminalem użytkownika. Niezmiennik słowa: EOF może wystąpić tylko na końcu; słowo ma długość `k` albo kończy się EOF wcześniej.

## DFA i warstwa LR(k)

Proponowany podział modułów:

```text
src/grammar/
    Grammar, GrammarBuilder, identyfikatory
src/first/
    LookaheadWord, WordSetK, FirstFollowK
src/lr/
    Item, State, LRkDfa
    Action, Conflict, ParseTable
    LRk, opcjonalnie LALRk
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

Jeden typ wystarczy dla każdego `k >= 1`:

```cpp
struct Item {
    RuleId rule;
    uint32_t dot;
    LookaheadWord lookahead;
};
```

Rdzeń itemu to tylko `(rule, dot)`. Pełna równość stanu kanonicznego obejmuje lookahead. Produkcja jest rozróżniana przez `RuleId`, nie przez treść, dzięki czemu identyczne reguły nadal mogą dać konflikt reduce/reduce.

### Domknięcie, GOTO i DFA stanów LR

Dla `[A -> α · B β, u]` domknięcie dodaje `[B -> · γ, v]` dla każdego `v` z `FIRST_k(βu)`. Algorytm powinien być iteracyjnym punktem stałym. `GOTO(I, X)` przesuwa kropkę we wszystkich itemach z symbolem `X`, po czym liczy domknięcie.

Kanoniczna kolekcja jest deterministycznym automatem skończonym:

```text
węzeł DFA       = domknięty zbiór itemów LR(k)
alfabet DFA     = terminale i nieterminale gramatyki
przejście I--X  = GOTO(I, X)
stan początkowy = closure({[S′ -> ·S, EOF]})
```

W kodzie powinien istnieć jawny typ `LRkDfa`, niezależny od tablicy ACTION/GOTO. Dzięki temu można osobno testować itemy, przejścia, osiągalność, liczbę stanów, eksport grafu i scalanie rdzeni LALR(k). Dopiero kolejna warstwa przekłada DFA na tablicę parsera.

Sam parser LR nie jest jednak zwykłym DFA. Wykonanie tablicy ACTION/GOTO korzysta ze stosu stanów, więc runtime jest deterministycznym automatem ze stosem. Należy rozdzielić:

```text
LRkDfa       — skończony graf kanonicznych zbiorów itemów;
ParseTable   — ACTION/GOTO wyprowadzone z tego grafu;
LRMachine    — automat ze stosem wykonujący shift/reduce/accept.
```

Stan należy internować po pełnym zbiorze itemów. Hash służy tylko do znalezienia kandydatów; zawsze trzeba sprawdzić równość, ponieważ kolizja hasha nie może zgubić stanu.

### Inne traktowanie hashy

`z_javy`, zgodnie z historyczną implementacją, buduje osobne 32- i 64-bitowe hashe, a hash zbioru itemów łączy operacją XOR. Żbik powinien potraktować hash wyłącznie jako indeks pomocniczy, nigdy jako tożsamość obiektu.

Zasady:

- item jest równy tylko wtedy, gdy równe są `RuleId`, pozycja kropki i pełny lookahead;
- stan jest równy tylko wtedy, gdy równe są jego kanonicznie posortowane itemy;
- mapa internująca stan może być `unordered_map<StateKey, StateId, StateHasher>`, ponieważ sama mapa po kolizji wywołuje porównanie `operator==`;
- alternatywnie można użyć `unordered_map<Hash, vector<StateId>>`, a kubeł zawsze przeszukiwać strukturalnie;
- nie wolno używać `unordered_set<size_t>` z samymi hashami, bo kolizja usuwa poprawny stan;
- nie należy składać hasha stanu samym XOR: XOR jest przemienny, ale łatwo kasuje powtarzające się składniki i daje słabą dystrybucję;
- itemy w `StateKey` powinny być najpierw posortowane, a następnie haszowane kolejno algorytmem 64- lub 128-bitowym;
- `std::hash` jest detalem procesu i nie nadaje się na trwały identyfikator ani format pliku;
- trwały fingerprint gramatyki, jeśli będzie potrzebny do cache, należy liczyć osobno z nazw i uporządkowanych produkcji;
- hash można cache'ować wyłącznie dla niezmiennych obiektów.

W testach trzeba wstrzyknąć hasher zwracający stałą wartość. DFA i tabela muszą wtedy pozostać identyczne; pogorszyć może się tylko wydajność. To najprostszy dowód, że kod nie myli hasha z tożsamością.

### Tablica i konflikty

Każdy wiersz ma:

- `GOTO: NonterminalId -> StateId`;
- `ACTION: LookaheadWord -> set<Action>`.

Zbiór akcji jest istotny: podczas budowy nie wolno wybierać pierwszej akcji ani automatycznie preferować shift. `Conflict` powinien przechowywać stan, słowo lookahead i komplet akcji oraz rozpoznawać co najmniej shift/reduce i reduce/reduce.

Dla `k > 1` shift nie jest poprawnie opisany samym pierwszym terminalem. Akcja shift obowiązuje dla słów `FIRST_k(aβu)` wynikających z itemu `[A -> α · a β, u]`. To subtelny, ale kluczowy warunek obecny w implementacji `z_javy`.

### `LRMachine`: wykonanie tablicy ze stosem

Minimalny runtime powinien przyjmować listę nazw lub ID terminali, dopisywać EOF i wykonywać shift/reduce/accept. Jeśli tablica ma konflikty, recognizer powinien odmówić deterministycznego parsowania zamiast wybierać przypadkową akcję. Na początek nie jest potrzebne budowanie drzewa składniowego; wystarczy wynik `accepted/rejected` i opcjonalny ślad redukcji.

Różnicę między grafem stanów a maszyną widać na pojedynczej redukcji. Załóżmy, że stos ma postać `[q0, q5]`, a tabela nakazuje `reduce A -> a`. Maszyna usuwa `q5`, odczytuje odsłonięte `q0`, sprawdza statyczne przejście `GOTO(q0, A) = q2` i odkłada `q2`, uzyskując `[q0, q2]`. `LRkDfa` zawiera przejście `q0 --A--> q2`, ale nie przechowuje ani nie wykonuje stosu. Ten sam stan DFA może więc występować na wielu różnych stosach maszyny.

```text
LRkDfa:     Grammar -> graf zbiorów itemów -> ParseTable
LRMachine:  ParseTable + stos + wejście/lookahead -> shift/reduce/accept
```

Pierwszy obiekt jest skończony i statyczny. Konfiguracji drugiego jest potencjalnie nieskończenie wiele, ponieważ stos może rosnąć bez ustalonego ograniczenia.

### LALR(k)

Najprostsza i najbezpieczniejsza implementacja LALR(k) to:

1. zbudować kanoniczne LR(k);
2. pogrupować stany po rdzeniu `(RuleId, dot)`;
3. zsumować pełne itemy i ich lookaheady;
4. przemapować przejścia;
5. ponownie zbudować ACTION/GOTO i zaraportować konflikty powstałe po scaleniu.

Nie należy na początku implementować bezpośredniego propagowania lookaheadów z LR(0). Wersja przez scalanie jest prostsza do zweryfikowania, a optymalizację można dodać później.

## Generator słów i drzew wyprowadzeń

### Analizy gramatyki wymagające przeniesienia

Analizy cykli i długości z `z_javy` nie są wyłącznie infrastrukturą LL.
Należy przenieść ich funkcjonalność do osobnych analiz niezmiennej `Grammar`,
z wynikami indeksowanymi `NonterminalId` i `RuleId`:

| Źródło w `z_javy/src/grammar` | Odpowiednik w Żbiku |
|---|---|
| `computeMinLen()`, `checkMinLen()` | `MinYieldLength`, produktywność symboli i reguł oraz diagnostyka |
| `computeNonNullableCount()` | liczba nienullowalnych wystąpień w RHS, oparta na `Nullable` |
| `Cycle`, `Cycles`, `detectCycles()` | cykle bez postępu terminalnego, ze świadkami przez `RuleId` i pozycję RHS |
| `RecurCycle`, `RecurCycles`, `detectRecursion()` | analiza lewej rekurencji, także przez nullable prefiksy; bez eliminowania jej dla LR |
| `computeMaxLen()`, `Symbol::maxLen` | maksymalna długość wyprowadzanego słowa, odróżniająca brak słów, skończone maksimum i nieograniczoność |

`Cycle` i `RecurCycle` przechowują świadków; znaczenie analizy wynika z grafów
budowanych w `Grammar.cpp`, a nie z samych nazw klas. Stare `detectRecursion()`
bada pierwszy symbol RHS; nowa analiza powinna uwzględnić cały nullable prefiks.
W grafie cykli bez postępu krawędź `A -> B` przez konkretne wystąpienie `B`
wymaga, by wszystkie pozostałe symbole tej produkcji mogły zniknąć do epsilonu.
Dla lewej rekurencji w `A -> α B β` wystarczy nullable `α`, niezależnie od
`β`; dla cyklu bez postępu nullable muszą być zarówno `α`, jak i `β`.
W każdym grafie raportować wszystkie cykliczne SCC i po jednym świadku na
składową. Jednoelementowa SCC wymaga krawędzi do siebie. Świadek jest zamkniętym
ciągiem `(RuleId, pozycja RHS od zera)`; pozycja rozróżnia powtórzenia symbolu
w jednej produkcji, a `RuleId` rozróżnia identyczne tekstowo produkcje.
Wynik ma być deterministyczny dla ustalonej gramatyki i ID. Świadek nie
zastępuje zbioru wszystkich reguł cyklicznych. Wyliczanie wszystkich cykli
Johnsonem powinno być opcjonalne i ograniczone limitem.

Ta analiza została przesunięta w roadmapie do punktu 3.6, przed tablice i
generator. Przypadki testowe powinny objąć `CycleTest.cpp` oraz pięć gramatyk
z oryginalnego `zubr-kit/src/main/resources/trapGrammars.dat`. Ten plik zawiera
po separatorze `---` także historyczne oczekiwane tabele FIRST/FOLLOW, dlatego
do zasobu Żbika należy przenieść tylko części gramatyczne bloków.

Istotne rozróżnienie: `S -> S | a` jest poprawną składniowo, produktywną CFG,
ale ma cykl bez postępu i nieskończenie wiele drzew dla `a`, więc nie spełnia
warunków podstawowego generatora. Nie należy mylić tego z błędem buildera
ani zakazywać zwykłej lewej rekurencji `S -> S a | a`.
Produktywność, osiągalność strukturalną i użyteczność w pełnym wyprowadzeniu
terminalnym trzeba raportować osobno, bez mutowania wejścia. Użyteczność
wyznaczać od produktywnego startu tylko przez produktywne produkcje.
Przykład `S -> A Dead | b`, `A -> A | a`, `Dead -> Dead` ma osiągalne,
produktywne `A`, które jednak nie uczestniczy w żadnym pełnym wyprowadzeniu
od `S`. Cykl bez postępu w części użytecznej świadczy o nieskończenie wielu
drzewach pewnego słowa startowego; sam cykl strukturalny nie wystarcza.
`S -> S` nie ma żadnego skończonego drzewa.

Polityka generatora korzysta z tych wyników, a także odcina nieproduktywne
rozwinięcia. Budowa kanonicznego LR(k) nie wymaga zakazu cykli: domknięcie
kończy się dzięki deduplikacji itemów. Szczegółowy kontrakt i wymagane testy
(w tym porównanie produktywności z niepustością `FIRST₀`) opisuje punkt 3.6
w osobnej dokumentacji wdrożeniowej. Minimum i maksimum długości pozostają w 5.1.

Stare `computeMaxLen()` oznacza cykle w grafie zależności jako nieskończoność.
Nie przenosić tego jako dokładnego algorytmu: `S -> S | a` ma maksimum 1,
`S -> S | ε` maksimum 0, `S -> S a | a` nieograniczoną długość, a `S -> S`
nie wyprowadza żadnego słowa. Potrzebna jest analiza produktywności i cykli
pozwalających zwiększać długość w produktywnym kontekście. Nie używać jednej
wartości `int` dla nieznanego wyniku, pustego języka, nieskończoności i overflow.

### Enumeracja i limity

Generator dostępny już w C++ `z_javy` i używany przez `testAmbig` jest zbyt wartościowy, aby go usuwać. Dla zadanej górnej granicy długości rozwija drzewa wyprowadzeń, zwraca ciąg terminali oraz zapis drzewa i dzięki temu potrafi znaleźć dwa różne wyprowadzenia tego samego słowa. W Żbiku powinien być osobnym klientem niezmiennej `Grammar`, a nie częścią klas symboli.

Należy zachować model iteratora z Javy: `Generator(grammar, maxLen, ruleOrder)` udostępnia kolejne wyprowadzenia przez `next()`. `maxLen` określa okno, w którym generator próbuje kolejnych produkcji. Budżet przekazywany dziecku uwzględnia już minimalne długości zarezerwowane dla dalszych symboli bieżącej produkcji. Wyniki mogą mieć dowolną długość od zera do `maxLen`; nie należy tworzyć osobnego `generateExact(n)`. Klient zainteresowany dokładną długością filtruje już wygenerowane wyniki. Jest to szczególnie ważne dla gramatyk, których wszystkie wyprowadzenia są krótsze niż `n`: generator nie powinien wykonywać pełnego, bezowocnego wyszukiwania tylko po to, by spełnić filtr klienta.

Ważne własności:

- wynikiem jest zarówno `vector<TerminalId>`, jak i drzewo albo jednoznaczny zapis wyprowadzenia;
- nie wolno deduplikować wyników wyłącznie po słowie terminalnym, ponieważ wtedy zniknie dowód niejednoznaczności;
- kolejność rozwijania reguł ma być deterministyczna; losowe tasowanie może być później opcjonalną strategią wyszukiwania;
- minimalna długość słowa wyprowadzanego z każdego symbolu jest osobną analizą `MinYieldLength`, a nie mutowalnym polem `Grammar`;
- generator może zwracać to samo słowo wielokrotnie z różnymi drzewami; `testAmbig` rozpoznaje niejednoznaczność już po dwóch różnych świadkach;
- generator przyjmuje tylko gramatyki spełniające warunki wyznaczone przez analizę 3.6; w szczególności nie próbuje enumerować użytecznych cykli bez postępu jako „gramatyk pułapkowych”;
- odrzucenie wejścia wskazuje konkretny niespełniony warunek, zamiast maskować problem limitem głębokości albo globalnym deduplikowaniem konfiguracji;
- dla obsługiwanej gramatyki zakończenie `exhausted` oznacza wyczerpanie wszystkich wyprowadzeń o długości najwyżej `maxLen`, a `witness-found` wcześniejsze zakończenie wyszukiwania niejednoznaczności;
- `testAmbig(n)` może dostarczyć świadka niejednoznaczności, ale brak świadka do długości `n` nie jest dowodem jednoznaczności gramatyki.

`AmbiguitySearch` może przechowywać mapę `słowo -> pierwsze drzewo`. Drugie, strukturalnie różne drzewo dla tego samego słowa kończy wyszukiwanie i zwraca oba świadki. Porównanie powinno opierać się na `RuleId` i strukturze drzewa, nie na sformatowanym tekście.

Generator ma jeszcze dwa zastosowania testowe:

1. dostarcza krótkie słowa do porównania `LRMachine`; brak słowa jest dowodem odrzucenia tylko przy kompletnym przeszukaniu, a testy muszą obejmować również wejścia spoza języka;
2. pozwala empirycznie sprawdzać FIRST/FOLLOW na małych długościach, nie zastępując właściwego algorytmu punktu stałego.

Z C++ `z_javy` należy przenieść ideę oraz zachowanie klas `Generator`, `Node` i `RuleOrder` dla gramatyk spełniających warunki generatora, razem z istniejącymi testami niebędącymi testami pułapek. Model własności powinien być napisany od nowa przy użyciu ID i wartości albo `unique_ptr`; nie należy kopiować drzewa surowych wskaźników, wiązać generatora z mutowalnymi polami symboli ani przenosić wewnętrznego maskowania cykli jako zamiennika walidacji z punktu 3.6.

## Automaty dla wyrażeń regularnych

Żbik może także przejąć część `regex` i `lex` z C++ `z_javy`, ale jest to drugi, niezależny tor rozwoju. Nie należy mieszać automatu LR z automatem wyrażenia regularnego:

| Automat | Stan | Alfabet | Wykonanie |
|---|---|---|---|
| `LRkDfa` | zbiór itemów LR(k) | terminale i nieterminale gramatyki | służy do budowy tablicy |
| `LRMachine` | stan z tablicy plus stos stanów | tokeny wejściowe | deterministyczny automat ze stosem |
| `RegexNfa` | węzeł Thompsona | znaki/klasy znaków plus epsilon | wiele możliwych stanów |
| `RegexDfa` | zbiór stanów NFA | znaki/klasy znaków | zwykły DFA leksera |

W `z_javy` istnieją już:

- parser prostych wyrażeń regularnych do AST: alternacja, konkatenacja, literały, `*`, `+`, `?` i grupy;
- podstawowe `NFA`/`NFAState` z przejściami epsilon;
- prosty komparator NFA oraz testy parsera regex.

Nie ma natomiast kompletnego ciągu produkcyjnego: AST → NFA Thompsona → DFA przez konstrukcję podzbiorów → opcjonalna minimalizacja → matcher. Obecny port używa też surowych wskaźników w AST, nie zwalnia całego drzewa regex, a `NFA` ma ręczny destruktor przy domyślnych operacjach kopiowania, co grozi podwójnym zwolnieniem. Z `z_javy` warto przenieść składnię, testy i ogólny podział, lecz nie kopiować modelu własności.

Docelowo:

```text
RegexParser -> RegexAst (unique_ptr lub wariant wartościowy)
            -> ThompsonBuilder -> RegexNfa
            -> SubsetConstruction -> RegexDfa
            -> opcjonalnie DfaMinimizer
            -> Lexer/Matcher
```

Stany NFA i DFA powinny mieć gęste `StateId`, a przejścia przechowywać ID, nie wskaźniki. Konstrukcja podzbiorów internuje posortowany zbiór `NfaStateId`; obowiązują tu te same zasady hashy co dla LR: hash znajduje kubeł, pełny zbiór rozstrzyga równość.

Przed implementacją trzeba ustalić alfabet regex. Obecne AST przechowuje literał jako `std::string`, a NFA przejście jako `char`, więc API jest niespójne. Rozsądne warianty to:

- kodowe punkty Unicode lub bajty dla klasycznego leksera tekstowego;
- `CharacterClassId` dla zakresów i klas znaków;
- `LiteralId`/token dla regex używanego na poziomie symboli gramatyki.

Nie warto tworzyć jednej generycznej klasy automatu dla wszystkiego. Wspólne mogą być tylko małe elementy infrastruktury: `StateId`, eksport DOT, przejście po ID, algorytm osiągalności i zasady internowania. `LRkDfa` oraz `RegexDfa` mają inne znaczenie stanów, alfabet i sposób budowy.

Po ukończeniu podstawowego LR(k) osobna ścieżka regex może wyglądać tak:

1. bezpieczny własnościowo `RegexAst` i przeniesienie testów z `z_javy`;
2. NFA Thompsona z epsilon-closure;
3. DFA przez konstrukcję podzbiorów;
4. matcher oraz testy równoważności na generowanych krótkich słowach;
5. minimalizacja DFA i integracja z lexerem dopiero po uzyskaniu poprawności.

## Roadmapa implementacji

Plan wykonawczy wraz ze stanem każdego punktu jest utrzymywany w historii
zmian projektu.

## Minimalny zestaw testów do przeniesienia

Z `z_javy` warto przenieść zachowanie istniejących testów C++, ale napisać je przeciwko nowemu API:

1. klasyczne gramatyki wyrażeń dla LR(0), konfliktowego LR(0) i poprawnego SLR/LR(1) — nawet jeśli SLR nie będzie API, przykład nadal bada konflikty;
2. gramatyka przypisań, która nie jest SLR(1), ale jest LALR(1);
3. `S -> C C`, `C -> c C | d`: 10 stanów LR(1), 7 po scaleniu LALR;
4. gramatyka LR(1), która po scaleniu daje dwa konflikty reduce/reduce;
5. gramatyka wymagająca dwóch tokenów lookahead, w tym rozróżnienie `a EOF` od `a a`;
6. gramatyka rzeczywiście wymagająca LR(3);
7. nullable start i łańcuch nullable;
8. lewa rekurencja i cykle nullable — algorytm musi osiągnąć punkt stały;
9. dwie identyczne tekstowo produkcje — konflikt reduce/reduce nie może zniknąć;
10. sztucznie kolidujące hashe stanów — równość musi ochronić kolekcję;
11. duży alfabet i `k=6` — liczba węzłów ACTION ma pozostać mała;
12. wyczerpujące porównanie wszystkich krótkich wejść z prostym oracle.

Testy tablic referencyjnych są szczególnie wartościowe, bo ujawniają błędną numerację redukcji, zgubione przejścia i niepoprawne lookaheady, których sam test `accepts()` może nie wykryć.

## Decyzje, których nie należy odkładać

- **Zakres:** kanoniczne LR(k) jest obowiązkowe; zdecydować, czy LALR(k) również jest częścią pierwszego wydania. Ten dokument zakłada, że tak.
- **FIRST/FOLLOW bez LL:** zaimplementować je jako osobne analizy gramatyki, ale nie budować tabel LL. SLR może kiedyś być małym dodatkiem wykorzystującym FOLLOW(1), lecz nie jest kamieniem milowym roadmapy. LR(1) i LR(k) nie zależą od FOLLOW.
- **DFA jako osobna warstwa:** kolekcja stanów i przejścia nie mogą być ukrytym efektem ubocznym budowy tablicy.
- **Automat ze stosem:** runtime LR ma być osobnym `LRMachine`, a nie metodą doczepioną do grafu stanów.
- **Regex osobno:** `RegexNfa/RegexDfa` mogą współdzielić narzędzia grafowe z LR, ale nie model stanu ani alfabet.
- **Niezmienność:** żadna budowa parsera nie powinna modyfikować `Grammar`.
- **Tożsamość:** identyczne produkcje pozostają różnymi `RuleId`.
- **Konflikty:** tabela przechowuje wszystkie akcje; polityki precedencji można dodać dopiero jako osobną warstwę.
- **Wydajność:** najpierw poprawny, prosty `WordSetK` i rozrzedzona mapa ACTION, potem profilowanie. Trie ACTION jest następną reprezentacją, gdy testy porównawcze już chronią semantykę.
- **Format danych:** komentarze i etykiety w `grammars.dat` nie są częścią składni reguł. Czytnik katalogu ma je oddzielać od `GrammarBuilder`.

## Najkrótsza sensowna ścieżka

Najmniejsze ryzyko daje kolejność:

```text
niezmienna Grammar
  -> nullable, FIRST(1), FOLLOW(1)
  -> FIRST(k), FOLLOW(k) jako zbiory krótkich słów
  -> produktywność, osiągalność, cykle bez postępu i lewa rekurencja
  -> kanoniczny DFA stanów LR(k)
  -> ACTION/GOTO i konflikty
  -> LRMachine ze stosem, także dla k > 1
  -> LALR(k) przez scalanie rdzeni
  -> generator z limitem długości n i testAmbig
  -> optymalizacje i klasyfikator katalogu
  -> programatyczny ograniczony EBNF -> BNF
  -> GLR na konfliktowych tablicach LR(1)
  -> IELR(1), potem eksperymentalne IELR(k) z fallbackiem do canonical LR(k)

następny tor analizy leksykalnej:
RegexAst -> RegexNfa -> RegexDfa -> minimalizacja -> Lexer -> LRMachine
```

To rozwija pierwotną ideę Żbika: mały, typowany rdzeń zamiast kopiowania całego wcześniejszego portu. `z_javy` pozostaje podstawowym źródłem działającego kodu, zachowania i przypadków testowych, ale nie wzorcem struktury projektu. Java `zubr-kit` pozostaje upstreamem pomocnym przy weryfikacji translacji i późniejszych aktualizacji.
