/*
  Test tastierino passivo 3x4 su Elegoo Uno R3, senza librerie esterne.
  Sette contatti -> D2, D3, D4, D5, D6, D7, D8, in ordine.
  Nessun collegamento a 5V, 3.3V o GND. Altre periferiche scollegate.
  Monitor seriale: 9600 baud. Usare solo pressioni di prova, non PIN reali.
*/
#include <Arduino.h>

const uint8_t pins[] = {2, 3, 4, 5, 6, 7, 8};
const char keys[] = "123456789*0#";
const uint8_t keyCount = 12;
const unsigned long debounceMs = 35;
uint32_t keyPairs[keyCount] = {};
uint8_t learned = 0;
uint32_t candidate = 0;
unsigned long changedAt = 0;
bool ready = false;

// Una sola uscita LOW per volta; gli altri contatti restano INPUT_PULLUP.
// Nessuna uscita HIGH: anche premendo piu' tasti non si oppongono uscite.
uint32_t scanContacts() {
  uint32_t result = 0;
  uint8_t bitIndex = 0;
  for (uint8_t i = 0; i < 7; ++i) {
    digitalWrite(pins[i], LOW);
    pinMode(pins[i], OUTPUT);
    delayMicroseconds(50);
    for (uint8_t j = i + 1; j < 7; ++j) {
      if (digitalRead(pins[j]) == LOW) {
        result |= uint32_t(1) << bitIndex;
      }
      ++bitIndex;
    }
    pinMode(pins[i], INPUT_PULLUP);
  }
  return result;
}

void printPair(uint32_t pair) {
  uint8_t bitIndex = 0;
  for (uint8_t i = 0; i < 7; ++i) {
    for (uint8_t j = i + 1; j < 7; ++j) {
      if (pair & (uint32_t(1) << bitIndex)) {
        Serial.print(F("D"));
        Serial.print(pins[i]);
        Serial.print(F(" - D"));
        Serial.print(pins[j]);
      }
      ++bitIndex;
    }
  }
}

void printPrompt() {
  Serial.print(F("Premi e rilascia il tasto: "));
  Serial.println(keys[learned]);
}

void handlePress(uint32_t pair) {
  if ((pair & (pair - 1)) != 0) {
    Serial.println(F("Piu' contatti chiusi: rilascia tutti i tasti e riprova."));
    return;
  }
  for (uint8_t i = 0; i < learned; ++i) {
    if (keyPairs[i] != pair) continue;
    if (learned < keyCount) {
      Serial.println(F("Contatto gia' assegnato. Premi il tasto richiesto."));
    } else {
      Serial.print(F("Tasto: "));
      Serial.println(keys[i]);
    }
    return;
  }
  if (learned == keyCount) {
    Serial.println(F("Contatto sconosciuto: controlla il cablaggio e ripeti la calibrazione."));
    return;
  }
  keyPairs[learned] = pair;
  Serial.print(keys[learned]);
  Serial.print(F(" -> "));
  printPair(pair);
  Serial.println();
  ++learned;
  if (learned < keyCount) {
    printPrompt();
    return;
  }
  Serial.println(F("Mappa completata (associazioni dichiarate durante la calibrazione):"));
  for (uint8_t i = 0; i < keyCount; ++i) {
    Serial.print(keys[i]);
    Serial.print(F(" -> "));
    printPair(keyPairs[i]);
    Serial.println();
  }
  Serial.println(F("TEST: premi ogni tasto e controlla il carattere mostrato."));
  Serial.println(F("RESET sulla scheda per rifare la calibrazione."));
}

void setup() {
  for (uint8_t i = 0; i < 7; ++i) pinMode(pins[i], INPUT_PULLUP);
  Serial.begin(9600);
  Serial.println(F("Test tastierino 3x4 - contatti collegati a D2-D8."));
  Serial.println(F("Un tasto per volta. Rilascia tutti i tasti prima di iniziare."));
  printPrompt();
}

void loop() {
  const uint32_t current = scanContacts();
  const unsigned long now = millis();
  if (current != candidate) {
    candidate = current;
    changedAt = now;
  }
  if (now - changedAt < debounceMs) return;
  if (candidate == 0) {
    ready = true;
  } else if (ready) {
    ready = false;
    handlePress(candidate);
  }
}
