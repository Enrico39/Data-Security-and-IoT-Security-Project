/*
  Prova da banco: Uno + tastierino + 74HC595 + modulo LED bicolore.
  Mantiene la mappa del test tastierino: QB-QE, colonne A1-A3,
  SER=D11, SRCLK=D13, RCLK=A0. Scollegare le altre periferiche.
  LED: QF (pin 5) -> 1 kohm -> S; QG (pin 6) -> 1 kohm -> centrale;
  pin '-' del modulo -> GND. Identificare i colori con la prova.
  Servono due resistenze aggiuntive, oltre alle quattro delle righe.
  Tasti fittizi: 1=canale S, 2=canale centrale, 0=spento.
  Monitor seriale 9600. Nessuna autenticazione o autorizzazione.
  Il LED del varco del progetto resta sotto il controllo del Pi.
*/
#include <Arduino.h>
#include <SPI.h>

const uint8_t latchPin = A0;
const uint8_t readerSelectPin = 10;
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

void writeRows(uint8_t rows) {
  // Ogni trasferimento conserva i bit dei LED; QA e QH restano LOW.
  const uint8_t value = (rows & rowMask) | ledState;
  SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
  digitalWrite(latchPin, LOW);
  SPI.transfer(value);
  digitalWrite(latchPin, HIGH);
  SPI.endTransaction();
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

void setup() {
  digitalWrite(readerSelectPin, HIGH);
  pinMode(readerSelectPin, OUTPUT);
  digitalWrite(latchPin, LOW);
  pinMode(latchPin, OUTPUT);
  for (uint8_t column = 0; column < 3; ++column) {
    pinMode(columnPins[column], INPUT_PULLUP);
  }
  SPI.begin();
  writeRows(rowMask);
  Serial.begin(9600);
  Serial.println(F("Test tastierino + 74HC595 + LED bicolore."));
  Serial.println(F("Rilascia tutto. 1=canale S, 2=centrale, 0=spento."));
}

void loop() {
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
