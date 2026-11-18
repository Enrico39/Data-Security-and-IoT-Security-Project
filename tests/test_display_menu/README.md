# Menu di prova HW-787AB

Demo autonoma per Elegoo Uno R3, senza librerie aggiuntive. Riutilizza il
profilo grafico 128×64 a `0x3C` del test pixel che l'utente ha
confermato funzionante, con inizializzazione compatibile SSD1306 e offset
orizzontale di due colonne in prova. La compatibilità osservata non identifica
il chip.

## Collegamenti

Caricare `test_display_menu.ino` con il modulo scollegato. Scollegare USB,
effettuare i collegamenti e ricollegare USB. Tastierino, RFID e sensore
d'impronta restano scollegati: D2–D6 sono riutilizzati solo per questa prova.

| Pin del modulo | Pin Uno |
| --- | --- |
| `3V3` | `3.3V` |
| `GND` | `GND` |
| `OLED_SDA` | `A4` |
| `OLED_SCL` | `A5` |
| `ENCODER_A` | `D2` |
| `ENCODER_B` | `D3` |
| `ENCODER_PUSH` | `D4` |
| `BACK` | `D5` |
| `CONFIRM` | `D6` |

Usare le etichette riportate sul retro del modulo. Non collegare alcun pin
del modulo a 5 V. Non spostare i fili con USB collegata. Il connettore non
saldato ha già prodotto comunicazione intermittente: un esito negativo con
questi contatti rimane ambiguo.

## Comandi

| Comando | Nel menu | Nella pagina Contatore |
| --- | --- | --- |
| Rotella | Cambia voce | Modifica il valore fra 0 e 99 |
| Pressione rotella | Apre la voce | Azzera il valore |
| CONFIRM | Apre la voce | Azzera il valore |
| BACK | Resta nel menu | Torna al menu |

Il menu contiene **Stato**, **Contatore** e **Informazioni**. Nelle pagine
Stato e Informazioni, BACK torna al menu e gli altri comandi non compiono
operazioni. Tutti i dati sono fittizi e locali; nessuna decisione di accesso,
enrollment o controllo LED è implementata. Contatore e selezione si perdono
al riavvio. Monitor seriale facoltativo a **9600 baud** per diagnosi.

## Ipotesi elettriche da verificare

Il bus usa solo uscite LOW e ingressi senza pull-up; i controlli sono sempre
ingressi senza pull-up. Non vengono quindi applicati pull-up a 5 V ai segnali
del modulo. Per funzionare, questa prova **presuppone pull-up sul modulo
verso 3,3 V**, sia per il bus sia per encoder e pulsanti. Presuppone inoltre
pulsanti attivi LOW, con contatto verso massa. Questi dettagli non sono
confermati da uno schema del produttore: il test non ne certifica la presenza.

Se il menu si muove da solo o i tasti non rispondono, non abilitare
`INPUT_PULLUP` a 5 V: occorre verificare i contatti e i pull-up a 3,3 V.
Il bus software già provato non rende automaticamente utilizzabili i
programmi basati su Wire con il medesimo cablaggio.

## Prove da eseguire

1. All'avvio compare MENU DEMO con tre voci; a riposo la selezione resta ferma.
2. La rotazione sposta la selezione nei due sensi, anche attraversando il limite
   fra la prima e l'ultima voce.
3. CONFIRM e la pressione della rotella aprono ciascuno la voce selezionata.
4. BACK torna al menu conservando la selezione.
5. Nel Contatore, la rotazione modifica il valore senza uscire da 0–99; i due
   comandi OK lo azzerano.
6. Tenere premuto un pulsante non ripete l'azione; dopo rilascio e nuova
   pressione viene accettata un'altra azione. Antirimbalzo: 35 ms.
7. Ripetere con rotazione lenta; rotazioni rapide possono saturare la coda
   limitata a dieci passi e non sono una prova di precisione dell'encoder.

L'encoder è acquisito tramite interrupt su D2 e D3. Sono richieste quattro
transizioni per passo (`transitionsPerStep`): se il modello produce due
transizioni per scatto, si osserverà un movimento ogni due scatti; verificare
questo comportamento prima di cambiare la costante. Il verso dipende dal
cablaggio A/B. Il display viene aggiornato in blocchi di sedici colonne per
interrogare i pulsanti fra i trasferimenti; il ridisegno completo può essere
percepibile. Gli interrupt restano attivi durante i trasferimenti.

In caso di errore del bus, lo sketch rilascia le linee e ritenta
l'inizializzazione dopo un secondo. La selezione rimane in RAM: il recupero
fisico da un'interruzione non è ancora verificato. Nessuna misura dei tempi
o prova hardware del recupero è ancora registrata.

## Esito hardware e correzione dei bordi — 17 novembre 2026

L'utente ha compilato la prima versione nell'Arduino IDE: 6670 byte di flash
e 666 byte di variabili globali, secondo l'output fornito. Ha successivamente
mostrato il menu sul display e confermato il funzionamento dei tasti. Non è
stato fornito un esito distinto per ogni prova dell'encoder e dei pulsanti.

La foto mostra prime lettere tagliate sul bordo sinistro e pixel residui sul
bordo destro. È un indizio di offset fra memoria e area visibile, compatibile
con pannelli SH1106; non identifica con certezza il controller. Sono presenti
anche artefatti nel testo: i contatti provvisori restano un'altra possibile
causa di trasferimenti incompleti.

La versione aggiornata aggiunge `displayColumnOffset = 2` a ogni blocco di
scrittura, prima di separare l'indirizzo di colonna nei due nibble. Le 128
colonne logiche sono scritte nelle colonne RAM 2–129, senza cambiare il
contenuto dei caratteri. Questo segue l'approccio di offset usato dal driver
Adafruit SH110X. L'inizializzazione e il cablaggio restano quelli della prova
precedente. La correzione deve ancora essere confermata sul modulo reale.

Dopo il caricamento verificare la M completa in MENU DEMO, la scomparsa dei
pixel residui a destra e la pulizia delle pagine dopo più cambi di schermata.

## Riferimenti

- [Cablaggio elettrico del test display](../test_display/README.md).
- [Prova pixel precedente](../test_display_pixels/README.md).
- [Arduino: attachInterrupt](https://docs.arduino.cc/language-reference/en/functions/external-interrupts/attachInterrupt/).
- [Adafruit SH110X: offset nell'indirizzamento di pagina](https://github.com/adafruit/Adafruit_SH110x/blob/master/Adafruit_SH110X.cpp).
