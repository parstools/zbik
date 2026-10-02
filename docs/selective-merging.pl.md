# Selektywne scalanie stanów kanonicznego LR(k)

Stan: pierwsza implementacja eksperymentalna i testy porównawcze;
pełny dowód i diagnostyka kontekstowa pozostają otwarte. Data: 2026-09-20.

Dokument opisuje konstrukcję
korzystającą z już zbudowanego automatu kanonicznego. Nie jest implementacją
publikowanego IELR(1) ani dowodem poprawności jego uogólnienia na IELR(k).

## 1. Cel i zakres

Chcemy zmniejszyć liczbę stanów i rozmiar eksportowanych tabel Agasa dla
bezkonfliktowej gramatyki LR(k), przede wszystkim LR(2) i LR(3).
Kanoniczny automat pozostaje niezmiennym punktem odniesienia i wynikiem
awaryjnym na etapie generowania. Runtime otrzymuje jeden wybrany parser;
nie wymaga drugiej, kanonicznej tabeli do diagnostyki. Nie zmieniamy
gramatyki, numerów produkcji ani wartości `k`.

LALR(k) scala wszystkie stany o tym samym rdzeniu LR(0). Selektywne scalanie
scala tylko takie grupy, dla których przejdą określone niżej kontrole.
W szczególności decyzja nie może zależeć wyłącznie od konfliktów w dwóch
bezpośrednio porównywanych wierszach: ich scalenie może wymusić scalenie
następników, a następnie kolejnych stanów.

Koszt budowy kanonicznego LR(k) nadal ponosimy. Optymalizacja dotyczy gotowego
automatu i artefaktu, nie początkowej konstrukcji kanonicznej.

W pierwszej wersji odrzucamy wejściowe tabele z konfliktami. Nie stosujemy
domyślnego shift, pierwszej redukcji ani precedencji do uzyskania pozornego
sukcesu. Gramatyki z polityką rozstrzygania konfliktów są osobnym rozszerzeniem.

## 2. Dwa kryteria zgodności, dwie różne gwarancje

### 2.1. Tryb `ExactActions`

Każdy stan grupy musi mieć takie same akcje dla każdego słowa lookahead,
po przeliczeniu celów shift na grupy. Brak akcji oznacza `error` i również
uczestniczy w porównaniu. Cele przejść muszą być zgodne.

Ten tryb zachowuje przebieg parsera krok po kroku, z dokładnością do numerów
stanów. Ma prosty dowód opisany dalej. Jest dobrym pierwszym wdrożeniem oraz
punktem kontrolnym, ale może pozostawić wszystkie stany osobno: różnice
lookaheadów, które LALR zwykle wykorzystuje do kompresji, często oznaczają
właśnie różnicę między redukcją a błędem.

### 2.2. Tryb `CompatibleUnion`

Łączymy zbiory akcji. `error` oznacza pusty zbiór, więc można połączyć błąd
z pojedynczą akcją. Nie można otrzymać dwóch różnych akcji dla tego samego
słowa lookahead. Cele przejść nadal muszą być zgodne.

To właściwy kierunek bardziej efektywnego selektywnego scalania. Może dodać
akcję w kontekście, w którym parser kanoniczny zgłosiłby już błąd. Nie wolno
więc przypisywać mu dowodu krokowej równoważności z trybu `ExactActions`.

W tym dokumencie podajemy kompletny algorytm konstrukcji i mechanicznej
walidacji partycji dla obu trybów. Dla `CompatibleUnion` przed dopuszczeniem
do normalnego eksportu wymagamy dodatkowo uzasadnienia zachowania języka
i zachowania na błędnych wejściach dla konkretnej konstrukcji LR(k) Żbika.
Samo przejście testów i brak konfliktów nie zastępują tego uzasadnienia.

## 3. Dane i oznaczenia

Wejście:

- skończony, kompletny kanoniczny `LRkDfa`, zbiorem stanów jest `Q`;
- `k >= 1`, ta sama gramatyka i konwencja EOF w całej konstrukcji;
- surowa, nieskompresowana i bezkonfliktowa `ParseTable`;
- stabilny porządek symboli, stanów oraz produkcji.

`core(q)` oznacza uporządkowany zbiór par `(RuleId, dot)`, bez lookaheadów.
Na początek należy użyć dokładnie konwencji `stateCore()` z
`src/lr/LALRkDfa.cpp`: rdzeń jest projekcją całego przechowywanego zbioru
itemów. Nie należy w tej samej zmianie zastępować go samym kernelem bez
uzasadnienia równoważności obu reprezentacji.

`delta(q, X)` jest przejściem po terminalu lub nieterminalu `X`.
`P` jest partycją stanów kanonicznych, a `group(q)` grupą zawierającą `q`.
Każdy stan należy do dokładnie jednej niepustej grupy.

Akcje po przeliczeniu do partycji:

```text
normalize(Shift(t), P) = Shift(group(t))
normalize(Reduce(r), P) = Reduce(r)
normalize(Accept, P) = Accept
normalize(error, P) = error
```

Redukcje porównujemy według `RuleId`, nie według samej długości prawej strony
lub nazwy nieterminalu. Różne produkcje mogą mieć różne znaczenie dla AST.

## 4. Pełne słowa lookahead

Dla `k=2` słowa `[a,b]`, `[a,c]`, `[a,EOF]` i `[EOF]` są różnymi kluczami.
Nie zastępujemy ich zbiorem pierwszych terminali. `EOF` jest specjalnym
symbolem końca wejścia, a nie terminalem użytkownika o podobnej nazwie.

Runtime Żbika pobiera do `k` terminali i dodaje pojedyncze `EOF`, jeśli
wejście skończyło się przed zapełnieniem okna. Nie powiela EOF do długości `k`.
Puste słowo FIRST oznacza epsilon i nie jest pustym tokenem wejściowym.

`ParseTable::ActionTrie::find()` wykonuje dokładne dopasowanie całego klucza,
bez dopasowania prefiksowego. Kontrola ma zachowywać tę semantykę. Gdyby
w przyszłości wprowadzono klucze prefiksowe, test zgodności musiałby również
wykrywać nakładające się domeny takich kluczy.

Nie enumerujemy całego alfabetu do potęgi `k`. Wystarcza suma istniejących
kluczy w porównywanych wierszach. Poza nią wszystkie wiersze mają błąd.

Ważny szczegół implementacji: dla `k>1` akcje shift w `buildActionRow()`
powstają przez `FIRST_k` pozostałej części produkcji z lookaheadem itemu.
Nie wolno budować ich wyłącznie z pierwszego terminalu przejścia.

## 5. Niezmienniki zatwierdzonej partycji

Po każdej udanej transakcji obowiązują:

1. Grupy są niepuste, rozłączne i pokrywają całe `Q`.
2. Wszystkie stany grupy mają ten sam `core`.
3. Dla każdego symbolu mają zgodną obecność przejścia; jeśli ono istnieje,
   wszystkie cele należą do jednej grupy.
4. Znormalizowane wiersze ACTION spełniają wybrany tryb zgodności.
5. Itemy stanu wynikowego są dokładną sumą itemów jego członków.
6. Startem jest grupa zawierająca kanoniczny stan początkowy.
7. Produkcje i symbole gramatyki nie są przenumerowywane.

Wspólny rdzeń powinien zapewniać zgodność obecności przejść, ale kontrolujemy
ją jawnie. Brak przejścia nie jest zgodny z istniejącym przejściem nawet
w trybie `CompatibleUnion`.

## 6. Algorytm bazowy: transakcyjne łączenie grup

### 6.1. Inicjalizacja

W trybie `CompatibleUnion` najpierw budujemy i sprawdzamy pełny LALR(k).
Jeżeli nie ma konfliktów, przyjmujemy jego grupy rdzeni, walidujemy wynik
i kończymy bez prób selektywnego scalania. Dopiero konfliktowy LALR(k)
uruchamia algorytm transakcyjny. `maxAttempts` ogranicza tylko te próby,
nie wyłącza wstępnego sprawdzenia LALR. Statystyki zapisują `usedLalr`,
liczbę stanów LALR i jego konfliktów. Tryb kontrolny `ExactActions` nie
korzysta z tego skrótu, ponieważ pełny LALR może zmieniać puste komórki.

Po nieudanej próbie LALR (lub w trybie ścisłym) zaczynamy od partycji
singletonowej: każdy stan kanoniczny jest osobną grupą.
Wyznaczamy koszyki jednakowych rdzeni. Tylko grupy z jednego koszyka mogą
być kandydatami do scalenia.

Pierwsza implementacja może używać pełnej kopii roboczej partycji podczas
każdej próby. Jest to prostsze do sprawdzenia niż przyrostowe struktury.
Później można zastosować union-find z dziennikiem cofania zmian.

### 6.2. Domknięcie scaleń wymuszonych przez przejścia

Próba scalenia `A` i `B` obejmuje całą transakcję:

```text
tryMerge(P, A, B, mode):
    W = kopia robocza P
    pending = [(A, B)]

    dopóki pending nie jest puste:
        (a, b) = pobierz pierwszą parę
        a = aktualny reprezentant a w W
        b = aktualny reprezentant b w W
        jeżeli a == b: kontynuuj

        jeżeli core(a) != core(b): odrzuć całą próbę
        jeżeli domeny przejść a i b są różne: odrzuć całą próbę

        dla każdego symbolu X w stabilnym porządku:
            dodaj do pending parę (target(a, X), target(b, X))

        połącz a i b w W

    jeżeli W nie spełnia niezmienników przejść: odrzuć całą próbę
    jeżeli wiersze ACTION nie są zgodne z mode: odrzuć całą próbę
    zwróć W jako zatwierdzony wynik
```

Robocza grupa może chwilowo mieć kilka celów po jednym symbolu. Należy
przechować te cele albo odpowiadające im zobowiązania w kolejce. Nie wolno
nadpisać jednego celu drugim i zgubić wymuszonego scalenia. `target` w tym
pseudokodzie oznacza reprezentatywny cel wraz z zachowaniem wszystkich
wcześniej utworzonych zobowiązań.

Kolejka może zawierać powtórzenia. Aktualne reprezentanty pozwalają je
pominąć. Cykl przejść nie powoduje nieskończonej rekurencji: każda faktyczna
unia zmniejsza liczbę grup, najwyżej `|Q|-1` razy.

Sprawdzanie akcji następuje po domknięciu przejść. Dwa shifty do różnych
stanów kanonicznych mogą stać się tą samą akcją po połączeniu ich celów.
Nie należy przedwcześnie klasyfikować ich jako konfliktu.

Odrzucenie cofa wszystkie zmiany transakcji, także wymuszone scalenia
odległych następników. Kanoniczny graf nigdy nie jest modyfikowany.

### 6.3. Kontrola ACTION

Dla każdej grupy roboczej `B` zbieramy klucze jej kanonicznych wierszy.
Dla każdego klucza `w`:

- `ExactActions`: wszystkie `normalize(ACTION(q,w),W)` muszą być równe,
  także wtedy, gdy jeden z nich jest błędem;
- `CompatibleUnion`: zbiór wszystkich różnych niepustych znormalizowanych
  akcji może mieć co najwyżej jeden element.

Wynik `Reduce(r1)` oraz `Reduce(r2)` dla różnych produkcji jest konfliktem.
Podobnie shift/reduce, shift/accept i reduce/accept. Nie można usuwać akcji
accept ani zastępować jej redukcją domyślną.

Prosta wersja kontroluje całą partycję po każdej próbie. Optymalizacja do
dotkniętych grup jest możliwa po dodaniu jawnych zależności i testów.

### 6.4. Kolejność prób i punkt stały

Porządkujemy koszyki według rdzeni, a grupy według najmniejszego kanonicznego
`StateId`. Próbujemy par w porządku leksykograficznym. Po udanej transakcji
rozpoczynamy przegląd par od nowa na aktualnej partycji.

Kończymy, gdy pełny przegląd nie da scalenia albo zostanie wyczerpany budżet.
Nie utrwalamy odrzucenia pary bez uwzględnienia wersji partycji. Cele akcji
są przeliczane względem aktualnych grup.

Wynik jest deterministyczny dla ustalonego porządku i budżetu liczby prób.
Limit czasu może zatrzymać algorytm w różnym miejscu; odtwarzalne artefakty
powinny używać limitu pracy lub pełnego przebiegu.

## 7. Dlaczego trzeba sprawdzać następniki

Załóżmy, że `p` i `q` mają ten sam rdzeń i lokalnie zgodne akcje:

```text
p --x--> r
q --x--> s

ACTION(r, [a,b]) = Reduce(R1)
ACTION(s, [a,b]) = Reduce(R2)
R1 != R2
```

Po scaleniu `p` i `q` musi istnieć jedno przejście po `x`. Wymaga to scalenia
`r` i `s`, które jest zabronione. Odrzucamy zatem również początkową parę.
Taki problem może ujawnić się po dowolnie długim łańcuchu przejść.

Przykład dopuszczalny tylko w trybie `CompatibleUnion`:

```text
ACTION(p, [a,b]) = Reduce(R1)    ACTION(q, [a,b]) = error
ACTION(p, [a,c]) = error         ACTION(q, [a,c]) = Reduce(R1)
```

Suma zawiera tę samą redukcję dla obu kluczy i nie tworzy konfliktu.
Nie zachowuje jednak identycznego momentu zgłaszania błędu.

## 8. Materializacja wyniku i certyfikat

Po wyborze partycji:

1. Utworzyć jeden stan na grupę, zachowując listę `canonicalOrigins`.
2. Połączyć, posortować i zdeduplikować pełne itemy członków grupy.
3. Przeliczyć wszystkie przejścia przez mapę `canonicalToMerged`.
4. Nadać stabilne gęste identyfikatory, najlepiej przez BFS od startu
   z uporządkowanymi symbolami przejść.
5. Zbudować tabelę od nowa przez te same reguły co `buildActionRow()`.
6. Sprawdzić jej zgodność z sumą znormalizowanych kanonicznych wierszy.
7. Dopiero potem uruchomić kompresję tabel i eksport DSL/JSON.

Ponowne zbudowanie wierszy jest ważne dla LR(k): sprawdza, czy połączenie
itemów i obliczenie FIRST(k) daje dokładnie oczekiwane akcje. W razie
rozbieżności zgłaszamy błąd konstrukcji, zamiast wybierać wygodniejszy wynik.

Certyfikat to partycja, mapa stanów i wybrany tryb. Niezależny walidator
powinien sprawdzać niezmienniki bez odtwarzania kolejności prób.
W `ExactActions` taki certyfikat wystarcza do poniższego dowodu. W trybie
sumowania poświadcza lokalne własności konstrukcji, nie zastępuje dowodu
zachowania języka.

Nie wolno używać skompresowanej tabeli jako źródła decyzji o scalaniu:
domyślne redukcje mogą celowo zastąpić część pustych komórek.

### 8.1. Jedna domyślna redukcja zamiast wielu wpisów

Po zatwierdzeniu scalenia można zapisać powtarzającą się redukcję jako
domyślną akcję wiersza, tak jak robi obecny `CompressedParseTable`:

```text
przed kompresją: [a,b] -> R7, [a,c] -> R7, [d,EOF] -> R7
po kompresji:   any -> R7
```

Drugi zapis może wykonywać R7 także w pustych wcześniej komórkach. Nie jest
więc dosłownie identyczną funkcją ACTION, choć taki sposób kompresji jest
używany do zmniejszania tabel. Jeżeli w wierszu występują shift, accept albo
inna redukcja, pozostają jawnymi wyjątkami. Nie wolno ich nadpisać przez R7.

Scalanie oraz kompresja uzupełniają się: pierwsze redukuje liczbę stanów,
drugie liczbę wpisów, w tym powtarzające się wiersze. Raport eksperymentu
uwzględnia oba etapy. Stany z podobnymi redukcjami mogą skorzystać z deduplikacji
wierszy nawet wtedy, gdy nie można połączyć ich rdzeni lub przejść.

Sam wspólny wybór redukcji domyślnej nie uprawnia do scalenia stanów.
Test zgodności nadal korzysta z nieskompresowanych ACTION, pełnych
lookaheadów i domknięcia następników. Rozszerzenie redukcji na puste komórki
jest osobną transformacją: dowodu i pomiarów opóźnienia błędu dla surowego
automatu nie przypisujemy automatycznie wykonaniu tabeli skompresowanej.

## 9. Uzasadnienie poprawności i jego granice

### 9.1. Dowód dla `ExactActions`

Zestawiamy stos kanoniczny `[q0,...,qn]` ze stosem wynikowym
`[group(q0),...,group(qn)]`, przy tej samej pozycji wejścia.

Początkowo relacja zachodzi dla stanów startowych. Następnie:

- oba parsery odczytują ten sam lookahead i wybierają tę samą akcję;
- shift zużywa ten sam terminal i prowadzi do odpowiadających sobie stanów;
- redukcja ma ten sam `RuleId`, usuwa tyle samo elementów obu stosów,
  a zgodność GOTO zachowuje relację po dołożeniu nowego stanu;
- produkcja pusta usuwa zero elementów i podlega temu samemu argumentowi;
- accept oraz error występują w tym samym kroku.

Indukcja po krokach daje ten sam wynik, sekwencję redukcji i pozycję błędu.
Jeżeli parser kanoniczny kończy działanie, kończy je również parser wynikowy.
Numery stanów i tekst komunikatu zawierający numery mogą być inne.

### 9.2. Twierdzenie o języku dla `CompatibleUnion`: założenia

Oznaczmy gramatykę przez `G`, parser kanoniczny przez `C`, a scalony przez
`M`. `L(M)` oznacza zbiór skończonych wejść, dla których `M` kończy się
akcją accept. Taka definicja sama nie obiecuje zakończenia na innych wejściach.

Droga dowodu polega na wykazaniu:

```text
L(C) ⊆ L(M) ⊆ L(G)
L(C) = L(G)
zatem L(M) = L(C) = L(G)
```

Wymagamy poprawnej i kompletnej konstrukcji kanonicznego LR(k), bezkonfliktowej
tabeli wejściowej oraz niezmienników z rozdziału 5. ACTION wyniku jest dokładną
sumą akcji kanonicznych po mapowaniu celów; nie usuwa się akcji w celu
rozstrzygania konfliktów. Redukcje dotyczą oryginalnych produkcji, a accept
dotyczy wyłącznie ukończonej produkcji syntetycznego startu i końca wejścia.

Dodatkowym obowiązkiem strukturalnym jest sprawdzenie własności itemów,
closure i GOTO opisanej w 9.4. Nie można jej zastąpić kontrolą, że wszystkie
wiersze są jednoznaczne. Poniższy argument jest uzasadnieniem projektu;
przed wdrożeniem należy odnieść jego lematy do reprezentacji Żbika.

### 9.3. Zachowanie poprawnych przebiegów: `L(C) ⊆ L(M)`

Na wejściu akceptowanym przez `C` zestawiamy stosy:

```text
C: [q0,        q1,        ..., qn]
M: [group(q0), group(q1), ..., group(qn)]
```

Początkowo odpowiadają sobie stany startowe. Przy tej samej pozycji wejścia
oba parsery pobierają ten sam pełny lookahead. Każda akcja `C` pozostaje
w sumie wierszy grupy, a brak konfliktów wymusza jej wybór przez `M`.

Shift zużywa ten sam token i zachowuje relację stosów dzięki mapowaniu celu.
Redukcja używa tego samego `RuleId`, usuwa tyle samo stanów, a zgodność GOTO
zachowuje relację po dołożeniu stanu. Dotyczy to także redukcji pustych.
Accept pozostaje tą samą akcją przy tej samej pozycji wejścia.

Indukcja po skończonym poprawnym przebiegu dowodzi akceptacji przez `M`.
Zachowana jest również sekwencja redukcji i, przy identycznych regułach
budowy AST, to samo drzewo składniowe. Nie potrzebujemy identycznych pustych
komórek, ponieważ poprawny przebieg nigdy z nich nie korzysta.

### 9.4. Lemat strukturalny: redukcja odpowiada symbolom stosu

Do dowodu dopisujemy pomocniczy stos symboli. Jego stan opisuje ścieżka:

```text
B0 --X1--> B1 --X2--> ... --Xn--> Bn
```

`Bi` są grupami, a `Xi` symbolami gramatyki. Nie trzeba fizycznie dodawać
tego stosu do produkcyjnego runtime; można go prowadzić w walidatorze testowym.

Jeżeli w rdzeniu `Bn` występuje ukończony item `A -> Y1 ... Ym ·`, to:

1. `n >= m`;
2. ostatnie `m` symboli stosu to dokładnie `Y1 ... Ym`;
3. po ich zdjęciu istnieje właściwe GOTO po `A`, gdy jest to zwykła redukcja.

Uzasadnienie pierwszych dwóch punktów opiera się na cofnięciu kropki.
Closure dodaje itemy z kropką na początku. Item z kropką za co najmniej
jednym symbolem w celu przejścia może więc pochodzić tylko z przesunięcia
odpowiadającego itemu w źródle. Dla każdej krawędzi kończącej ścieżkę stosu
cofamy kropkę o jeden i odczytujemy wymagany symbol prawej strony.

Scalenie nie może zgubić tej własności: wszystkie stany grupy mają tę samą
projekcję itemów, a każda jej krawędź pochodzi z kanonicznego przejścia do
grupy o tej samej projekcji celu. Należy sprawdzić tę własność dla projekcji
pełnych itemów używanej przez Żbika, nie zakładać jej wyłącznie na podstawie
nazwy „rdzeń”.

Zbyt krótki stos wymagałby itemu z dodatnią pozycją kropki w stanie
początkowym; konstrukcja startowa na to nie pozwala. Po cofnięciu całej
prawej strony otrzymujemy item `A -> · Y1 ... Ym` w stanie sprzed uchwytu.
Dla zwykłej produkcji jego obecność wynika z closure aktywnego oczekiwania
na `A`, co zapewnia GOTO po `A`. Produkcja syntetyczna jest wyjątkiem
obsługiwanym przez accept, a nie zwykłą redukcję. Dla produkcji pustej
cofamy zero krawędzi; istnienie GOTO wymaga tego samego argumentu z closure.

### 9.5. Poprawność każdej akceptacji: `L(M) ⊆ L(G)`

Każdemu symbolowi na pomocniczym stosie przypisujemy drzewo wyprowadzenia.
Niezmiennik mówi, że wszystkie te drzewa są poprawne względem `G`, a ich
liście w kolejności tworzą dokładnie zużyty fragment wejścia.

Shift dokłada liść z przeczytanym terminalem. Redukcja `A -> Y1 ... Ym`
korzysta z lematu 9.4: ostatnie symbole stosu rzeczywiście odpowiadają prawej
stronie. Zastępujemy ich drzewa jednym drzewem z korzeniem `A`, bez zmiany
ciągu liści. Redukcja pusta tworzy drzewo o pustym ciągu liści.

Przy accept ukończony syntetyczny item startowy wraz z własnością ścieżki
stosu i warunkiem EOF zapewnia drzewo symbolu startowego dla całego wejścia.
Zatem każde zaakceptowane słowo jest wyprowadzalne w gramatyce.

Ten argument nie wymaga zgodności dodatkowych kroków z parserem kanonicznym.
Parser scalony może na błędnym wejściu próbować dalszych redukcji, ale żadna
kończąca się akceptacją analiza nie może stworzyć drzewa dla słowa spoza `G`.
W połączeniu z 9.3 i poprawnością kanonicznego LR(k) daje to równość języków
przy podanych założeniach strukturalnych.

### 9.6. Zakończenie pracy na błędnych wejściach

Równość zbiorów akceptowanych słów nie jest jeszcze dowodem, że `M` zawsze
zakończy pracę. Gdy `C` wybiera error, `M` może wykonać dodatkową redukcję
i poprzednia relacja obu przebiegów przestaje obowiązywać.

Na skończonym wejściu liczba shiftów jest ograniczona jego długością.
Ewentualna nieskończona analiza musiałaby więc od pewnego momentu wykonywać
wyłącznie redukcje. Trzeba wykluczyć takie przebiegi, szczególnie z użyciem
produkcji pustych oraz cykli przez nullable konteksty.

Pierwszy wariant twierdzenia o zakończeniu można oprzeć na odpowiednim
warunku braku cykli wyprowadzeń bez przyrostu długości. Należy dokładnie
zdefiniować ten warunek i wykazać jego wystarczalność, a potem sprawdzić,
czy pokrywa go istniejąca analiza pułapek Żbika. Sam brak bezpośredniej
produkcji `A -> A` nie wystarcza. Nie oznaczamy tego obowiązku jako
rozwiązanego przez samo uruchomienie istniejącego detektora cykli.

Lemat 9.4 ma wykluczać również niedomiar stosu i brak GOTO po redukcji.
W testach warto sprawdzać te własności jawnie i stosować limit kroków,
ale limit nie zastępuje dowodu zakończenia.

### 9.7. Zakres uzyskanych gwarancji

Po uzasadnieniu lematów strukturalnych otrzymujemy zgodność języka i drzew
na poprawnych wejściach dla bezkonfliktowego LR(k), także `k=2` i `k=3`.
Po osobnym dowodzie zakończenia otrzymujemy również parser rozstrzygający
każde skończone wejście. Moment błędu i zbiór oczekiwanych tokenów mogą być
inne niż w parserze kanonicznym.

Nie obejmuje to gramatyk, których kanoniczne konflikty usunięto polityką
precedencji lub wyborem redukcji. Dla takich parserów `L(C)` może być
właściwym podzbiorem `L(G)`, więc łańcuch inkluzji z 9.2 nie daje już
równości `L(M) = L(C)`. To właśnie wymaga dodatkowej analizy zgodności
z polityką konfliktów, istotnej w publikowanym IELR(1).

Do czasu sprawdzenia założeń w implementacji oraz zamknięcia obowiązku
zakończenia `CompatibleUnion` pozostaje eksperymentem. Jego statusu nie
zmienia sam brak konfliktów ani sukces ograniczonych testów różnicowych.

### 9.8. Priorytet: nie akceptować błędnych zdań i wskazywać użyteczne miejsce

Najważniejszym warunkiem użytkowym jest brak fałszywej akceptacji:
parser scalony nie może zaakceptować żadnego zdania spoza języka gramatyki.
Mniejsza tabela, szybsze działanie ani lepszy komunikat nie usprawiedliwiają
naruszenia tego warunku. Nadal obowiązuje zachowanie wszystkich poprawnych
zdań i ich analiz. Zawieszenie na błędnym wejściu również nie jest poprawnym
odrzuceniem — zakończenie wymaga osobnej gwarancji z 9.6.

Dopuszczenie innego miejsca wykrycia błędu nie oznacza zgody na dowolnie
nieprecyzyjną diagnostykę. Nie wystarczy odrzucić plik na końcu, jeśli
użyteczną pozycję można było wskazać dużo wcześniej. Nie wolno też wskazywać
początku pliku jako zastępczej pozycji, gdy nie odtworzono właściwego zakresu.

Należy odróżnić trzy pojęcia:

- moment wykrycia: krok parsera, w którym zabrakło akcji;
- pozycję wejścia: indeks aktualnego tokenu i okno do `k` tokenów;
- miejsce diagnozy: zakres źródłowy pokazany użytkownikowi wraz z wyjaśnieniem.

Dodatkowe redukcje mogą zmienić stan i zbiór oczekiwanych symboli bez
przesunięcia pozycji wejścia. To inna sytuacja niż zużycie kolejnych tokenów
i wykrycie błędu dopiero w odległym miejscu. Dla `k>1` przyczyną niezgodności
może być dalszy token okna lookahead, więc nie należy automatycznie oznaczać
pierwszego tokenu jako winnego. Gdy nie potrafimy wskazać jednego tokenu,
komunikat powinien uczciwie wskazywać problematyczne okno.

Pozycja błędu parsera kanonicznego jest punktem odniesienia, a nie dowodem
lokalizacji pomyłki autora. Na przykład brak zamykającego nawiasu może
uzasadniać diagnozę przy EOF. W takim przypadku warto dodatkowo wskazać
pasujący nawias otwierający, jeśli model diagnostyki potrafi go ustalić.
Nie należy zakazywać wskazania końca lub początku pliku, gdy jest ono
merytorycznie uzasadnione; zakaz dotyczy pozycji zastępczych i opóźnienia
wynikającego wyłącznie z optymalizacji bez użytecznej diagnozy.

### 9.9. Jeden parser i logicznie trafna diagnoza

Docelowy runtime Agasa używa wyłącznie parsera scalonego. Nie przechowuje
równolegle parsera kanonicznego i nie odtwarza nim analizy po błędzie.
Automat kanoniczny służy generatorowi oraz testom jako oracle; po wyborze
i zweryfikowaniu artefaktu nie jest zależnością jego wykonania.

W eksperymencie porównujemy pierwszy błąd obu parserów na tym samym
niezmiennym strumieniu tokenów, przed jakimkolwiek odzyskiwaniem po błędzie.
Raportujemy osobno dodatkowe redukcje, dodatkowe zużyte tokeny, zakres
bajtowy oraz zmianę oczekiwanych słów lookahead. Mierzymy także najgorsze
opóźnienie w korpusie, nie tylko średnią. Nie zakładamy bez dowodu, że
opóźnienie jest ograniczone przez `k`. Te pomiary są informacją badawczą,
a nie wymaganiem wskazania tego samego lub sąsiedniego tokenu.

Kryterium użytkowe to bliskość logiczna: zaznaczone miejsce musi mieć
wyjaśniony związek z konstrukcją, której nie można poprawnie zakończyć.
Może być odległe w bajtach, tokenach lub liniach. Przykłady:

- EOF jako miejsce wykrycia oraz wcześniejszy nawias otwierający jako
  powiązany zakres niedomkniętej konstrukcji;
- token kończący deklarację oraz zakres jej niekompletnej listy parametrów;
- początek konstrukcji, dla której nie pasuje dalsza część wejścia,
  z dodatkowym wskazaniem miejsca, gdzie kontynuacja stała się niemożliwa;
- całe niezgodne okno LR(k), jeśli nie można uzasadnić winy jednego tokenu.

Diagnoza powinna rozróżniać miejsce wykrycia od zakresu kontekstu. Nie
przedstawiać początku konstrukcji jako miejsca pewnej pomyłki autora.
Wskazujemy konkretną niedomkniętą lub niekompletną konstrukcję, dla której
istnieje przesłanka diagnostyczna. Nie wolno automatycznie zastępować jej
rodzicem, następnie kolejnym przodkiem, aż zakres obejmie cały plik.
Jeśli brakuje `)` w liście parametrów, kontekstem jest odpowiednie `(`
lub ta lista, a nie początek funkcji, klasy czy jednostki kompilacji.
Zakresy redukcji powinny zachowywać potrzebną informację o takim lokalnym
kontekście; sam początek coraz większego węzła AST nie jest wystarczającą
kotwicą diagnozy. Jeśli nie potrafimy wiarygodnie ustalić konkretnej
konstrukcji, pozostajemy przy miejscu wykrycia, zamiast wskazywać przodka.
Komunikat może opisywać niezgodność i oczekiwane kontynuacje bez zgadywania
jedynej poprawki. Odległe wskazanie jest dopuszczalne, ale samo przesunięcie
do EOF lub początku pliku bez wyjaśnienia nie spełnia tego kryterium.

Źródłem kontekstu mają być zakresy tokenów i redukowanych konstrukcji,
informacje na stosie oraz metadane reguł gramatyki, dostępne jednemu
parserowi. Jeśli potrzebne będą adnotacje diagnostyczne lub informacje
o delimiterach, należy je jawnie zaprojektować i mierzyć ich rozmiar.
Nie zakładamy, że same numery stanów albo listy `canonicalOrigins`
pozwolą odtworzyć sensowny kontekst. Wybór głównego i powiązanych zakresów
jest częścią projektu diagnostyki Agasa, nie gotową własnością scalania.

Gdy kontekst nie wystarcza do szczegółowej diagnozy, wskazujemy rzeczywiste
miejsce wykrycia i dostępną informację o niezgodności, zamiast wymyślać
wcześniejsze miejsce błędu. Powtarzające się nieużyteczne diagnozy w testach
wymagają poprawy metadanych lub strategii scalania. Nie rozwiązujemy ich
przez dołączenie drugiego parsera do runtime.

Testy diagnostyki powinny obejmować usunięcie, dodanie i zamianę tokenu
w początku, środku i końcu poprawnych zdań, brak delimitera, niezgodność
w drugim lub trzecim tokenie lookahead oraz długi poprawny sufiks po błędzie.
Sprawdzamy pozycje pokazywane użytkownikowi, powiązane zakresy i uzasadnienie
ich związku z błędem, także przy EOF. Zestaw powinien zawierać przypadki,
w których trafny kontekst znajduje się wiele linii od miejsca wykrycia.
Osobny test zagnieżdżenia powinien sprawdzać, że brak delimitera wskazuje
właściwą wewnętrzną konstrukcję, a nie jej rodziców ani początek pliku.
Kryterium wdrożenia: poprawność języka oraz uzasadniona, użyteczna lokalizacja
błędu; samo końcowe odrzucenie wejścia nie wystarcza jako ocena diagnostyki.

## 10. Alternatywna konstrukcja dla trybu ścisłego

`ExactActions` można obliczać wydajniej przez uszlachetnianie partycji:

1. Początkowo grupować wszystkie stany według `core`.
2. Dla każdego stanu obliczać sygnaturę: rdzeń, pełna rzadka mapa ACTION
   z celami shift zamienionymi na bieżące grupy oraz mapa przejść
   z celami zamienionymi na bieżące grupy.
3. Dzielić każdą grupę według sygnatur.
4. Powtarzać, aż żadna grupa się nie podzieli.

Nieobecny klucz oznacza error; mapy o różnych domenach nie są równe.
Partycje tylko się uszczegóławiają, więc obliczenie kończy się.
Ta konstrukcja daje najgrubszą stabilną partycję dla przyjętej ścisłej
równoważności i ograniczenia wspólnym rdzeniem. Nie oznacza to globalnie
najmniejszego parsera rozpoznającego ten sam język.

Nie można mechanicznie zastąpić równości sygnatur zgodnością sumowania.
Zgodność liberalna nie jest przechodnia: A może pasować do B, B do C,
ale A do C już nie. Dlatego dla `CompatibleUnion` opisaliśmy transakcje
z kontrolą całej grupy i wszystkich wymuszonych scaleń.

## 11. Rozmiary, koszty i limity

W tej konstrukcji każdy stan wynikowy jest grupą stanów wejściowych:

```text
liczba różnych rdzeni <= liczba grup <= liczba stanów kanonicznych
```

Dolna granica odpowiada scaleniu LALR(k) przy tej samej konwencji rdzenia
i obsłudze osiągalności. Nie obiecujemy jej osiągnięcia, szczególnie
w trybie `ExactActions`.

Liczba grup nie przekracza liczby stanów kanonicznych nawet w trakcie próby.
Jednocześnie pamięć może być większa: przechowujemy automat referencyjny,
partycję zatwierdzoną, kopię roboczą, wiersze i kolejkę zależności. Limit
liczby stanów nie zastępuje limitu pamięci.

Warunek wyboru selektywnego wyniku jest ścisły: musi mieć przynajmniej
jeden stan mniej od automatu kanonicznego. Jeżeli liczba stanów jest równa,
odrzucamy selektywny wariant i zwracamy oryginalny automat kanoniczny
(`retainedCanonical=true`), zachowując statystyki przeprowadzonych prób.
Dotyczy to również prób przerwanych limitem bez uzyskania zmniejszenia.
Wcześniejszy wybór bezkonfliktowego LALR(k) jest osobną ścieżką.

Dla koszyków rdzeni o rozmiarach `m_i` liczba potencjalnych par na przegląd
jest rzędu sumy `m_i^2`. Każda próba może objąć znaczną część grafu.
Prosta wersja z kopiowaniem i pełnym sprawdzaniem jest celowo referencyjna;
nie należy reklamować jej jako algorytmu liniowego.

Limity: liczba prób, liczba wymuszonych unii, czas i dodatkowa pamięć.
Przerwanie wycofuje bieżącą próbę i zostawia ostatnią zatwierdzoną partycję.
Wynik raportuje przyczynę przerwania. Jeśli nie ma zysku albo walidacja
wyniku zawiedzie, używamy automatu kanonicznego.

Mniej stanów nie musi oznaczać mniejszej skompresowanej tabeli: sumowanie
lookaheadów może zagęścić wiersze. Raport powinien podawać osobno:

- stany, przejścia, itemy oraz wpisy ACTION i GOTO przed i po;
- rozmiar tabeli surowej, skompresowanej i eksportu tekstowego;
- czas budowy kanonicznej, czas scalania i czas kompresji;
- liczbę prób, zatwierdzeń, odrzuceń oraz szczytową dodatkową pamięć;
- tryb zgodności i informację, czy osiągnięto punkt stały.

Wybór artefaktu dla Agasa powinien uwzględniać jego rzeczywisty rozmiar,
a nie wyłącznie liczbę stanów.

## 12. Testy przed wdrożeniem

Testy konstrukcji:

- wspólny rdzeń i rozłączne lookaheady; różnica między oboma trybami;
- konflikt R/R dla tego samego pełnego słowa, ale brak konfliktu między
  `[a,b]` i `[a,c]`;
- shifty do różnych celów, które można połączyć;
- konflikt ujawniający się dopiero w następniku lub po kilku przejściach;
- cykle przejść, scalenie grup mających więcej niż dwóch członków;
- pełny rollback po nieudanej próbie;
- EOF, krótkie okna przy końcu wejścia i produkcje puste;
- deterministyczne wyniki, limity i fallback;
- niezależna walidacja certyfikatu i ponownie zbudowanych wierszy.

Testy parsera:

- porównanie z kanonicznym LR(1), LR(2) i LR(3);
- gramatyka LR(1), która nie jest LALR(1), oraz gramatyka LALR(1);
- wyczerpujące krótkie wejścia, także spoza języka, i większe próbki z seeda;
- porównanie akceptacji oraz `RuleId` redukcji na poprawnych wejściach;
- w `ExactActions` również zgodność chwili błędu i całego śladu po mapowaniu
  stanów; w trybie sumowania osobna kontrola zakończenia błędnych przebiegów;
- gramatyki Agasa i CMinus wymagające `k>1`, z pomiarem rozmiarów artefaktów.

Pierwszy przykład Denny'ego i Malloya:

```text
S -> a A a
S -> b A b
A -> a
A -> a a
```

Powinien służyć do porównania LR(1) z LR(2) i do późniejszych testów polityk
konfliktów. Poprawne słowa to dokładnie `aaa`, `aaaa`, `bab`, `baab`.
Pierwsza wersja scalania odrzuca jego konfliktową tabelę LR(1), zamiast
narzucać redukcję. Dla LR(2) sprawdzamy zachowanie wszystkich czterech słów.

Rysunki 2–4 artykułu opisują gramatyki niejednoznaczne. Zwiększenie `k` nie
usuwa ich niejednoznaczności; początkowo testujemy odrzucenie takich wejść
przez API scalania. Dopiero rozszerzenie z politykami konfliktów może
odtwarzać ich zachowanie opisane w artykule.

## 13. Integracja i kolejność prac

Proponowana nazwa robocza komponentu: `SelectiveLRkMerger`, z jawnym
`MergeMode`. Nie dodajemy przeciążenia sugerującego, że wynik jest LALR(k)
lub publikowanym IELR(k).

Warto wykorzystać istniejące `ItemCore`, `LookaheadWord`, `WordSetK`,
`buildActionRow()` i konwencję `canonicalOrigins` z `LALRkDfa`.
Nie należy utożsamiać wyniku `mergeConflicts()` z pełnym certyfikatem:
brak nowych konfliktów nie sprawdza wszystkich niezmienników.

Kolejność implementacji:

1. Model partycji, mapowanie stanów i niezależny walidator.
2. Tryb `ExactActions`, materializacja tabel i test krokowej równoważności.
3. Raport rozmiarów, integracja z kompresją i jawny fallback.
4. Eksperymentalny `CompatibleUnion`, transakcje i wymagane uzasadnienie.
5. Pomiary dla LR(2)/LR(3) oraz ewentualne ulepszanie kolejności kandydatów.
6. Osobno publikowany IELR(1), polityki konfliktów i badanie IELR(k).

Algorytm zachłanny nie gwarantuje najlepszego grupowania w trybie sumowania.
Można później uruchamiać kilka stabilnych porządków kandydatów i wybierać
najmniejszy zwalidowany artefakt. Nie zmienia to kryteriów poprawności.

## 14. Źródło i związek z IELR

Joel E. Denny, Brian A. Malloy, *The IELR(1) algorithm for generating minimal
LR(1) parser tables for non-LR(1) grammars with conflict resolution*,
Science of Computer Programming 75 (2010), 943–979,
DOI: `10.1016/j.scico.2009.08.001`.

Przeczytana kopia tekstowa:
`1-s2.0-S0167642309001191-main.txt` (lokalny plik pobrany poza repozytorium).
Rysunki odczytano z `/tmp/ie_wybrane`; jest to lokalizacja tymczasowa,
nie trwały zasób repozytorium.

Istotne miejsca: rozdział 2.6 o ograniczeniach testów zgodności Pager'a,
3.1 o konstrukcji IELR od LALR, 3.5.3 o stabilności polityki konfliktów,
3.8 o nieoptymalnym scalaniu i 6 o proponowanym uogólnieniu na IELR(k).

Publikowany IELR(1) nie wymaga zachowania błędów w tych samych komórkach
co kanoniczny LR(1). Tryb ścisły z tego dokumentu stawia mocniejsze wymaganie.
Z kolei zwykłe sumowanie bez konfliktów nie jest pełnym kryterium IELR dla
gramatyk z rozstrzyganiem konfliktów. Te trzy pojęcia trzeba utrzymywać
oddzielnie w dokumentacji, kodzie i raportach.

## 15. Pierwszy eksperyment na Ag.ag

Implementacja: `src/lr/SelectiveLRkMerger.{h,cpp}`. Dostępne są oba tryby,
transakcyjne domknięcie scaleń, wycofanie całej nieudanej próby, limit liczby
prób oraz niezależna kontrola gotowej partycji. Wynik przechowuje własny graf
i mapowanie pochodzenia, bez kopii kanonicznego automatu. Walidator ponownie
buduje ACTION z itemów i porównuje je z sumą wierszy kanonicznych.

Po pierwszym pomiarze dodano docelową kolejność: najpierw LALR(k), a dopiero
gdy ma konflikty — selektywne scalanie. Dla Ag.ag wybierany jest więc
bezkonfliktowy LALR(2), z `usedLalr=true` i zerową liczbą prób. Wyniki
rozmiarów w tabeli pozostają takie same. Opis 21 transakcji i czasów niżej
dokumentuje wcześniejszy pomiar bez tego skrótu, nie obecną ścieżkę wyboru.

Pierwsza wersja używa pełnych kopii partycji, pełnych kontroli zgodności oraz
stabilnego porządku par według numerów stanów. Numeracja wynikowa jest według
najmniejszego numeru stanu pochodzenia, nie BFS. Implementacja nie realizuje
jeszcze wszystkich proponowanych limitów pamięci/czasu ani automatycznego
wyboru mniejszego artefaktu. Niezgodność certyfikatu przerywa eksperyment
wyjątkiem. Dotychczasowy normalny generator Agasa pozostaje kanoniczny.

Pomiar wykonuje osobny program, bez zmiany domyślnego eksportu:

```bash
cmake -S agas-cpp -B agas-cpp/cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build agas-cpp/cmake-build-release -j 4
agas-cpp/cmake-build-release/agas-merge-report
```

Polecenia uruchamiać z katalogu zawierającego `agas-cpp`. Opcjonalny argument
programu wskazuje inny plik `.ag`; `k` jest pobierane z jego opcji.
LALR(k) jest budowany i raportowany również wtedy, gdy ma konflikty:
liczba jego stanów pokazuje liczbę grup rdzeni. Konfliktowej tabeli nie
kompresujemy i nie uznajemy za wykonywalny parser deterministyczny.

Wynik dla aktualnego `agas-cpp/grammars/Ag.ag`, `k=2`:

| Konstrukcja | Stany | Itemy | Przejścia | Surowa tabela, B | Skompresowana, B | DSL, B |
|---|---:|---:|---:|---:|---:|---:|
| Kanoniczny LR(2) | 206 | 23846 | 294 | 280156 | 33832 | 45322 |
| LALR(2) | 130 | 12283 | 184 | 146060 | 23576 | 31619 |
| ExactActions | 206 | 23846 | 294 | 280156 | 33832 | 45322 |
| CompatibleUnion | 130 | 12283 | 184 | 146060 | 23576 | 31617 |

Wszystkie cztery tabele są bezkonfliktowe. Ag.ag jest zatem w tym pomiarze
LALR(2): selektywne scalanie osiąga dolną granicę rdzeni, ale nie wykazuje
przewagi nad LALR(2). Test z gramatyką, której LALR ma konflikty, osobno
sprawdza odrzucanie niebezpiecznych scaleń przy zachowaniu pozostałych.

Zysk względem kanonicznego: około 36,9% stanów, 47,9% bajtów surowej tabeli
i 30,3% bajtów po kompresji. Bajty tabel to szacunek zwartego zapisu według
`TableStorageStats`, nie pomiar pamięci kontenerów C++. Rozmiar DSL jest
rzeczywistą długością napisu pomiarowego. Eksperymentalny graf trafia obecnie
do ogólnego konstruktora `ParseTable(LRkDfa)`, więc napis używa rodziny `LR`,
nie osobnego znacznika selektywnej konstrukcji. Różnica 2 B względem LALR
wynika z nazwy w nagłówku; nie jest dodatkowym zyskiem strukturalnym.
To pomiar formatu, nie włączenie eksperymentalnego eksportu do normalnego CLI.

Przykładowe czasy Release: budowa kanoniczna około 20 ms, samo LALR około
2 ms, selektywne sumowanie wraz z budową tabel referencyjnych i walidacją
około 30 ms, już po konstrukcji kanonicznej. Są to pojedyncze pomiary lokalne,
nie stabilny benchmark. Kompresja wykonywana do raportu nie wchodzi do tych
czasów. Tryb liberalny wykonał 21 udanych transakcji (część łączyła wiele
grup); ścisły odrzucił 122 próby i nie zmniejszył automatu.

Test integracyjny buduje parser z Ag.ag i analizuje 11 rzeczywistych plików
`.ag` z repozytorium. Tokeny dostarcza bootstrapowy lexer ANTLR; nie wymaga
to dalszych prac nad własnym lekserem. Wykonano też 2200 deterministycznych
mutacji: usunięcia, dodania, zamiany tokenów i obcięcia wejścia, seed 20260920.

W tej próbie 1839 mutacji zostało odrzuconych przez oba parsery. Pozostałe
mutacje były nadal poprawne; test nie zakłada, że każda zmiana daje błędne
zdanie. Nie stwierdzono różnic akceptacji ani redukcji poprawnych analiz.
Największe zaobserwowane opóźnienie błędu wyniosło 1 zużyty token. To wynik
korpusu, nie uniwersalne ograniczenie. Porównywano surowe tabele; default
reductions tabel skompresowanych mogą wpływać na diagnostykę osobno.

Interpreter testowy kontroluje symbole zdejmowanych uchwytów, GOTO, warunek
accept i budżet kroków. Poprawne pliki sprawdzono także produkcyjnym
`LRMachine`. Testy mikrogramatyk obejmują `k=1,2,3`, nullable, rzeczywiste
LR(2), konflikty w następnikach, rollback, częściowe scalanie, limit prób
i uszkodzone certyfikaty. Pełny zestaw Żbika Debug: 319 testów; Agasa Release:
13 testów, wszystkie przeszły. Nadal nie jest to pełny dowód ani gotowa
diagnostyka logicznej konstrukcji z punktu 9.9.

## 16. Przykłady z korpusu Żubra, dla których LALR ma konflikty

Źródło: `zubr-kit/src/main/resources/grammars.dat` w osobnym checkoutcie,
etykiety przy liniach 225 i 240 w odczytanej wersji. Produkcje utrwalono
w teście `ComparesSmallReferenceCorpusExamples`, aby test nie zależał od
obecności prywatnego sąsiedniego repozytorium. Etykiety zweryfikowano przez
budowę automatów; nie potraktowano ich jako rozstrzygającego oracle.

Pierwszy przykład, opisany w korpusie jako LR(2), nie-LALR(2):

```text
X -> Y
X -> b Y a
Y -> c
Y -> c a
```

| k | Kanoniczny: stany / konflikty | LALR: stany / konflikty | Selektywny: stany / konflikty | Bajty po kompresji: przed → po |
|---|---|---|---|---|
| 1 | 10 / 1 | 8 / 1 | wejście odrzucone | — |
| 2 | 10 / 0 | 8 / 1 | 9 / 0 | 720 → 704 |
| 3 | 10 / 0 | 8 / 1 | 9 / 0 | 772 → 756 |

To przykład ścisłej nierówności `|LALR(k)| < |selective LR(k)| < |LR(k)|`.
Algorytm zachowuje konieczne rozróżnienie, ale łączy inną zgodną parę.
Zmniejszenie liczby stanów nie daje tu proporcjonalnego zysku bajtów, ponieważ
kompresja kanonicznej tabeli już współdzieli część danych.

Drugi przykład to klasyczna gramatyka LR(1), nie-LALR(1):

```text
S -> a A d
S -> b B d
S -> a B e
S -> b A e
A -> c
B -> c
```

| k | Kanoniczny: stany / konflikty | LALR: stany / konflikty | Selektywny: stany / konflikty | Bajty po kompresji: przed → po |
|---|---|---|---|---|
| 1 | 14 / 0 | 13 / 2 | 14 / 0 | 992 → 992 |
| 2 | 14 / 0 | 13 / 2 | 14 / 0 | 1096 → 1096 |
| 3 | 14 / 0 | 13 / 2 | 14 / 0 | 1192 → 1192 |

Tutaj nie ma zysku: jedyne scalenie po rdzeniu wprowadza konflikt, więc
zachowujemy wszystkie stany. Jest to poprawny wynik eksperymentu, a nie
powód do osłabienia warunków zgodności.

Dla każdego bezkonfliktowego przypadku porównano wszystkie słowa do długości
5 z parserem kanonicznym, w tym redukcje poprawnych analiz. Dodatkowo
niezależny generator wyprowadzeń porównuje język gramatyki z tabelą wynikową
w tym samym ograniczonym zakresie. Kontrolowane są brak konfliktów,
oczekiwane liczby stanów i odrzucenie wejściowej konfliktowej tabeli LR(1).

W trybie `CompatibleUnion`, bez wyczerpania budżetu, bezkonfliktowy pełny
LALR(k) powinien pozwolić osiągnąć wszystkie grupy rdzeni: każda częściowa
suma akcji jest zawarta w końcowej zgodnej sumie, po odpowiednim utożsamieniu
celów. To uzasadnienie nie dotyczy trybu `ExactActions` ani prób przerwanych
limitem. Gdy LALR ma konflikty, liczba grup po selektywnym scalaniu zależy
od bezpiecznych scaleń i ich kolejności; nie obiecujemy globalnego minimum.
