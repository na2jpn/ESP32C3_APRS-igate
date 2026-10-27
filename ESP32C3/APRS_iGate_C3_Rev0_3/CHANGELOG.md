# Changelog

## Rev0.3 — 2026-10-27（正本）

- 正常接続時のWi-Fi LEDを30秒周期の0〜10秒に点灯。
- 正常認証時のAPRS LEDを同周期の10〜20秒に点灯（10秒ずらす）。
- 20〜30秒は両方消灯。未接続/未認証時の点滅と受信消灯表示は維持。
- millis()による非ブロッキング制御。版表記を0.3に統一。
- 受信・復調・APRS-IS・設定・LCD処理はv0.2のまま維持。

## Rev0.2 — 2026-10-27（正本）

- 実機で基本動作を確認したRev0.1 FIX5を統合。
- Web、起動ログ、LCD、APRS-ISログインの版表記を0.2へ統一。
- 起動完了時に通常LCD画面への更新を明示要求し、バックライト時間を再設定。
- 旧FIX説明をdocs/DEVELOPMENT_HISTORY.mdへ集約。
- FIX5実機確認：STA Wi-Fi、DRA818V、ADC DMA、Web、APRS-IS verified、
  FM試験電波の復調2件・送出1件・重複除外1件。DMA overflow/read errorは0。
- 他局受信性能、長期運転、サーバー側での受理・表示と今回のLCD変更の実機確認は継続課題。

## Rev0.1 FIX5 — 2026-10-27

- Reduce AFSK diagnostic square roots from 79,200/s to 16/s while retaining all monitor fields.
- Start Wi-Fi/Web before ADC DMA; add per-call boot results and health logging.
- Bound ADC processing, use priority 1 with explicit tick sleeps, enlarge stack and DMA pool.
- Publish synchronized decoder statistics, stack watermark, read errors and DMA overflow events.
- Preserve FIX3 demodulation and RF-to-APRS-IS logic; add reference-based host regression tests.
- Hardware boot/RF/forwarding verification remains pending.

## Rev0.1

- Initial firmware for XIAO ESP32C3 + DRA818V RX-only APRS iGate PCB.
- Fixed 144.640 MHz receive configuration.
- Bell 202 AFSK1200 / AX.25 receive path.
- APRS-IS receive-only iGate using qAO.
- Wi-Fi setup AP + Web settings + mDNS.
- 1602 I2C LCD, two status LEDs, one multifunction SETUP button.
- NVS settings and long-run reconnect/reboot recovery.
