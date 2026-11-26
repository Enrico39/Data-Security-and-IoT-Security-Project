// Prova da banco. Impronta scelta al reset, badge di prova, PIN fittizio.
#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>
#include <SPI.h>
#include <MFRC522.h>
#include "oled_ui.h"

// Riferimento temporaneo nel buffer 2 del sensore, campioni successivi nel buffer 1.
bool selectReference = true;
SoftwareSerial fingerprintSerial(5, 6);
Adafruit_Fingerprint finger(&fingerprintSerial);
enum Phase { WAIT_FINGER, WAIT_BADGE, ENTER_PIN };
Phase phase = WAIT_FINGER;
bool requireRelease;
unsigned long lastScan;

MFRC522 reader(10, 0xFF);
const byte rows[] = {7, 8, 9, A0};
const byte cols[] = {A1, A2, A3};
const char keys[] = "123456789*0#";
const char demoPin[] = "1234";
bool readerReady, armed, revealing;
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
  phase = WAIT_FINGER;
  requireRelease = true;
  count = 0;
  revealing = false;
  screen(message, "RIPRESENTA IL DITO");
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
  finger.begin(57600);
  setLine(0, "TEST DITO RFID PIN");
  screen("SCEGLI IL DITO", "APPOGGIA IL DITO");
}

// Comando AS608 Match: confronta i due buffer senza scrivere nella memoria flash.
uint8_t compareFingerBuffers() {
  uint8_t command[] = {0x03};
  Adafruit_Fingerprint_Packet packet(FINGERPRINT_COMMANDPACKET, sizeof(command), command);
  finger.writeStructuredPacket(packet);
  if (finger.getStructuredPacket(&packet) != FINGERPRINT_OK ||
      packet.type != FINGERPRINT_ACKPACKET ||
      (packet.length != 3 && packet.length != 5)) return FINGERPRINT_PACKETRECIEVEERR;
  const byte payloadSize = packet.length - 2;
  uint16_t sum = packet.type + packet.length;
  for (byte i = 0; i < payloadSize; ++i) sum += packet.data[i];
  const uint16_t received = (uint16_t(packet.data[payloadSize]) << 8) | packet.data[payloadSize + 1];
  if (received != sum || (packet.data[0] == FINGERPRINT_OK && payloadSize != 3))
    return FINGERPRINT_PACKETRECIEVEERR;
  return packet.data[0];
}

void pollFinger() {
  if (millis() - lastScan < 150) return;
  lastScan = millis();
  uint8_t status = finger.getImage();
  if (status == FINGERPRINT_NOFINGER) {
    requireRelease = false;
    return;
  }
  if (status != FINGERPRINT_OK) {
    if (strcmp(lines[2], "ERRORE SENSORE") != 0) finish("ERRORE SENSORE");
    return;
  }
  if (requireRelease) return;
  status = finger.image2Tz(selectReference ? 2 : 1);
  if (status != FINGERPRINT_OK) {
    finish(status == FINGERPRINT_PACKETRECIEVEERR ? "ERRORE SENSORE" : "ERRORE IMMAGINE");
    return;
  }
  if (selectReference) {
    selectReference = false;
    finish("DITO IMPOSTATO");
    return;
  }
  status = compareFingerBuffers();
  if (status == FINGERPRINT_NOMATCH) finish("DITO NON ATTESO");
  else if (status == FINGERPRINT_OK) {
    if (!readerReady) {
      finish("ERRORE SPI");
      return;
    }
    started = millis();
    phase = WAIT_BADGE;
    screen("DITO OK", "AVVICINA BADGE");
  }
  else finish("ERRORE SENSORE");
}

void loop() {
  pollDisplay();
  if (phase == WAIT_FINGER) {
    pollFinger();
    return;
  }
  if (millis() - started >= 30000UL) {
    finish("TEMPO SCADUTO");
    return;
  }
  if (phase == WAIT_BADGE) {
    if (!reader.PICC_IsNewCardPresent()) return;
    MFRC522::StatusCode status = reader.PICC_Select(&reader.uid);
    if (status != MFRC522::STATUS_OK) {
      Serial.println(reader.GetStatusCodeName(status));
      return;
    }
    reader.PICC_HaltA();
    phase = ENTER_PIN;
    count = 0;
    revealing = false;
    armed = false;  // Richiede rilascio stabile dopo la lettura RFID.
    candidate = scanKey();
    changed = millis();
    screen("INSERISCI PIN", "");
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
      phase = WAIT_BADGE;
      screen("AVVICINA BADGE", "");
      return;
    }
    --count;
    revealing = false;
    showDigits(0);
  } else if (key == '#') {
    if (count != 4 || !prefixMatches[count]) {
      finish("PIN ERRATO");
      return;
    }
    finish("PROVA OK");
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
