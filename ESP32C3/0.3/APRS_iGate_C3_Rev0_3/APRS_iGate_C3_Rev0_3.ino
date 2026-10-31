#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Wire.h>

#include "config.h"
#include "settings.h"
#include "lcd1602_pcf8574.h"
#include "dra818v.h"
#include "afsk1200.h"
#include "audio_adc.h"
#include "ax25.h"
#include "aprsis.h"

// -----------------------------------------------------------------------------
// Global devices / state
// -----------------------------------------------------------------------------

struct FrameMsg {
  uint16_t len;
  uint8_t data[512];
};

SettingsStore settingsStore;
AppConfig config;
Lcd1602PCF8574 lcd;
HardwareSerial draSerial(1);
DRA818VController dra(draSerial);
AprsIsClient aprs;
WebServer web(80);
DNSServer dns;

QueueHandle_t frameQueue = nullptr;

void onAx25Frame(const uint8_t *frame, size_t len);
Afsk1200Decoder afsk(onAx25Frame);
AudioAdcCapture audio(afsk);

bool lcdPresent = false;
bool draReady = false;
bool adcReady = false;
bool setupApActive = false;
bool mdnsActive = false;
bool rebootPending = false;
uint32_t rebootAt = 0;

uint32_t wifiAttemptStart = 0;
uint32_t lastWifiRetry = 0;
uint32_t lastWifiStackReset = 0;
uint32_t lastDraAttempt = 0;
uint32_t lastInternetGood = 0;
uint32_t lcdOnUntil = 0;
uint32_t lastLcdUpdate = 0;
uint32_t lcdPageSince = 0;
uint8_t lcdPage = 0;

bool buttonWasDown = false;
uint32_t buttonDownAt = 0;

uint32_t rxPacketCount = 0;
uint32_t gatedPacketCount = 0;
uint32_t duplicateCount = 0;
uint32_t blockedCount = 0;
uint32_t noIsDropCount = 0;
String lastRxCall = "-";
String lastRxLine = "";
String lastReject = "";
uint32_t aprsLedPulseUntil = 0;

struct DedupEntry {
  uint32_t hash = 0;
  uint32_t at = 0;
};
DedupEntry dedup[32];
uint8_t dedupPos = 0;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static String htmlEscape(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  s.replace("'", "&#39;");
  return s;
}

static String uptimeText() {
  uint32_t sec = millis() / 1000UL;
  uint32_t d = sec / 86400UL; sec %= 86400UL;
  uint32_t h = sec / 3600UL;  sec %= 3600UL;
  uint32_t m = sec / 60UL;
  char b[32];
  snprintf(b, sizeof(b), "%lud %02lu:%02lu", (unsigned long)d, (unsigned long)h, (unsigned long)m);
  return String(b);
}

static uint32_t fnv1a(const String &s) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < s.length(); ++i) {
    h ^= static_cast<uint8_t>(s[i]);
    h *= 16777619u;
  }
  return h;
}

static bool isDuplicate(const String &line) {
  const uint32_t now = millis();
  const uint32_t h = fnv1a(line);
  for (const auto &e : dedup) {
    if (e.hash == h && static_cast<uint32_t>(now - e.at) < 30000UL) return true;
  }
  dedup[dedupPos].hash = h;
  dedup[dedupPos].at = now;
  dedupPos = (dedupPos + 1) % 32;
  return false;
}

static String makeSetupSsid() {
  uint64_t id = ESP.getEfuseMac();
  char b[32];
  snprintf(b, sizeof(b), "APRS-iGate-%04X", static_cast<unsigned>(id & 0xFFFF));
  return String(b);
}

static void lcdWake() {
  lcdOnUntil = millis() + LCD_ON_MS;
  if (lcdPresent) lcd.setBacklight(true);
}

static void scheduleReboot(uint32_t delayMs = 1200) {
  rebootPending = true;
  rebootAt = millis() + delayMs;
}

// -----------------------------------------------------------------------------
// Wi-Fi / Setup AP / mDNS
// -----------------------------------------------------------------------------

static void startSetupAp() {
  if (setupApActive) {
    lcdWake();
    return;
  }
  DBG_PRINTLN("[BOOT] WiFi AP_STA mode");
  const bool modeOk = WiFi.mode(WIFI_AP_STA);
  DBG_PRINTF("[BOOT] WiFi AP_STA mode result=%d\n", modeOk);
  if (!modeOk) return;
  const String ssid = makeSetupSsid();
  DBG_PRINTLN("[BOOT] softAP start");
  if (WiFi.softAP(ssid.c_str(), SETUP_AP_PASSWORD)) {
    dns.start(53, "*", WiFi.softAPIP());
    setupApActive = true;
    DBG_PRINTF("Setup AP: %s / %s / %s\n", ssid.c_str(), SETUP_AP_PASSWORD, WiFi.softAPIP().toString().c_str());
    lcdWake();
  } else {
    DBG_PRINTLN("[BOOT] softAP failed");
  }
}

static void beginStationWifi() {
  if (config.wifiSsid.length() == 0) return;
  const uint32_t started = millis();
  DBG_PRINTF("[BOOT] Station WiFi mode begin heap=%u\n", ESP.getFreeHeap());
  const bool modeOk = WiFi.mode(setupApActive ? WIFI_AP_STA : WIFI_STA);
  DBG_PRINTF("[BOOT] Station WiFi mode result=%d elapsed=%lu ms\n", modeOk, (unsigned long)(millis() - started));
  if (!modeOk) { wifiAttemptStart = millis(); lastWifiRetry = millis(); return; }
  DBG_PRINTLN("[BOOT] WiFi hostname begin");
  const bool hostOk = WiFi.setHostname("aprs-igate");
  DBG_PRINTF("[BOOT] WiFi hostname result=%d\n", hostOk);
  DBG_PRINTLN("[BOOT] WiFi.begin begin");
  const wl_status_t result = WiFi.begin(config.wifiSsid.c_str(), config.wifiPass.c_str());
  DBG_PRINTF("[BOOT] WiFi.begin result=%d elapsed=%lu ms heap=%u\n", (int)result, (unsigned long)(millis() - started), ESP.getFreeHeap());
  wifiAttemptStart = millis();
  lastWifiRetry = millis();
}

static void updateMdns() {
  if (WiFi.status() == WL_CONNECTED && !mdnsActive) {
    if (MDNS.begin("aprs-igate")) {
      MDNS.addService("http", "tcp", 80);
      mdnsActive = true;
    }
  }
}

static void serviceWifi() {
  static wl_status_t prev = WL_NO_SHIELD;
  wl_status_t st = WiFi.status();

  if (st == WL_CONNECTED) {
    if (prev != WL_CONNECTED) {
      DBG_PRINTF("WiFi connected: %s\n", WiFi.localIP().toString().c_str());
      lastInternetGood = millis();
      aprs.configure(config);
      aprs.connectNow();
    }
    updateMdns();
  } else {
    if (config.wifiSsid.length()) {
      if (millis() - lastWifiRetry >= WIFI_RETRY_MS) {
        lastWifiRetry = millis();
        WiFi.disconnect(false, false);
        WiFi.begin(config.wifiSsid.c_str(), config.wifiPass.c_str());
      }
      if (!setupApActive && wifiAttemptStart && millis() - wifiAttemptStart >= WIFI_CONNECT_TIMEOUT_MS) {
        startSetupAp();
      }
      if (millis() - lastInternetGood >= WIFI_STACK_RESET_MS && millis() - lastWifiStackReset >= WIFI_STACK_RESET_MS) {
        lastWifiStackReset = millis();
        WiFi.disconnect(false, false);
        WiFi.mode(WIFI_OFF);
        delay(100);
        beginStationWifi();
      }
    }
  }
  prev = st;
}

// -----------------------------------------------------------------------------
// Web UI
// -----------------------------------------------------------------------------

static String statusBadge(const String &text, bool ok) {
  return String("<span class='badge ") + (ok ? "ok" : "ng") + "'>" + htmlEscape(text) + "</span>";
}

static String makePage() {
  const bool wifiOk = WiFi.status() == WL_CONNECTED;
  String page;
  page.reserve(7000);
  page += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>APRS iGate</title><style>body{font-family:system-ui,sans-serif;max-width:760px;margin:24px auto;padding:0 14px;color:#222}h1{font-size:1.5rem}fieldset{margin:14px 0;padding:14px;border:1px solid #ccc;border-radius:8px}label{display:block;margin:9px 0}input{width:100%;box-sizing:border-box;padding:8px;font-size:16px}button{padding:10px 14px;margin:5px 5px 5px 0}.badge{display:inline-block;padding:3px 8px;border-radius:999px;margin:2px}.ok{background:#d9f5df}.ng{background:#f7d9d9}.mono{font-family:monospace;word-break:break-all}.small{font-size:.9rem;color:#555}</style></head><body>");
  page += F("<h1>APRS iGate ESP32C3 Rev0.3</h1>");
  page += statusBadge(String("Wi-Fi ") + (wifiOk ? "OK" : "NG"), wifiOk);
  page += statusBadge(String("DRA818V ") + (draReady ? "OK" : "NG"), draReady);
  page += statusBadge(String("ADC ") + (adcReady ? "OK" : "NG"), adcReady);
  page += statusBadge(String("APRS-IS ") + (aprs.verified() ? "VERIFIED" : (aprs.connected() ? "UNVERIFIED" : "OFF")), aprs.verified());
  page += F("<p><b>RF:</b> 144.640 MHz / RX only / qAO</p>");
  page += F("<p><b>IP:</b> ");
  page += wifiOk ? WiFi.localIP().toString() : String("-");
  page += F(" &nbsp; <b>mDNS:</b> aprs-igate.local &nbsp; <b>Uptime:</b> ");
  page += uptimeText();
  page += F("</p>");
  page += F("<p><b>Decoded:</b> "); page += rxPacketCount;
  page += F(" &nbsp; <b>Gated:</b> "); page += gatedPacketCount;
  page += F(" &nbsp; <b>Dup:</b> "); page += duplicateCount;
  page += F(" &nbsp; <b>Blocked:</b> "); page += blockedCount;
  page += F(" &nbsp; <b>CRC bad:</b> "); page += audio.monitor().crcBad;
  page += F(" &nbsp; <b>AF RMS:</b> "); page += String(audio.monitor().rms, 5);
  page += F("</p><fieldset><legend>AFSK receive monitor (auto-update 1 s)</legend>");
  page += F("<div class='mono' id='afdiag'>Loading monitor...</div>");
  page += F("<p class='small'>ADC Min/Max are raw codes in the last 250 ms window. Mark/Space are detector magnitudes, not calibrated dB. Peak holds the maximum RMS over approximately 5 s.</p></fieldset>");
  page += F("<script>async function refreshAF(){try{const r=await fetch('/status',{cache:'no-store'});if(!r.ok)throw Error(r.status);const s=await r.json();const f=x=>Number(x||0).toFixed(5);document.getElementById('afdiag').textContent='AF RMS: '+f(s.af_rms)+' | Peak 5s: '+f(s.af_peak5s)+' | ADC Min/Max: '+s.adc_min+'/'+s.adc_max+' | 1200Hz: '+f(s.mark1200)+' | 2200Hz: '+f(s.space2200)+' | CRC bad: '+s.crc_bad+' | Decoded: '+s.decoded+' | Samples: '+s.samples+' | DMA overflow: '+s.adc_overflows+' | Read errors: '+s.adc_read_errors+' | Stack free: '+s.adc_stack_free_bytes+' B | Max block: '+s.adc_max_block_us+' us';}catch(e){document.getElementById('afdiag').textContent='Monitor unavailable: '+e.message;}}refreshAF();setInterval(refreshAF,1000);</script>");
  page += F("<p class='mono'><b>Last RX:</b> "); page += htmlEscape(lastRxLine); page += F("</p>");

  page += F("<form method='post' action='/save'><fieldset><legend>Wi-Fi / APRS-IS</legend>");
  page += F("<label>Wi-Fi SSID<input name='ssid' value='"); page += htmlEscape(config.wifiSsid); page += F("'></label>");
  page += F("<label>Wi-Fi Password<input type='password' name='wpass' placeholder='blank = keep current'></label>");
  page += F("<label>iGate Callsign-SSID<input name='call' maxlength='9' value='"); page += htmlEscape(config.callsign); page += F("'></label>");
  page += F("<label>APRS-IS Passcode<input name='apass' value='"); page += htmlEscape(config.aprsPasscode); page += F("'></label>");
  page += F("<label>APRS-IS Server<input name='server' value='"); page += htmlEscape(config.aprsServer); page += F("'></label>");
  page += F("<label>Port<input name='port' type='number' min='1' max='65535' value='"); page += config.aprsPort; page += F("'></label>");
  page += F("<label>Grid (saved for future use)<input name='grid' maxlength='8' value='"); page += htmlEscape(config.grid); page += F("'></label>");
  page += F("<label>Comment<input name='comment' maxlength='40' value='"); page += htmlEscape(config.comment); page += F("'></label>");
  page += F("<button type='submit'>Save & Reboot</button></fieldset></form>");

  page += F("<fieldset><legend>Maintenance</legend><form method='post' action='/reboot' style='display:inline'><button>Reboot</button></form>");
  page += F("<form method='post' action='/factory' style='display:inline' onsubmit=\"return confirm('Erase settings?')\"><button>Factory reset</button></form></fieldset>");
  page += F("<p class='small'>SETUP button: short press = LCD backlight 60 s / 5 s = setup AP / 15 s = factory reset. Setup AP password: <b>aprssetup</b>.</p>");
  page += F("</body></html>");
  return page;
}

static void setupWebRoutes() {
  web.on("/", HTTP_GET, []() { web.send(200, "text/html; charset=utf-8", makePage()); });

  web.on("/status", HTTP_GET, []() {
    String j = "{";
    j += "\"wifi\":"; j += String(WiFi.status() == WL_CONNECTED ? "true" : "false");
    j += ",\"dra\":"; j += String(draReady ? "true" : "false");
    j += ",\"adc\":"; j += String(adcReady ? "true" : "false");
    j += ",\"aprs_connected\":"; j += String(aprs.connected() ? "true" : "false");
    j += ",\"aprs_verified\":"; j += String(aprs.verified() ? "true" : "false");
    j += ",\"decoded\":"; j += String(rxPacketCount);
    j += ",\"gated\":"; j += String(gatedPacketCount);
    const AfMonitorSnapshot m = audio.monitor();
    j += ",\"crc_bad\":"; j += String(m.crcBad);
    j += ",\"frames_good\":"; j += String(m.framesGood);
    j += ",\"adc_updated_ms\":"; j += String(m.updatedAtMs);
    j += ",\"af_rms\":"; j += String(m.rms, 6);
    j += ",\"af_peak5s\":"; j += String(m.peak5s, 6);
    j += ",\"adc_min\":"; j += String(m.adcMin);
    j += ",\"adc_max\":"; j += String(m.adcMax);
    j += ",\"mark1200\":"; j += String(m.mark1200, 6);
    j += ",\"space2200\":"; j += String(m.space2200, 6);
    j += ",\"samples\":"; j += String(m.sampleCount);
    j += ",\"adc_read_errors\":"; j += String(m.readErrors);
    j += ",\"adc_timeouts\":"; j += String(m.readTimeouts);
    j += ",\"adc_overflows\":"; j += String(m.poolOverflows);
    j += ",\"adc_max_block_us\":"; j += String(m.maxBlockUs);
    j += ",\"adc_processing_us\":"; j += String(m.processingUs);
    j += ",\"adc_stack_free_bytes\":"; j += String(m.stackFreeBytes);
    j += ",\"frame_errors\":"; j += String(m.frameErrors);
    j += "}";
    web.send(200, "application/json", j);
  });

  web.on("/save", HTTP_POST, []() {
    AppConfig next = config;
    next.wifiSsid = web.arg("ssid");
    if (web.arg("wpass").length()) next.wifiPass = web.arg("wpass");
    next.callsign = web.arg("call");
    next.aprsPasscode = web.arg("apass");
    next.aprsServer = web.arg("server");
    long p = web.arg("port").toInt();
    next.aprsPort = (p > 0 && p <= 65535) ? static_cast<uint16_t>(p) : DEFAULT_APRS_PORT;
    next.grid = web.arg("grid");
    next.comment = web.arg("comment");
    SettingsStore::normalize(next);

    if (next.wifiSsid.length() == 0 || !SettingsStore::validCallsign(next.callsign)) {
      web.send(400, "text/plain; charset=utf-8", "SSID or callsign is invalid.");
      return;
    }
    if (!settingsStore.save(next)) {
      web.send(500, "text/plain; charset=utf-8", "Failed to save settings.");
      return;
    }
    web.send(200, "text/html; charset=utf-8", "<meta charset='utf-8'><p>Saved. Rebooting...</p>");
    scheduleReboot();
  });

  web.on("/reboot", HTTP_POST, []() {
    web.send(200, "text/html; charset=utf-8", "<meta charset='utf-8'><p>Rebooting...</p>");
    scheduleReboot();
  });

  web.on("/factory", HTTP_POST, []() {
    settingsStore.clear();
    web.send(200, "text/html; charset=utf-8", "<meta charset='utf-8'><p>Settings erased. Rebooting...</p>");
    scheduleReboot();
  });

  web.onNotFound([]() {
    if (setupApActive) {
      web.sendHeader("Location", "http://192.168.4.1/", true);
      web.send(302, "text/plain", "");
    } else {
      web.send(404, "text/plain", "Not found");
    }
  });
  // FIX2: Do not start TCP server until Wi-Fi interface is initialized.
}

// -----------------------------------------------------------------------------
// AFSK / AX.25 receive path
// -----------------------------------------------------------------------------

void onAx25Frame(const uint8_t *frame, size_t len) {
  if (!frameQueue || !frame || len == 0 || len > 512) return;
  FrameMsg msg{};
  msg.len = static_cast<uint16_t>(len);
  memcpy(msg.data, frame, len);
  xQueueSend(frameQueue, &msg, 0); // never block the audio task
}

static void handleFrame(const FrameMsg &msg) {
  Ax25Packet p;
  if (!Ax25Parser::parse(msg.data, msg.len, config.callsign, p)) return;

  ++rxPacketCount;
  lastRxCall = p.source.length() ? p.source : "?";
  lastRxLine = p.tnc2;
  lcdWake();
  aprsLedPulseUntil = millis() + 100;

  DBG_PRINTF("RF: %s\n", p.tnc2.c_str());

  if (!p.gateAllowed) {
    ++blockedCount;
    lastReject = p.rejectReason;
    return;
  }
  if (isDuplicate(p.tnc2)) {
    ++duplicateCount;
    return;
  }
  if (!aprs.verified()) {
    ++noIsDropCount;
    return;
  }
  if (aprs.sendRfPacket(p.gatedTnc2)) {
    ++gatedPacketCount;
    DBG_PRINTF("IS: %s\n", p.gatedTnc2.c_str());
  }
}

// -----------------------------------------------------------------------------
// UI: button, LEDs, LCD
// -----------------------------------------------------------------------------

static void serviceButton() {
  const bool down = digitalRead(PIN_SETUP_SW) == LOW;
  if (down && !buttonWasDown) {
    buttonWasDown = true;
    buttonDownAt = millis();
  } else if (!down && buttonWasDown) {
    const uint32_t held = millis() - buttonDownAt;
    buttonWasDown = false;
    if (held >= BUTTON_RESET_MS) {
      settingsStore.clear();
      scheduleReboot(500);
    } else if (held >= BUTTON_SETUP_MS) {
      startSetupAp();
    } else if (held >= 30) {
      lcdWake();
    }
  }
}

static void serviceLeds() {
  const uint32_t now = millis();
  // Shared non-blocking cycle: Wi-Fi [0,10), APRS [10,20), both off [20,30).
  const uint32_t phase = now % LED_STATUS_PERIOD_MS;
  if (WiFi.status() == WL_CONNECTED)
    digitalWrite(PIN_LED_WIFI, phase < LED_STATUS_ON_MS ? HIGH : LOW);
  else digitalWrite(PIN_LED_WIFI, ((now / 400) & 1) ? HIGH : LOW);

  if (now < aprsLedPulseUntil) {
    digitalWrite(PIN_LED_APRS, LOW); // visible blink-off when normally connected
  } else if (aprs.verified()) {
    const bool on = phase >= LED_APRS_OFFSET_MS &&
                    phase < LED_APRS_OFFSET_MS + LED_STATUS_ON_MS;
    digitalWrite(PIN_LED_APRS, on ? HIGH : LOW);
  } else {
    digitalWrite(PIN_LED_APRS, ((now / 700) & 1) ? HIGH : LOW);
  }
}

static String fit16(String s) {
  if (s.length() > 16) s = s.substring(0, 16);
  while (s.length() < 16) s += ' ';
  return s;
}

static void serviceLcd() {
  if (!lcdPresent) return;
  const uint32_t now = millis();

  const bool shouldOn = setupApActive || static_cast<int32_t>(lcdOnUntil - now) > 0;
  if (lcd.backlight() != shouldOn) lcd.setBacklight(shouldOn);
  if (!shouldOn) return;

  if (now - lastLcdUpdate < 500) return;
  lastLcdUpdate = now;

  if (setupApActive && WiFi.status() != WL_CONNECTED) {
    lcd.printLine(0, "SETUP AP ACTIVE");
    lcd.printLine(1, "192.168.4.1");
    return;
  }

  if (now - lcdPageSince > 4000) {
    lcdPage = (lcdPage + 1) % 3;
    lcdPageSince = now;
  }

  if (lcdPage == 0) {
    String l1 = config.callsign.length() ? config.callsign + " iGate" : "APRS iGate 0.3";
    String l2 = String("W:") + (WiFi.status() == WL_CONNECTED ? "OK" : "--") + " IS:" + (aprs.verified() ? "OK" : "--");
    lcd.printLine(0, l1);
    lcd.printLine(1, l2);
  } else if (lcdPage == 1) {
    lcd.printLine(0, "144.640 RX ONLY");
    lcd.printLine(1, String("RX ") + lastRxCall);
  } else {
    char b[17];
    snprintf(b, sizeof(b), "RX:%lu IS:%lu", (unsigned long)(rxPacketCount % 10000), (unsigned long)(gatedPacketCount % 10000));
    lcd.printLine(0, b);
    String l2 = String("AF:") + String(audio.monitor().rms, 4) + " C:" + String(audio.monitor().crcBad % 100);
    lcd.printLine(1, l2);
  }
}

// -----------------------------------------------------------------------------
// Long-run recovery
// -----------------------------------------------------------------------------

static void serviceRecovery() {
  if (aprs.connected()) lastInternetGood = millis();

  // A live TCP session counts as Internet reachability even when APRS-IS login
  // is unverified. Therefore a wrong passcode does not cause reboot loops.
  const bool configured = SettingsStore::validForStation(config);
  if (!configured || aprs.connected()) return;

  const uint32_t lostFor = millis() - lastInternetGood;
  if (lostFor >= WIFI_STACK_RESET_MS && millis() - lastWifiStackReset >= WIFI_STACK_RESET_MS) {
    lastWifiStackReset = millis();
    DBG_PRINTLN("Internet path timeout -> restart WiFi stack");
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    delay(100);
    beginStationWifi();
  }

  if (lostFor >= INTERNET_REBOOT_MS) {
    DBG_PRINTLN("Internet/APRS-IS timeout -> reboot");
    ESP.restart();
  }
}

// -----------------------------------------------------------------------------
// Arduino entry points
// -----------------------------------------------------------------------------

void setup() {
  DBG_BEGIN();
  delay(100);
  DBG_PRINTLN("\nAPRS iGate ESP32C3 Rev0.3");
  DBG_PRINTF("[BOOT] IDF=%s tick_hz=%u setup_priority=%u heap=%u\n",
    ESP.getSdkVersion(), (unsigned)configTICK_RATE_HZ,
    (unsigned)uxTaskPriorityGet(nullptr), ESP.getFreeHeap());

  pinMode(PIN_LED_WIFI, OUTPUT);
  pinMode(PIN_LED_APRS, OUTPUT);
  pinMode(PIN_SETUP_SW, INPUT_PULLUP);
  digitalWrite(PIN_LED_WIFI, LOW);
  digitalWrite(PIN_LED_APRS, LOW);

  DBG_PRINTF("[BOOT] settings load heap=%u\n", ESP.getFreeHeap());
  settingsStore.load(config);
  SettingsStore::normalize(config);
  DBG_PRINTLN("[BOOT] settings ready");

  DBG_PRINTLN("[BOOT] LCD begin");
  lcdPresent = lcd.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  DBG_PRINTF("[BOOT] LCD result=%d\n", lcdPresent);
  if (lcdPresent) {
    lcd.setBacklight(true);
    lcd.printLine(0, "APRS iGate 0.3");
    lcd.printLine(1, "Booting...");
    lcdOnUntil = millis() + LCD_ON_MS;
  }

  DBG_PRINTLN("[BOOT] DRA begin");
  draReady = dra.begin();
  lastDraAttempt = millis();
  DBG_PRINTF("[BOOT] DRA result=%d\n", draReady);

  DBG_PRINTLN("[BOOT] frame queue create");
  frameQueue = xQueueCreate(6, sizeof(FrameMsg));
  if (!frameQueue) DBG_PRINTLN("[ERROR] frame queue allocation failed");
  else DBG_PRINTLN("[BOOT] frame queue ready");
  DBG_PRINTLN("[BOOT] AFSK decoder begin");
  afsk.begin();
  DBG_PRINTLN("[BOOT] AFSK decoder ready");
  DBG_PRINTLN("[BOOT] Web routes begin");
  setupWebRoutes();
  DBG_PRINTLN("[BOOT] Web routes ready (not listening yet)");
  lastInternetGood = millis();

  if (SettingsStore::validForStation(config)) {
    DBG_PRINTLN("[BOOT] Station WiFi begin");
    beginStationWifi();
  } else {
    DBG_PRINTLN("[BOOT] Setup AP begin");
    startSetupAp();
  }
  DBG_PRINTLN("[BOOT] Web server begin after WiFi init");
  web.begin();
  DBG_PRINTLN("[BOOT] Web server ready");
#if APRS_DIAGNOSTIC_SKIP_ADC
  DBG_PRINTLN("[DIAG] ADC DMA intentionally disabled (RX decode unavailable)");
  adcReady = false;
#else
  DBG_PRINTLN("[BOOT] ADC DMA begin");
  adcReady = audio.begin();
  DBG_PRINTF("[BOOT] ADC DMA result=%d\n", adcReady);
#endif
  // Refresh the normal screen explicitly once all boot stages have returned.
  // Restart the 60-second backlight timer here, not at the beginning of setup.
  lcdWake();
  lcdPage = 0;
  lcdPageSince = millis();
  lastLcdUpdate = millis() - 500UL;
  serviceLcd();
  DBG_PRINTLN("[BOOT] LCD normal screen requested");
  DBG_PRINTF("[BOOT] setup() finished heap=%u\n", ESP.getFreeHeap());
}

void loop() {
  serviceButton();
  serviceWifi();
  aprs.loop();
  serviceRecovery();

  if (setupApActive) dns.processNextRequest();
  web.handleClient();

  FrameMsg msg;
  while (frameQueue && xQueueReceive(frameQueue, &msg, 0) == pdTRUE) {
    handleFrame(msg);
  }

  if (!draReady && millis() - lastDraAttempt > 30000UL) {
    lastDraAttempt = millis();
    draReady = dra.begin();
  }

  serviceLeds();
  serviceLcd();

  if (rebootPending && static_cast<int32_t>(millis() - rebootAt) >= 0) {
    delay(50);
    ESP.restart();
  }

  static uint32_t lastHealth = 0;
  if (millis() - lastHealth >= 10000UL) {
    lastHealth = millis();
    const AfMonitorSnapshot m = audio.monitor();
    DBG_PRINTF("[HEALTH] heap=%u loop_stack=%u adc_stack=%lu samples=%lu overflow=%lu read_err=%lu timeout=%lu max_block_us=%lu processing_us=%lu\n",
      ESP.getFreeHeap(), (unsigned)uxTaskGetStackHighWaterMark(nullptr),
      (unsigned long)m.stackFreeBytes, (unsigned long)m.sampleCount,
      (unsigned long)m.poolOverflows, (unsigned long)m.readErrors,
      (unsigned long)m.readTimeouts, (unsigned long)m.maxBlockUs, (unsigned long)m.processingUs);
  }
  delay(2);
}
