# Prove RC522 MINI con Arduino Uno

Due sketch separati, da caricare uno alla volta con Arduino IDE.
Il montaggio sulla mini breadboard è stato comunicato dall'utente.

## Esiti hardware comunicati — 18 novembre 2026

- Prima prova con `VersionReg: 0x00`: lettore non raggiungibile.
- Prova successiva: `VersionReg: 0x82`, cinquanta letture coerenti,
  scrittura/rilettura e ripristino del registro riusciti, `PASS SPI`.
- Test badge: lettura UID riuscita, tipo indicato dalla libreria **MIFARE 1KB**
  e successivo controllo di raggiungibilità del lettore riuscito.

L'UID reale non è trascritto nella documentazione. Il valore `0x82` non
identifica con certezza il chip. Non sono ancora confermati dieci tentativi,
più riavvii, una seconda carta o l'autenticazione del settore.
La prova successiva è in [test_rc522_auth](../test_rc522_auth/README.md).

## Cablaggio usato il 26 novembre 2026 per la prova del solo RC522 MINI

Collegamenti riportati per il montaggio su breadboard fotografato durante la prova:

| Elegoo Uno R3 | RC522 MINI | Collegamento |
| --- | --- | --- |
| D10 | SDA (SS) | Diretto |
| D11 | MOSI | Diretto |
| D12 | MISO | Diretto |
| D13 | SCK | Diretto |
| 3.3V | 3V3 e RST | Alimentazione e reset alto |
| GND | GND | Massa comune |
| — | IRQ | Scollegato |

Il log seriale mostra due UID distinti letti con successo, entrambi classificati
come MIFARE 1KB, e alcuni tentativi di selezione falliti. La causa degli errori
resta indeterminata. In questa prova D10, D11 e D13 erano collegati senza
partitori. Per il montaggio previsto dal progetto si usa lo
[schema con adattamento da 5 V a 3,3 V](../../../docs/hardware/rc522_mini_breadboard.md).

## Preparazione comune

- Mantenere [il cablaggio con i tre partitori](../../../docs/hardware/rc522_mini_breadboard.md).
- Display, tastierino e sensore d'impronta restano scollegati.
- Installare dal gestore librerie **MFRC522** (progetto miguelbalboa/rfid).
  Non serve MFRC522v2. Annotare la versione realmente installata.
- Selezionare **Arduino Uno** e la porta USB attuale.
- Monitor seriale a **9600 baud**. RESET ripete l'avvio.
- Non spostare fili sotto alimentazione. Il reset del lettore resta a 3,3 V:
  gli sketch usano `0xFF` come pin reset non gestito dall'Uno.

## Test 1 — comunicazione con il lettore

Aprire `../test_rc522_spi/test_rc522_spi.ino` e caricare.
Non occorre un badge. La prova:

1. Legge `VersionReg` e rifiuta `0x00` e `0xFF`.
2. Ripete cinquanta letture della versione con intervalli di 20 ms.
3. Solo se coerenti, spegne l'antenna, alterna otto volte il bit 6 di
   `TxASKReg` e verifica la rilettura; ripristina e verifica il valore originale.
4. Stampa `PASS SPI` solo se queste verifiche hanno successo.

Il registro della modulazione viene ripristinato anche se una rilettura
fallisce; un fallimento del ripristino viene segnalato. L'antenna rimane
spenta: caricare il secondo sketch prima di provare un badge. Non si usa un
pin reset a 5 V e non si scrive sulla memoria del badge.

Il valore di versione non certifica provenienza o autenticità del chip.
La stabilità osservata in una prova breve non misura livelli elettrici,
consumi o qualità dei fronti SPI. I tempi sono imposti da sketch e libreria:
non sono misure di prestazioni del dispositivo.

## Test 2 — lettura UID e tipo PICC

Aprire `test_rc522.ino`, caricare e avvicinare un badge di prova.
Il risultato atteso è UID, tipo indicato dalla libreria e `LETTURA OK`.
Allontanare il badge dall'antenna e riavvicinarlo per ripetere la lettura,
perché dopo il successo viene inviato HALT alla carta.

Ogni cinque secondi il test controlla nuovamente la versione del lettore.
Un valore diverso arresta le letture e richiede ricontrollo e riavvio:
non è implementato un recupero automatico a caldo.

La selezione fallita viene segnalata, ma una carta non compatibile potrebbe
non essere rilevata affatto. L'assenza di risposta non permette di distinguere
automaticamente carta incompatibile, antenna, alimentazione o cablaggio.

Usare dati di prova. L'UID è mostrato soltanto sul monitor seriale locale;
nelle evidenze condivise sostituirlo con un valore fittizio. Nessun PIN,
template o dato biometrico viene acquisito. Non si leggono né scrivono settori
applicativi della carta. **UID e tipo PICC non provano challenge-response** e
non identificano con certezza ogni variante commerciale del badge.

## Verifiche hardware da registrare

- PASS SPI e versione osservata, dopo più avvii a freddo.
- Almeno dieci presentazioni separate dello stesso badge con UID coerente.
- Nessuna lettura duplicata mantenendo fermo un badge dopo HALT.
- Eventuale secondo badge: lettura e tipo rilevato, senza dedurre unicità
  garantita o resistenza alla clonazione dall'UID.
- Stabilità con i collegamenti fermi; correggere contatti a USB scollegata.

La versione corrente della libreria usa di default SPI a 4 MHz. Non è stato
modificato questo parametro: una configurazione diversa va verificata nella
libreria effettivamente compilata, non impostata soltanto nel file `.ino`
se la libreria compila le transazioni in un'unità separata.

## Fonti e limiti delle verifiche locali

- [NXP MFRC522, rev. 3.9](https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf):
  SPI, registro TxASK e limiti elettrici.
- [MFRC522.cpp](https://github.com/miguelbalboa/rfid/blob/master/src/MFRC522.cpp):
  API dei registri, antenna, inizializzazione e gestione delle carte.

La verifica C++ con periferiche simulate non sostituisce la compilazione AVR
né la prova con il lettore. `arduino-cli` non è disponibile in questa sessione;
compilazione e caricamento effettivi vanno eseguiti nell'Arduino IDE.

Controlli locali eseguiti: compilazione C++ con warning trattati come errori
e periferiche simulate; PASS SPI con registri coerenti, rifiuto di versioni
`0x00`/`0xFF`, versione intermittente e scritture ignorate; ripristino del
registro nel percorso riuscito; lettura UID fittizio con zero iniziale,
HALT senza duplicazione nella simulazione, selezione fallita e arresto dopo
perdita del lettore. La simulazione delle carte usa risposte predefinite:
non verifica radiofrequenza, protocollo RF o comportamento di un badge reale.
