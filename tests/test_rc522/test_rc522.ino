/*
  Test RC522 su Elegoo Uno R3. Monitor seriale: 9600 baud.

  SDA -> D10, MOSI -> D11, SCK -> D13, ciascuno tramite partitore.
  MISO -> D12 direttamente. RST e 3.3V -> 3.3V. GND -> GND.
  IRQ scollegato. Libreria richiesta: MFRC522.
*/

#include <SPI.h>
#include <MFRC522.h>

MFRC522 lettore(10, 0xFF);

void setup() {
  Serial.begin(9600);
  SPI.begin();
  lettore.PCD_Init();

  byte versione = lettore.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print(F("Versione lettore: 0x"));
  Serial.println(versione, HEX);

  if (versione == 0x00 || versione == 0xFF) {
    Serial.println(F("Il lettore non risponde. Controlla fili e resistenze."));
    while (true) {
      delay(100);
    }
  }

  Serial.println(F("Avvicina la tessera bianca o il tag blu."));
}

void loop() {
  if (!lettore.PICC_IsNewCardPresent() || !lettore.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print(F("UID:"));
  for (byte i = 0; i < lettore.uid.size; i++) {
    Serial.print(' ');
    if (lettore.uid.uidByte[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(lettore.uid.uidByte[i], HEX);
  }
  Serial.println();

  Serial.print(F("Tipo: "));
  Serial.println(lettore.PICC_GetTypeName(
      lettore.PICC_GetType(lettore.uid.sak)));

  lettore.PICC_HaltA();
}
