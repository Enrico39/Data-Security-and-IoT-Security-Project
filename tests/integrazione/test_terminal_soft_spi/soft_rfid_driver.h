#pragma once
#include <Arduino.h>
#include <MFRC522Driver.h>

// Trasporto registri SPI mode 0, MSB first; protocollo carte gestito da MFRC522v2.
// Mappa richiesta: SDA/SS D12, SCK D10, MOSI D9, MISO D8.
class SoftRfidDriver : public MFRC522Driver {
 public:
  bool init() override {
    digitalWrite(12, HIGH);
    pinMode(12, OUTPUT);
    digitalWrite(10, LOW);
    pinMode(10, OUTPUT);
    digitalWrite(9, LOW);
    pinMode(9, OUTPUT);
    digitalWrite(8, LOW);
    pinMode(8, INPUT);
    return true;
  }

  void PCD_WriteRegister(const PCD_Register reg, const byte value) override {
    digitalWrite(12, LOW);
    transfer(static_cast<byte>(static_cast<byte>(reg) << 1));
    transfer(value);
    digitalWrite(12, HIGH);
  }

  void PCD_WriteRegister(const PCD_Register reg, const byte count,
                         byte *const values) override {
    if (count == 0) return;
    digitalWrite(12, LOW);
    transfer(static_cast<byte>(static_cast<byte>(reg) << 1));
    for (byte index = 0; index < count; ++index) transfer(values[index]);
    digitalWrite(12, HIGH);
  }

  byte PCD_ReadRegister(const PCD_Register reg) override {
    byte value = 0;
    PCD_ReadRegister(reg, 1, &value, 0);
    return value;
  }

  void PCD_ReadRegister(const PCD_Register reg, const byte count,
                        byte *const values, const byte rxAlign = 0) override {
    if (count == 0 || rxAlign > 7) return;
    const byte address = 0x80 | (static_cast<byte>(reg) << 1);
    digitalWrite(12, LOW);
    transfer(address);
    for (byte index = 0; index < count; ++index) {
      const byte received = transfer(index + 1 < count ? address : 0);
      const byte mask = index == 0 ? static_cast<byte>(0xFFu << rxAlign) : 0xFF;
      if (mask == 0xFF) values[index] = received;
      else values[index] = (values[index] & ~mask) | (received & mask);
    }
    digitalWrite(12, HIGH);
  }

 private:
  byte transfer(byte value) {
    byte received = 0;
    for (byte mask = 0x80; mask != 0; mask >>= 1) {
      digitalWrite(9, (value & mask) ? HIGH : LOW);
      delayMicroseconds(5);
      digitalWrite(10, HIGH);
      delayMicroseconds(5);
      received = static_cast<byte>((received << 1) | (digitalRead(8) == HIGH));
      digitalWrite(10, LOW);
    }
    return received;
  }
};
