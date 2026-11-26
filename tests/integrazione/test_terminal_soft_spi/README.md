# Terminale da banco con RFID sui pin D12/D10/D9/D8

La [mappa unica del montaggio attuale](../../../docs/hardware/current_wiring.md)
salva tutti i fili e i pin riservati: l'utente indica impronta e schermo
come ancora da collegare. La gestione OLED inclusa nel codice non significa
che lo schermo sia già montato o verificato.

**19 novembre 2026: lettura del badge confermata dall'utente dopo la correzione
del conflitto su D12.** La validazione completa del montaggio resta aperta.
Mantiene tastierino, 74HC595, LED bicolore e OLED della
[prova combinata](../test_terminal_spi/README.md), ma separa i bus SPI.

| RC522 MINI | Uno |
| --- | --- |
| SDA / SS | **D12**, attraverso partitore 1 kΩ / 2 kΩ |
| SCK | **D10**, attraverso partitore 1 kΩ / 2 kΩ |
| MOSI | **D9**, attraverso partitore 1 kΩ / 2 kΩ |
| MISO | **D8**, diretto |
| 3V3 e RST | **3.3V** |
| GND | Massa comune |
| IRQ | Scollegato |

Per ciascun partitore: Uno → 1 kΩ → nodo RFID; dal nodo 2 kΩ → GND.
Il 595 conserva dati D11, clock D13 e latch A0; righe QB–QE e LED QF/QG
mantengono le resistenze da 1 kΩ. Non collegare MOSI/SCK RFID anche a D11/D13.
OLED resta su A4/A5, alimentato a 3,3 V; tastierino colonne A1–A3.

Installare dal gestore librerie **MFRC522v2** (OSSLibraries), diversa da
MFRC522 usata negli sketch precedenti. Il motivo è il trasporto sostituibile:
`soft_rfid_driver.h` implementa solo lettura/scrittura dei registri via GPIO,
mentre selezione carta, CRC, UID e HALT restano nella libreria consolidata.
Non vengono introdotte autenticazione Crypto1 o primitive crittografiche.
Annotare la versione della libreria effettivamente installata.

Aprire `test_terminal_soft_spi.ino` mantenendo i due file `.h` nella stessa
cartella. Selezionare Uno, caricare con display scollegato, scollegare USB
prima di ricablare, poi aprire il monitor a **9600 baud**.
Attesi versione RFID diversa da 00/FF e UID sul monitor avvicinando un badge
di prova; sul display compare BADGE LETTO. Tasti 1/2/0 comandano il LED come
prima. Non usare PIN reali: i tasti vengono mostrati a scopo diagnostico.

SPI RFID software: mode 0, MSB first, pause di 5 µs per semiperiodo più costo
GPIO, frequenza effettiva non misurata. Anche il 595 è pilotato in software
su D11/D13: lo SPI hardware non deve essere attivato, perché sull'ATmega328P
forzerebbe D12/MISO in ingresso, impedendone l'uso come SS RFID.
Nessuna operazione concorrente nei bus.

Restano i limiti della prova combinata: budget 3,3 V simultaneo da misurare,
condensatore 100 nF non disponibile, pressioni simultanee non garantite,
latenze e offset OLED da verificare. Encoder, pulsanti display e impronta
non sono gestiti. Il LED del varco finale e la decisione restano sul Pi.
Se il lettore manca o perde la versione attesa, RFID viene disabilitato fino
al riavvio; tastierino/LED/OLED continuano. Nessun recupero fisico automatico.

## Fonti e verifiche

- [MFRC522v2, API e driver](https://github.com/OSSLibraries/Arduino_MFRC522v2):
  `MFRC522v2.h`, `MFRC522DriverSPI.h/.cpp`, interfaccia del trasporto.
- [NXP MFRC522, rev. 3.9, sez. 8.1.2](https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf):
  indirizzamento SPI e letture/scritture dei registri.

Compilazione AVR e misura RAM/flash non eseguite dall'agente. La lettura del
badge sul montaggio reale è confermata dall'utente; versione della libreria,
numero di tentativi e stabilità prolungata non sono stati registrati.
Le prove simulate non validano radiofrequenza o cablaggi.

Eseguito controllo C++11 con `clang++ -Wall -Wextra -Werror` e sostituti
Arduino/SPI/MFRC522v2: indirizzi e ordine dei bit del trasporto, letture
singole/multiple e maschera rxAlign (anche un solo byte), scrittura, conteggio
zero, rilascio SS e clock LOW, dodici tasti nei tre stati LED e timeout OLED.
I sostituti non costituiscono compilazione della libreria MFRC522v2 reale.

## Correzione dopo mancata lettura

La prima versione conservava lo SPI hardware del 595 e quindi impediva a
D12 di funzionare come uscita SS. Rimossi `SPI.begin()` e le transazioni
hardware: il 595 usa ora clock e dati GPIO, senza cambiare i fili. Questo
è un errore software identificato, non una prova che ogni problema del
montaggio sia risolto. Dopo la correzione l'utente ha confermato che il badge
veniva letto. Non sono stati forniti UID o un elenco dei tentativi; nessuna
autenticazione Crypto1 o autorizzazione è dimostrata da questa lettura.
Riferimento: ATmega328P, datasheet 7810D–AVR–01/15, sezione 14.3.1, PB4/MISO
(https://docs.arduino.cc/resources/datasheets/ATmega328P-datasheet.pdf).
