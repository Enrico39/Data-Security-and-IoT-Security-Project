/*
  Test 1: comunicazione SPI del RC522, senza badge.
  Uno: SDA/SS D10, MOSI D11, SCK D13 tramite i tre partitori.
  MISO D12 diretto. RST e 3V3 a 3.3V, GND comune, IRQ scollegato.
  Schema: docs/hardware/rc522_mini_breadboard.md.
  Libreria: MFRC522. Monitor seriale: 9600 baud.
*/
#include <SPI.h>
#include <MFRC522.h>

MFRC522 reader(10, 0xFF);  // Reset non comandato dall'Uno: RST a 3.3V.

void printHex(byte value) {
  Serial.print(F("0x"));
  if (value < 0x10) Serial.print('0');
  Serial.print(value, HEX);
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("TEST 1 - SPI RC522. Non serve avvicinare il badge."));
  SPI.begin();
  reader.PCD_Init();
  delay(50);
  const byte version = reader.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print(F("VersionReg: "));
  printHex(version);
  Serial.println();
  if (version == 0x00 || version == 0xFF) {
    Serial.println(F("FAIL: lettore non raggiungibile. Scollega USB e controlla i collegamenti."));
    return;
  }

  byte different = 0;
  for (byte i = 0; i < 50; ++i) {
    if (reader.PCD_ReadRegister(MFRC522::VersionReg) != version) ++different;
    delay(20);
  }
  Serial.print(F("Letture versione coerenti: "));
  Serial.print(50 - different);
  Serial.println(F("/50"));
  if (different != 0) {
    Serial.println(F("FAIL: comunicazione instabile. Non proseguire con il test badge."));
    return;
  }

  // Test reversibile di un bit scrivibile, con antenna spenta.
  // Non scrive alcun dato sul badge.
  reader.PCD_AntennaOff();
  const byte original = reader.PCD_ReadRegister(MFRC522::TxASKReg);
  bool writesOk = true;
  for (byte i = 0; i < 8; ++i) {
    const byte expected = (i & 1) ? 0x40 : 0x00;
    reader.PCD_WriteRegister(MFRC522::TxASKReg, expected);
    if ((reader.PCD_ReadRegister(MFRC522::TxASKReg) & 0x40) != expected) {
      writesOk = false;
    }
  }
  reader.PCD_WriteRegister(MFRC522::TxASKReg, original);
  const bool restored = reader.PCD_ReadRegister(MFRC522::TxASKReg) == original;
  Serial.println(writesOk ? F("PASS: scrittura e rilettura registro.")
                         : F("FAIL: scrittura/rilettura registro incoerente."));
  Serial.println(restored ? F("PASS: registro ripristinato.")
                         : F("FAIL: ripristino registro non confermato."));
  if (writesOk && restored) {
    Serial.println(F("PASS SPI: ora carica test_rc522 per leggere il badge."));
  }
  Serial.println(F("Antenna lasciata spenta. RESET ripete questa prova."));
  Serial.println(F("Il test SPI non verifica antenna, badge o autenticazione."));
}

void loop() {
  // La prova viene eseguita una volta: RESET per ripeterla.
}
