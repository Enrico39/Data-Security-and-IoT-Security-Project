/*
  Prova isolata HW-787AB, solo Uno e fili. Monitor seriale: 9600 baud.
  3V3 -> 3.3V, GND -> GND, OLED_SDA -> A4, OLED_SCL -> A5.
  Caricare questo sketch PRIMA di collegare il display, a USB scollegata.
  Richiede pull-up a 3.3V sul modulo: presenza ancora da verificare.
  I2C software: solo OUTPUT LOW oppure INPUT senza pull-up, mai HIGH.
  Non usare Wire o altri sketch con questo cablaggio senza rivalutarli.
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

// 0 = ACK, 1 = nessuna risposta, 2 = linea bloccata o senza pull-up.
uint8_t probeAddress(uint8_t address) {
  releaseBus();
  if (!waitHigh(clockPin) || !waitHigh(dataPin)) return 2;
  pullLow(dataPin);  // START, con SCL alto.
  delayMicroseconds(10);
  pullLow(clockPin);
  const uint8_t value = address << 1;  // Indirizzo + bit scrittura.
  for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
    if (value & mask) releaseLine(dataPin);
    else pullLow(dataPin);
    delayMicroseconds(10);
    releaseLine(clockPin);
    if (!waitHigh(clockPin)) {
      releaseBus();
      return 2;
    }
    pullLow(clockPin);
  }
  releaseLine(dataPin);
  delayMicroseconds(10);
  releaseLine(clockPin);
  if (!waitHigh(clockPin)) {
    releaseBus();
    return 2;
  }
  const bool acknowledged = digitalRead(dataPin) == LOW;
  pullLow(clockPin);
  if (!stopBus()) return 2;
  return acknowledged ? 0 : 1;
}

void setup() {
  releaseBus();
  Serial.begin(9600);
  Serial.println(F("HW-787AB: scansione I2C software, senza pull-up a 5V."));
  Serial.println(F("Il test non accende lo schermo e non identifica il controller."));
}

void loop() {
  uint8_t found = 0;
  Serial.println(F("Scansione..."));
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    const uint8_t status = probeAddress(address);
    if (status == 2) {
      Serial.println(F("Linee non pronte: controllare fili o pull-up del modulo."));
      delay(3000);
      return;
    }
    if (status == 0) {
      Serial.print(F("Dispositivo trovato a 0x"));
      if (address < 0x10) Serial.print('0');
      Serial.println(address, HEX);
      ++found;
    }
  }
  if (found == 0) Serial.println(F("Nessun dispositivo rilevato."));
  if (found > 2) Serial.println(F("Troppe risposte: risultato sospetto, bus da verificare."));
  Serial.println(F("Nuova scansione tra 3 secondi."));
  delay(3000);
}
