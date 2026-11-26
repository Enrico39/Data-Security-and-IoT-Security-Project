# Prima prova display HW-787AB: indirizzo I²C

Prova isolata con Elegoo Uno R3, display e quattro fili, senza librerie
aggiuntive. Il controller e la risoluzione del display non sono confermati.

## Collegamenti

1. Con il display ancora scollegato, caricare **questa versione** dello sketch.
2. Scollegare USB e tutte le altre periferiche, compreso il tastierino.
3. Collegare i quattro fili secondo le etichette della scheda.
4. Ricollegare USB e aprire il monitor seriale a **9600 baud**.

| Display | Uno |
| --- | --- |
| `3V3` | `3.3V` |
| `GND` | `GND` |
| `OLED_SDA` | `A4` |
| `OLED_SCL` | `A5` |

Lasciare scollegati `CONFIRM`, `ENCODER_PUSH`, `ENCODER_A`, `ENCODER_B` e
`BACK`. Non collegare il display a 5 V. Seguire le etichette, non l'ordine
apparente dei contatti nelle foto.

## Perché questa versione non usa il convertitore

Il precedente scanner basato su Wire richiedeva un convertitore I²C. Questa
versione usa invece i GPIO come bus software: ogni linea può essere soltanto
portata a zero o rilasciata come ingresso **senza pull-up interno**. Non usa
Wire, `INPUT_PULLUP` o uscite HIGH su A4/A5.

È necessario che il modulo abbia già pull-up verso la propria alimentazione
a 3,3 V. Questa presenza non è confermata da uno schema: la prova è
condizionata a tale ipotesi. Se mancano, i fili da soli non bastano per un bus
funzionante. Lo scanner non misura le resistenze e non ne certifica la presenza.

L'ATmega328P legge i pin come GPIO: il datasheet 7810D–AVR–01/15, sezione
28.2, indica VIH minimo pari a 0,6 VCC (3 V con VCC a 5 V). Non si usa la
periferica TWI, che ha specifiche elettriche distinte. Il margine va rivalutato
nell'integrazione definitiva, insieme ad alimentazione e cablaggi reali.

**Il collegamento diretto è previsto soltanto con questo sketch.** Prima di
caricare librerie grafiche o esempi basati su Wire, scollegare il display:
quei programmi possono attivare pull-up a 5 V. Non dedurre da questa prova
che il modulo tolleri segnali a 5 V.

## Risultati

- `Dispositivo trovato a 0x3C` (o altro indirizzo): annotare il risultato.
- `Nessun dispositivo rilevato`: nessuna conferma; non dimostra un guasto.
- `Linee non pronte`: linea bassa, collegamento errato o pull-up assenti/inadeguati;
  il test non distingue queste cause.
- Molti indirizzi rilevati: risultato sospetto, non identificazione valida.

Gli ingressi senza pull-up possono fluttuare se i pull-up del modulo mancano;
anche letture apparentemente valide vanno confermate con la successiva prova
funzionale. La scansione interroga soltanto indirizzi non riservati e impone
un timeout di 2 ms nell'attesa del livello alto. Non è un driver I²C generale:
è una sonda diagnostica per un solo dispositivo su un bus isolato.

**Lo schermo può restare nero.** Lo scanner cerca un ACK, non inizializza i
pixel e non identifica SSD1306/SH1106. Encoder e pulsanti restano da provare.
Nessuna prova hardware del display è ancora registrata.

## Documentazione consultata

- [ATmega328P, datasheet 7810D–AVR–01/15, sezioni 13 e 28.2](https://docs.arduino.cc/resources/datasheets/ATmega328P-datasheet.pdf).
- [Arduino: modalità dei pin](https://docs.arduino.cc/language-reference/en/variables/constants/inputOutputPullup/).
- [Core AVR: pull-up abilitati da TWI](https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/Wire/src/utility/twi.c).
- [Pagina Manuals+ HW-787AB fornita dall'utente](https://manuals.plus/ae/3256812024178410):
  riporta 3,3–5 V e suggerisce SSD1306, ma non fornisce schema elettrico,
  limiti dei segnali o identificazione verificabile del controller. È una
  fonte secondaria e non risolve questi punti.

La compilazione per Uno e la prova hardware restano da eseguire nell'Arduino
IDE. Il controllo C++ sul computer non sostituisce queste verifiche.
