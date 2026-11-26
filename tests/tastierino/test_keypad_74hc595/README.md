# Prova tastierino con 74HC595

**Stato, 19 novembre 2026:** configurazione della prova salvata; funzionamento
del tastierino con 74HC595 confermato dall'utente dopo la sostituzione di un
cavetto guasto. La configurazione usa QB–QE, pin fisici 1–4, per le righe e
A1–A3 dell'Uno per le colonne, senza condensatore di disaccoppiamento.

La prova riguarda Uno, 74HC595 e tastierino. Non conferma la convivenza con
RFID, display e impronta. Per ripeterla, scollegare le altre periferiche;
la foto condivisa in conversazione mostra anche altro hardware montato,
ma non prova che sia gestito contemporaneamente dallo sketch.
La variante per il terminale completo è descritta in
[terminal_integration.md](../../../docs/hardware/terminal_integration.md).

Il 74HC595 conserva otto uscite digitali impostate tramite dati, clock e latch.
Non legge ingressi: non sostituisce un GPIO bidirezionale, la UART o un bus I²C.
Nel progetto quattro uscite possono pilotare le quattro righe del tastierino;
le tre colonne rimangono ingressi diretti dell'Uno.

## Materiale e montaggio

- 74HC595 in contenitore DIP a 16 piedini; verificare sigla e produttore.
- Quattro resistenze da **1 kΩ**, una per riga del tastierino.
- Un condensatore ceramico da **100 nF**, spesso marcato `104`, raccomandato
  dal datasheet ma **non disponibile e non montato nella prova riferita**.
- Breadboard e fili; la mini già occupata dai partitori richiede un nuovo
  piano di disposizione: non sovrapporre questi collegamenti alle coordinate
  già assegnate.

Scollegare USB prima del montaggio. Inserire il DIP a cavallo del canale della
breadboard. Guardando il chip dall'alto con la tacca rivolta in alto, il pin 1
è in alto a sinistra: la colonna sinistra conta 1–8 verso il basso e quella
destra 9–16 verso l'alto. Il pin 16 è quindi in alto a destra.

| Pin fisico 74HC595 | Segnale | Collegamento |
| --- | --- | --- |
| 16 | VCC | Uno 5V |
| 8 | GND | Uno GND |
| 14 | SER / DS | Uno D11, dati |
| 11 | SRCLK / SH_CP | Uno D13, clock |
| 12 | RCLK / ST_CP | Uno A0, latch |
| 13 | /OE | GND, uscite abilitate |
| 10 | /SRCLR / /MR | 5V, reset disattivato |
| 1 | QB / Q1 | Resistenza 1 kΩ → riga `1 2 3` |
| 2 | QC / Q2 | Resistenza 1 kΩ → riga `4 5 6` |
| 3 | QD / Q3 | Resistenza 1 kΩ → riga `7 8 9` |
| 4 | QE / Q4 | Resistenza 1 kΩ → riga `* 0 #` |
| 15, 5–7 | QA, QF–QH / Q0, Q5–Q7 | Scollegati nella prova |
| 9 | QH′, uscita seriale per cascata | Scollegato; non è una nona uscita indipendente |

Il collegamento previsto per il condensatore è **fra pin 16 e pin 8**, con
collegamenti corti vicino al chip. La prova è riuscita senza questo componente:
non è una verifica di stabilità prolungata o immunità ai disturbi. La sua
assenza non si compensa con una resistenza. L'alimentazione del 595 a 5 V non
modifica le alimentazioni previste per RFID, display o sensore.

| Contatto tastierino, orientamento già calibrato | Collegamento |
| --- | --- |
| 1, riga `1 2 3` | QB, pin fisico 1, attraverso 1 kΩ |
| 2, riga `4 5 6` | QC, pin fisico 2, attraverso 1 kΩ |
| 3, riga `7 8 9` | QD, pin fisico 3, attraverso 1 kΩ |
| 4, riga `* 0 #` | QE, pin fisico 4, attraverso 1 kΩ |
| 5, colonna `1 4 7 *` | Uno A1 |
| 6, colonna `2 5 8 0` | Uno A2 |
| 7, colonna `3 6 9 #` | Uno A3 |

Nessun collegamento separato di alimentazione al tastierino passivo.

La numerazione del tastierino deriva dalla calibrazione precedente, ma il
lato di osservazione del connettore non è stato registrato: non identifica
automaticamente un ordine da sinistra a destra. La selezione delle righe nello
sketch ora usa i bit 1–4 del 595, corrispondenti ai pin fisici consecutivi 1–4.

Riferimento visivo della foto fornita il 19 novembre: con Uno a sinistra e
il cavo piatto del tastierino rivolto verso il basso, sul connettore si vedono
da sinistra a destra fili arancione, blu, verde/turchese, giallo, bianco,
rosso e nero. È un riferimento per riconoscere il montaggio fotografato,
non una misura della continuità dei fili o una nuova verifica del pinout.

## Perché servono le resistenze e limiti della prova

Si porta una riga LOW e si leggono le colonne con pull-up interno; le altre
righe sono HIGH. Due tasti della stessa colonna possono unire uscite HIGH e
LOW. **Non collegare direttamente le quattro uscite alle righe.** Le quattro
resistenze limitano la corrente fra uscite, anche con più contatti chiusi.
Non sono le resistenze dei partitori RFID: sono quattro componenti aggiuntivi.

Il test è destinato a **un tasto per volta**. Rifiuta maschere con più tasti
rilevati, ma i livelli intermedi prodotti da più pressioni possono anche
nascondere tasti: non garantisce rilevamento o rifiuto di ogni combinazione.
Prima dell'uso nel terminale occorre validare la scansione ed eventualmente
isolare le righe con diodi opportunamente orientati. Non considerare questa
prova una soluzione già validata per acquisire un PIN.

Con /OE a GND le uscite all'accensione non hanno stato garantito fino alla
prima scrittura. Qui pilotano solo contatti protetti da resistenze; non usare
questo schema di avvio per comandi che richiedono uno stato sicuro immediato.

## Avvio e risultati attesi

1. Aprire `test_keypad_74hc595.ino` in Arduino IDE, selezionare **Arduino Uno**
   e la porta USB, compilare e caricare.
2. Aprire il monitor seriale a **9600 baud** e rilasciare tutti i tasti.
3. Premere separatamente `1 2 3 4 5 6 7 8 9 * 0 #`: deve apparire il carattere
   corretto una volta per pressione. Usare esclusivamente tasti fittizi, mai PIN reali.
4. Tenere premuto un tasto: nessuna ripetizione. Rilasciare almeno 35 ms e
   ripremere: deve comparire un nuovo messaggio.
5. Verificare assenza di messaggi a riposo e ripartenza dopo RESET/USB.
6. Registrare il comportamento con pressioni simultanee, senza presumere
   che vengano sempre riconosciute; le resistenze devono rimanere montate.

Lo sketch usa Arduino core e SPI, senza librerie esterne. Non gestisce RFID,
display, impronta, sessioni, autorizzazioni o LED del varco.

## Integrazione futura: tre GPIO recuperati

Per la prova con LED bicolore su QF/QG usare lo sketch separato
[test_keypad_74hc595_two_color](../test_keypad_74hc595_two_color/README.md).
Con il LED collegato, questo sketch originale porta entrambi i canali HIGH
a riposo: non conserva uno stato indipendente del LED.

D11 e D13 sono già MOSI e SCK del lettore RFID. Collegare SER e SRCLK del 595
**dal lato Uno a 5 V, prima dei partitori**; il lettore mantiene i propri
partitori a 3,3 V. Non prelevare per il 595 a 5 V i segnali già abbassati.

Le righe lasciano D7, D8, D9 e A0; A0 diventa il latch. Si recuperano quindi
**D7, D8 e D9**, mentre le tre colonne restano A1–A3. Le altre quattro uscite
del 595 sono disponibili per segnali di uscita compatibili, non per leggere
BACK, CONFIRM o encoder. I LED del varco rimangono sul Pi.

Il firmware unico dovrà serializzare gli accessi SPI: SS del lettore HIGH
durante ogni scrittura al 595; nessun fronte crescente del latch durante i
trasferimenti RFID; riscrivere tutti gli otto bit prima di ogni aggiornamento.
Il 595 riceve anche i clock dell'RFID ma le uscite conservano lo stato fino al
fronte del latch. Usare transazioni SPI con configurazione adatta al dispositivo.
Non caricare questo sketch come firmware integrato.

## Fonti e verifiche

- [Texas Instruments, SNx4HC595, SCLS041J, revisione J, ottobre 2021](https://www.ti.com/lit/ds/symlink/sn74hc595.pdf),
  sezioni 5, 6, 8 e 10: piedinatura, livelli, registri, latch e disaccoppiamento.
  Se il chip è di un altro produttore, controllarne il datasheet effettivo.
- [Mappa integrata del progetto](../../../docs/hardware/terminal_integration.md):
  assegnazione GPIO, partitori, perimetro e verifiche ancora aperte.

Eseguito controllo C++11 con `clang++ -Wall -Wextra -Werror` e simulazione
con sostituti di Arduino/SPI: corrispondenza dei dodici tasti, righe a riposo,
antirimbalzo, pressione mantenuta, rilascio e rifiuto di una maschera multipla.
La simulazione usa contatti ideali: non modella livelli intermedi, rimbalzi
elettrici, ghosting o trasferimenti RFID concorrenti.

La modifica per QB–QE è stata sottoposta a controllo sintattico C++11 con
gli stessi avvisi abilitati. La compilazione AVR non è stata eseguita
dall'agente: `arduino-cli` e il compilatore AVR non sono disponibili
nell'ambiente. Lo sketch è stato caricato ed eseguito sulla scheda dall'utente.
Il controllo C++ sul computer non misura RAM/flash dell'Uno né valida
cablaggio e temporizzazioni.

## Esito hardware riferito — 19 novembre 2026

| Verifica | Evidenza e risultato |
| --- | --- |
| Ingressi A1, A2, A3 | Con colonne scollegate, collegamento temporaneo di ciascun ingresso a GND: ricevuti tre messaggi `Piu' tasti rilevati: rilascia tutto.`, coerenti con la prova richiesta. |
| Mancata risposta della prima colonna | L'utente ha identificato un cavetto guasto e lo ha sostituito. Non era un guasto dimostrato del 74HC595 o di A1. |
| Tastierino con 74HC595 | Dopo la sostituzione, l'utente conferma «ora funziona» e chiede di salvare questa configurazione. Non è stato fornito un elenco separato degli esiti dei dodici tasti. |
| Disaccoppiamento | Prova senza condensatore da 100 nF, non disponibile. |

Restano da verificare su questa configurazione: comportamento di tutti i
dodici tasti con risultati registrati, pressione mantenuta e rilascio,
pressioni simultanee e ghosting, stabilità prolungata, disturbi, riavvii e
convivenza SPI con RFID e con le altre periferiche. La prova non conferma
autenticazione, autorizzazione, sessioni o i tre scenari completi del varco.
