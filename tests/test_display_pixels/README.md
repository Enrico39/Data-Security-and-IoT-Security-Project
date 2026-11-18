# Prova visiva sperimentale del display

Il display risponde ripetutamente a `0x3C`, ma lo scanner ha mostrato anche
errori intermittenti. L'utente ha confermato che il connettore non è saldato.
La prova seguente può mostrare i pixel; non certifica stabilità o assenza di
guasti. Un esito negativo resta ambiguo finché i contatti sono provvisori.

## Uso

1. Con display scollegato, caricare `test_display_pixels.ino` nell'Arduino IDE,
   selezionando Arduino Uno. Nessuna libreria aggiuntiva.
2. Scollegare USB e collegare `3V3` a `3.3V`, `GND` a `GND`, `OLED_SDA` ad
   `A4`, `OLED_SCL` ad `A5`. Tenere scollegati tutti gli altri pin e periferiche.
3. Ricollegare USB e aprire il monitor seriale a **9600 baud**.
4. Cercare sullo schermo una **cornice con scacchiera** che cambia ogni tre
   secondi. Annotare ritagli, spostamenti, righe mancanti e sfarfallii.

Si usa lo stesso bus software dello scanner: solo uscite LOW oppure ingressi
senza pull-up, mai segnali pilotati a 5 V. La presenza di pull-up sul modulo
verso 3,3 V resta un'ipotesi. Non usare esempi Wire con questo cablaggio senza
rivalutare i livelli elettrici. Non muovere fili alimentati: scollegare USB
prima di riposizionarli ed evitare contatti tra fori adiacenti.

## Interpretazione

Il profilo **SSD1306 128×64** è un tentativo di compatibilità basato sulla
libreria suggerita dalla pagina Manuals+ fornita dall'utente. Controller e
geometria non sono identificati con certezza. Non vengono provati in automatico
altri profili. Il disegno usa indirizzamento a pagine e contrasto moderato,
senza framebuffer da 1024 byte in RAM.

- Cornice e scacchiera visibili: il modulo visualizza dati con questo profilo;
  non costituisce identificazione certa del chip.
- ACK ricevuti ma schermo nero: possono esserci parametri o controller diversi,
  problemi di contatto o alimentazione. Gli ACK non provano che i pixel funzionino.
- Errore seriale: trasferimento incompleto o bus non pronto; viene ritentata
  l'inizializzazione dopo tre secondi. Il disegno può rimanere parziale.

## Esito hardware — 17 novembre 2026

L'utente ha eseguito lo sketch e comunicato che la prova visiva funziona.
Il profilo grafico è quindi utilizzabile come base di compatibilità per le
prove successive; non identifica con certezza il controller. Non sono stati
forniti misure, fotografie del disegno o dettagli su eventuali ritagli.
I contatti restano non saldati e la stabilità non è dimostrata. Encoder e
pulsanti non sono coinvolti in questa prova.

## Riferimenti tecnici

- [Adafruit SSD1306: implementazione e sequenza di inizializzazione](https://github.com/adafruit/Adafruit_SSD1306/blob/master/Adafruit_SSD1306.cpp).
- [Datasheet SSD1306 distribuito da Adafruit](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf).
- [Dettagli elettrici della prova con GPIO](../test_display/README.md).
