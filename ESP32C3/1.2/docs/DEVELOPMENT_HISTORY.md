# Rev0.1 FIX1–FIX5 開発記録

この文書は旧版の履歴です。現行の仕様・検証状況はREADME_JA.md / TEST_NOTES.mdを参照。

---

## FIX1_README_JA.md

# Rev0.1 FIX1-DIAG（原因切り分け用）

- DRA818V初期化後に発生する `xQueueSemaphoreTake` アサーションの原因調査用。
- `config.h` の `APRS_DIAGNOSTIC_SKIP_ADC 1` によりADC DMAのみ一時停止。
- **AFSK受信・APRS転送はこの状態では動作しません。** Wi-Fi AP/LCD/Web UIの起動確認が目的です。
- 起動処理に `[BOOT]` ログを追加。Arduino ESP32 core 3.3.12で報告された `AprsIsClient::connected() const` コンパイルエラーも修正済み。

## 試験
1. XIAO単体をUSB-C接続、USB CDC On Bootを有効にし書き込み。
2. シリアルモニタ115200bpsでリセット。
3. `[BOOT] setup() finished` と `Setup AP:` が出るか確認。
4. Wi-Fiに `APRS-iGate-XXXX` が出るか確認。
5. 成功したらログを共有。次にADC DMAを段階的に修正する。

**注意:** これは切り分け版であり、完成ファームではありません。

---

## FIX2_README_JA.md

# FIX2 診断版

- Webルート登録時の `web.begin()` を削除し、Wi-Fi初期化後に移動。
- 起動ログを追加。
- FIX1同様ADC/DMAは無効（APRS受信不可）。
- まずシリアルモニタ115200bpsで `[BOOT] Web server ready` と `[BOOT] setup() finished` を確認。
- 本修正は実機・Arduino Core 3.3.12でのコンパイル未検証。

---

## FIX3_README_JA.md

# APRS iGate C3 Rev0.1 FIX3

FIX2のWi-Fi/Web初期化順序修正を維持し、ADC連続DMAサンプリングを再有効化しました。

- `config.h`: `APRS_DIAGNOSTIC_SKIP_ADC 0`
- `audio_adc.h`: ADCハンドル生成、設定、開始、タスク生成に診断ログと失敗時の後片付けを追加
- DRA818V、Wi-Fi、APRS-IS、LCD、LED、SETUP操作はFIX2から維持

## テスト

Arduino ESP32 Core 3.3.12 / XIAO ESP32C3を選択してコンパイル、書き込み。
115200bpsのログで `[ADC] ... OK`、`[BOOT] ADC DMA result=1`、`[BOOT] setup() finished` を確認。

**注意:** コンパイルと実機動作は未検証です。実機でクラッシュした場合、ログの最後の `[ADC]` / `[BOOT]` 行を保存してください。APRSパケットの復調精度とAPRS-IS転送は別途実測が必要です。

## 切り分け

ADC起因で再起動する場合、`config.h` の `APRS_DIAGNOSTIC_SKIP_ADC` を `1` に戻すとFIX2相当の受信無効モードになります。

---

## FIX4_README_JA.md

# APRS iGate ESP32C3 Rev0.1 FIX4

FIX3のADC/DMA受信を維持し、Web診断表示を追加しました。

- `/status` JSONに `af_rms`, `af_peak5s`, `adc_min`, `adc_max`, `mark1200`, `space2200`, `samples` を追加。
- Web画面に1秒ごとの自動更新モニターを追加（画面全体は再読み込みしません）。
- 5秒ピークホールドは250ms×20窓のリングバッファで管理。
- ADC Min/Maxは直近250msの生ADC値。1200/2200Hzは復調器の複素相関振幅であり校正済み電圧ではありません。
- 以前のAF RMSは平均二乗値でした。FIX4では平方根を取った実効値へ修正したため、FIX3以前の数値とは直接比較できません。
- 受信タスクとWeb側の診断値受け渡しはクリティカルセクションで保護。

Arduino IDE: XIAO ESP32C3 / ESP32 Core 3.3.12を使用。FIX3からの設定は通常引き継がれます。

**注意**: 実機でのコンパイル・動作は未検証です。受信/送信試験は法令とバンドプランに従ってください。

---

## FIX5_README_JA.md

# Rev0.1 FIX5 — 起動停止対策とAFSK診断の軽量化

作成日：2026-10-27（日本時間）。FIX4フルソースを基に修正。

## 差分解析で確定したこと

FIX3→FIX4で変更された実装ファイルは config.h、afsk1200.h、audio_adc.h、.ino の4本。
DRA818V、LCD、設定/NVS、AX.25パーサー、APRS-ISクライアントは変更されていない。
Wi-Fi/Web初期化順序、ADC DMA設定、受信タスクの優先度3/スタック4096バイトも同じ。

重要な差分は audio_adc.h の observe()。
FIX4は26,400サンプル/秒の各サンプルに対して rms()/markLevel()/spaceLevel() を呼び、
それぞれsqrtf()を実行する。つまり毎秒79,200回の平方根計算を追加している。
ESP32-C3はFPUを持たず、浮動小数点演算はソフトウェアで処理される。

Arduino ESP32 Core 3.3.12 の cores/esp32/main.cpp では、setup()/loop()を実行する
loopTask が優先度1で作られている。受信タスクは優先度3で、正常読出し時に明示的な待機がない。
DMAバッファに常時データがあると adc_continuous_read() は待機せず戻り続けるため、
追加計算で処理が追いつかなくなると低優先度の setup()/loop()/IDLE が実行されなくなる構造である。
1000 msというreadタイムアウトは「毎回待機する時間」ではない。

一方、Web/LCDから復調器のフィールドを直接読む共有アクセスもあった。
FIX4のモニター用排他区間は短い代入だけであり、コード上、そこに長い待機や再帰ロックは見つからない。
スタック不足を直接示す証拠もないが、復調成功時は512バイトのフレーム配列とキュー用配列がネストする。
FIX5では余裕を増やし、実測の最小残量を公開する。

## 現時点の原因判定の限界

**最有力は診断計算増加による受信タスクのCPU占有。実機での停止原因としては未確定。**
手元に対象実機がなく、停止時のタスク状態・バックトレース・処理時間を採取できていないため、
「Wi-Fiドライバーのデッドロック」「ADCとWi-Fiのハードウェア競合」と断定していない。
ADCはGPIO3 / ADC1_CH3を使用しており、ADC2/Wi-Fiの一般論だけで説明することもできない。

既存の「Station WiFi mode begin」は WiFi.mode() の直前にしかなく、その後の
setHostname()/WiFi.begin()との間にログがなかった。従って、旧ログだけでは
この3 APIのどこで停止したかも区別できなかった。
Core 3.3.12 の WiFiGeneric.cpp では mode() 内にNetwork/無線ドライバーの初期化が含まれる。

## FIX5の修正

- 各サンプルでは平方根を取らず、RMSの二乗値の最大を保持する。
- RMS、ピーク、1200/2200 Hz検出値の平方根は250 msごとの公開時にだけ計算する。
  診断由来のsqrtfは毎秒16回。RMS/ピークの意味はFIX4と同じ。
- ADC Min/Maxは全サンプルを評価。Webは従来通り1秒更新。
- 5秒ピークは20×250 msのサンプル窓の最大値。
  欠落がない場合の約5秒であり、DMA欠落/停止時の厳密な壁時計5秒ではない。
- 復調本体の信号判定・NRZI・HDLC・CRCロジックはFIX3から変更しない。
- 受信タスク優先度は1。最大4回のDMA読出しごとにvTaskDelay(1)で1 tick待機する。
  単なるyield()と違い、低優先度タスクにも実行機会を作る。
- タスクスタックを4096→6144バイト、DMAプールを2048→8192バイトへ拡張。
  C3の4バイト/結果、26.4 kspsでは約77 ms分。バッファ拡大だけで過負荷を解決する設計ではない。
- 初回のWi-Fiインターフェース初期化→Web開始→ADC DMA開始に順序変更。
  AP/STAともADCを恒久的に無効化しない。APRS-ISへの接続は従来通りloop()から行う。
- 開始後はADCの読出しを受信タスク1本に限定する。Web/LCDは固定サイズのスナップショットを読む。
  クリティカル区間内に平方根・文字列生成・ネットワーク処理を入れない。
- CRC/フレーム統計のWeb/LCD参照もスナップショット経由に変更する。
- DMA overflowをISRの整数カウンターで記録。ISRでは浮動小数点・ログ・確保をしない。
- overflow変化/読出し異常/タイムアウト後は復調状態をリセットし、不連続なHDLC断片を結合しない。
- 各初期化段階の戻り値、ヒープ、RTOS tick、setup優先度をログに追加。
- ADC側のログもUSBデバッグ設定に従わせ、CDC無効時のUART0への出力を避ける。

## 新しい診断値

Webの /status とシリアルの [HEALTH] を参照。

| 値 | 意味 |
| --- | --- |
| samples | 正常なADC1_CH3サンプルの累計。通常は約26,400/秒で増える |
| adc_overflows | DMAプールあふれイベントの累計。持続的に増える場合は処理能力不足などを疑う |
| adc_read_errors | タイムアウトを除くread APIエラー回数 |
| adc_timeouts | 1秒のreadタイムアウト回数 |
| adc_stack_free_bytes | ADCタスクのスタック最小残量。ESP-IDFでは単位バイト |
| adc_max_block_us | 512バイト1読出し分の処理経過時間の最大値 |
| adc_processing_us | readの待機時間を除くブロック処理経過時間の累計（32ビット） |
| adc_updated_ms | 最後に音声の250 ms窓を公開したmillis() |
| frames_good | 復調器でCRCを通過したフレームの累計 |
| decoded | AX.25パーサーで受理されたパケット数（従来通り） |
| frame_errors | HDLC長さ・ビットスタッフィングなどのエラー |

経過時間には割込みや他タスクの実行時間も含むため、processing_usは厳密なCPU使用率ではない。
32ビット累計は周回する。差分を符号なしで取り、短い期間の傾向を見る。
[HEALTH]はloop()が進んでいる間に約10秒ごとに出力する。ネットワーク接続待機などで遅れる場合がある。
ADC停止中もread異常カウンターは更新するが、音声値は最終値のままなのでsamples/adc_updated_msも確認する。

## 実機確認手順

1. 旧フォルダーへの部分上書きではなく、このZIPを別フォルダーへ展開する。
   同名の .ino を開き、既存と同じSeeed Studio XIAO ESP32C3 / ESP32 Core 3.3.12を使用する。
   USB CDC On BootはEnabled。通常試験ではFlash/NVSを全消去しない。
2. シリアルモニターを開いて再起動し、起動先頭からログを保存する。
   新しい順序はWiFi mode result→hostname result→WiFi.begin result→Web server ready→ADC→setup() finished。
3. 保存済み設定でSTA接続、Web表示、APRS-IS verifiedを確認する。
   AF RMS、Min/Max、トーン値、samplesが更新され、[HEALTH]が継続することを確認する。
4. 設定ボタンによるAP起動も確認する。未設定起動は、必要なら別の実機/設定バックアップ後に確認する。
5. 144.640 MHzの既知のAPRS送信を受信し、frames_good→Decoded→RFログ→ISログ/Gatedを確認する。
   実験用の合成フレームはPC内のみで使い、APRS-ISへ送らない。
6. 少なくとも30分、Webを開いた状態で運転し、DMA overflow、read error、スタック残量、Wi-Fi/APRS接続を記録する。
   ADC波形にクリップがないかMin/Maxも確認する。波形だけで電圧へ換算しない。
7. 停止が残る場合は最後の[BOOT]とCore Debug Levelを上げた全ログを保存する。
   FIX5でADC開始前に同じ停止が起きれば、受信タスク負荷だけでは説明できず、Wi-Fi初期化側を再調査する。

受信・転送の実機確認は未完了。ハードウェア変更は要求しない。
