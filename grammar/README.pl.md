# Import skompresowanych tabel

`CompressedTable.g4` opisuje aktualny wynik `CompressedParseTable::dumpDsl()`.
Regułą wejściową jest `document`; wymaga ona końca wejścia. Gramatyka nie
zawiera akcji zależnych od języka docelowego. Pierwszy importer może użyć
ANTLR4, a po uruchomieniu Agasa przeniesiemy tę składnię do `.ag`.

Przykład generowania parsera C++ (pliki wynikowe poza źródłami):

```bash
antlr4 -Dlanguage=Cpp -visitor -no-listener -o /tmp/zbik-dsl-parser \
  grammar/CompressedTable.g4
```

Sam wygenerowany parser rozpoznaje strukturę; importer powinien następnie:

- odrzucać dokument po dowolnym błędzie leksykalnym lub składniowym, także
  gdy ANTLR odzyskał możliwość dalszego parsowania;
- zdekodować ciągi zgodnie z regułami JSON i sprawdzić nazwę parsera:
  `SLR` oznacza k=1, a `LR(k)` i `LALR(k)` wymagają dodatniego k;
- sprawdzić zakresy liczb przed konwersją do typów C++;
- sprawdzić unikalność numerów wierszy oraz istnienie wszystkich odwołań;
- sprawdzić równą długość obu mapowań stanów, zakres stanu początkowego
  i celów shift/GOTO; indeks w mapowaniu jest numerem stanu;
- sprawdzić symbole i numery reguł względem dostarczonej osobno gramatyki
  (numery reguł są od zera; eksport tabeli nie zawiera produkcji);
- sprawdzić brak powtórzonych kluczy, długości lookaheadów oraz pozycję EOF:
  krótsze słowo musi kończyć się EOF, a accept dotyczy wyłącznie EOF.

Gramatyka wymaga dokładnie jednej akcji domyślnej na końcu każdego wiersza
ACTION. Pusty wiersz GOTO jest dozwolony; brak wpisu oznacza brak przejścia.

Niecytowane `EOF` oznacza koniec wejścia; `"EOF"` oraz `"$"` to zwykłe terminale.
Przykłady: `[EOF]`, `["i", EOF]`, `["EOF", EOF]`. Importer rozróżnia te przypadki
według alternatywy reguły `lookaheadSymbol`, przed dekodowaniem ciągu znaków.
W eksporcie JSON odpowiednikiem EOF jest obiekt `{"eof": true}`, natomiast
terminal pozostaje ciągiem, np. `"EOF"` lub `"$"`. Ponadto redukcje domyślne
mogą opóźniać wykrycie błędu — importer ma odtworzyć tę semantykę.

Dodanie gramatyki `.g4` nie podłącza ANTLR do budowania ani do `main` Żbika.
