/*
  Test sensore impronte AS608 / JM-101B con Arduino UNO

  Collegamenti:
    Sensore giallo (TX) -> Arduino pin 2 (RX software)
    Sensore bianco (RX) -> Arduino pin 3 (TX software), tramite partitore
    Sensore rosso (V+)  -> 3.3V
    Sensore nero (GND)  -> GND

  Libreria necessaria: "Adafruit Fingerprint Sensor Library"
  (Sketch > Include Library > Manage Libraries...)

  Apri il Monitor Seriale a 9600 baud.
  Comandi (scrivi e premi invio):
    e  -> registra una nuova impronta (poi scrivi l'ID, es. 1, 2, 3...)
    s  -> cerca un'impronta in tutta la memoria
    g  -> prova guidata con il dito registrato: 20 appoggi, 1 vs 2 slot
    n  -> stessa prova con un dito diverso (controllo negativo)
    q  -> interrompi la prova guidata durante l'attesa del dito
  Per g/n, inserisci due ID gia registrati con acquisizioni distinte
  dello stesso dito. Il primo ID e il riferimento per la prova a 1 slot.
    d  -> cancella una singola impronta (scrivi l'ID)
    c  -> cancella tutte le impronte salvate
*/

#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(2, 3);  // RX = 2, TX = 3
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void guidedTrial(bool genuineFinger);

void setup() {
  Serial.begin(9600);
  while (!Serial);
  delay(100);
  Serial.println(F("\nTest sensore impronte AS608"));

  finger.begin(57600);  // baud di default del modulo
  delay(500);

  if (finger.verifyPassword()) {
    Serial.println(F("Sensore trovato!"));
  } else {
    Serial.println(F("Sensore NON trovato. Controlla i fili (TX/RX) e l'alimentazione."));
    while (1) { delay(1); }
  }

  finger.getParameters();
  Serial.print(F("Capacita' (impronte): ")); Serial.println(finger.capacity);
  Serial.print(F("Livello sicurezza: "));    Serial.println(finger.security_level);
  Serial.print(F("Baud rate: "));            Serial.println(finger.baud_rate);

  finger.getTemplateCount();
  Serial.print(F("Impronte salvate: "));     Serial.println(finger.templateCount);

  Serial.println(F("\nComandi: e = registra, s = cerca, g = prova dito registrato, n = prova dito diverso, d = cancella un ID, c = cancella tutto"));
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'e') {
      uint8_t id = readId(F("Scrivi l'ID da assegnare (1-127) e premi invio: "));
      if (id > 0) enrollFinger(id);
    } else if (cmd == 's') {
      searchFinger();
    } else if (cmd == 'g' || cmd == 'n') {
      guidedTrial(cmd == 'g');
    } else if (cmd == 'd') {
      uint8_t id = readId(F("Scrivi l'ID da cancellare (1-127) e premi invio: "));
      if (id > 0) {
        if (finger.deleteModel(id) == FINGERPRINT_OK) Serial.println(F("Impronta cancellata."));
        else Serial.println(F("Errore nella cancellazione."));
      }
    } else if (cmd == 'c') {
      finger.emptyDatabase();
      Serial.println(F("Database svuotato."));
    }
  }
}

// ---------- Legge un ID da Monitor Seriale ----------
uint8_t readId(const __FlashStringHelper *msg) {
  Serial.print(msg);
  while (Serial.available()) Serial.read();  // svuota il buffer
  while (!Serial.available());
  long n = Serial.parseInt();
  while (Serial.available()) Serial.read();
  Serial.println(n);
  if (n < 1 || n > 127) {
    Serial.println(F("ID non valido."));
    return 0;
  }
  return (uint8_t)n;
}

// ---------- Cerca / riconosci ----------
void searchFinger() {
  Serial.println(F("Appoggia il dito sul sensore..."));
  unsigned long start = millis();
  while (finger.getImage() != FINGERPRINT_OK) {
    if (millis() - start > 10000) { Serial.println(F("Timeout.")); return; }
  }
  if (finger.image2Tz() != FINGERPRINT_OK) { Serial.println(F("Immagine non valida.")); return; }

  uint8_t p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print(F("Trovata! ID #")); Serial.print(finger.fingerID);
    Serial.print(F("  confidenza: ")); Serial.println(finger.confidence);
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println(F("Impronta non riconosciuta."));
  } else {
    Serial.println(F("Errore di comunicazione."));
  }
}

// ---------- Registra ----------
void enrollFinger(uint8_t id) {
  Serial.print(F("Registrazione ID #")); Serial.println(id);

  // Prima scansione
  Serial.println(F("Appoggia il dito..."));
  if (!waitImage()) return;
  if (finger.image2Tz(1) != FINGERPRINT_OK) { Serial.println(F("Errore immagine 1.")); return; }

  Serial.println(F("Togli il dito."));
  delay(1500);
  while (finger.getImage() != FINGERPRINT_NOFINGER);

  // Seconda scansione
  Serial.println(F("Appoggia di nuovo lo stesso dito..."));
  if (!waitImage()) return;
  if (finger.image2Tz(2) != FINGERPRINT_OK) { Serial.println(F("Errore immagine 2.")); return; }

  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println(F("Le due scansioni non coincidono, riprova."));
    return;
  }
  if (finger.storeModel(id) == FINGERPRINT_OK) {
    Serial.println(F("Impronta salvata!"));
  } else {
    Serial.println(F("Errore nel salvataggio."));
  }
}

bool waitImage() {
  unsigned long start = millis();
  while (finger.getImage() != FINGERPRINT_OK) {
    if (millis() - start > 10000) { Serial.println(F("Timeout.")); return false; }
  }
  return true;
}

// ---------- Prova guidata: stesso campione, uno o due slot ----------
const uint8_t TRIAL_ATTEMPTS = 20;
const unsigned long TRIAL_WAIT_MS = 15000;

bool trialCancelled() {
  while (Serial.available()) {
    if (Serial.read() == 'q') return true;
  }
  return false;
}

// Il comando SEARCH accetta inizio e ampiezza della ricerca. Con ampiezza 1
// controlliamo un solo slot senza sovrascrivere il campione nel buffer 1.
uint8_t searchSlot(uint16_t slot) {
  uint8_t payload[] = {
    FINGERPRINT_SEARCH, 1, uint8_t(slot >> 8), uint8_t(slot & 0xFF), 0, 1
  };
  Adafruit_Fingerprint_Packet packet(FINGERPRINT_COMMANDPACKET, sizeof(payload), payload);
  finger.writeStructuredPacket(packet);
  if (finger.getStructuredPacket(&packet) != FINGERPRINT_OK ||
      packet.type != FINGERPRINT_ACKPACKET || packet.length < 3) {
    return FINGERPRINT_PACKETRECIEVEERR;
  }
  // La lunghezza ricevuta comprende i due byte di checksum.
  uint16_t checksum = packet.type + (packet.length >> 8) + (packet.length & 0xFF);
  uint16_t dataLength = packet.length - 2;
  for (uint16_t i = 0; i < dataLength; ++i) checksum += packet.data[i];
  uint16_t received = (uint16_t(packet.data[dataLength]) << 8) | packet.data[dataLength + 1];
  if (checksum != received) return FINGERPRINT_PACKETRECIEVEERR;
  if (packet.data[0] == FINGERPRINT_OK) {
    if (dataLength < 5) return FINGERPRINT_PACKETRECIEVEERR;
    uint16_t foundSlot = (uint16_t(packet.data[1]) << 8) | packet.data[2];
    if (foundSlot != slot) return FINGERPRINT_PACKETRECIEVEERR;
  }
  return packet.data[0];
}

bool isSearchResult(uint8_t code) {
  return code == FINGERPRINT_OK || code == FINGERPRINT_NOTFOUND;
}

void printTrialResult(const __FlashStringHelper *label, uint8_t accepted,
                      uint8_t valid, bool genuineFinger) {
  Serial.print(label);
  Serial.print(F(": accettati ")); Serial.print(accepted);
  Serial.print(F(", respinti ")); Serial.print(valid - accepted);
  Serial.print(genuineFinger ? F(", falsi rifiuti ") : F(", false accettazioni "));
  Serial.print(genuineFinger ? valid - accepted : accepted);
  Serial.print(F("/")); Serial.println(valid);
}

void guidedTrial(bool genuineFinger) {
  Serial.println(F("Prova guidata: scegli due template distinti dello stesso dito."));
  uint8_t first = readId(F("ID di riferimento (usato anche nella prova a 1 slot): "));
  if (!first) return;
  uint8_t second = readId(F("ID del secondo template: "));
  if (!second || first == second) {
    Serial.println(F("Servono due ID validi e diversi."));
    return;
  }
  if (finger.loadModel(first) != FINGERPRINT_OK ||
      finger.loadModel(second) != FINGERPRINT_OK) {
    Serial.println(F("Uno degli slot non si carica. Verifica ID e sensore."));
    return;
  }

  Serial.println(genuineFinger ? F("Usa il dito registrato per tutti gli appoggi.")
                               : F("Usa lo stesso dito diverso per tutti gli appoggi."));
  Serial.println(F("Solleva il dito tra un tentativo e l'altro. Invia q per terminare."));
  uint8_t valid = 0, acceptedOne = 0, acceptedTwo = 0;
  uint8_t timeouts = 0, errors = 0;
  for (uint8_t attempt = 1; attempt <= TRIAL_ATTEMPTS; ++attempt) {
    Serial.println(F("Tieni il dito sollevato..."));
    unsigned long started = millis();
    uint8_t imageCode = finger.getImage();
    while (imageCode == FINGERPRINT_OK && millis() - started < TRIAL_WAIT_MS) {
      if (trialCancelled()) break;
      delay(80);
      imageCode = finger.getImage();
    }
    if (imageCode != FINGERPRINT_NOFINGER) {
      Serial.println(F("Prova terminata: impossibile confermare il rilascio del dito."));
      break;
    }
    Serial.print(F("Appoggio ")); Serial.print(attempt);
    Serial.print(F("/")); Serial.print(TRIAL_ATTEMPTS);
    Serial.println(F(": appoggia il dito una volta."));
    started = millis();
    bool cancelled = false;
    do {
      cancelled = trialCancelled();
      if (cancelled) break;
      imageCode = finger.getImage();
      if (imageCode == FINGERPRINT_NOFINGER) delay(80);
    } while (imageCode == FINGERPRINT_NOFINGER && millis() - started < TRIAL_WAIT_MS);
    if (cancelled) break;
    if (imageCode == FINGERPRINT_NOFINGER) {
      ++timeouts;
      Serial.println(F("Tempo scaduto senza appoggio."));
      continue;
    }
    if (imageCode != FINGERPRINT_OK || finger.image2Tz(1) != FINGERPRINT_OK) {
      ++errors;
      Serial.println(F("Errore di acquisizione o conversione."));
      continue;
    }

    uint8_t result = searchSlot(first);
    if (!isSearchResult(result)) {
      ++errors;
      Serial.print(F("Ricerca mirata fallita, codice: ")); Serial.println(result, HEX);
      continue;
    }
    bool matchOne = result == FINGERPRINT_OK;
    if (!matchOne) result = searchSlot(second);
    if (!isSearchResult(result)) {
      ++errors;
      Serial.print(F("Ricerca sul secondo slot fallita, codice: ")); Serial.println(result, HEX);
      continue;
    }
    bool matchTwo = result == FINGERPRINT_OK;
    ++valid;
    if (matchOne) ++acceptedOne;
    if (matchTwo) ++acceptedTwo;
    Serial.print(matchOne ? F("1 slot: MATCH") : F("1 slot: NO MATCH"));
    Serial.println(matchTwo ? F(" | 2 slot: MATCH") : F(" | 2 slot: NO MATCH"));
  }

  Serial.println(F("Riepilogo prova guidata"));
  Serial.print(F("Appoggi validi: ")); Serial.println(valid);
  Serial.print(F("Timeout senza appoggio: ")); Serial.println(timeouts);
  Serial.print(F("Errori del sensore: ")); Serial.println(errors);
  printTrialResult(F("1 slot"), acceptedOne, valid, genuineFinger);
  printTrialResult(F("2 slot"), acceptedTwo, valid, genuineFinger);
}
