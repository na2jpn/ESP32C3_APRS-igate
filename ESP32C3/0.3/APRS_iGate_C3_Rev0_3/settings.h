#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

struct AppConfig {
  String wifiSsid;
  String wifiPass;
  String callsign;
  String aprsPasscode;
  String aprsServer = DEFAULT_APRS_SERVER;
  uint16_t aprsPort = DEFAULT_APRS_PORT;
  String grid;
  String comment = "RX-only APRS iGate";
};

class SettingsStore {
public:
  bool load(AppConfig &cfg) {
    Preferences p;
    if (!p.begin("igate", true)) return false;
    cfg.wifiSsid     = p.getString("ssid", "");
    cfg.wifiPass     = p.getString("wpass", "");
    cfg.callsign     = p.getString("call", "");
    cfg.aprsPasscode = p.getString("apass", "");
    cfg.aprsServer   = p.getString("server", DEFAULT_APRS_SERVER);
    cfg.aprsPort     = p.getUShort("port", DEFAULT_APRS_PORT);
    cfg.grid         = p.getString("grid", "");
    cfg.comment      = p.getString("comment", "RX-only APRS iGate");
    p.end();
    normalize(cfg);
    return true;
  }

  bool save(AppConfig cfg) {
    normalize(cfg);
    Preferences p;
    if (!p.begin("igate", false)) return false;
    p.putString("ssid", cfg.wifiSsid);
    p.putString("wpass", cfg.wifiPass);
    p.putString("call", cfg.callsign);
    p.putString("apass", cfg.aprsPasscode);
    p.putString("server", cfg.aprsServer);
    p.putUShort("port", cfg.aprsPort);
    p.putString("grid", cfg.grid);
    p.putString("comment", cfg.comment);
    p.end();
    return true;
  }

  void clear() {
    Preferences p;
    if (p.begin("igate", false)) {
      p.clear();
      p.end();
    }
  }

  static bool validForStation(const AppConfig &cfg) {
    return cfg.wifiSsid.length() > 0 && validCallsign(cfg.callsign);
  }

  static bool validCallsign(String s) {
    s.trim();
    s.toUpperCase();
    if (s.length() < 3 || s.length() > 9) return false;
    int hyphen = s.indexOf('-');
    String base = hyphen >= 0 ? s.substring(0, hyphen) : s;
    String ssid = hyphen >= 0 ? s.substring(hyphen + 1) : "";
    if (base.length() < 3 || base.length() > 6) return false;
    for (size_t i = 0; i < base.length(); ++i) {
      if (!isAlphaNumeric(base[i])) return false;
    }
    if (hyphen >= 0) {
      if (ssid.length() < 1 || ssid.length() > 2) return false;
      for (size_t i = 0; i < ssid.length(); ++i) {
        if (!isAlphaNumeric(ssid[i])) return false;
      }
      if (ssid == "0") return false;
    }
    return true;
  }

  static void normalize(AppConfig &cfg) {
    cfg.wifiSsid.trim();
    cfg.callsign.trim();
    cfg.callsign.toUpperCase();
    cfg.aprsPasscode.trim();
    cfg.aprsServer.trim();
    cfg.grid.trim();
    cfg.grid.toUpperCase();
    cfg.comment.trim();
    if (cfg.aprsServer.length() == 0) cfg.aprsServer = DEFAULT_APRS_SERVER;
    if (cfg.aprsPort == 0) cfg.aprsPort = DEFAULT_APRS_PORT;
    if (cfg.comment.length() > 40) cfg.comment = cfg.comment.substring(0, 40);
  }
};
