/*
  Prova isolata: Uno + 74HC595 a 5 V + tastierino passivo 3x4.
  Scollegare le altre periferiche. Monitor seriale 9600, solo tasti fittizi.
  SER=D11, SRCLK=D13, RCLK=A0. OE=GND, SRCLR=5V.
  QB/QC/QD/QE (pin fisici 1/2/3/4) -> ciascuna attraverso 1 kohm -> righe 1/2/3/4.
  Colonne=A1/A2/A3. Condensatore 100 nF raccomandato tra VCC e GND del 595;
  non montato nella prova riuscita riferita dall'utente il 19/11/2026.
  Vedere README: ordine dei contatti e limiti delle pressioni simultanee.
  Non implementa autenticazione, gestione sessioni o firmware integrato.
*/
#include <Arduino.h>
#include <SPI.h>

const uint8_t latchPin = A0;
const uint8_t readerSelectPin = 10;
const uint8_t columnPins[] = {A1, A2, A3};
const char keys[] = "123456789*0#";
const unsigned long debounceMs = 35;
uint16_t candidate = 0;
unsigned long changedAt = 0;
bool ready = false;

void writeOutputs(uint8_t value) {
  SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
  digitalWrite(latchPin, LOW);
  SPI.transfer(value);
  digitalWrite(latchPin, HIGH);
  SPI.endTransaction();
}

uint16_t scanKeys() {
  uint16_t result = 0;
  for (uint8_t row = 0; row < 4; ++row) {
    // Il 595 non offre alta impedenza per singola uscita:
    // le resistenze sulle quattro righe sono obbligatorie per questa prova.
    // Le righe usano i bit 1-4 (QB-QE); QA, sul pin fisico 15, resta libera.
    writeOutputs(static_cast<uint8_t>(0xFFu ^ (1u << (row + 1))));
    delayMicroseconds(50);
    for (uint8_t column = 0; column < 3; ++column) {
      if (digitalRead(columnPins[column]) == LOW) {
        result |= uint16_t(1) << (row * 3 + column);
      }
    }
  }
  writeOutputs(0xFF);
  return result;
}

void setup() {
  // D10 deve restare uscita per il ruolo SPI master dell'Uno.
  digitalWrite(readerSelectPin, HIGH);
  pinMode(readerSelectPin, OUTPUT);
  digitalWrite(latchPin, LOW);
  pinMode(latchPin, OUTPUT);
  for (uint8_t column = 0; column < 3; ++column) {
    pinMode(columnPins[column], INPUT_PULLUP);
  }
  SPI.begin();
  writeOutputs(0xFF);
  Serial.begin(9600);
  Serial.println(F("Test 74HC595: un tasto fittizio per volta; rilascia tutto."));
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
      Serial.print(F("Tasto di prova: "));
      Serial.println(keys[index]);
      return;
    }
  }
}
