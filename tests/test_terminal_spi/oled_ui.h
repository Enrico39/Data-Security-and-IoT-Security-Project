#pragma once
// Driver ripreso dal test_display_menu: GPIO LOW/INPUT, nessun pull-up a 5 V.
#include <avr/pgmspace.h>
#include <string.h>
const uint8_t dataPin = A4;
const uint8_t clockPin = A5;
const unsigned long lineTimeoutUs = 2000;
// La foto mostra testo tagliato a sinistra e colonne residue a destra.
// Prova di allineamento tipica dei pannelli SH1106: chip non identificato.
const uint8_t displayColumnOffset = 2;
static_assert(displayColumnOffset <= 4, "Offset fuori dalla RAM ipotizzata di 132 colonne");

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

// Font minimale 5x7: A-Z e 0-9, colonne dal basso valore di bit all'alto.
const uint8_t font[][5] PROGMEM = {
  {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
  {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
  {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
  {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
  {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
  {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
  {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
  {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
  {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},
  {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
  {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
  {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}
};

uint8_t glyphColumn(char character, uint8_t column) {
  if (column >= 5) return 0;
  int8_t index = -1;
  if (character >= 'A' && character <= 'Z') index = character - 'A';
  if (character >= '0' && character <= '9') index = 26 + character - '0';
  if (index >= 0) return pgm_read_byte(&font[index][column]);
  if (character == '>') return column == 1 ? 0x22 : (column == 2 ? 0x14 : (column == 3 ? 0x08 : 0));
  if (character == ':') return column == 2 ? 0x24 : 0;
  if (character == '-') return 0x08;
  return 0;
}


char lines[8][22] = {};
bool displayReady = false;
bool dirty = true;
uint8_t renderPage = 8;
uint8_t renderColumn = 0;
unsigned long retryAt = 0;
void setLine(uint8_t row, const char* text) {
  strncpy(lines[row], text, 21);
  lines[row][21] = '\0';
}
bool renderChunk() {
  if (renderPage >= 8) return true;
  // Applicare l'offset prima di separare i nibble, anche ai blocchi successivi.
  const uint8_t memoryColumn = renderColumn + displayColumnOffset;
  const uint8_t position[] = {
    uint8_t(0xB0 | renderPage), uint8_t(memoryColumn & 0x0F),
    uint8_t(0x10 | (memoryColumn >> 4))
  };
  if (!sendCommands(position, sizeof(position))) return false;
  if (!beginTransfer(0x40)) { stopBus(); releaseBus(); return false; }
  const uint8_t end = renderColumn + 16;
  for (uint8_t x = renderColumn; x < end; ++x) {
    const uint8_t characterIndex = x / 6;
    uint8_t pixels = 0;
    if (characterIndex < 21) pixels = glyphColumn(lines[renderPage][characterIndex], x % 6);
    if (!writeByte(pixels)) { stopBus(); releaseBus(); return false; }
  }
  if (!stopBus()) return false;
  renderColumn = end;
  if (renderColumn == 128) { renderColumn = 0; ++renderPage; }
  if (renderPage == 8) {
    const uint8_t on[] = {0xAF};
    return sendCommands(on, sizeof(on));
  }
  return true;
}

void pollDisplay() {
  const unsigned long now = millis();
  if (!displayReady) {
    if (static_cast<long>(now - retryAt) < 0) return;
    displayReady = initializeDisplay();
    if (!displayReady) {
      releaseBus();
      retryAt = millis() + 1000;
      return;
    }
    dirty = true;
  }
  if (dirty) {
    renderPage = 0;
    renderColumn = 0;
    dirty = false;
  }
  if (!renderChunk()) {
    releaseBus();
    displayReady = false;
    retryAt = millis() + 1000;
  }
}
