/*
  Prova da banco: Uno + tastierino + 74HC595 + LED + RC522 MINI su SPI software + OLED.
  Mantiene la mappa del test tastierino: QB-QE, colonne A1-A3,
  SER=D11, SRCLK=D13, RCLK=A0. OLED: 3V3=3.3V, GND comune, SDA=A4, SCL=A5. Impronta scollegata.
  Encoder e pulsanti del display restano scollegati.
  RFID: SS=D12, MOSI=D9, SCK=D10 tramite tre partitori 1k/2k;
  MISO=D8; VCC e RST=3.3V, GND comune, IRQ scollegato.
  LED: QF (pin 5) -> 1 kohm -> S; QG (pin 6) -> 1 kohm -> centrale;
  pin '-' del modulo -> GND. Identificare i colori con la prova.
  Servono due resistenze aggiuntive, oltre alle quattro delle righe.
  Tasti fittizi: 1=canale S, 2=canale centrale, 0=spento.
  Monitor seriale 9600. Nessuna autenticazione o autorizzazione.
  Il LED del varco del progetto resta sotto il controllo del Pi.
*/
#include <Arduino.h>
#include <MFRC522v2.h>
#include "soft_rfid_driver.h"
#include "oled_ui.h"

const uint8_t latchPin = A0;
const uint8_t readerSelectPin = 12;
const uint8_t columnPins[] = {A1, A2, A3};
const char keys[] = "123456789*0#";
const uint8_t rowMask = 0x1E;  // QB-QE, bit 1-4.
const uint8_t signalLedMask = 0x20;  // QF, pin fisico 5 -> S.
const uint8_t middleLedMask = 0x40;  // QG, pin fisico 6 -> centrale.
const unsigned long debounceMs = 35;
uint8_t ledState = 0;
uint16_t candidate = 0;
unsigned long changedAt = 0;
bool ready = false;
SoftRfidDriver rfidDriver;
MFRC522 reader(rfidDriver);
bool readerReady = false;
byte readerVersion = 0;
unsigned long lastReaderPoll = 0;
unsigned long lastReaderCheck = 0;
bool badgeSeen = false;
char lastTestKey = ' ';

void refreshScreen() {
  setLine(0, "TEST BANCO");
  setLine(1, readerReady ? "RFID PRONTO" : "RFID NON PRONTO");
  setLine(2, badgeSeen ? "BADGE LETTO" : "AVVICINA BADGE");
  setLine(3, "TASTO ");
  if (lastTestKey == '*') setLine(3, "TASTO ASTERISCO");
  else if (lastTestKey == '#') setLine(3, "TASTO CANCELLETTO");
  else lines[3][6] = lastTestKey;
  setLine(4, ledState == signalLedMask ? "LED CANALE S" :
             ledState == middleLedMask ? "LED CENTRALE" : "LED SPENTO");
  setLine(6, "1 S  2 CENTRALE");
  setLine(7, "0 OFF  SOLO TEST");
}

void writeRows(uint8_t rows) {
  // Ogni trasferimento conserva i bit dei LED; QA e QH restano LOW.
  const uint8_t value = (rows & rowMask) | ledState;
  digitalWrite(readerSelectPin, HIGH);  // RFID deselezionato prima dei clock.
  digitalWrite(latchPin, LOW);
  for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
    digitalWrite(13, LOW);
    digitalWrite(11, (value & mask) ? HIGH : LOW);
    delayMicroseconds(5);
    digitalWrite(13, HIGH);
    delayMicroseconds(5);
  }
  digitalWrite(13, LOW);
  digitalWrite(latchPin, HIGH);
}

uint16_t scanKeys() {
  uint16_t result = 0;
  for (uint8_t row = 0; row < 4; ++row) {
    // Una riga LOW; le altre HIGH. Mantenere le quattro resistenze da 1 kohm.
    writeRows(static_cast<uint8_t>(rowMask ^ (1u << (row + 1))));
    delayMicroseconds(50);
    for (uint8_t column = 0; column < 3; ++column) {
      if (digitalRead(columnPins[column]) == LOW) {
        result |= uint16_t(1) << (row * 3 + column);
      }
    }
  }
  writeRows(rowMask);
  return result;
}

void handleKey(char key) {
  lastTestKey = key;
  dirty = true;
  Serial.print(F("Tasto di prova: "));
  Serial.println(key);
  if (key == '1') {
    ledState = signalLedMask;
    Serial.println(F("LED: canale S acceso."));
  } else if (key == '2') {
    ledState = middleLedMask;
    Serial.println(F("LED: canale centrale acceso."));
  } else if (key == '0') {
    ledState = 0;
    Serial.println(F("LED: spento."));
  } else {
    return;
  }
  writeRows(rowMask);
}

void pollReader() {
  if (!readerReady) return;
  const unsigned long now = millis();
  if (now - lastReaderPoll < 100) return;
  lastReaderPoll = now;
  // Bus RFID separato: i suoi clock su D10 non raggiungono il 595.
  if (now - lastReaderCheck >= 5000) {
    lastReaderCheck = now;
    if (rfidDriver.PCD_ReadRegister(MFRC522Constants::PCD_Register::VersionReg) != readerVersion) {
      readerReady = false;
      dirty = true;
      Serial.println(F("ERRORE RFID: lettore non raggiungibile. Tastierino e LED restano attivi."));
      Serial.println(F("Scollega USB, controlla i collegamenti e riavvia."));
      return;
    }
  }
  if (!reader.PICC_IsNewCardPresent()) return;
  if (!reader.PICC_ReadCardSerial()) {
    Serial.println(F("RFID: selezione fallita; allontana il badge e riprova."));
    return;
  }
  badgeSeen = true;
  dirty = true;
  Serial.print(F("UID di prova:"));
  for (byte index = 0; index < reader.uid.size; ++index) {
    Serial.print(' ');
    if (reader.uid.uidByte[index] < 0x10) Serial.print('0');
    Serial.print(reader.uid.uidByte[index], HEX);
  }
  Serial.println();
  Serial.println(F("LETTURA UID OK: non e' autenticazione o accesso autorizzato."));
  reader.PICC_HaltA();
}

void setup() {
  releaseBus();
  digitalWrite(readerSelectPin, HIGH);
  pinMode(readerSelectPin, OUTPUT);
  digitalWrite(latchPin, LOW);
  pinMode(latchPin, OUTPUT);
  for (uint8_t column = 0; column < 3; ++column) {
    pinMode(columnPins[column], INPUT_PULLUP);
  }
  // Nessun SPI hardware: D12 deve poter funzionare come uscita SS RFID.
  digitalWrite(11, LOW);
  pinMode(11, OUTPUT);
  digitalWrite(13, LOW);
  pinMode(13, OUTPUT);
  writeRows(rowMask);
  Serial.begin(9600);
  Serial.println(F("Test terminale: RFID software D12/D10/D9/D8."));
  Serial.println(F("Rilascia tutto. 1=canale S, 2=centrale, 0=spento."));
  reader.PCD_Init();
  readerVersion = rfidDriver.PCD_ReadRegister(MFRC522Constants::PCD_Register::VersionReg);
  Serial.print(F("Versione RFID: 0x"));
  Serial.println(readerVersion, HEX);
  readerReady = readerVersion != 0x00 && readerVersion != 0xFF;
  if (readerReady) {
    Serial.println(F("Avvicina un badge di prova. UID solo sul monitor locale."));
    Serial.println(F("Allontana e riavvicina il badge per rileggerlo."));
  } else {
    Serial.println(F("RFID non risponde: controlla alimentazione, partitori e fili a USB scollegata."));
  }
  writeRows(rowMask);
  lastReaderCheck = millis();
}

void loop() {
  pollReader();
  if (dirty) refreshScreen();
  pollDisplay();
  const uint16_t current = scanKeys();
  const unsigned long now = millis();
  if (current != candidate) {
    candidate = current;
    changedAt = now;
  }
  if (now - changedAt < debounceMs) return;
  if (candidate == 0) {
    ready = true;
    return;
  }
  if (!ready) return;
  ready = false;
  if ((candidate & (candidate - 1)) != 0) {
    Serial.println(F("Piu' tasti rilevati: rilascia tutto."));
    return;
  }
  for (uint8_t index = 0; index < 12; ++index) {
    if (candidate == (uint16_t(1) << index)) {
      handleKey(keys[index]);
      return;
    }
  }
}
