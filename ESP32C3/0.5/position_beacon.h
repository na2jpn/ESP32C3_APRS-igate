#pragma once
#include <cmath>
#include <cstdlib>
#include <cstdio>

// Decimal coordinates only: no exponent, trailing text, NaN or infinity.
static bool parseCoordinate(const String &s, double limit, double &out) {
  if (!s.length()) return false;
  unsigned digits = 0, dots = 0;
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (i == 0 && (c == '+' || c == '-')) continue;
    if (c == '.' && ++dots <= 1) continue;
    if (c < '0' || c > '9') return false;
    ++digits;
  }
  if (!digits) return false;
  char *end = nullptr;
  out = strtod(s.c_str(), &end);
  return end && !*end && std::isfinite(out) && fabs(out) <= limit;
}
static bool parsePosition(const String &latText, const String &lonText, double &lat, double &lon) {
  return parseCoordinate(latText, 90, lat) && parseCoordinate(lonText, 180, lon);
}
static String positionPacket(const AppConfig &cfg) {
  double lat, lon;
  if (!parsePosition(cfg.latitude, cfg.longitude, lat, lon)) return "";
  // Round total hundredths of minutes first, carrying 59.995 minutes correctly.
  long a = lround(fabs(lat) * 6000.0), b = lround(fabs(lon) * 6000.0);
  char pos[64];
  // Alternate table I/& = I-overlay gateway symbol; no RF messaging claim.
  snprintf(pos, sizeof(pos), "!%02ld%02ld.%02ld%cI%03ld%02ld.%02ld%c&",
    a / 6000, (a / 100) % 60, a % 100, lat < 0 ? 'S' : 'N',
    b / 6000, (b / 100) % 60, b % 100, lon < 0 ? 'W' : 'E');
  String comment = cfg.comment;
  comment.replace("\r", " "); comment.replace("\n", " ");
  return cfg.callsign + ">APRS,TCPIP*:" + String(pos) + comment;
}
static uint32_t beaconSentCount = 0;
static uint32_t beaconLastSent = 0;
static uint32_t beaconLastAttempt = 0;
static bool beaconEverSent = false;
static bool beaconAttempted = false;
static void servicePositionBeacon(AprsIsClient &client, const AppConfig &cfg) {
  if (!cfg.beaconEnabled || !client.verified()) return;
  const uint32_t now = millis();
  if (beaconEverSent && uint32_t(now - beaconLastSent) < 1800000UL) return;
  if (beaconAttempted && uint32_t(now - beaconLastAttempt) < 60000UL) return;
  String line = positionPacket(cfg);
  if (!line.length()) return;
  beaconAttempted = true; beaconLastAttempt = now;
  if (client.sendRfPacket(line)) {
    beaconEverSent = true; beaconLastSent = now; ++beaconSentCount;
    DBG_PRINTF("[BEACON] TCP write OK: %s\n", line.c_str());
  } else DBG_PRINTLN("[BEACON] TCP write failed; retry after 60 s");
}
