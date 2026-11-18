/*
  Demo menu HW-787AB: OLED + encoder + PUSH + BACK + CONFIRM.
  Display come nel test pixel: 3V3->3.3V, GND->GND, SDA->A4, SCL->A5.
  ENCODER_A->D2, ENCODER_B->D3, ENCODER_PUSH->D4, BACK->D5, CONFIRM->D6.
  Tastierino e RFID scollegati. Caricare con modulo scollegato.
  Nessun pull-up a 5V, nessuna libreria esterna. Monitor: 9600 baud.
  Controlli attivi LOW con pull-up sul modulo: IPOTESI da verificare.
*/
#include <Arduino.h>
struct Button;

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


#include <avr/pgmspace.h>
#include <stdio.h>
#include <string.h>

const uint8_t encoderA = 2;
const uint8_t encoderB = 3;
const uint8_t pushPin = 4;
const uint8_t backPin = 5;
const uint8_t confirmPin = 6;
const int8_t transitionsPerStep = 4;

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

volatile uint8_t lastEncoder = 0;
volatile int8_t partialStep = 0;
volatile int8_t queuedSteps = 0;

void encoderChanged() {
  const int8_t direction[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
  const uint8_t current = (digitalRead(encoderA) << 1) | digitalRead(encoderB);
  if ((current ^ lastEncoder) == 3) partialStep = 0;
  else partialStep += direction[(lastEncoder << 2) | current];
  lastEncoder = current;
  if (partialStep >= transitionsPerStep) {
    if (queuedSteps < 10) ++queuedSteps;
    partialStep = 0;
  } else if (partialStep <= -transitionsPerStep) {
    if (queuedSteps > -10) --queuedSteps;
    partialStep = 0;
  }
}

struct Button {
  uint8_t pin;
  bool lastRaw;
  bool stable;
  bool armed;
  unsigned long changedAt;
};
Button buttons[] = {
  {pushPin, HIGH, HIGH, false, 0},
  {backPin, HIGH, HIGH, false, 0},
  {confirmPin, HIGH, HIGH, false, 0}
};

bool wasPressed(Button& button, unsigned long now);

bool wasPressed(Button& button, unsigned long now) {
  const bool raw = digitalRead(button.pin) != LOW;
  if (raw != button.lastRaw) {
    button.lastRaw = raw;
    button.changedAt = now;
  }
  if (now - button.changedAt < 35) return false;
  // Prima di accettare pressioni, richiedere un rilascio stabile.
  if (raw) button.armed = true;
  if (raw == button.stable) return false;
  button.stable = raw;
  if (!raw && button.armed) {
    button.armed = false;
    return true;
  }
  return false;
}

// 0=menu, 1=stato, 2=contatore, 3=informazioni.
uint8_t screen = 0;
uint8_t selected = 0;
uint8_t counter = 0;
char lines[8][22] = {};
bool dirty = true;
bool displayReady = false;
uint8_t renderPage = 8;
uint8_t renderColumn = 0;
unsigned long retryAt = 0;

void setLine(uint8_t row, const char* text) {
  strncpy(lines[row], text, 21);
  lines[row][21] = '\0';
}

void prepareScreen() {
  memset(lines, 0, sizeof(lines));
  if (screen == 0) {
    setLine(0, "MENU DEMO");
    setLine(2, "  STATO");
    setLine(3, "  CONTATORE");
    setLine(4, "  INFORMAZIONI");
    lines[2 + selected][0] = '>';
    setLine(6, "ROT: SCEGLI");
    setLine(7, "OK: APRI");
  } else if (screen == 1) {
    setLine(0, "STATO");
    setLine(2, "TERMINALE PRONTO");
    setLine(3, "SOLO DEMO LOCALE");
    setLine(4, "NESSUN ACCESSO REALE");
    setLine(7, "BACK: MENU");
  } else if (screen == 2) {
    setLine(0, "CONTATORE");
    snprintf(lines[2], sizeof(lines[2]), "VALORE: %u", unsigned(counter));
    setLine(4, "ROT: CAMBIA VALORE");
    setLine(5, "OK: AZZERA");
    setLine(7, "BACK: MENU");
  } else {
    setLine(0, "INFORMAZIONI");
    setLine(2, "DISPLAY HW-787AB");
    setLine(3, "I2C: 0X3C");
    setLine(4, "OFFSET COLONNE: 2");
    setLine(7, "BACK: MENU");
  }
  renderPage = 0;
  renderColumn = 0;
  dirty = false;
}

void handleRotation(int8_t steps) {
  if (steps == 0) return;
  if (screen == 0) {
    int value = int(selected) + steps;
    while (value < 0) value += 3;
    selected = value % 3;
  } else if (screen == 2) {
    int value = int(counter) + steps;
    if (value < 0) value = 0;
    if (value > 99) value = 99;
    counter = value;
  }
  dirty = true;
}

void handleConfirm() {
  if (screen == 0) screen = selected + 1;
  else if (screen == 2) counter = 0;
  dirty = true;
}

// Un blocco di 16 colonne per iterazione: i pulsanti restano interrogabili
// durante il ridisegno. L'encoder viene acquisito tramite interrupt.
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

void setup() {
  releaseBus();
  Serial.begin(9600);
  // Nessun INPUT_PULLUP a 5V su segnali del modulo a 3.3V.
  const uint8_t controls[] = {encoderA, encoderB, pushPin, backPin, confirmPin};
  for (uint8_t i = 0; i < sizeof(controls); ++i) releaseLine(controls[i]);
  lastEncoder = (digitalRead(encoderA) << 1) | digitalRead(encoderB);
  attachInterrupt(digitalPinToInterrupt(encoderA), encoderChanged, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderB), encoderChanged, CHANGE);
  Serial.println(F("Menu demo: rotella per scegliere, PUSH/CONFIRM per aprire, BACK per tornare."));
  Serial.println(F("Comandi attivi LOW ipotizzati; pull-up del modulo da verificare."));
  retryAt = millis() + 500;
}

void loop() {
  const unsigned long now = millis();
  noInterrupts();
  const int8_t steps = queuedSteps;
  queuedSteps = 0;
  interrupts();
  handleRotation(steps);
  const bool push = wasPressed(buttons[0], now);
  const bool back = wasPressed(buttons[1], now);
  const bool confirm = wasPressed(buttons[2], now);
  if (back) {
    screen = 0;
    dirty = true;
    Serial.println(F("BACK"));
  } else if (push || confirm) {
    handleConfirm();
    Serial.println(push ? F("ENCODER_PUSH") : F("CONFIRM"));
  }
  if (!displayReady) {
    if (static_cast<long>(now - retryAt) < 0) return;
    displayReady = initializeDisplay();
    if (!displayReady) {
      releaseBus();
      retryAt = now + 1000;
      Serial.println(F("Display non pronto: riprovo tra un secondo."));
      return;
    }
    dirty = true;
  }
  if (dirty) prepareScreen();
  if (!renderChunk()) {
    releaseBus();
    displayReady = false;
    retryAt = millis() + 1000;
    Serial.println(F("Trasferimento fallito: controllare i contatti."));
  }
}
