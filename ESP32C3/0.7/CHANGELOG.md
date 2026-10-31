# Changelog

## Rev0.7 — 2026-10-31

- 最大4KBのRAM本体ログ。Web表示・ダウンロード・消去、OTA設定に応じた認証。
- 起動・接続・ADCエラー集計・OTA進捗・失敗理由を記録。パスワードや高頻度ログは除外。
- 直前のOTA状態をRTCに記録し、ソフト再起動後に表示（電源断保持は保証しない）。
- OTAを2KB単位の受信確認付きHTTP送信へ変更。セッション・オフセット・サイズ・イメージを検証。
- 更新中はADCを読み捨て、復調・転送を休止。中止・失敗後に再開。
- 中止、120秒タイムアウト、進捗表示、成功応答後2秒の再起動猶予、再接続確認を追加。
- 感度・通常時の復調アルゴリズムは変更なし。実機での通信断は原因未確定、再検証する。

## Rev0.6 — 2026-10-31

- 設定保存・Web再起動・Web初期化後に、60秒後のトップ画面への自動遷移を追加。
- 待機案内と手動でトップ画面を開くリンクを表示。遷移はGETで行い、POSTを再送しない。
- 実際の再起動は従来どおり約1.2秒後に開始し、60秒の待機で受信処理を止めない。
- 位置ビーコン・OTA・15分ごとのLCD約24秒点灯を継承。

## Rev0.5 — 2026-10-31

- LCDバックライトを15分ごとに自動点灯。先頭画面から3画面×4秒×2周（約24秒）表示して消灯。
- 短押し60秒・起動時・設定AP中の点灯を維持。手動点灯中の周期到来では表示と点灯期限を変更しない。
- 位置ビーコン・Web OTA・受信診断・LED動作をv0.4から継承。
- LCDのバージョン未設定時表示も0.5へ更新。

## Rev0.4 — 2026-10-31

- APRS-IS自局位置ビーコン（10進度設定、認証後初回・30分周期、TCPIP*パス、Iオーバーレイ）。
- Web OTA（専用パスワード、リクエストトークン、非稼働スロット、サイズ・イメージ検証、異常時中止）。
- OTAパスワード設定後はWeb設定保存・再起動・初期化にも認証を要求。
- 旧NVS設定を継承。新機能は明示設定して有効化。
- APRS-IS部分書き込み時は成功扱いにせず切断。RFと自局位置の送信件数を分離。
- ADC復調・診断、LCD、LEDの実装を継承。
- v0.4の新機能は実機試験待ち。

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
