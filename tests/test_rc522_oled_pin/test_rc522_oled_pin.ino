// Prova da banco. Qualunque badge selezionato avvia una prova PIN fittizia.
#include <SPI.h>
#include <MFRC522.h>
#include "oled_ui.h"

MFRC522 reader(10, 0xFF);
const byte rows[] = {7, 8, 9, A0};
const byte cols[] = {A1, A2, A3};
const char keys[] = "123456789*0#";
const char demoPin[] = "1234";
bool readerReady, entering, armed, revealing;
bool prefixMatches[sizeof(demoPin)] = {true};
unsigned long revealedAt;
byte count;
unsigned long started, changed;
char candidate;

char scanKey() {
  char found = 0;
  for (byte r = 0; r < 4; ++r) {
    digitalWrite(rows[r], LOW);
    pinMode(rows[r], OUTPUT);
    delayMicroseconds(50);
    for (byte c = 0; c < 3; ++c) {
      if (digitalRead(cols[c]) == LOW) found = found ? '!' : keys[3*r+c];
    }
    pinMode(rows[r], INPUT);
  }
  return found;
}

void screen(const char* message, const char* detail) {
  setLine(2, message);
  setLine(4, detail);
  dirty = true;
  Serial.println(message);
}

void finish(const char* message) {
  entering = false;
  count = 0;
  revealing = false;
  screen(message, "AVVICINA BADGE");
}

void showDigits(char latest) {
  char field[sizeof(demoPin)] = {};
  memset(field, '*', count);
  if (latest && count) field[count - 1] = latest;
  setLine(4, field);
  // Aggiorna subito la pagina del PIN prima di ridisegnare le successive.
  if (!dirty && renderPage >= 4) {
    renderPage = 4;
    renderColumn = 0;
  }
}

void setup() {
  Serial.begin(9600);
  for (byte pin : cols) pinMode(pin, INPUT_PULLUP);
  releaseBus();
  SPI.begin();
  reader.PCD_Init();
  delay(50);
  byte version = reader.PCD_ReadRegister(MFRC522::VersionReg);
  readerReady = version != 0 && version != 0xFF;
  setLine(0, "TEST RFID E PIN");
  screen(readerReady ? "AVVICINA BADGE" : "ERRORE SPI", "");
}

void loop() {
  pollDisplay();
  if (!readerReady) return;
  if (!entering) {
    if (!reader.PICC_IsNewCardPresent()) return;
    MFRC522::StatusCode status = reader.PICC_Select(&reader.uid);
    if (status != MFRC522::STATUS_OK) {
      Serial.println(reader.GetStatusCodeName(status));
      return;
    }
    reader.PICC_HaltA();
    entering = true;
    count = 0;
    revealing = false;
    armed = false;  // Richiede rilascio stabile dopo la lettura RFID.
    candidate = scanKey();
    started = changed = millis();
    screen("INSERISCI PIN", "");
    return;
  }
  if (millis() - started >= 30000UL) {
    finish("TEMPO SCADUTO");
    return;
  }
  if (revealing && millis() - revealedAt >= 650UL) {
    revealing = false;
    showDigits(0);
  }
  char key = scanKey();
  if (key != candidate) {
    candidate = key;
    changed = millis();
  }
  if (millis() - changed < 35) return;
  if (!key) {
    armed = true;
    return;
  }
  if (!armed) return;
  armed = false;
  if (key == '*') {
    if (count == 0) {
      finish("ANNULLATO");
      return;
    }
    --count;
    revealing = false;
    showDigits(0);
  } else if (key == '#') {
    finish(count == 4 && prefixMatches[count] ? "PIN OK" : "PIN ERRATO");
  } else if (key != '!') {
    if (count >= 4) {
      finish("TROPPE CIFRE");
      return;
    }
    prefixMatches[count + 1] = prefixMatches[count] && key == demoPin[count];
    ++count;
    showDigits(key);
    revealedAt = millis();
    revealing = true;
  }
}
