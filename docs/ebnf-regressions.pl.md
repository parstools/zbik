# Testy EBNF na gramatykach zbliżonych do realnych języków

## Cel

Ten dokument opisuje testy,
które mają odpowiedzieć na praktyczne pytanie: czy ograniczony EBNF Agasa po
konwersji do BNF daje użyteczne gramatyki deterministyczne LR, zanim Żbik
otrzyma GLR.

Eksperyment nie służy udowodnieniu, że wszystkie języki programowania są
LR(k), ani że niepowodzenie dla kilku wartości `k` oznacza nieskończony
lookahead. Ma rozdzielić:

- konflikt wynikający z niejednoznacznej gramatyki;
- konflikt wynikający z konkretnego zapisu języka, który można równoważnie
  przeformułować;
- wspólny prefiks trudny dla LL(k), ale naturalny dla LR(1);
- zależność kontekstową, której nie rozwiązuje zwiększanie `k`;
- przypadek rzeczywiście wymagający pozostawienia wielu interpretacji GLR.

Badamy język ciągów tokenów osobno od kształtu AST. Dwie gramatyki mogą
akceptować ten sam język, ale tworzyć inne drzewa. Każde przeformułowanie musi
więc raportować oba skutki.

## Materiał wejściowy i poziomy realizmu

Testy należy dodawać stopniowo:

1. **Cmm:** `agas-cpp/grammars/examples/cmm.ag` i jego źródło
   `agas-cpp/grammars/examples/cmm.g4`. To mała gramatyka z deklaracjami,
   inicjalizacją, wywołaniami, przypisaniem, pętlą i wyrażeniami.
2. **CMinus:** `agas-cpp/grammars/examples/cminus.ag` i
   `agas-cpp/grammars/examples/cminus.g4`. Dodaje funkcje, `if/else`,
   `for`, `return`, operatory multiplikatywne i więcej postaci przypisania.
3. **Deklaratory C90:** `agas-cpp/grammars/c/CDeclaratorSubset.g4`
   należy najpierw możliwie mechanicznie przepisać do
   `agas-cpp/grammars/c/CDeclaratorSubset.ag`, a następnie odwzorować w modelu EBNF.
   Gramatyka obejmuje prototypy, definicje funkcji, wskaźniki na funkcje,
   tablice, listy deklaratorów jednego typu i składnię K&R.
4. **Izolowany kontekst `typedef`:** mikrogramatyka pokazująca, dlaczego
   klasyfikacji `Identifier`/`TYPE_NAME` nie należy mylić z lookaheadem LR.
5. Dopiero po przejściu powyższych testów można traktować pełniejsze
   `agas-cpp/grammars/c/C.ag` lub
   `agas-cpp/grammars/c/C90.ag` jako test obciążeniowy. Nie są one
   kryterium koniecznym do rozpoczęcia bootstrapu Agasa.

W punkcie 8.5 parserowa część `.ag` jest ręcznie odwzorowywana w programatycznym
modelu EBNF. `node`, `inline`, pola AST, kanały i reguły leksera nie trafiają
do modelu języka parsera. Końcowe `EOF` z pliku `.ag` oznacza wbudowany koniec
wejścia Żbika, a nie zwykły `TerminalId`.

## Wspólny protokół badania

Dla każdej wersji gramatyki należy:

1. zbudować programatyczną specyfikację EBNF;
2. przekonwertować ją przez etap 8.2 do zwykłej `Grammar` wraz z pochodzeniem
   wszystkich reguł pomocniczych;
3. uruchomić analizę nullable, pułapek, minimalnych długości, FIRST i FOLLOW;
4. zbudować canonical LR kolejno dla `k=1`, `k=2`, `k=3`, kończąc po
   pierwszym sukcesie, lecz umożliwiając pełną diagnostykę nieudanego `k`;
5. dla pierwszego udanego `k` sprawdzić LALR(k), nie utożsamiając konfliktu
   wprowadzonego przez scalenie z konfliktem canonical LR(k);
6. uruchomić parser na nazwanych programach poprawnych i błędnych;
7. porównać język z generatorem dla wszystkich krótkich ciągów w osiągalnym
   limicie oraz zachować każdy znaleziony kontrprzykład;
8. zapisać liczby stanów, itemów, przejść, konfliktów, rozmiary tablic i czas
   każdego etapu.

Raport konfliktu musi zawierać stan, pełne itemy z lookaheadem, wszystkie
akcje komórki ACTION oraz pochodzenie reguł z EBNF. Dla akcji reduce należy
wskazać redukowany `RuleId`. Dla shift trzeba wypisać wszystkie itemy stanu,
które uzasadniają przesunięcie danego terminala. Każdy uczestniczący `RuleId`
należy przełożyć przez `EbnfRuleOrigin` na nazwę reguły `.ag`, numer
alternatywy, pozycję elementu i kwantyfikator; helper BNF nie może zasłaniać
reguły napisanej przez autora. Sama informacja „shift/reduce” nie wystarcza do
rozstrzygnięcia przyczyny.

Brak konfliktu jest własnością konkretnej gramatyki i konstrukcji tablicy.
Brak świadka niejednoznaczności do długości `n` nie jest dowodem
jednoznaczności. Podobnie konflikt dla `k <= 3` nie dowodzi braku dowolnego
skończonego `k`.

## `dangling else`

### Skąd bierze się niejednoznaczność

Uproszczony zapis:

```text
statement
    : IF '(' condition ')' statement
    | IF '(' condition ')' statement ELSE statement
    | otherStatement
    ;
```

ma dwa drzewa dla ciągu odpowiadającego:

```c
if (a)
    if (b)
        first;
    else
        second;
```

`else` może należeć do wewnętrznego albo zewnętrznego `if`. Zwiększenie `k`
nie usuwa niejednoznaczności gramatyki: oba wyprowadzenia mają ten sam pełny
ciąg tokenów. GLR może zachować oba drzewa, ale sam nie określi semantyki
języka.

Typową regułą języka jest związanie `else` z najbliższym niezamkniętym `if`.
W Agasie powinny istnieć dwie jawne metody uzyskania tej semantyki.

### Jednoznaczna gramatyka `closed`/`open`

Wersja testowa powinna rozdzielić instrukcje, które nie mogą już przejąć
zewnętrznego `else`, od instrukcji zawierających niezamknięty `if`:

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

Propagacja przez `while` i `for` jest istotna. Samo rozdzielenie dwóch reguł
`if` nie wystarcza, jeśli ciało innej instrukcji może kończyć się otwartym
`if`. `compoundStatement` jest zamknięty na zewnątrz, ponieważ `else` nie
przechodzi przez klamrę.

Reguły techniczne mogą być `inline`, ale test AST powinien sprawdzić, że
pominięcie tych węzłów daje takie samo docelowe drzewo jak polityka
„najbliższy `if`”. Język ciągów tokenów wersji pierwotnej i `closed/open`
powinien być taki sam; zmienia się liczba dopuszczalnych drzew.

### Jawne rozstrzygnięcie konfliktu

Przepisywanie gramatyki jest poprawnym oracle, lecz bywa rozwlekłe. Agas
powinien później otrzymać deklarację rozwiązującą konkretny konflikt. Poniższy
zapis pokazuje kontrakt, a nie zatwierdzoną jeszcze składnię `.ag`:

```text
resolve danglingElse {
    terminal = ELSE;
    choose = shift;
    over = reduce ifStatement#IfStatement;
    expect = 1;
}
```

Znaczenie:

- deklaracja dotyczy wyłącznie konfliktu na `ELSE`;
- konkurującą redukcją musi być wskazana alternatywa krótkiego `if`;
- wybierany jest `shift`, więc `else` wiąże się z najbliższym `if`;
- po zbudowaniu tablicy musi istnieć dokładnie jeden pasujący konflikt;
- zero, dwa lub inny rodzaj konfliktu oznaczają nieaktualną deklarację i błąd
  generatora, a nie ciche zastosowanie ogólnego priorytetu.

Nie należy dodawać globalnego „zawsze wybierz shift”. Rozwiązana tablica musi
zachować w diagnostyce pierwotną komórkę konfliktową, wybraną akcję i nazwę
polityki. `expect` samo nie wybiera akcji; tylko zabezpiecza oczekiwaną liczbę
konfliktów.

Nie nazywamy takiej deklaracji po prostu `ambig`, ponieważ konflikt tablicy
nie jest dowodem niejednoznaczności. Może pochodzić z niedostatecznego `k` lub
ze scalenia LALR. Nazwa `resolve` opisuje faktyczną operację.

### Testy odbiorcze `dangling else`

- pierwotny `cminus` daje oczekiwany konflikt shift/reduce i generator znajduje
  dwa drzewa dla możliwie krótkiego świadka;
- wersja `closed/open` nie ma tego konfliktu w canonical LR(1);
- ograniczone porównanie języków pierwotnej i przepisanej gramatyki nie znajduje
  różnic w ciągach tokenów;
- jawne `resolve` wybiera to samo drzewo co wersja `closed/open`;
- zagnieżdżenia przez `while`, `for` i blok sprawdzają właściwy zasięg `else`;
- zmiana lub usunięcie wskazanej produkcji powoduje błąd nieaktualnej
  deklaracji `resolve`, zamiast rozwiązania innego konfliktu;
- przyszły GLR bez polityki zachowuje oba drzewa, natomiast kompilator języka
  z regułą najbliższego `if` nie pozostawia wyboru przypadkowi.

Etapu 8.5 nie wolno zamknąć samym raportem konfliktu oryginalnego `cminus`.
Wersja `closed/open` musi być uruchamialną ścieżką deterministyczną i przejść
nazwane przykłady parsera. Oryginalny zapis pozostaje przypadkiem
diagnostycznym oraz wejściem dla przyszłego GLR. Test deklaratorów
`CDeclaratorSubset` jest od tego niezależny, ponieważ nie zawiera instrukcji
`if` ani `else`.

## Prototyp i definicja funkcji ze wspólnym prefiksem

Problem LL(k) może wyglądać następująco:

```text
externalDeclaration
    : declarationSpecifiers declarator ';'
    | declarationSpecifiers declarator compoundStatement
    ;
```

`declarator` może być dowolnie długi. Parser LL musi wybrać alternatywę przed
jego przeczytaniem, więc żadne małe stałe `k` nie musi wystarczyć. Parser LR
nie podejmuje tej decyzji na początku. Po przesunięciu wspólnego prefiksu
widzi `;` dla prototypu/deklaracji albo `{` rozpoczynające ciało definicji.
Dlatego ten przypadek powinien zostać najpierw sprawdzony jako LR(1), bez
automatycznego zwiększania `k` i bez przepisywania gramatyki.

Wariant równoważny, przydatny gdy pełna gramatyka nadal powoduje konflikt, to:

```text
externalDeclaration
    : declarationSpecifiers declarator externalDeclarationSuffix
    ;

inline externalDeclarationSuffix
    : ';'
    | compoundStatement
    ;
```

Nie jest to konieczne lewostronne faktoryzowanie dla algorytmu LR, lecz może
uprościć gramatykę i diagnostykę. `inline` zapobiega narzuceniu dodatkowego
węzła AST.

Test powinien zawierać deklaratory o rosnącym zagnieżdżeniu, prototypy,
definicje, listy parametrów i kilka deklaracji z tym samym początkiem. Dla
wersji bezpośredniej i z sufiksem należy porównać:

- minimalne znalezione `k` i konflikty;
- wszystkie krótkie ciągi tokenów;
- drzewa po pominięciu węzła `inline`;
- rozmiar automatu i tablicy, aby reorganizacja nie była oceniana tylko jako
  „przechodzi/nie przechodzi”.

Jeżeli LR(1) przyjmuje wersję bezpośrednią, jest to ważny wynik pozytywny:
pokazuje przewagę LR nad dawnym LL(k), a nie potrzebę LR(2) lub LR(3).

## `typedef` jest problemem kontekstowym

Osobnym przypadkiem jest kod w rodzaju:

```c
T * x;
```

Jeżeli lexer zwróci `T` jako zwykły `Identifier`, znaczenie zależy od tego,
czy wcześniejsza deklaracja w aktualnym zakresie wprowadziła `T` przez
`typedef`. Ten sam zapis może przypominać deklarację albo wyrażenie mnożenia.
Nie jest to problem wspólnego prefiksu, który LR rozwiąże po przeczytaniu
dalszych tokenów. Informacja potrzebna do decyzji znajduje się w tablicy
symboli i może pochodzić z dowolnie odległego miejsca.

Należy osobno zbadać trzy architektury:

1. **Klasyfikacja leksykalna:** lexer lub warstwa pomiędzy lexerem i parserem
   zwraca `TYPE_NAME` zamiast `Identifier`, korzystając z aktualnej tablicy
   symboli i zakresów. Jest to klasyczne, wydajne rozwiązanie, ale tworzy
   kontrolowaną pętlę informacji parser–semantyka–lexer.
2. **Kontekstowy dostawca tokenów:** surowy lexer pozostaje niezależny, a
   parser pyta warstwę kontekstową o klasyfikację identyfikatora. Ta wersja
   lepiej zachowuje rozdział modułów, ale jej kontrakt musi określać moment
   aktualizacji zakresu i zachowanie podczas odzyskiwania po błędzie.
3. **GLR i filtr semantyczny:** parser zachowuje obie interpretacje, a tablica
   symboli odrzuca niemożliwą. Jest to najbardziej ogólne, lecz może być
   droższe i komplikuje wykonywanie odroczonych akcji semantycznych.

Zwiększenie `k` nie zastępuje żadnej z tych metod. Mikrotest `typedef` ma
celowo najpierw użyć jednego tokenu `Identifier` i wykazać konflikt lub dwie
interpretacje, a następnie rozdzielić `Identifier`/`TYPE_NAME` i potwierdzić
bezkonfliktową gramatykę. Wynik nie powinien blokować Cmm ani CMinus, które nie
mają kontekstowych nazw typów.

## Proponowana kolejność implementacji w Agasie

1. Ukończyć model EBNF, konwerter i pochodzenie reguł z 8.1–8.4.
2. Przeprowadzić test Cmm bez żadnych deklaracji rozwiązywania konfliktów.
3. Przeprowadzić surowy test CMinus i uzyskać dokładny świadek `dangling else`.
4. Dodać wersję `closed/open` jako oracle jednoznacznego języka.
5. Przepisać `CDeclaratorSubset.g4` do `CDeclaratorSubset.ag`, porównać ich
   strukturę i uruchomić pełny protokół LR(1..3) na próbkach C90.
6. Zaprojektować i dopiero wtedy dodać składnię `resolve` do `Ag.g4` i `Ag.ag`.
7. Sprawdzić wspólny prefiks prototypu i definicji najpierw w LR(1), potem w
   wariancie z `inline` sufiksem.
8. Traktować `typedef` jako osobny eksperyment integracji kontekstu, nie jako
   powód zwiększania domyślnego lookaheadu.
9. Rozpocząć GLR dopiero z listą konfliktów, które mają pozostać świadomie
   nierozstrzygnięte.

## Kryterium wyniku

Eksperyment wspiera deterministyczny bootstrap Agasa, jeżeli Cmm oraz
jednoznaczny wariant CMinus mają bezkonfliktową tablicę dla małego `k`, a
wykonanie parsera zgadza się z ograniczonym oracle. Nie wymaga się, aby
niejednoznaczny zapis `dangling else` stał się LR(k) przez zwiększenie `k`.

Jeżeli prototyp i definicja funkcji przechodzą w LR(1), należy zachować ten
przypadek jako test pokazujący różnicę względem LL(k). Jeżeli wymagają
przeformułowania, raport musi wskazać konkretny konflikt i uzasadnić
równoważność wersji z sufiksem.

Agas pozostaje użyteczny także wtedy, gdy część języków potrzebuje jawnej
polityki konfliktów, kontekstowej klasyfikacji tokenów albo GLR. Negatywnym
wynikiem byłby dopiero brak czytelnej, stabilnej ścieżki dla prostych,
jednoznacznych gramatyk po konwersji EBNF — nie sam fakt, że dowolna gramatyka
zapisana przez użytkownika może być niejednoznaczna.
