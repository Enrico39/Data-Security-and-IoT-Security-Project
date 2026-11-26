// Autotest interno del lettore, senza badge. Monitor seriale a 9600 baud.
// D10 -> SDA, D11 -> MOSI, D13 -> SCK tramite partitori 5 V / 3,3 V.
// D12 <- MISO diretto. RST e 3V3 a 3,3 V, GND comune, IRQ libero.
#include <SPI.h>
#include <MFRC522.h>

MFRC522 reader(10, 0xFF);

void setup() {
  Serial.begin(9600);
  SPI.begin();
  reader.PCD_Init();
  delay(50);

  byte version = reader.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print(F("VersionReg = 0x"));
  Serial.println(version, HEX);
  if (version == 0 || version == 0xFF) {
    Serial.println(F("Risposta SPI assente o non valida. Controllare il cablaggio."));
    return;
  }

  Serial.println(reader.PCD_PerformSelfTest()
    ? F("Autotest interno superato.")
    : F("Autotest fallito o versione non supportata dalla libreria."));
}

void loop() {
  // Premere RESET per ripetere.
}
