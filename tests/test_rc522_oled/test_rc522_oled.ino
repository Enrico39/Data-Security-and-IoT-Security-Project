// RC522 su SPI hardware D10-D13, OLED su A4/A5.
#include <SPI.h>
#include <MFRC522.h>
#include "oled_ui.h"

MFRC522 reader(10, 0xFF);
bool readerReady;

void setup() {
  Serial.begin(9600);
  releaseBus();
  SPI.begin();
  reader.PCD_Init();
  delay(50);
  byte version = reader.PCD_ReadRegister(MFRC522::VersionReg);
  readerReady = version != 0 && version != 0xFF;
  setLine(0, "TEST RFID E OLED");
  setLine(2, readerReady ? "AVVICINA BADGE" : "ERRORE SPI");
  Serial.println(lines[2]);
  pollDisplay();
  if (!displayReady) Serial.println(F("OLED senza risposta su A4/A5"));
}

void loop() {
  pollDisplay();
  if (!readerReady || !reader.PICC_IsNewCardPresent()) return;
  MFRC522::StatusCode status = reader.PICC_Select(&reader.uid);
  if (status != MFRC522::STATUS_OK) {
    Serial.println(reader.GetStatusCodeName(status));
    setLine(2, "SELEZIONE FALLITA");
    setLine(3, "");
  } else {
    char uid[21];
    const char digits[] = "0123456789ABCDEF";
    for (byte i = 0; i < reader.uid.size; ++i) {
      uid[2*i] = digits[reader.uid.uidByte[i] >> 4];
      uid[2*i+1] = digits[reader.uid.uidByte[i] & 15];
    }
    uid[2*reader.uid.size] = '\0';
    setLine(2, "LETTURA OK");
    setLine(3, uid);
    Serial.print(F("UID: "));
    Serial.println(uid);
    reader.PICC_HaltA();
  }
  dirty = true;
  // Completa la schermata anche in caso di errori di selezione ripetuti.
  do { pollDisplay(); } while (displayReady && renderPage < 8);
}
