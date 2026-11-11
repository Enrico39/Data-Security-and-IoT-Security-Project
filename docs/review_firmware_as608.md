# Analisi dello sketch di prova del sensore d'impronta AS608

**Componente:** firmware Arduino Uno, sensore biometrico
**Oggetto:** sketch di prova con menu seriale (registrazione, ricerca, cancellazione)
**Riferimenti interni:** [`architecture_guidelines.md`](../architecture_guidelines.md), [`docs/lessons_learned.md`](lessons_learned.md), [`AGENTS.md`](../AGENTS.md)
**Libreria analizzata:** Adafruit Fingerprint Sensor Library, versione 2.1.4 ([sorgenti](https://github.com/adafruit/Adafruit-Fingerprint-Sensor-Library))

---

## 1. Scopo del documento

Lo sketch serve a verificare che il modulo AS608 risponda, registri e riconosca le impronte. Come strumento di prova è adeguato. Questo documento confronta il suo comportamento con i requisiti già concordati nelle linee guida e indica cosa cambiare per passare dalla prova al firmware del terminale.

Le osservazioni derivano dalla lettura del codice dello sketch e dei sorgenti della libreria. **Nessuna prova è stata ancora eseguita sull'hardware**: le soluzioni proposte vanno verificate sul modulo reale, come richiesto da `AGENTS.md`.

---

## 2. Cosa fa oggi lo sketch

| Funzione | Implementazione |
|---|---|
| Collegamento al sensore | `SoftwareSerial` sui pin 2 e 3, sensore a 57600 baud, controllo con `verifyPassword()` |
| Menu | Scelta da monitor seriale: 1 registra, 2 cerca, 3 cancella tutto, 4 cancella una posizione |
| Registrazione | Due acquisizioni dello stesso dito (`image2Tz(1)`, `image2Tz(2)`), creazione del modello, salvataggio nella posizione indicata |
| Riconoscimento | Acquisizione, estrazione delle caratteristiche e ricerca su tutto il database con `fingerFastSearch()` |
| Cancellazione | `emptyDatabase()` per tutto il database, `deleteModel(id)` per una posizione |

**Aspetto già corretto:** lo sketch non stampa mai template, caratteristiche o immagini sulla seriale. È uno degli errori riscontrati nel progetto di riferimento (`lessons_learned.md`, riga «Dati sensibili nei log»), qui già evitato.

---

## 3. Confronto con i requisiti

| Requisito delle linee guida | Stato nello sketch | Sezione |
|---|---|---|
| Confronto 1:1 con il template associato al badge | Non rispettato: ricerca 1:N | §4.1 |
| Lo slot 0 è valido | Non rispettato: ID accettati da 1 a 127 | §4.2 |
| Timeout, annullamento, stati espliciti dell'enrollment | Non rispettato: attese senza limite | §4.3 |
| Esiti distinti e motivati | Parziale: errore e rifiuto non distinti, punteggio ignorato | §4.4 |
| Seriale affidabile verso il Pi | Da verificare: SoftwareSerial a 57600 baud | §4.5 |
| Enrollment e amministrazione autorizzati dal servizio che li esegue | Non applicabile alla prova: menu libero | §4.6 |
| Nessun dato biometrico nei log | Rispettato | §2 |

---

## 4. Problemi e soluzioni

### 4.1 Ricerca 1:N al posto del confronto 1:1

**Cosa succede.** `fingerFastSearch()` confronta l'impronta acquisita con tutti i template salvati e restituisce la posizione più simile. Nella libreria la ricerca copre in modo fisso le posizioni da `0x0000` a `0x00A3`.

**Perché è un problema.** Le linee guida prevedono che il Pi indichi quale template verificare, usando l'associazione con il badge (flusso utente, passo 3). Con la ricerca 1:N:
- il numero di confronti, e quindi la probabilità di un falso positivo, cresce con il numero di impronte registrate;
- il risultato è «somiglia alla posizione X», non «è la persona di questo badge», e va riportato a mano al badge.

**Soluzione A, provvisoria.** Si mantiene la ricerca ma si accetta solo se `finger.fingerID` coincide con la posizione attesa per il badge della sessione. È semplice e utile per le prime prove, ma lascia invariato il problema dei falsi positivi.

**Soluzione B, consigliata: confronto 1:1 vero.** Il protocollo del modulo prevede un comando *Match* che confronta i due buffer interni di caratteristiche (CharBuffer1 e CharBuffer2) e restituisce esito e punteggio. La libreria Adafruit 2.1.4 non offre un metodo per questo comando, ma espone le funzioni per inviare pacchetti arbitrari (`writeStructuredPacket`, `getStructuredPacket`).

Nella libreria `loadModel(slot)` carica sempre il template salvato nel **CharBuffer1** (secondo parametro fisso a `0x01`). L'impronta acquisita va quindi messa nel **CharBuffer2**:

```cpp
// Codice comando Match del protocollo del modulo.
// Da confermare sul datasheet del modulo in uso prima dell'integrazione.
#define FP_CMD_MATCH 0x03

// Confronta CharBuffer1 e CharBuffer2.
// Restituisce FINGERPRINT_OK se corrispondono, FINGERPRINT_NOMATCH se no,
// un altro codice in caso di errore. In *score scrive il punteggio.
uint8_t matchBuffers(uint16_t *score) {
  uint8_t data[] = { FP_CMD_MATCH };
  Adafruit_Fingerprint_Packet packet(FINGERPRINT_COMMANDPACKET, sizeof(data), data);
  finger.writeStructuredPacket(packet);
  if (finger.getStructuredPacket(&packet) != FINGERPRINT_OK) return FINGERPRINT_PACKETRECIEVEERR;
  if (packet.type != FINGERPRINT_ACKPACKET) return FINGERPRINT_PACKETRECIEVEERR;
  *score = ((uint16_t)packet.data[1] << 8) | packet.data[2];
  return packet.data[0];
}

// Verifica 1:1 dell'impronta presente sul sensore contro la posizione attesa.
uint8_t verifyAgainstSlot(uint16_t expectedSlot, uint16_t *score) {
  uint8_t p = finger.image2Tz(2);          // impronta acquisita -> CharBuffer2
  if (p != FINGERPRINT_OK) return p;
  p = finger.loadModel(expectedSlot);      // template salvato  -> CharBuffer1
  if (p != FINGERPRINT_OK) return p;
  return matchBuffers(score);
}
```

L'acquisizione dell'immagine (`getImage()`) resta come oggi e va eseguita prima di `verifyAgainstSlot`, con il timeout descritto al §4.3.

**Prove da eseguire sul modulo:**
1. dito registrato nella posizione attesa: corrispondenza;
2. dito registrato in un'altra posizione: nessuna corrispondenza;
3. dito non registrato: nessuna corrispondenza;
4. posizione attesa vuota: errore distinto dalla mancata corrispondenza;
5. posizione 0: funziona come le altre.

### 4.2 Posizione 0 esclusa e capienza scritta a mano

**Cosa succede.** Lo sketch accetta ID da 1 a 127 e, nella ricerca, considera valido solo `foundID > 0`.

**Perché è un problema.**
- `lessons_learned.md` stabilisce che lo slot 0 è valido. Escluderlo con controlli come `id > 0` o `if (fingerprint_id)` è proprio l'errore trovato nel progetto di riferimento, dove la cancellazione saltava lo slot 0.
- La capienza reale varia tra modelli di AS608 (le schede dei rivenditori riportano valori diversi). Scriverla nel codice rischia di non corrispondere al modulo.
- Nella libreria il valore predefinito di `capacity` è 64 finché non si chiama `getParameters()`. `fingerSearch()` usa questo valore come limite della ricerca: senza `getParameters()` i template oltre la posizione 63 non verrebbero mai trovati. `fingerFastSearch()` invece usa un limite fisso, per questo oggi il problema non si manifesta.

**Soluzione.** In `setup()`, dopo `verifyPassword()`:

```cpp
if (finger.getParameters() != FINGERPRINT_OK) {
  // errore: sensore non interrogabile, terminale fuori servizio
}
// posizioni valide: da 0 a finger.capacity - 1
```

Tutti i controlli diventano `id >= 0 && id < finger.capacity`, e il valore «non trovato» deve essere distinto da 0 (per esempio `0xFFFF`, che è il valore usato dalla libreria).

### 4.3 Attese senza limite e assenza di stati

**Cosa succede.** In `getFingerprintEnroll()` i cicli `while (p != FINGERPRINT_OK)` e `while (p != FINGERPRINT_NOFINGER)` aspettano il dito senza limite di tempo. Inoltre `while(!getFingerprintEnroll(id));` ripete la registrazione all'infinito finché non riesce.

**Perché è un problema.** Le linee guida richiedono timeout, annullamento tramite rotella e stati espliciti dell'enrollment (in corso, completato, fallito), con pulizia dei dati parziali. Un terminale bloccato in attesa non può ricevere l'annullamento, né i messaggi del Pi.

**Soluzione.**
- Ogni attesa del dito ha una scadenza (per esempio 10 secondi) calcolata con `millis()`, come già avviene in `getFingerprintID()`.
- Durante l'attesa il firmware controlla anche la rotella e i messaggi dal Pi, così annullamento e chiusura della sessione hanno effetto immediato.
- La registrazione restituisce un esito esplicito (`OK`, `TIMEOUT`, `CANCELLED`, `NO_MATCH` tra le due acquisizioni, `STORE_ERROR`) e non viene ripetuta automaticamente: la decisione di riprovare spetta al servizio che gestisce l'enrollment.
- Se il salvataggio fallisce dopo un'eventuale scrittura parziale, la posizione va cancellata con `deleteModel()` prima di segnalare l'errore.

### 4.4 Esiti non distinti e punteggio ignorato

**Cosa succede.** `getFingerprintID()` restituisce `-1` sia quando l'impronta non corrisponde sia per timeout o errori di lettura. Il punteggio `finger.confidence` non viene usato.

**Perché è un problema.**
- Il formato degli eventi prevede motivi di diniego distinti: un'impronta che non corrisponde (`BIO_MISMATCH`) è diversa da un dito non appoggiato in tempo (`TIMEOUT`) o da un guasto del sensore. Confonderli altera il registro di audit e rende impossibile distinguere un tentativo fraudolento da un problema tecnico.
- Senza una soglia sul punteggio, ogni corrispondenza accettata dal modulo vale allo stesso modo.

**Soluzione.**
- Codici di esito separati: corrispondenza, nessuna corrispondenza, timeout, annullato, errore del sensore.
- Il punteggio viene inviato al Pi insieme all'esito. La soglia di accettazione è un parametro della policy sul Pi, non una costante del firmware.
- Il livello di sicurezza del modulo si può impostare con `setSecurityLevel()` (livelli da 1 a 5 nella libreria). Il valore scelto va documentato insieme ai tassi di falsa accettazione e falso rifiuto dichiarati dal produttore.

### 4.5 SoftwareSerial a 57600 baud

**Cosa succede.** Il sensore è collegato tramite `SoftwareSerial` a 57600 baud.

**Perché va verificato.** L'Uno ha una sola seriale hardware, che nel progetto è riservata al collegamento USB con il Pi: il sensore deve quindi restare su `SoftwareSerial`. Questa è una seriale emulata via software, sensibile agli interrupt degli altri componenti (lettore NFC su SPI, tastierino, display). A velocità alte aumenta il rischio di byte persi, che si manifestano come errori di pacchetto (`FINGERPRINT_PACKETRECIEVEERR`) intermittenti.

**Soluzione.**
- Provare il sensore con tutte le periferiche attive e contare gli errori di pacchetto su un numero significativo di acquisizioni.
- Se gli errori compaiono, abbassare la velocità del modulo con `setBaudRate()` (per esempio a 9600 o 19200 baud), quindi riavviare il modulo e la comunicazione alla nuova velocità. La modifica è persistente nel modulo: va documentata.

### 4.6 Menu libero e operazioni non autorizzate

**Cosa succede.** Chiunque abbia accesso al monitor seriale può registrare, cercare e cancellare impronte. La cancellazione dell'intero database avviene senza conferma.

**Perché è accettabile solo per la prova.** Nel sistema finale:
- l'Arduino **non decide**: riceve dal Pi richieste del tipo «verifica la posizione X per la sessione S» e risponde con identificativo di sessione, esito e punteggio;
- registrazione e cancellazione partono solo da un'operazione amministrativa autorizzata sul Pi (`lessons_learned.md`, righe «Autenticazione senza autorizzazione amministrativa» ed «Enrollment non autorizzato»);
- ogni risposta contiene l'identificativo della sessione, così il Pi può rifiutare esiti tardivi o appartenenti a un'altra sessione;
- l'ID operatore resta separato dalla posizione nel sensore: il firmware conosce solo posizioni, il Pi conserva l'associazione operatore ↔ badge ↔ posizione.

Il formato dei messaggi tra Arduino e Pi è ancora da definire (`architecture_guidelines.md`, sezione C, «Messaggi seriali»). Le modifiche del §4.6 dipendono da quella decisione.

---

## 5. Considerazioni di sicurezza aggiuntive

### 5.1 Perimetro della seriale del sensore

Il protocollo del modulo permette anche di **trasferire un template** dal sensore all'host (nella libreria: `getModel()`). Chi ha accesso al collegamento tra sensore e Arduino può quindi:
- leggere un template durante un trasferimento;
- inviare all'Arduino risposte false, per esempio una corrispondenza mai avvenuta.

La frase «il template rimane nel sensore» vale quindi solo se nessuno può accedere a quel collegamento. Come già indicato in `architecture_guidelines.md`, il bus del sensore ha un perimetro di fiducia separato da quello della seriale Arduino–Pi, e la cifratura di quest'ultima non lo protegge. Il firmware non deve mai chiamare `getModel()` né trasferire template verso l'Arduino o verso il Pi.

### 5.2 Password del modulo

Lo sketch usa la password predefinita del modulo (`verifyPassword()` con il valore di fabbrica). La libreria permette di impostarne una diversa con `setPassword()`. È una misura utile contro l'uso del modulo da parte di un dispositivo estraneo, ma **non autentica crittograficamente le risposte** del sensore: non va presentata come protezione del canale (`lessons_learned.md`, «Precisazioni»).

### 5.3 Dati biometrici e GDPR

Le impronte sono dati biometrici e rientrano nelle categorie particolari dell'art. 9 del [GDPR](https://eur-lex.europa.eu/eli/reg/2016/679/oj). Per il prototipo:
- registrare solo le impronte dei componenti del gruppo, con consenso documentato;
- svuotare il database del modulo a fine progetto e annotarlo;
- nei test e negli esempi del repository usare solo posizioni e identificativi fittizi.

---

## 6. Riepilogo delle modifiche

| Priorità | Modifica |
|---|---|
| Alta | Confronto 1:1 con il comando Match e la posizione indicata dal Pi |
| Alta | Posizione 0 valida e capienza letta con `getParameters()` |
| Alta | Timeout e annullamento in tutte le attese, esito esplicito della registrazione |
| Media | Codici di esito distinti e invio del punteggio al Pi |
| Media | Prova di affidabilità di `SoftwareSerial` con tutte le periferiche attive |
| Dopo la definizione del protocollo seriale | Sostituzione del menu con i comandi del Pi, legati all'identificativo di sessione |

## 7. Verifiche richieste sull'hardware

- [ ] Codice del comando Match confermato sul datasheet del modulo in uso
- [ ] Confronto 1:1 provato nei cinque casi del §4.1
- [ ] Capienza reale letta dal modulo
- [ ] Posizione 0 registrata, verificata e cancellata
- [ ] Tasso di errori di pacchetto misurato con tutte le periferiche attive
- [ ] Livello di sicurezza scelto e documentato
