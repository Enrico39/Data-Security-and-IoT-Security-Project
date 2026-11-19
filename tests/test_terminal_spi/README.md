# Prova combinata: tastierino, 74HC595, LED, RFID e OLED

**19 novembre 2026: preparata, non ancora validata su hardware.** Mantiene
la configurazione salvata del tastierino e la prova LED separata. Nessuna
autenticazione, policy, impronta o sessione; nessun accesso autorizzato.

## Collegamenti

USB scollegata durante il montaggio. Mantenere tastierino e LED secondo
[la prova precedente](../test_keypad_74hc595_two_color/README.md).

| Pin RC522 MINI | Uno / collegamento |
| --- | --- |
| 3V3 | 3.3V, mai 5V |
| GND | Massa comune |
| RST | 3.3V; reset software, costruttore con pin inutilizzato `0xFF` |
| SDA / SS | D10 attraverso partitore 1 kΩ / 2 kΩ |
| MOSI | D11 attraverso partitore 1 kΩ / 2 kΩ |
| SCK | D13 attraverso partitore 1 kΩ / 2 kΩ |
| MISO | D12 direttamente |
| IRQ | Scollegato |

Per ciascuno dei tre partitori: uscita Uno → **1 kΩ** → nodo ingresso RFID;
dal medesimo nodo **2 kΩ** → GND. Sono tre coppie aggiuntive rispetto alle
resistenze di tastierino e LED. Si riusa il principio del
[cablaggio RFID già provato](../../docs/hardware/rc522_mini_breadboard.md),
non le coordinate fisiche di una breadboard diversa.

**D11 e D13 restano anche collegati al 595, dal lato Uno prima dei partitori.**
Non spostare SER/SRCLK del 595 sul nodo abbassato a 3,3 V. Il suo latch
rimane A0. Il programma deseleziona RFID durante le scritture al 595 e non
produce fronti sul latch durante i trasferimenti RFID. Ogni aggiornamento
riscrive l'intero byte conservando i bit dei LED.

| Pin display HW-787AB | Uno |
| --- | --- |
| 3V3 | 3.3V |
| GND | Massa comune |
| OLED_SDA | A4 |
| OLED_SCL | A5 |
| ENCODER_A, ENCODER_B, ENCODER_PUSH, BACK, CONFIRM | Scollegati in questa prova |

Il display usa lo stesso driver sperimentale dei test precedenti: indirizzo
`0x3C`, profilo compatibile 128×64, offset di due colonne, nessun framebuffer
da 1024 byte. Controller e pull-up del modulo non sono identificati con
certezza. `oled_ui.h` pilota A4/A5 solo LOW oppure INPUT senza pull-up; non
usa Wire né segnali HIGH a 5 V. Non caricare esempi Wire con questo cablaggio.

OLED e RC522 condividono l'alimentazione a 3,3 V: il successo dei test
separati non certifica il budget simultaneo. Il riferimento Uno indica
50 mA su 3.3V; capacità della Elegoo effettiva, consumi e picchi restano da
misurare. Una sorgente separata non è assunta necessaria: se quella dell'Uno
risultasse insufficiente, rivalutare l'alimentazione senza unire regolatori.
Il 595 resta a 5 V; mancano ancora i 100 nF nella configurazione disponibile.

## Avvio

1. Installare **MFRC522** di miguelbalboa/rfid dal gestore librerie; annotare
   la versione installata. Il driver OLED non richiede librerie aggiuntive.
2. Con display scollegato, aprire `test_terminal_spi.ino` mantenendo
   `oled_ui.h` nella stessa cartella; selezionare Uno e caricare.
3. Scollegare USB, completare i collegamenti e ricollegare. Monitor **9600 baud**.
4. Attesi versione RFID diversa da `0x00`/`0xFF` e schermo di prova con stato
   lettore, indicazione badge, ultimo tasto e canale LED.
5. Avvicinare un badge di prova: UID sul monitor locale e `BADGE LETTO`
   sul display. Allontanare e riavvicinare per rileggerlo. Nessuna scrittura
   sulla carta e nessuna autenticazione Crypto1 in questo sketch.
6. Premere 1/2/0: canale S / centrale / spento. Gli altri tasti vengono
   mostrati senza cambiare LED. Usare tasti fittizi, mai PIN reali.

Il badge non cambia il LED e non viene memorizzato come credenziale. `BADGE
LETTO` rimane come indicazione dell'ultima lettura fino al riavvio: non
significa che il badge sia ancora presente o che esista una sessione attiva.

## Errori e prove richieste

- Versione RFID errata all'avvio: letture disabilitate, tastierino/LED attivi.
- Versione diversa durante il controllo ogni cinque secondi: arresto del
  solo RFID fino al riavvio; controllare i fili a USB scollegata.
- Selezione badge fallita: errore seriale; allontanare e riprovare.
- OLED senza ACK o linea bloccata: timeout di 2 ms per attesa e tentativo di
  inizializzazione dopo un secondo; tastierino e RFID continuano. Non è
  implementato un recupero fisico del bus tramite impulsi supplementari.
- Ridisegno in blocchi da 16 colonne. RFID viene interrogato ogni 100 ms;
  le chiamate della libreria e i trasferimenti OLED sono sincroni: brevi
  pressioni possono essere perse e le latenze vanno misurate sull'Uno.
- Verificare tutti i tasti durante letture RFID e ridisegni, conservazione
  del LED, riavvii, lettore assente, OLED assente, errori e ripartenza.
- Restano i limiti sulle pressioni simultanee della scansione con resistenze.

La prova completa richiede hardware. Compilazione AVR e misura RAM/flash
non eseguite nell'ambiente; il controllo C++ sul computer non le sostituisce.
Il LED del varco finale e l'autorizzazione restano sul Pi.

Controlli locali eseguiti con `clang++ -std=c++11 -Wall -Wextra -Werror` e
periferiche simulate: dodici tasti nei tre stati LED, conservazione delle
uscite durante traffico RFID, lettura UID fittizio, selezione fallita e perdita
del lettore, rifiuto di versione `0xFF`, rendering di tutte le 128×64 colonne
su otto pagine e timeout OLED. Verificata nella simulazione l'assenza di
OUTPUT HIGH e INPUT_PULLUP su A4/A5. Le risposte simulate non misurano
consumi, radiofrequenza, margini elettrici, RAM AVR o latenze reali.

## Fonti

- [NXP MFRC522, rev. 3.9](https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf): SPI e limiti elettrici.
- [MFRC522.cpp](https://github.com/miguelbalboa/rfid/blob/master/src/MFRC522.cpp): transazioni SPI e API usate.
- [Test OLED del progetto](../test_display_menu/README.md): profilo, offset e limiti elettrici.
- [Adafruit SH110X](https://github.com/adafruit/Adafruit_SH110x/blob/master/Adafruit_SH110X.cpp): riferimento per offset delle colonne.
