#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "settings.h"
#include "config.h"

class AprsIsClient {
public:
  void configure(const AppConfig &cfg) {
    _host = cfg.aprsServer;
    _port = cfg.aprsPort;
    _call = cfg.callsign;
    _pass = cfg.aprsPasscode;
    disconnect();
  }

  void loop() {
    if (WiFi.status() != WL_CONNECTED) {
      disconnect();
      return;
    }

    if (!_client.connected()) {
      if (millis() - _lastAttempt >= APRS_RETRY_MS) connectNow();
      return;
    }

    while (_client.available()) {
      String line = _client.readStringUntil('\n');
      line.trim();
      if (line.startsWith("# logresp")) {
        _lastServerLine = line;
        if (line.indexOf("verified") >= 0 && line.indexOf("unverified") < 0) {
          _verified = true;
          _lastGood = millis();
        } else if (line.indexOf("unverified") >= 0) {
          _verified = false;
        }
      } else if (line.startsWith("#")) {
        _lastGood = millis(); // server heartbeat/comment proves TCP path is alive
      }
    }
  }

  bool connectNow() {
    _lastAttempt = millis();
    _verified = false;
    _lastServerLine = "";
    if (_host.length() == 0 || _call.length() < 3) return false;

    DBG_PRINTF("APRS-IS connect %s:%u\n", _host.c_str(), _port);
    if (!_client.connect(_host.c_str(), _port)) {
      return false;
    }

    String pass = _pass.length() ? _pass : "-1";
    String login = String("user ") + _call + " pass " + pass + " vers " + FW_NAME + " " + FW_VERSION;
    _client.print(login + "\r\n");
    _lastGood = millis();

    // Wait briefly for logresp so status becomes useful immediately.
    uint32_t start = millis();
    while (millis() - start < 3500 && _client.connected()) {
      if (_client.available()) {
        String line = _client.readStringUntil('\n');
        line.trim();
        if (line.startsWith("# logresp")) {
          _lastServerLine = line;
          _verified = (line.indexOf("verified") >= 0 && line.indexOf("unverified") < 0);
          if (_verified) _lastGood = millis();
          break;
        }
      }
      delay(10);
    }
    return _client.connected();
  }

  void disconnect() {
    if (_client.connected()) _client.stop();
    _verified = false;
  }

  bool sendRfPacket(const String &line) {
    if (!_client.connected() || !_verified || line.length() == 0) return false;
    if (line.length() > 500) return false;
    size_t n = _client.print(line + "\r\n");
    if (n > 0) {
      _lastGood = millis();
      ++_sent;
      return true;
    }
    return false;
  }

  bool connected() { return _client.connected(); }
  bool verified() const { return _verified; }
  uint32_t sentCount() const { return _sent; }
  uint32_t lastGoodMs() const { return _lastGood; }
  String lastServerLine() const { return _lastServerLine; }

private:
  WiFiClient _client;
  String _host = DEFAULT_APRS_SERVER;
  uint16_t _port = DEFAULT_APRS_PORT;
  String _call;
  String _pass;
  bool _verified = false;
  uint32_t _lastAttempt = 0;
  uint32_t _lastGood = 0;
  uint32_t _sent = 0;
  String _lastServerLine;
};
