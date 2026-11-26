#include <SPI.h>
#include <MFRC522.h>
#include "oled_ui.h"

MFRC522 reader(10, 0xFF);
const byte rows[] = {7, 8, 9, A0};
const byte cols[] = {A1, A2, A3};
const char keys[] = "123456789*0#";

enum Phase { Offline, Badge, WaitingPin, Entering, WaitingResult, Result };
Phase phase = Offline;
char session[17], pinDigits[5], frame[32];
byte count, used;
bool discardFrame, readerReady, armed;
char candidate;
unsigned long started, changed, resultAt;

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

void screen(const char* title, const char* detail = "") {
  setLine(2, title);
  setLine(4, detail);
  dirty = true;
}

void clearPin() {
  volatile char* data = pinDigits;
  for (byte i = 0; i < sizeof(pinDigits); ++i) data[i] = 0;
  count = 0;
}

void emitMessage(const __FlashStringHelper* verb) {
  Serial.print(verb);
  Serial.print(' ');
  Serial.println(session);
}

void finish(const char* message) {
  clearPin();
  session[0] = 0;
  phase = Result;
  resultAt = millis();
  screen(message);
}

bool validId(const char* id) {
  if (strlen(id) != 16) return false;
  for (byte i = 0; i < 16; ++i) {
    if (!(id[i] >= '0' && id[i] <= '9') && !(id[i] >= 'A' && id[i] <= 'F')) return false;
  }
  return true;
}

void receiveFrame() {
  char* id = strchr(frame, ' ');
  if (!id) return;
  *id++ = 0;
  if (!validId(id)) return;
  if (phase == Offline && readerReady && strcmp(frame, "ARM") == 0) {
    strcpy(session, id);
    phase = Badge;
    screen("AVVICINA BADGE");
    emitMessage(F("ARMED"));
    return;
  }
  if (strcmp(id, session) != 0) return;
  if (strcmp(frame, "CANCEL") == 0) {
    finish("ANNULLATO");
  } else if (phase == WaitingPin && strcmp(frame, "ASK") == 0) {
    candidate = scanKey();
    changed = millis();
    armed = false;
    phase = Entering;
    screen("INSERISCI PIN");
  } else if ((phase == WaitingPin || phase == WaitingResult) && strcmp(frame, "DENY") == 0) {
    finish("ACCESSO NEGATO");
  } else if (phase == WaitingResult && strcmp(frame, "ALLOW") == 0) {
    finish("ACCESSO CONSENTITO");
  }
}

void pollSerial() {
  for (byte budget = 32; budget && Serial.available(); --budget) {
    char c = Serial.read();
    if (c == '\n') {
      frame[used] = 0;
      if (!discardFrame) receiveFrame();
      used = 0;
      discardFrame = false;
    } else if (c < ' ' || c > '~' || used >= sizeof(frame) - 1) {
      discardFrame = true;
    } else if (!discardFrame) {
      frame[used++] = c;
    }
  }
}

void pollBadge() {
  if (!reader.PICC_IsNewCardPresent() || reader.PICC_Select(&reader.uid) != MFRC522::STATUS_OK) return;
  reader.PICC_HaltA();
  byte size = reader.uid.size;
  if (size != 4 && size != 7 && size != 10) return;
  started = millis();
  phase = WaitingPin;
  Serial.print(F("BADGE "));
  Serial.print(session);
  Serial.print(' ');
  for (byte i = 0; i < size; ++i) {
    if (reader.uid.uidByte[i] < 16) Serial.print('0');
    Serial.print(reader.uid.uidByte[i], HEX);
  }
  Serial.println();
  screen("ATTENDI");
}

void pollKeypad() {
  char key = scanKey();
  if (key != candidate) {
    candidate = key;
    changed = millis();
  }
  if (millis() - changed < 35) return;
  if (!key) { armed = true; return; }
  if (!armed) return;
  armed = false;
  if (key == '!') return;
  if (key == '#') {
    if (count != 4) return;
    phase = WaitingResult;
    Serial.print(F("PIN "));
    Serial.print(session);
    Serial.print(' ');
    Serial.println(pinDigits);
    clearPin();
    screen("ATTENDI");
    return;
  }
  if (key == '*' && count == 0) {
    emitMessage(F("CANCEL"));
    finish("ANNULLATO");
    return;
  }
  if (key == '*') pinDigits[--count] = 0;
  else if (count < 4) pinDigits[count++] = key;
  else {
    emitMessage(F("CANCEL"));
    finish("TROPPE CIFRE");
    return;
  }
  char mask[] = "****";
  mask[count] = 0;
  screen("INSERISCI PIN", mask);
}

void setup() {
  Serial.begin(9600);
  for (byte col : cols) pinMode(col, INPUT_PULLUP);
  releaseBus();
  SPI.begin();
  reader.PCD_Init();
  delay(50);
  byte version = reader.PCD_ReadRegister(MFRC522::VersionReg);
  readerReady = version != 0 && version != 0xFF;
  setLine(0, "TERMINALE 2FA");
  screen(readerReady ? "ATTESA PI" : "ERRORE SPI");
  Serial.println(readerReady ? F("READY") : F("ERROR RFID"));
}

void loop() {
  pollDisplay();
  if ((phase == WaitingPin || phase == Entering || phase == WaitingResult)
      && millis() - started >= 30000UL) {
    emitMessage(F("TIMEOUT"));
    finish("TEMPO SCADUTO");
  }
  if (phase == Result && millis() - resultAt >= 5000UL) {
    phase = Offline;
    screen("ATTESA PI");
    Serial.println(F("READY"));
  }
  pollSerial();
  if (phase == Badge) pollBadge();
  if (phase == Entering) pollKeypad();
}
