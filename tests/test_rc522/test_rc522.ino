/*
  Test RC522 su Elegoo Uno R3. Monitor seriale: 9600 baud.

  SDA -> D10, MOSI -> D11, SCK -> D13, ciascuno tramite partitore.
  MISO -> D12 direttamente. RST e 3.3V -> 3.3V. GND -> GND.
  IRQ scollegato. Libreria richiesta: MFRC522.
*/

#include <SPI.h>
#include <MFRC522.h>

MFRC522 lettore(10, 0xFF);
bool readerReady = false;
byte readerVersion = 0;
unsigned long lastCheck = 0;

void setup() {
  Serial.begin(9600);
  Serial.println(F("TEST 2 - lettura badge RC522. Nessuna scrittura sul badge."));
  SPI.begin();
  lettore.PCD_Init();

  byte versione = lettore.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print(F("Versione lettore: 0x"));
  Serial.println(versione, HEX);

  if (versione == 0x00 || versione == 0xFF) {
    Serial.println(F("Il lettore non risponde. Controlla fili e resistenze."));
    Serial.println(F("Scollega USB prima di correggere i collegamenti; poi ripeti il test."));
    return;
  }

  readerVersion = versione;
  readerReady = true;
  lastCheck = millis();
  Serial.println(F("Avvicina la tessera bianca o il tag blu."));
  Serial.println(F("Per rileggere lo stesso badge, allontanalo e riavvicinalo."));
  Serial.println(F("Usa badge di prova: l'UID verra' mostrato sul monitor locale."));
}

void loop() {
  if (!readerReady) return;
  const unsigned long now = millis();
  if (now - lastCheck >= 5000) {
    lastCheck = now;
    if (lettore.PCD_ReadRegister(MFRC522::VersionReg) != readerVersion) {
      readerReady = false;
      Serial.println(F("ERRORE SPI: versione cambiata o lettore non raggiungibile. Test fermato."));
      Serial.println(F("Scollega USB e controlla il cablaggio; poi riavvia."));
      return;
    }
    Serial.println(F("Lettore raggiungibile. In attesa del badge..."));
  }
  if (!lettore.PICC_IsNewCardPresent()) return;
  if (!lettore.PICC_ReadCardSerial()) {
    Serial.println(F("Badge rilevato ma selezione fallita: allontana e riprova."));
    delay(200);
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

  Serial.print(F("Tipo indicato dalla libreria: "));
  Serial.println(lettore.PICC_GetTypeName(
      lettore.PICC_GetType(lettore.uid.sak)));
  Serial.println(F("LETTURA OK. Questo non dimostra autenticazione crittografica."));

  lettore.PICC_HaltA();
}
