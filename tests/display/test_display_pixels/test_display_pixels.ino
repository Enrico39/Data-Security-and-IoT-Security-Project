/*
  Prova visiva SPERIMENTALE: profilo SSD1306 128x64, indirizzo 0x3C.
  Controller e geometria del modulo non ancora confermati.
  Solo Uno e display: 3V3->3.3V, GND->GND, OLED_SDA->A4, OLED_SCL->A5.
  I2C software senza pull-up a 5V. Nessuna libreria esterna.
  Come nello scanner, richiede pull-up sul modulo verso 3.3V.
  Caricare a display scollegato. Altri pin e periferiche scollegati.
  Non usare PIN o dati personali. Monitor seriale: 9600 baud.
*/
#include <Arduino.h>

const uint8_t dataPin = A4;
const uint8_t clockPin = A5;
const unsigned long lineTimeoutUs = 2000;

void releaseLine(uint8_t pin) {
  digitalWrite(pin, LOW);
  pinMode(pin, INPUT);
}

void pullLow(uint8_t pin) {
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
}

bool waitHigh(uint8_t pin) {
  const unsigned long started = micros();
  while (digitalRead(pin) == LOW) {
    if (micros() - started >= lineTimeoutUs) return false;
  }
  delayMicroseconds(10);
  return true;
}

void releaseBus() {
  releaseLine(dataPin);
  releaseLine(clockPin);
}

bool stopBus() {
  pullLow(clockPin);
  pullLow(dataPin);
  delayMicroseconds(10);
  releaseLine(clockPin);
  if (!waitHigh(clockPin)) {
    releaseBus();
    return false;
  }
  releaseLine(dataPin);
  return waitHigh(dataPin);
}

// Trasmette un byte e verifica l'ACK. Le linee non vengono mai pilotate HIGH.
bool writeByte(uint8_t value) {
  for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
    if (value & mask) releaseLine(dataPin);
    else pullLow(dataPin);
    delayMicroseconds(10);
    releaseLine(clockPin);
    if (!waitHigh(clockPin)) return false;
    pullLow(clockPin);
  }
  releaseLine(dataPin);
  delayMicroseconds(10);
  releaseLine(clockPin);
  if (!waitHigh(clockPin)) return false;
  const bool acknowledged = digitalRead(dataPin) == LOW;
  pullLow(clockPin);
  return acknowledged;
}

bool beginTransfer(uint8_t control) {
  releaseBus();
  if (!waitHigh(clockPin) || !waitHigh(dataPin)) return false;
  pullLow(dataPin);
  delayMicroseconds(10);
  pullLow(clockPin);
  return writeByte(0x3C << 1) && writeByte(control);
}

bool sendCommands(const uint8_t* commands, uint8_t count) {
  if (!beginTransfer(0x00)) {
    releaseBus();
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) {
    if (!writeByte(commands[i])) {
      stopBus();
      releaseBus();
      return false;
    }
  }
  return stopBus();
}

bool initializeDisplay() {
  // Parametri candidati per un OLED SSD1306 128x64, contrasto moderato.
  const uint8_t commands[] = {
    0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
    0x8D, 0x14, 0x20, 0x02, 0xA1, 0xC8, 0xDA, 0x12,
    0x81, 0x40, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6
  };
  return sendCommands(commands, sizeof(commands));
}

bool drawPattern(bool inverted) {
  for (uint8_t page = 0; page < 8; ++page) {
    const uint8_t position[] = {uint8_t(0xB0 | page), 0x00, 0x10};
    if (!sendCommands(position, sizeof(position))) return false;
    if (!beginTransfer(0x40)) {
      releaseBus();
      return false;
    }
    for (uint8_t x = 0; x < 128; ++x) {
      // Cornice di un pixel e scacchiera con quadrati 8x8.
      uint8_t pixels = ((x / 8 + page) & 1) ? 0xFF : 0x00;
      if (inverted) pixels = ~pixels;
      if (page == 0) pixels |= 0x01;
      if (page == 7) pixels |= 0x80;
      if (x == 0 || x == 127) pixels = 0xFF;
      if (!writeByte(pixels)) {
        stopBus();
        releaseBus();
        return false;
      }
    }
    if (!stopBus()) return false;
  }
  const uint8_t displayOn[] = {0xAF};
  return sendCommands(displayOn, sizeof(displayOn));
}

bool initialized = false;
bool inverted = false;

void setup() {
  releaseBus();
  Serial.begin(9600);
  Serial.println(F("Prova visiva: profilo candidato SSD1306 128x64 a 0x3C."));
  Serial.println(F("Atteso: cornice e scacchiera, alternata ogni 3 secondi."));
  delay(500);
}

void loop() {
  if (!initialized) initialized = initializeDisplay();
  if (initialized && drawPattern(inverted)) {
    Serial.println(F("Disegno inviato con ACK. Controllare visivamente lo schermo."));
    inverted = !inverted;
  } else {
    Serial.println(F("Trasferimento fallito: contatto o bus instabile. Riprovo."));
    initialized = false;
    releaseBus();
  }
  delay(3000);
}
