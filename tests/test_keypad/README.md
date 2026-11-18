# Test tastierino 3×4

Sketch autonomo per Elegoo Uno R3, senza librerie aggiuntive e senza multimetro.
Identifica le coppie di contatti senza presumere l'ordine di righe e colonne.

## Collegamenti

Scollegare USB e altre periferiche prima di cablare. Collegare soltanto il
tastierino passivo a sette contatti. Numerare i contatti da un bordo all'altro
del connettore, mantenendo lo stesso orientamento per tutta la prova.

| Contatto del connettore | Pin Uno |
| --- | --- |
| 1 | D2 |
| 2 | D3 |
| 3 | D4 |
| 4 | D5 |
| 5 | D6 |
| 6 | D7 |
| 7 | D8 |

Non collegare il tastierino a 5 V, 3,3 V o GND. Display, RFID e sensore
d'impronta devono restare scollegati. Questa assegnazione serve solo alla prova
isolata e non definisce il cablaggio del terminale completo.

## Avvio

1. Aprire `test_keypad.ino` nell'Arduino IDE.
2. Selezionare **Arduino Uno** e la porta USB della scheda.
3. Caricare lo sketch e aprire il monitor seriale a **9600 baud**.
4. Se le istruzioni iniziali non compaiono, premere RESET sulla scheda.
5. Premere e rilasciare il tasto richiesto, uno alla volta, nella sequenza
   `1 2 3 4 5 6 7 8 9 * 0 #`. Attendere il messaggio successivo.
6. A calibrazione completata, copiare la mappa stampata e premere nuovamente
   tutti i tasti: il carattere mostrato deve coincidere con quello premuto.

Le associazioni dipendono dai tasti premuti durante la calibrazione: una
pressione sul tasto sbagliato produce una mappa sbagliata. RESET ricomincia da
zero; la mappa rimane solo in RAM e si perde allo spegnimento.

## Verifiche hardware da eseguire

- Ogni tasto produce un solo messaggio, anche tenendolo premuto.
- Dopo il rilascio, una seconda pressione dello stesso tasto viene rilevata.
- Nessuna pressione non produce messaggi di tasto.
- Durante la calibrazione, una coppia gia' assegnata non fa avanzare la sequenza.
- Piu' contatti rilevati contemporaneamente vengono rifiutati; rilasciare tutto
  prima di riprovare. Il test e' destinato alla pressione di un tasto per volta,
  non alla validazione di combinazioni simultanee o del ghosting.
- Se un tasto non risponde, controllare i sette collegamenti a USB scollegata.

Lo sketch usa antirimbalzo di 35 ms e una sola uscita LOW per volta, con gli
altri pin come ingressi con pull-up. Stampa singoli tasti esclusivamente per
diagnostica: non inserire PIN reali. Non implementa autenticazione o accesso.

La compilazione per Uno e la prova fisica vanno eseguite con Arduino IDE e
scheda collegata; un eventuale controllo C++ sul computer non le sostituisce.

## Esito della prova hardware — 17 novembre 2026

Prova eseguita dall'utente sul tastierino disponibile, con esito positivo
comunicato in conversazione. Rilevate tutte le dodici coppie; confermate la
corrispondenza dei caratteri in modalità TEST, la singola lettura mantenendo
premuto un tasto e una nuova lettura dopo rilascio e seconda pressione.

| Linea | Tasti | Pin Uno |
| --- | --- | --- |
| Riga 1 | `1 2 3` | D2 |
| Riga 2 | `4 5 6` | D3 |
| Riga 3 | `7 8 9` | D4 |
| Riga 4 | `* 0 #` | D5 |
| Colonna 1 | `1 4 7 *` | D6 |
| Colonna 2 | `2 5 8 0` | D7 |
| Colonna 3 | `3 6 9 #` | D8 |

La mappa vale per il cablaggio della prova. Non risultano verificati il
rifiuto di coppie duplicate in calibrazione, le pressioni simultanee o
l'integrazione con le altre periferiche. Nessun risultato di autenticazione
o autorizzazione è deducibile da questa prova.
