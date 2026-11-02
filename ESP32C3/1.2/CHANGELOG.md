# v1.2 — 2026-11-02

- Eight validated receive presets: 144.640, 144.660 (JA/default), 144.390 (US), 144.800 (EU),
  145.175 (AU), 144.575 (NZ), 144.930 (AR), 145.570 (BR). Saved in NVS and applied after reboot.
- Japanese/English management, reboot response, audio monitor, RX diagnostics, logs and OTA UI;
  Japanese by default. URL language is retained by navigation/forms/return links. User data and raw API/log formats remain intact.
- Added tests for all eight real UART parameter selections, settings migration and both language renderings;
  OTA retry/resume and scope controls exercised in both languages. English README and updated Japanese setup PDF.
- RF sensitivity, AF gain and AFSK1200 decoding unchanged. No 9600bps decoder or RF transmit feature.

# v1.1 - 2026-11-02

- 管理画面に受信周波数のプルダウンを追加。144.660MHz（初期値）/144.640MHzを選択可能。
- NVSへ選択値を保存し、再起動後のDRA設定・初期化再試行・Web RF表示・LCD・起動ログに反映。
- 旧版や未設定からは144.660MHz。既存のWi-Fi/APRS/位置/OTA設定を保持。
- 保存値の不正時は144.660MHzへフォールバック。Webの不正入力は保存前に拒否。
- 旧クライアントなど周波数項目がない保存では現在の選択を保持。
- /statusにrf_mhz、afsk_baudを追加。どちらも1200bps AFSKで、9600bps復調には非対応と明記。
- v1.0の復調・感度・音声ゲイン・ADC・OTA・診断機能は継承。

# v1.0 - 2026-11-02

- 国内1200bps AFSK運用に合わせ、固定受信周波数を144.640MHzから144.660MHzへ修正。
- DRA818V設定値、管理画面の周波数・1200bps表記、LCD周波数を整合。
- 版番号をWeb/LCD/起動ログ/音声モニター/診断CSVで1.0に統一。
- 起動ログにRF周波数・1200bps Bell202 AFSKとDRA設定要求を追加。
- v0.12の音声モニター、記録、ADC復旧、OTA、NTP、ビーコン、LED/LCDを継承。
  AFゲイン・RF感度・復調しきい値は追加変更しない。
- v0.12実機で試験パケットの受信・復調・転送・aprs.fi位置表示を確認。
  v1.0の144.660MHz実機受信・通常局の受信率・長期運転は継続確認。
- 初期設定PDF、README、OTAファイル名とスケッチフォルダーをv1.0向けに整理。

# v0.12 — 2026-11-02

- 音量計を本体での連続集計に変更。約200ms AC RMSを緑バー、
  直近約5秒の最大40ms AC RMSを赤い線で表示。
- 画面更新の間の短い入力もピークへ反映。複数ブラウザーでも非破壊読出し。
- 波形は従来の40ms表示・倍率・一時停止を維持。音量計と波形の測定窓を明示。
- 欠落/復旧/OTA/閲覧終了で履歴をリセットし、無効な音量を表示しない。
- 固定RAM3232B、v0.11から1048B増。RF/AFゲイン・復調しきい値は変更なし。

# v0.11 — 2026-11-02

- 管理画面から音声入力モニター /audio を開けるように追加。
- 約40msのADC波形（4サンプルの最小・最大264組）、AC RMSメーター、
  取得した値の5秒ピーク、波形ピーク、DC平均、Min/Max、端付近の表示。
- 固定/自動倍率表示、一時停止・保持、非表示時の要求停止、通信断時の再試行。
- 3秒リース方式で閲覧中だけ計算。固定RAM2184B、DMAの追加起動なし。
- ADCの異常・欠落・OTAを考慮し、古い/無効な波形を正常な無音と扱わない。
- v0.10の復旧/記録/OTA、RFゲイン・しきい値は維持。
- 実機の音声経路・RF復調・追加負荷の検証は未実施。

# v0.10 — 2026-11-01

- ADC停止3秒検知、所有タスクでstop/flush/start、3回上限、実データ確認後に復旧成功記録。
- Web手動ADC復旧、認証・トークン・OTA排他。鮮度に基づくADC状態表示。
- Timeout増分ログ、取得/破棄/復旧/取得速度メトリクス、CSV adc_valid列。
- DMAプール16KiB、優先度2＋処理上限/休止、欠落後の同期リセットと有限破棄。
- 毎サンプルの判別式除算を同値の乗算比較へ変更。RF/復調しきい値は据え置き。
- Wi-Fi継続断60秒のAP移行タイマー修正、AP/スタック再初期化後のADC復旧予約。
- ESP-IDF 5.5.5のC3 EOF対策済み公開ドライバーAPIを利用。レジスター直接操作なし。
- 実機の根本原因・RF復調率・長時間耐久性は未確定。

# v0.9 — 2026-11-01

- 不安定な通信を前提としたWeb OTAの再接続・受信位置照合・自動再試行を追加。
- 1KB送信、操作ごとの約8分復帰待ち、本体の無通信待機10分。進捗がある間は総所要時間を制限しない。
- 同一ブラウザーで同じbinを選び直したときの途中再開を追加（セッション存続中）。
- 開始要求・直前ブロック・検証要求の重複を安全に処理。二重Flash書き込みを防止。
- ファイル全体SHA-256をブラウザー/本体で照合。起動・セッション・ファイル不一致時は継続停止。
- 検証後再起動のRTC記録を拡張し、検証応答が失われた場合の結果照合に対応。
- OTA中のWi-Fi再接続を追加。APRS再接続やWi-Fiモードの再初期化はOTA中休止。
- v0.8の受信診断/NTP/小幅しきい値調整を継承。感度の追加変更なし。
- コンパイル・PCテスト済み。弱電界Wi-Fiでの実転送・停電/スリープ復帰は未検証。
- 初回の旧版→v0.9導入には旧版の送信処理が使われる。安定した通信かUSBで一度導入が必要。

# v0.8 — 2026-11-01

- 受信変化前後の約20ms診断記録（最大4件、固定RAM約13KB）とCSV出力を追加。
- RMS上下・トーン合計/比率・ADC端付近・復調/ADCエラーの変化を記録。
- 記録停止/再開/消去/手動記録を追加。通常4KBログと分離。
- 非同期NTP同期、JST日時、64bit経過ms、同期状態/最終同期経過秒を追加。
- 試験として弱入力の2電力ゲートを20%下げた。RFゲイン/CRC/ビット同期は維持。
- v0.7実機でOTA全量受信・検証・再起動・再接続に成功した経路を継承。
- v0.8はPCテスト/コンパイル済み。実機の受信改善・診断性能はこれから評価。

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
