#pragma once

#include <Arduino.h>
#include "config.h"

class DRA818VController {
public:
  DRA818VController(HardwareSerial &port) : _port(port) {}

  bool begin() {
    _port.begin(9600, SERIAL_8N1, PIN_DRA_RX, PIN_DRA_TX);
    delay(200);

    bool ok = false;
    for (int i = 0; i < 3; ++i) {
      String r = command("AT+DMOCONNECT", 700);
      DBG_PRINTF("DRA handshake: %s\n", r.c_str());
      if (r.indexOf("+DMOCONNECT:0") >= 0 || r.indexOf("+DMOCONNECT: 0") >= 0) {
        ok = true;
        break;
      }
      delay(200);
    }
    if (!ok) {
      _ready = false;
      return false;
    }

    String group = String("AT+DMOSETGROUP=0,") + DRA_TX_FREQ + "," + DRA_RX_FREQ + ",0000," + String(DRA_SQL) + ",0000";
    String r1 = command(group, 900);
    DBG_PRINTF("DRA group: %s\n", r1.c_str());
    bool groupOk = (r1.indexOf("+DMOSETGROUP:0") >= 0 || r1.indexOf("+DMOSETGROUP: 0") >= 0);

    String r2 = command(String("AT+DMOSETVOLUME=") + String(DRA_VOLUME), 700);
    DBG_PRINTF("DRA volume: %s\n", r2.c_str());

    // Disable pre/de-emphasis, HPF and LPF to keep the AFSK path as flat as possible.
    // Some compatible module revisions may reject this command; that is non-fatal.
    String r3 = command("AT+SETFILTER=1,1,1", 700);
    DBG_PRINTF("DRA filter: %s\n", r3.c_str());

    _ready = groupOk;
    return _ready;
  }

  bool ready() const { return _ready; }

  String command(const String &cmd, uint32_t timeoutMs) {
    while (_port.available()) _port.read();
    _port.print(cmd);
    _port.print("\r\n");

    String out;
    uint32_t start = millis();
    uint32_t lastRx = start;
    while (millis() - start < timeoutMs) {
      while (_port.available()) {
        char c = static_cast<char>(_port.read());
        out += c;
        lastRx = millis();
      }
      if (out.length() && millis() - lastRx > 60) break;
      delay(2);
    }
    out.trim();
    return out;
  }

private:
  HardwareSerial &_port;
  bool _ready = false;
};
