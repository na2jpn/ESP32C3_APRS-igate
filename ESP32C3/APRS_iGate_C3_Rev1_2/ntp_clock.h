#pragma once
#include "device_clock.h"
#include "esp_sntp.h"
#include <sys/time.h>

// lwIP task callback: small synchronized anchor only; no logging/allocation.
static void ntpSynced(struct timeval *tv){
  deviceClock.sync(DeviceClock::uptime(),uint64_t(tv->tv_sec)*1000+tv->tv_usec/1000);
}
static void serviceClock(bool connected){
  static bool started=false;static uint32_t logged=0;
  if(connected&&!started){
    sntp_set_time_sync_notification_cb(ntpSynced);
    configTime(0,0,"ntp.nict.jp","time.cloudflare.com","pool.ntp.org");
    started=true;appLog.printf("[TIME] NTP started asynchronously; waiting for synchronization");
  }
  auto g=deviceClock.generation();
  if(g!=logged){logged=g;appLog.printf("[TIME] NTP synchronized generation=%lu",(unsigned long)g);}
}
static String clockStatus(){
  char text[90];DeviceClock::format(deviceClock.now(),text,sizeof(text));
  String s(text);
  if(deviceClock.generation()){s+=" / last NTP sync ";s+=String((unsigned long)((DeviceClock::uptime()-deviceClock.lastSync())/1000));s+=" s ago";}
  return s;
}
