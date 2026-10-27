#pragma once

#include <Arduino.h>
#include <Wire.h>

class Lcd1602PCF8574 {
public:
  bool begin(uint8_t sdaPin, uint8_t sclPin) {
    Wire.begin(sdaPin, sclPin);
    _addr = probeAddress();
    if (_addr == 0) return false;

    delay(50);
    _backlight = true;
    expanderWrite(0);
    delay(1000);

    // HD44780 4-bit initialization sequence.
    write4bits(0x03 << 4);
    delayMicroseconds(4500);
    write4bits(0x03 << 4);
    delayMicroseconds(4500);
    write4bits(0x03 << 4);
    delayMicroseconds(150);
    write4bits(0x02 << 4);

    command(0x28); // 4-bit, 2 lines, 5x8
    command(0x08); // display off
    clear();
    command(0x06); // entry mode
    command(0x0C); // display on, cursor off
    return true;
  }

  bool present() const { return _addr != 0; }
  uint8_t address() const { return _addr; }

  void setBacklight(bool on) {
    _backlight = on;
    expanderWrite(0);
  }

  bool backlight() const { return _backlight; }

  void clear() {
    if (!_addr) return;
    command(0x01);
    delayMicroseconds(2000);
  }

  void setCursor(uint8_t col, uint8_t row) {
    if (!_addr) return;
    static const uint8_t rowOffsets[] = {0x00, 0x40};
    row = row > 1 ? 1 : row;
    command(0x80 | (col + rowOffsets[row]));
  }

  void printLine(uint8_t row, String text) {
    if (!_addr) return;
    if (text.length() > 16) text = text.substring(0, 16);
    while (text.length() < 16) text += ' ';
    setCursor(0, row);
    for (size_t i = 0; i < 16; ++i) writeChar(static_cast<uint8_t>(text[i]));
  }

private:
  // Common PCF8574 backpack mapping:
  // P0=RS, P1=RW, P2=EN, P3=Backlight, P4..P7=D4..D7
  static constexpr uint8_t MASK_RS = 0x01;
  static constexpr uint8_t MASK_EN = 0x04;
  static constexpr uint8_t MASK_BL = 0x08;

  uint8_t _addr = 0;
  bool _backlight = true;

  uint8_t probeAddress() {
    const uint8_t common[] = {0x27, 0x3F};
    for (uint8_t a : common) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) return a;
    }
    // Fall back to scanning the standard 7-bit range.
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) return a;
    }
    return 0;
  }

  void command(uint8_t value) { send(value, 0); }
  void writeChar(uint8_t value) { send(value, MASK_RS); }

  void send(uint8_t value, uint8_t mode) {
    uint8_t high = value & 0xF0;
    uint8_t low  = (value << 4) & 0xF0;
    write4bits(high | mode);
    write4bits(low | mode);
  }

  void write4bits(uint8_t value) {
    expanderWrite(value);
    pulseEnable(value);
  }

  void expanderWrite(uint8_t data) {
    if (!_addr) return;
    Wire.beginTransmission(_addr);
    Wire.write(data | (_backlight ? MASK_BL : 0));
    Wire.endTransmission();
  }

  void pulseEnable(uint8_t data) {
    expanderWrite(data | MASK_EN);
    delayMicroseconds(1);
    expanderWrite(data & ~MASK_EN);
    delayMicroseconds(50);
  }
};
