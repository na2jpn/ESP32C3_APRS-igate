#pragma once
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

// ADC continues sampling. Flash writes may lose RF packets; overflow recovery
// already resets the decoder. The main loop prioritizes uploads while active.
static bool otaBusy = false;
static bool otaSuccess = false;
static bool otaAuthorized = false;
static uint32_t otaActivity = 0;
static size_t otaExpected = 0;
static size_t otaReceived = 0;
static String otaError;
static String otaToken;

static void abortOta(const char *reason) {
  if (otaBusy) Update.abort();
  otaBusy = false; otaSuccess = false; otaError = reason;
  DBG_PRINTF("[OTA] %s\n", reason);
}
static void serviceOtaTimeout() {
  if (otaBusy && uint32_t(millis() - otaActivity) > 30000UL) abortOta("Upload timed out; old firmware retained.");
}
static void setupOtaRoutes(WebServer &server, AppConfig &cfg) {
  char token[33];
  snprintf(token, sizeof(token), "%08lx%08lx%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random());
  otaToken = token;
  server.on("/update", HTTP_GET, [&server, &cfg]() {
    if (cfg.otaPassword.length() < 8) { server.send(403, "text/plain", "Set an OTA password in settings first, then save and reboot."); return; }
    if (!server.authenticate("admin", cfg.otaPassword.c_str())) { server.requestAuthentication(); return; }
    const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
    if (!part) { server.send(409, "text/plain", "No OTA partition. Install v0.4 by USB using an OTA-capable partition scheme."); return; }
    String page = "<!doctype html><meta charset='utf-8'><h1>APRS iGate firmware update</h1><p>Upload only this project's ESP32C3 application .bin (not merged/bootloader/partitions). RF packets may be lost during update. Keep power connected.</p><input id='f' type='file' accept='.bin'><button onclick='go()'>Update</button><pre id='s'></pre><script>async function go(){const f=document.getElementById('f').files[0];if(!f)return;const d=new FormData();d.append('firmware',f);document.getElementById('s').textContent='Uploading...';try{const r=await fetch('/update?token=";
    page += otaToken;
    page += "&size='+f.size,{method:'POST',body:d});document.getElementById('s').textContent=await r.text();}catch(e){document.getElementById('s').textContent='Connection lost: reload and check firmware version before retrying.';}}</script>";
    server.sendHeader("Cache-Control", "no-store"); server.send(200, "text/html; charset=utf-8", page);
  });
  server.on("/update", HTTP_POST, [&server, &cfg]() {
    if (cfg.otaPassword.length() < 8 || !server.authenticate("admin", cfg.otaPassword.c_str())) { server.requestAuthentication(); return; }
    if (!otaAuthorized) { server.send(403, "text/plain", "Invalid update token."); return; }
    if (!otaSuccess) { server.send(400, "text/plain", otaError.length() ? otaError : String("No valid firmware received.")); return; }
    server.send(200, "text/plain", "Update complete. Rebooting; reload after 15 seconds.");
    delay(250); ESP.restart();
  }, [&server, &cfg]() {
    HTTPUpload &u = server.upload();
    if (u.status == UPLOAD_FILE_START) {
      otaAuthorized = cfg.otaPassword.length() >= 8 && server.authenticate("admin", cfg.otaPassword.c_str()) && server.arg("token") == otaToken;
      otaSuccess = false; otaError = "";
      if (!otaAuthorized) return;
      if (otaBusy) { abortOta("Multiple files are not supported."); return; }
      long declaredSize = server.arg("size").toInt();
      const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
      if (u.name != "firmware" || !u.filename.endsWith(".bin") || declaredSize <= 0 || !part || (size_t)declaredSize > part->size) {
        otaError = "Invalid file or size, or no suitable OTA partition."; return;
      }
      otaExpected = (size_t)declaredSize; otaReceived = 0;
      if (!Update.begin(otaExpected, U_FLASH)) { otaError = Update.errorString(); return; }
      otaBusy = true; otaActivity = millis();
      DBG_PRINTF("[OTA] begin size=%u slot=%s\n", (unsigned)otaExpected, part->label);
    } else if (u.status == UPLOAD_FILE_WRITE && otaAuthorized && otaBusy) {
      otaActivity = millis();
      if (u.currentSize > otaExpected - otaReceived || Update.write(u.buf, u.currentSize) != u.currentSize) { abortOta("Firmware write failed; old firmware retained."); return; }
      otaReceived += u.currentSize;
    } else if (u.status == UPLOAD_FILE_END && otaAuthorized && otaBusy) {
      if (otaReceived != otaExpected || u.totalSize != otaExpected) { abortOta("Incomplete firmware; old firmware retained."); return; }
      otaSuccess = Update.end(false);
      if (!otaSuccess) otaError = Update.errorString();
      otaBusy = false;
      DBG_PRINTF("[OTA] verification result=%d\n", otaSuccess);
    } else if (u.status == UPLOAD_FILE_ABORTED && otaAuthorized) abortOta("Upload aborted; old firmware retained.");
  });
}
