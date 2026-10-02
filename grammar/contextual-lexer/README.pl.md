# Małe gramatyki z lexerem i składanymi klasami

Wykonywalne definicje są w
[ContextualLexerFixturesTest.cpp](../../tests/ContextualLexerFixturesTest.cpp).
Każda definicja zawiera produkcje parsera, regexy lexera, przypisania klas
oraz wejścia z oczekiwaną sekwencją tokenów. Nie są to same strumienie
terminali z korpusu `.data`.

Przykłady sprawdzają teraz zarówno niezależny wzorzec wyboru klas, jak i
rzeczywiste API `zbik_core`: maski reguł lexera, deklaracje klas przy
nieterminalach, specjalizację zakresów i pobieranie podglądu przez
`ContextualLRMachine`. Testy porównują tokeny, tekst i offsety dla obu
ścieżek oraz lexerów bajtowego i UTF-8, przy LR(1) i LR(2).
[Opis publicznego API i jego ograniczeń](../../docs/lexer-classes.pl.md).

## Reprezentacja klas

`READ` i `SHR` są dwoma niezależnymi składnikami klasy. W przykładzie nazwano
je tak samo jak tokeny, których reguły włączają. `IDENT` i `GT` należą do
bazy. Klasa jest maską włączonych składników; cztery kombinacje do testowania:

| Maska | Dodatkowo aktywne reguły | Rozpoznanie `read>>` |
| --- | --- | --- |
| `0` | brak | `IDENT GT GT` |
| `READ` | słowo `read` | `READ GT GT` |
| `SHR` | znak `>>` | `IDENT SHR` |
| `READ \| SHR` | obie | `READ SHR` |

Nie tworzymy osobnego automatu dla każdej maski. Wzorzec kompiluje raz
każdy regex do `LexerAutomaton`, filtruje reguły według maski i wybiera
najdłuższe aktywne dopasowanie; przy remisie wygrywa wcześniejsza reguła.
`READ` znajduje się przed `IDENT`; `reader` nadal jest jednym `IDENT`.
Test z bitem 63 sprawdza, że duży indeks składnika nie powoduje tworzenia
wszystkich podzbiorów.

Wzorzec obsługuje ASCII i zachłanne regexy użyte w przykładach. Jego koszt
dopasowania zależy od liczby reguł i długości dopasowań, nie od `2^n`
kombinacji składników. Docelowy wspólny automat wymaga osobnego projektu:
musi zachować kandydatów/warunki aktywności, aby wyłączenie reguły o wyższym
priorytecie pozwalało wybrać inną regułę lub krótszy token.

## 1. `SHR` kontra `GT GT`

```text
S     -> A ruleA | C ruleB
ruleA -> SHR B
ruleB -> GT GT D

A='a', C='c', B='b', D='d'
SHR='>>' [requires SHR]
GT='>'
WS=[ \t\n]+ [skip]

after A: SHR
after C: 0
```

Przykłady: `a>>b`, `c>>d`, `c > > d`. Sprawdzane są rodzaje tokenów,
ich tekst i offsety bajtowe oraz akceptacja LR(1) i LR(2).

Dodatkowy test pokazuje konkretny problem LR(2): pierwszy podgląd dla
`a>>b` musi zawierać `[A SHR]`. Użycie klasy początkowej także dla drugiego
tokenu daje `[A GT]`, którego nie ma w ACTION stanu początkowego.

## 2. `READ` kontra `IDENT`

```text
S     -> AT ruleA | HASH ruleB
ruleA -> READ END | WRITE END
ruleB -> IDENT END

AT='@', HASH='#', END=';'
READ='read' [requires READ]
WRITE='write' [requires READ]
IDENT=[a-z]+
WS=[ \t\n]+ [skip]

after AT: READ
after HASH: 0
```

Przykłady: `@read;`, `#read;`, `#reader;`, `@write;`, `#write;`. Rozpoznanie słowa zależy od klasy,
a długość tokenu w przypadku `read` pozostaje taka sama.

## 3. Oba mechanizmy razem

```text
S       -> ZERO plain | ONE keyword | TWO shift | THREE both
plain   -> IDENT GT GT END
keyword -> READ GT GT END
shift   -> IDENT SHR END
both    -> READ SHR END

ZERO='0', ONE='1', TWO='2', THREE='3', END=';'
READ='read' [requires READ]
IDENT=[a-z]+
SHR='>>' [requires SHR]
GT='>'
WS=[ \t\n]+ [skip]

after ZERO: 0
after ONE: READ
after TWO: SHR
after THREE: READ | SHR
```

Wejścia `0read>>;`, `1read>>;`, `2read>>;`, `3read>>;` muszą być przyjęte,
z różnymi strumieniami tokenów. W LR(2) `read` jest już drugim tokenem
początkowego podglądu, zanim zostanie wykonana pierwsza akcja parsera.

## 4. Celowo błędne przypisanie klas

Używamy gramatyki z punktu 3, ale przypisujemy `READ | SHR` wszystkim
czterem gałęziom. Najdłuższe dopasowanie i priorytet słowa kluczowego
powodują odrzucenie pierwszych trzech prawidłowych wejść. Tylko gałąź
`THREE` nadal działa. Test odtwarza błąd, któremu ma zapobiegać przyszła
walidacja klas; sam pokazuje skutki błędnego przypisania. Dodatkowe testy API odrzucają sprzeczne wymagania klas i niedostępne tokeny przed parsowaniem.

Oczekiwana docelowa diagnostyka ma wskazać deklarację gałęzi/klasy,
kontekst LR i prefiks podglądu oraz konkretną kolizję, np. `READ` zamiast
`IDENT` dla tekstu `read` albo `SHR` zamiast `GT GT` dla `>>`.

## Uruchomienie z katalogu głównego repozytorium

```sh
cmake --build build --target zbik_tests -j 4
build/tests/zbik_tests --gtest_filter='ContextualLexerFixturesTest.*'
```
