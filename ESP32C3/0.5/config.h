#pragma once

#include <Arduino.h>

// -----------------------------------------------------------------------------
// APRS iGate firmware v0.5 - fixed hardware definition for Rev0.1 PCB
// Target: Seeed Studio XIAO ESP32C3 + DRA818V (RX only)
// -----------------------------------------------------------------------------

#define FW_NAME       "APRS-iGate-C3"
#define FW_VERSION    "0.5"

// FIX3: ADC continuous DMA re-enabled for live AFSK receive.
// To isolate an ADC-related crash, temporarily set this to 1.
#define APRS_DIAGNOSTIC_SKIP_ADC 0

// XIAO ESP32C3 pin mapping (Seeed pin names -> GPIO)
static constexpr uint8_t PIN_AF_ADC      = 3;   // D1 / A1 / GPIO3 / ADC1_CH3
static constexpr uint8_t PIN_LED_WIFI    = 4;   // D2 / GPIO4
static constexpr uint8_t PIN_LED_APRS    = 5;   // D3 / GPIO5
static constexpr uint8_t PIN_I2C_SDA     = 6;   // D4 / GPIO6
static constexpr uint8_t PIN_I2C_SCL     = 7;   // D5 / GPIO7
static constexpr uint8_t PIN_DRA_TX      = 21;  // D6 / GPIO21 -> DRA818V RXD
static constexpr uint8_t PIN_DRA_RX      = 20;  // D7 / GPIO20 <- DRA818V TXD
static constexpr uint8_t PIN_SETUP_SW    = 10;  // D10 / GPIO10, switch to GND

// RF parameters are intentionally fixed for the first RX-only build.
static constexpr const char *DRA_RX_FREQ = "144.6400";
static constexpr const char *DRA_TX_FREQ = "144.6400"; // PTT is hard-wired RX, never keyed by firmware
static constexpr uint8_t DRA_SQL         = 0;           // Monitor mode: avoid clipping packet edges
static constexpr uint8_t DRA_VOLUME      = 5;

// APRS-IS defaults
static constexpr const char *DEFAULT_APRS_SERVER = "rotate.aprs2.net";
static constexpr uint16_t DEFAULT_APRS_PORT = 14580;

// Wi-Fi setup AP
static constexpr const char *SETUP_AP_PASSWORD = "aprssetup";
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 60000UL;
static constexpr uint32_t WIFI_RETRY_MS           = 15000UL;
static constexpr uint32_t APRS_RETRY_MS           = 30000UL;
static constexpr uint32_t WIFI_STACK_RESET_MS     = 30UL * 60UL * 1000UL;
static constexpr uint32_t INTERNET_REBOOT_MS      = 60UL * 60UL * 1000UL;

// Button actions are decided on release.
static constexpr uint32_t BUTTON_SETUP_MS = 5000UL;
static constexpr uint32_t BUTTON_RESET_MS = 15000UL;
static constexpr uint32_t LCD_ON_MS        = 60000UL;
static constexpr uint32_t LCD_AUTO_INTERVAL_MS = 15UL * 60UL * 1000UL;
static constexpr uint32_t LCD_PAGE_MS = 4000UL;
static constexpr uint8_t LCD_PAGE_COUNT = 3;
static constexpr uint32_t LCD_AUTO_CYCLES = 2;
static constexpr uint32_t LCD_AUTO_ON_MS = LCD_PAGE_MS * LCD_PAGE_COUNT * LCD_AUTO_CYCLES;

// Bell 202 receive path
static constexpr uint32_t AFSK_SAMPLE_RATE = 26400; // 22 samples/bit; 1200 Hz=22 samples/cycle, 2200 Hz=12
static constexpr uint16_t AFSK_BAUD        = 1200;

// Normal connected/verified LED duty cycle (millis-based; never blocks RX).
static constexpr uint32_t LED_STATUS_PERIOD_MS = 30000UL;
static constexpr uint32_t LED_STATUS_ON_MS = 10000UL;
static constexpr uint32_t LED_APRS_OFFSET_MS = 10000UL;

// Set to 1 only while debugging with Arduino Serial Monitor over native USB CDC.
#ifndef ENABLE_USB_DEBUG
#define ENABLE_USB_DEBUG 1
#endif

#if ENABLE_USB_DEBUG && defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  #define DBG_BEGIN() Serial.begin(115200)
  #define DBG_PRINT(x) Serial.print(x)
  #define DBG_PRINTLN(x) Serial.println(x)
  #define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DBG_BEGIN() do {} while (0)
  #define DBG_PRINT(x) do {} while (0)
  #define DBG_PRINTLN(x) do {} while (0)
  #define DBG_PRINTF(...) do {} while (0)
#endif
