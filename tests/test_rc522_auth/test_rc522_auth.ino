/*
  Test 3: autenticazione di settore MIFARE Classic 1K con Crypto1.
  Stesso cablaggio dei test RC522, inclusi tre partitori e RST a 3.3V.
  Libreria MFRC522, monitor seriale 9600 baud.
  Solo badge di prova. Nessuna lettura/scrittura di dati o modifica chiavi.
  La chiave pubblica di fabbrica qui usata NON e' un segreto di progetto.
*/
#include <SPI.h>
#include <MFRC522.h>

enum AttemptResult { AUTH_OK, AUTH_FAILED, SETUP_FAILED };
MFRC522 reader(10, 0xFF);
const byte sectorBlock = 4;  // Settore 1, primo blocco; non il trailer.
byte expectedUid[10] = {};
byte expectedSize = 0;
byte expectedVersion = 0;
bool readerReady = false;
bool finished = false;

bool sameBadge() {
  if (reader.uid.size != expectedSize) return false;
  for (byte i = 0; i < expectedSize; ++i) {
    if (reader.uid.uidByte[i] != expectedUid[i]) return false;
  }
  return reader.PICC_GetType(reader.uid.sak) == MFRC522::PICC_TYPE_MIFARE_1K;
}

AttemptResult authenticate(MFRC522::MIFARE_Key& key) {
  reader.PCD_StopCrypto1();
  // Una nuova attivazione RF evita di riusare la sessione precedente,
  // anche dopo l'errore di autenticazione con la chiave diversa.
  reader.PCD_AntennaOff();
  delay(30);
  reader.PCD_AntennaOn();
  delay(50);
  if (reader.PCD_ReadRegister(MFRC522::VersionReg) != expectedVersion) {
    Serial.println(F("INCONCLUSIVO: comunicazione SPI cambiata."));
    reader.PCD_StopCrypto1();
    return SETUP_FAILED;
  }
  if (!reader.PICC_IsNewCardPresent() || !reader.PICC_ReadCardSerial()) {
    Serial.println(F("INCONCLUSIVO: badge non riselezionato. Tienilo fermo sul lettore."));
    reader.PCD_StopCrypto1();
    return SETUP_FAILED;
  }
  if (!sameBadge()) {
    Serial.println(F("INCONCLUSIVO: UID o tipo diverso dal badge iniziale."));
    reader.PICC_HaltA();
    reader.PCD_StopCrypto1();
    return SETUP_FAILED;
  }

  const MFRC522::StatusCode status = reader.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A, sectorBlock, &key, &reader.uid);
  Serial.print(F("Esito PCD_Authenticate: "));
  Serial.println(reader.GetStatusCodeName(status));
  if (status == MFRC522::STATUS_OK) reader.PICC_HaltA();
  reader.PCD_StopCrypto1();
  return status == MFRC522::STATUS_OK ? AUTH_OK : AUTH_FAILED;
}

void runTest() {
  MFRC522::MIFARE_Key factoryKey;
  MFRC522::MIFARE_Key differentKey;
  for (byte i = 0; i < 6; ++i) {
    factoryKey.keyByte[i] = 0xFF;
    differentKey.keyByte[i] = 0xFF;
  }
  differentKey.keyByte[0] = 0x00;

  Serial.println(F("1/3 - Chiave A di fabbrica, settore 1."));
  if (authenticate(factoryKey) != AUTH_OK) {
    Serial.println(F("STOP: chiave di fabbrica non confermata oppure errore di comunicazione."));
    Serial.println(F("Non significa che il badge non supporti autenticazione. Nessuna chiave modificata."));
    return;
  }
  Serial.println(F("PASS: autenticazione con chiave di fabbrica riuscita."));

  Serial.println(F("2/3 - Chiave A diversa: atteso nessun successo."));
  const AttemptResult negative = authenticate(differentKey);
  if (negative == SETUP_FAILED) return;
  if (negative == AUTH_OK) {
    Serial.println(F("ANOMALIA: anche la chiave diversa risulta accettata. Non validare il test."));
    return;
  }
  Serial.println(F("Chiave diversa: autenticazione non riuscita. Verifico il recupero."));

  Serial.println(F("3/3 - Nuova autenticazione con chiave di fabbrica."));
  if (authenticate(factoryKey) != AUTH_OK) {
    Serial.println(F("INCONCLUSIVO: recupero non confermato dopo il tentativo negativo."));
    return;
  }
  Serial.println(F("PASS FUNZIONALE: successo, insuccesso con chiave diversa, nuovo successo."));
  Serial.println(F("Verificata autenticazione di settore Crypto1, non resistenza alla clonazione."));
  Serial.println(F("La chiave di fabbrica e' pubblica. Crypto1 non e' una protezione moderna."));
}

void setup() {
  Serial.begin(9600);
  SPI.begin();
  reader.PCD_Init();
  delay(50);
  expectedVersion = reader.PCD_ReadRegister(MFRC522::VersionReg);
  if (expectedVersion == 0x00 || expectedVersion == 0xFF) {
    Serial.println(F("STOP: lettore non raggiungibile. Ripetere il test SPI."));
    return;
  }
  readerReady = true;
  Serial.println(F("TEST 3 - Autenticazione MIFARE Classic 1K, chiave A, settore 1."));
  Serial.println(F("Avvicina un solo badge di prova e tienilo fermo fino alla fine."));
  Serial.println(F("Nessuna modifica a dati, chiavi o permessi del badge."));
}

void loop() {
  if (!readerReady || finished) return;
  if (!reader.PICC_IsNewCardPresent() || !reader.PICC_ReadCardSerial()) return;
  finished = true;
  if (reader.PICC_GetType(reader.uid.sak) != MFRC522::PICC_TYPE_MIFARE_1K ||
      reader.uid.size == 0 || reader.uid.size > sizeof(expectedUid)) {
    Serial.println(F("STOP: questa prova richiede il badge classificato MIFARE 1KB."));
    reader.PICC_HaltA();
  } else {
    expectedSize = reader.uid.size;
    for (byte i = 0; i < expectedSize; ++i) expectedUid[i] = reader.uid.uidByte[i];
    runTest();
  }
  reader.PCD_StopCrypto1();
  reader.PCD_AntennaOff();
  Serial.println(F("Test terminato. RESET sulla scheda per ripeterlo."));
}
