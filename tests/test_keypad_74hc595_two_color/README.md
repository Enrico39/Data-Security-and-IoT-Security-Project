# Tastierino, 74HC595 e LED bicolore

**Stato, 19 novembre 2026:** estensione da banco preparata, non ancora provata
su hardware. Deriva dalla [configurazione tastierino funzionante](../test_keypad_74hc595/README.md),
che conserva il proprio sketch separato. Nessun GPIO Uno aggiuntivo occupato.

## Modulo e identificazione

La foto dell'utente mostra un LED bicolore a tre contatti, una serigrafia `-`
a sinistra e `S` a destra sul connettore, e R1 marcata `0`. Questa resistenza
da zero ohm è un ponticello: non limita la corrente del LED.

L'aspetto è compatibile con la famiglia KY-011/KY-029 a catodo comune;
produttore e modello esatto non sono identificati dalla sola foto.
La documentazione Joy-IT del KY-011 conferma il catodo comune e la necessità
di resistenze in serie. Il collegamento usa il contatto marcato `-` come
massa e gli altri due come canali. Non assegnare definitivamente rosso e verde
a centrale/S prima della prova sul modulo effettivo.

## Collegamenti aggiuntivi

Scollegare USB prima di montare. Guardare il modulo **come nella foto**, con
LED in alto, componenti visibili e tre contatti del connettore verso il basso.

| Contatto modulo | Collegamento |
| --- | --- |
| Sinistro, marcato `-` | GND comune con Uno e 74HC595 |
| Centrale | Un capo di una nuova resistenza da **1 kΩ**; altro capo al **pin fisico 6, QG**, del 74HC595 |
| Destro, marcato `S` | Un capo di un'altra nuova resistenza da **1 kΩ**; altro capo al **pin fisico 5, QF**, del 74HC595 |

Sono **due resistenze aggiuntive**: le quattro da 1 kΩ sulle righe del
tastierino restano montate. Non mettere un'unica resistenza condivisa sul
contatto di massa. Le resistenze non hanno polarità; ciascuna deve unire
due nodi distinti della breadboard.

Si scelgono 1 kΩ per contenere la corrente sulle uscite del 595: con 5 V e
caduta LED indicativa di 2–2,5 V, la stima è 2,5–3 mA per canale, con possibile
riduzione di luminosità. Non si usa il valore di 220 Ω dell'esempio Joy-IT
come valore universale per il 595. TI specifica il pilotaggio a circa 6 mA
a 5 V: i massimi assoluti non sono correnti di lavoro da raggiungere.

Le uscite QB–QE (pin 1–4) restano dedicate alle righe; QA (15), QH (7) e
l'uscita seriale QH′ (9) restano scollegate. D11, D13, A0 e A1–A3 mantengono
la mappa già salvata. Nessun collegamento del modulo direttamente a 5 V.

## Avvio e comportamento atteso

Per aggiungere RFID e OLED è disponibile la
[prova combinata test_terminal_spi](../test_terminal_spi/README.md), con un
solo sketch per tutti questi dispositivi. Non usare il test isolato RFID
o il menu display con il montaggio combinato.

1. Scollegare RFID, display e sensore d'impronta per questa prova.
2. Caricare `test_keypad_74hc595_two_color.ino` selezionando Arduino Uno.
3. Aprire il monitor seriale a **9600 baud**; rilasciare tutti i tasti.
4. Premere **1**: si accende solo il canale collegato a S.
5. Rilasciare e premere **2**: si accende solo il canale centrale.
6. Premere **0**: entrambi spenti.
7. Provare gli altri tasti: vengono stampati e il colore selezionato resta
   invariato durante la scansione. Usare solo tasti fittizi, mai PIN reali.

Registrare quale colore corrisponde a ciascun canale. Lo sketch conserva
i bit QF/QG in ogni scrittura al registro; il vecchio sketch del tastierino
scrive `0xFF` a riposo e accende entrambi i canali se il LED resta collegato.

All'avvio software i LED vengono spenti; con /OE già a GND è possibile un
lampo prima della prima scrittura, perché lo stato iniziale delle uscite
non è garantito. Il condensatore da 100 nF resta raccomandato e manca nella
configurazione disponibile. Non è verificata l'immunità ai disturbi.

Questo è un indicatore per una **prova da banco**, senza autenticazione o
autorizzazione. Non simula un accesso autorizzato premendo 1 o 2. Nel progetto
completo la decisione e il LED del varco restano gestiti dal Raspberry Pi.
Non sono implementati temporizzatore di cinque secondi o sessioni.

## Verifiche e fonti

Eseguiti compilazione C++11 con `clang++ -Wall -Wextra -Werror` e controllo
con sostituti Arduino/SPI: dodici tasti in ciascuno dei tre stati LED,
conservazione dei canali durante ogni trasferimento di scansione, comandi
1/2/0, pressione mantenuta e maschera multipla. Il modello usa contatti ideali
e non verifica correnti, livelli elettrici, disturbi, ghosting o luminosità.

La prova hardware, l'associazione dei colori ai canali e la compilazione AVR
restano da eseguire. Si applicano i limiti delle pressioni simultanee del
test tastierino con resistenze: non è garantito il rifiuto di ogni combinazione.

- [Joy-IT SensorKit, KY-011](https://sensorkit.joy-it.net/en/sensors/ky-011):
  modulo di riferimento a catodo comune, caduta LED e resistenze in serie;
  non identifica il produttore del modulo fotografato.
- [TI SNx4HC595, SCLS041J, revisione J, ottobre 2021](https://www.ti.com/lit/ds/symlink/sn74hc595.pdf),
  sezioni 5, 6, 8 e 10: uscite, limiti elettrici, registri e disaccoppiamento.
