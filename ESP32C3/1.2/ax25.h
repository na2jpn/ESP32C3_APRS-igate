#pragma once

#include <Arduino.h>

struct Ax25Packet {
  String source;
  String destination;
  String path;
  String info;
  String tnc2;
  String gatedTnc2;
  bool gateAllowed = false;
  String rejectReason;
};

class Ax25Parser {
public:
  static bool parse(const uint8_t *frame, size_t len, const String &igateCall, Ax25Packet &out) {
    out = Ax25Packet{};
    if (!frame || len < 16) {
      out.rejectReason = "short frame";
      return false;
    }

    Address addr[10];
    size_t addrCount = 0;
    size_t pos = 0;
    bool last = false;

    while (!last) {
      if (pos + 7 > len || addrCount >= 10) {
        out.rejectReason = "bad address field";
        return false;
      }
      addr[addrCount] = decodeAddress(frame + pos);
      last = (frame[pos + 6] & 0x01) != 0;
      ++addrCount;
      pos += 7;
    }

    if (addrCount < 2 || pos + 2 > len) {
      out.rejectReason = "missing src/dst/control";
      return false;
    }

    const uint8_t control = frame[pos++];
    const uint8_t pid = frame[pos++];
    if (control != 0x03 || pid != 0xF0) {
      out.rejectReason = "not AX.25 UI/F0";
      return false;
    }

    out.destination = addr[0].call;
    out.source = addr[1].call;

    String path;
    for (size_t i = 2; i < addrCount; ++i) {
      if (path.length()) path += ',';
      path += addr[i].call;
      if (addr[i].repeated) path += '*';
    }
    out.path = path;

    String info;
    info.reserve(len - pos);
    for (; pos < len; ++pos) {
      // APRS UI payload is normally text. Preserve common control bytes, but
      // avoid embedding NUL in Arduino String/TCP output.
      char c = static_cast<char>(frame[pos]);
      if (c == '\0') c = ' ';
      info += c;
    }
    out.info = info;

    out.tnc2 = out.source + ">" + out.destination;
    if (out.path.length()) { out.tnc2 += ","; out.tnc2 += out.path; }
    out.tnc2 += ":"; out.tnc2 += out.info;

    // Conservative RX-only IGate policy for Rev0.1.
    // Official APRS-IS guidance says not to gate these RF paths/types.
    for (size_t i = 2; i < addrCount; ++i) {
      String p = addr[i].call;
      p.toUpperCase();
      if (p == "TCPIP" || p == "TCPXX" || p == "NOGATE" || p == "RFONLY") {
        out.rejectReason = "blocked RF path";
        return true;
      }
    }
    if (out.info.startsWith("?")) {
      out.rejectReason = "generic query";
      return true;
    }
    // Third-party frames require special unwrapping rules. Rev0.1 drops them
    // rather than risk injecting a malformed or loop-prone packet.
    if (out.info.startsWith("}")) {
      out.rejectReason = "third-party deferred";
      return true;
    }

    if (igateCall.length() < 3) {
      out.rejectReason = "igate call missing";
      return true;
    }

    out.gatedTnc2 = out.source + ">" + out.destination;
    if (out.path.length()) { out.gatedTnc2 += ","; out.gatedTnc2 += out.path; }
    out.gatedTnc2 += ",qAO,"; out.gatedTnc2 += igateCall; out.gatedTnc2 += ":"; out.gatedTnc2 += out.info;
    if (out.gatedTnc2.length() > 500) {
      out.rejectReason = "line too long";
      return true;
    }

    out.gateAllowed = true;
    return true;
  }

private:
  struct Address {
    String call;
    bool repeated = false;
  };

  static Address decodeAddress(const uint8_t *p) {
    Address a;
    String base;
    for (int i = 0; i < 6; ++i) {
      char c = static_cast<char>(p[i] >> 1);
      if (c != ' ') base += c;
    }
    base.trim();
    base.toUpperCase();
    const uint8_t ssid = static_cast<uint8_t>((p[6] >> 1) & 0x0F);
    a.call = base;
    if (ssid) { a.call += '-'; a.call += String(ssid); }
    a.repeated = (p[6] & 0x80) != 0;
    return a;
  }
};
