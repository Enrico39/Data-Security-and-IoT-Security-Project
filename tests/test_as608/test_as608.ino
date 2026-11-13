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
    e  -> registra una nuova impronta (ID 1)
    s  -> cerca/riconosci un'impronta
    c  -> cancella tutte le impronte salvate
*/

#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(2, 3);  // RX = 2, TX = 3
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

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

  Serial.println(F("\nComandi: e = registra, s = cerca, c = cancella tutto"));
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'e') enrollFinger(1);
    else if (cmd == 's') searchFinger();
    else if (cmd == 'c') {
      finger.emptyDatabase();
      Serial.println(F("Database svuotato."));
    }
  }
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
