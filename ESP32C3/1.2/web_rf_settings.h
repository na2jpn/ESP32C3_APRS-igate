#pragma once
#include "settings.h"
static String makeRfFrequencySelect(const AppConfig &cfg) {
  String page = F("<label>{{rf.title}}<select name='rxfreq' style='width:100%;padding:8px;font-size:16px'>");
  for (const auto &channel : RF_CHANNELS) {
    page += F("<option value='"); page += channel.frequency; page += '\'';
    if (cfg.rxFrequency == channel.frequency) page += F(" selected");
    page += '>'; page += channel.label;
    if (String(channel.frequency) == DEFAULT_DRA_RX_FREQ) page += F(" ({{rf.default}})");
    page += F("</option>");
  }
  page += F("</select></label><p class='small'>{{rf.note}}</p>");
  return page;
}
