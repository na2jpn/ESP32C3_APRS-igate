#pragma once
#include "ui_texts.h"

// Language is a presentation preference in the URL. No NVS writes or reboot.
// New requests default to Japanese; explicit lang=en is the only English mode.
static inline bool uiEnglish(WebServer &s) { return s.arg("lang") == "en"; }
static inline void uiRenderPageInPlace(String &page, bool english) {
  const char *lang = english ? "en" : "ja";
  page.replace("<html>", "<html lang='ja'>");
  if (english) {
    page.replace("<html lang='ja'>", "<html lang='en'>");
    page.replace("<html lang=\"ja\">", "<html lang=\"en\">");
  }
  // Rewrite only known navigation targets, never tokens, API paths or raw logs.
  for (const char *path : {"/", "/audio", "/rxdiag", "/logs", "/update"}) {
    for (const char *quote : {"'", "\""}) {
      String from = String("href=") + quote + path + quote;
      String to = String("href=") + quote + path + "?lang=" + lang + quote;
      page.replace(from.c_str(), to.c_str());
    }
  }
  for (const char *path : {"/save", "/reboot", "/factory", "/adc/recover"}) {
    String from = String("action='") + path + "'";
    String to = String("action='") + path + "?lang=" + lang + "'";
    page.replace(from.c_str(), to.c_str());
  }
  for (const char *path : {"/logs.txt", "/rxdiag.csv"}) {
    String from = String("href='") + path + "?download=1'";
    String to = String("href='") + path + "?download=1&amp;lang=" + lang + "'";
    page.replace(from.c_str(), to.c_str());
  }
  String redirect = String("60;url=/?lang=") + lang;
  page.replace("60;url=/", redirect.c_str());
  const char *languagePath = std::strstr(page.c_str(), "http-equiv='refresh'") ? "/" : "";
  String nav = F("<nav style='margin:12px 0;font-family:system-ui' aria-label='Language'>");
  nav += english ? F("<a onclick='return !globalThis.igateOtaRunning' href='") : F("<strong>日本語</strong> | <a onclick='return !globalThis.igateOtaRunning' href='");
  nav += languagePath;
  nav += english ? F("?lang=ja'>日本語</a> | <strong>English</strong>") : F("?lang=en'>English</a>");
  nav += F("</nav><h1");
  page.replace("<h1", nav.c_str());
  for (const auto &t : UI_TEXTS) page.replace(t.marker, english ? t.en : t.ja);
}
static inline String uiRenderPage(String page, bool english) {
  uiRenderPageInPlace(page, english); return page;
}
static inline void sendUiPage(WebServer &s, String page) {
  uiRenderPageInPlace(page, uiEnglish(s));
  s.send(200, "text/html; charset=utf-8", page);
}
