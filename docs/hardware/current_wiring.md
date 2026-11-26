# Banco a due fattori — 26 novembre 2026

Elegoo Uno R3 con RC522 MINI, display HW-787AB e tastierino passivo 3×4.
Lo [sketch di prova](../../tests/test_rc522_oled_pin/test_rc522_oled_pin.ino) usa badge → PIN.
AS608, 74HC595, rotella e pulsanti del display sono scollegati.

| Elemento | Collegamenti sull’Uno |
|---|---|
| RC522 MINI | SDA/SS → D10, MOSI → D11, MISO → D12, SCK → D13. 3V3 e RST → 3,3 V, GND comune, IRQ libero. |
| Display | OLED_SDA → A4, OLED_SCL → A5, 3V3 → 3,3 V, GND comune. Lo sketch usa I²C software senza pull-up a 5 V. |
| Tastierino, contatti 1–4 | Righe da cima a fondo → D7, D8, D9, A0 rispettivamente. |
| Tastierino, contatti 5–7 | Colonne da sinistra a destra → A1, A2, A3 rispettivamente. Sette fili diretti, pull-up interne, nessun filo di alimentazione. |

Per la numerazione del tastierino mantenere l’orientamento della
[calibrazione originale](../../tests/test_keypad/README.md).
D0/D1 sono riservati alla seriale USB. D2–D6 restano liberi.

Nella prova RFID riferita il 26 novembre i segnali erano diretti, senza resistenze.
Il cablaggio previsto usa un partitore su ciascuna uscita D10, D11 e D13:
1 kΩ tra uscita Uno e ingresso RC522, 2 kΩ tra ingresso RC522 e massa.
MISO è diretto. Il budget di corrente complessivo sulla linea 3,3 V resta da verificare.

Per la prova presentare un badge, digitare il PIN fittizio `1234` e premere `#`.
Ogni cifra resta visibile per 650 ms. Il tasto `*` cancella una cifra e, a campo vuoto,
torna al badge. Il tentativo scade dopo 30 secondi dalla lettura del badge.
Lo sketch accetta qualunque UID leggibile. L’associazione badge-operatore,
la verifica del PIN sul Pi e la decisione secondo ruolo, turno e revoca appartengono al progetto.

---

## Archivio del banco — 19 novembre 2026

**Riferimento per riprendere il montaggio senza rifare i collegamenti.**
Configurazione con Uno, 74HC595, tastierino, LED bicolore e RC522 MINI sui
pin personalizzati indicati dall'utente. **Restano da collegare impronta e
schermo**, inclusi encoder e CONFIRM secondo il piano sotto.

Il funzionamento del tastierino è stato confermato dopo la sostituzione di
un cavetto guasto. Per il LED non è stato registrato l'esito distinto dei
due colori. L'utente ha confermato che il badge veniva letto con questa
configurazione dopo la correzione del conflitto software su D12. È una prova
di lettura del badge, non di autenticazione Crypto1 o autorizzazione.

## Uno e 74HC595

| Pin fisico 74HC595 | Collegamento attuale |
| --- | --- |
| 16 — VCC | Uno 5V |
| 8 — GND | GND comune |
| 10 — /SRCLR | Uno 5V |
| 13 — /OE | GND comune |
| 14 — SER | Uno D11 |
| 11 — SRCLK | Uno D13 |
| 12 — RCLK / latch | Uno A0 |
| 1 — QB | Resistenza 1 kΩ in serie → riga tastierino `1 2 3` |
| 2 — QC | Resistenza 1 kΩ in serie → riga `4 5 6` |
| 3 — QD | Resistenza 1 kΩ in serie → riga `7 8 9` |
| 4 — QE | Resistenza 1 kΩ in serie → riga `* 0 #` |
| 5 — QF | Resistenza 1 kΩ in serie → contatto `S` del LED bicolore |
| 6 — QG | Resistenza 1 kΩ in serie → contatto centrale del LED |
| 15 — QA, 7 — QH, 9 — QH′ | Scollegati |

Il 595 è pilotato **in software**, senza `SPI.begin()` o transazioni SPI
hardware. Lo SPI hardware dell'Uno renderebbe D12/MISO un ingresso e
impedirebbe di usarlo come uscita di selezione per l'RFID.
Il condensatore da 100 nF fra VCC e GND resta raccomandato, ma non è
disponibile e non è stato montato nella prova riferita.

## Tastierino e LED

| Linea | Collegamento |
| --- | --- |
| Colonna `1 4 7 *` | Uno A1 direttamente |
| Colonna `2 5 8 0` | Uno A2 direttamente |
| Colonna `3 6 9 #` | Uno A3 direttamente |
| Contatto `-` del modulo LED | GND comune |

Le quattro righe sono nella tabella del 595. Il tastierino passivo non
richiede alimentazione propria. La numerazione del connettore derivata
dalla calibrazione non definisce da sola un verso sinistra/destra; conservare
i fili attuali e riferirsi ai gruppi di tasti sopra.

LED guardato come nella foto: LED in alto, componenti visibili, connettore in
basso; sinistro `-`, centrale primo canale, destro `S` secondo canale. Non è
ancora registrato quale dei due canali sia rosso o verde. Le due resistenze
da 1 kΩ del LED sono aggiuntive alle quattro delle righe; R1 marcata `0` sul
modulo non limita la corrente. È un LED da banco, non una decisione di
spostare il LED del varco finale dal Pi all'Uno.

## RC522 MINI — mappa personalizzata attuale

| Pin RC522 | Collegamento |
| --- | --- |
| SDA / SS | Uno **D12**, attraverso partitore |
| SCK | Uno **D10**, attraverso partitore |
| MOSI | Uno **D9**, attraverso partitore |
| MISO | Uno **D8**, diretto |
| 3V3 | Uno 3.3V |
| RST | 3.3V |
| GND | GND comune |
| IRQ | Scollegato |

Per ciascuno dei tre partitori, un terminale della **1 kΩ** va all'uscita
Uno e l'altro al nodo diretto al lettore. Dal medesimo nodo una **2 kΩ** va
a GND. I tre nodi segnale devono essere distinti; le masse sono comuni.
Servono quindi tre resistenze da 1 kΩ e tre da 2 kΩ per l'RFID, oltre alle
sei da 1 kΩ di tastierino/LED. Non usare le vecchie coordinate di breadboard
come se descrivessero automaticamente il montaggio attuale.

Non collegare l'RFID anche a D11/D13: qui il suo bus è separato dal 595.
Non alimentare RFID a 5 V e non collegare direttamente le tre uscite Uno
agli ingressi del lettore senza i partitori.

## Collegamenti ancora mancanti — pin riservati

| Componente / segnale futuro | Pin Uno riservato | Stato |
| --- | --- | --- |
| OLED SDA | A4 | Da collegare; driver software solo LOW/INPUT senza pull-up a 5 V |
| OLED SCL | A5 | Da collegare; stesso vincolo di SDA |
| Display 3V3 / GND | 3.3V / GND comune | Da collegare; verificare budget 3,3 V insieme al lettore |
| Display ENCODER_A | D2 | Da collegare e integrare nel firmware, senza pull-up a 5 V |
| Display ENCODER_B | D3 | Da collegare e integrare nel firmware, senza pull-up a 5 V |
| Display CONFIRM | D4 | Da collegare e integrare nel firmware, senza pull-up a 5 V |
| Display BACK / ENCODER_PUSH | Nessuno | Restano scollegati; `*` previsto per indietro/annullamento |
| TX sensore impronta → RX Uno | D5 | Riservato; modulo, livelli e pinout da confermare |
| TX Uno → RX sensore impronta | D6 | Riservato; adattamento dei livelli da confermare prima del cablaggio |
| Alimentazione impronta | Da confermare sul modulo reale | Non dedurre tensione o colori dei fili dal solo nome AS608 |

D0/D1 restano riservati alla seriale USB verso il Pi. **D7 resta libero**;
D8/D9 sono ora occupati dall'RFID. Con i sette segnali futuri del display e
del sensore si occupano 17 dei 18 GPIO disponibili dopo D0/D1, senza contare
le uscite aggiuntive del 595. Questo è un conteggio dei pin, non una prova
di compatibilità elettrica o temporale dell'insieme.

## Sketch da mantenere insieme al cablaggio

Usare [test_terminal_soft_spi.ino](../../tests/test_terminal_soft_spi/test_terminal_soft_spi.ino),
con `soft_rfid_driver.h` e `oled_ui.h` nella stessa cartella e libreria
**MFRC522v2**. Monitor seriale **9600 baud**. Tasti fittizi 1/2/0: canale S,
centrale e LED spento. L'UID è stampato solo per diagnostica con badge di prova.

Lo sketch include già la gestione grafica OLED, anche se qui lo schermo è
segnato ancora da collegare: senza display il bus va in timeout e riprova.
Non implementa ancora impronta, encoder, CONFIRM, sessioni o autenticazione.
Prima di collegare quelle periferiche servirà adattare il firmware; non
caricare i relativi sketch isolati sul banco completo.

Dettagli e limiti in [test_terminal_soft_spi](../../tests/test_terminal_soft_spi/README.md)
e nel [piano di integrazione](terminal_integration.md). I tre scenari di
accesso finale non sono ancora verificati con questo montaggio.
