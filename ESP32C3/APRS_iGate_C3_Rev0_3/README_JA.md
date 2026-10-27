# APRS iGate ESP32C3 Rev0.3

**v0.3 正本（2026-10-27）**。ハードウェアは従来のRev0.1基板を使用します。実機動作を確認できたRev0.1 FIX5を基準に統合しました。
検証結果は **TEST_NOTES.md**、旧FIXの解析記録は **docs/DEVELOPMENT_HISTORY.md** を参照。

確認済み：IC-7100 FMからの試験電波をDRA818Vで受信し、AFSK/AX.25復調、重複除外、
verified状態でのAPRS-ISへの書込みまで動作。サーバー側での受理・表示は未確認です。
起動完了時に通常LCD画面への描画を明示要求し、その時点からバックライト60秒を開始します。
このLCD変更自体の実機確認と、他局の受信・長期安定性は引き続き確認します。

対象ハードウェア：

- Seeed Studio XIAO ESP32C3
- DRA818V（VHF 134–174 MHz）
- 144.640 MHz 固定受信
- DRA818V PTT はハードウェアで受信固定
- DRA818V PD はハードウェアで通常動作固定
- AF_OUT → 1 µF → 3.3 kΩ → D1/A1(GPIO3) + 47 kΩ/47 kΩ bias + 10 nF
- 1602A + PCF8574 I2C backpack（5 V）
- BSS138 ×2 による 3.3 V ↔ 5 V I2C level shift
- D2 = Wi-Fi LED
- D3 = APRS-IS / RX LED
- D4 = SDA
- D5 = SCL
- D6/TX = DRA818V RXD
- D7/RX = DRA818V TXD
- D10 = SETUP button to GND

## Rev0.3 の内容

Rev0.3 は **RX-only APRS iGate の基本動作確認版**です。

実装済み：

- DRA818V 起動時自動設定
  - 144.6400 MHz
  - SQL=0（monitor mode）
  - CTCSS/CDCSSなし
  - Volume=5
  - audio filter off commandを送信（拒否されても継続）
- ESP32-C3 ADC continuous/DMA でAFを 26.4 ksample/s 取得
- 自前 Bell 202 AFSK 1200/2200 Hz demodulator
- NRZI → HDLC → bit unstuff → CRC-16/X.25
- AX.25 UI / PID F0 decode
- TNC2形式へ変換
- APRS-IS RF→INET gate
  - RX-onlyのため `qAO` を使用
- 30秒重複除外
- Wi-Fi設定Web画面
- 初回/救済用 Setup AP
- `aprs-igate.local` mDNS
- 1602 LCD状態表示
- LED状態表示
- ボタン短押し/長押し
- NVS設定保存
- Wi-Fi/APRS-IS再接続
- 長時間ネットワーク不通時の自己復旧/再起動
- Flashへの受信ログ蓄積なし

## 必要な開発環境

Arduino IDE 2.x を想定しています。

- Board package: **esp32 by Espressif Systems 3.x**
- Board: **XIAO_ESP32C3**
- USB CDC On Boot: **Enabled 推奨**

Rev0.3 は追加Arduinoライブラリを必要としません。WiFi / WebServer / DNSServer / Preferences / Wire / ESP-IDF ADC driver はESP32 Arduino core側を使用します。

> 注意: `ADC continuous` APIを使うため、古いArduino-ESP32 2.xではなく **3.x** を推奨します。

## 初回起動

設定が保存されていない場合、ESP32はSetup APを開始します。

- SSID: `APRS-iGate-XXXX`
- Password: `aprssetup`
- URL: `http://192.168.4.1/`

設定項目：

- Wi-Fi SSID
- Wi-Fi Password
- iGate Callsign-SSID
- APRS-IS Passcode
- APRS-IS Server
- Port
- Grid（Rev0.3では保存のみ）
- Comment（Rev0.3では保存のみ）

保存すると再起動します。

通常運用時、同一LANから以下でも設定画面へ入れます。

`http://aprs-igate.local/`

## ボタン

操作はボタンを**離した時点**で判定します。

- 短押し: LCDバックライトを60秒点灯
- 5秒以上15秒未満: Setup APを起動
- 15秒以上: NVS設定を消去して再起動

この方式なので、15秒初期化を狙っている途中で5秒設定モードが発動することはありません。

## LED

- Wi-Fi LED
  - 接続済み: 点灯
  - 未接続/接続中: 点滅
- APRS LED
  - APRS-IS verified: 点灯
  - 未接続/unverified: 点滅
  - RFパケット受信時: 短時間消灯して受信を表示

## LCD

PCF8574 backpackのアドレスは、まず `0x27`、次に `0x3F` を試し、その後I2Cバスをscanします。

通常は60秒後にバックライトだけOFFにします。LCD/PCF8574自体は通電したままです。

## APRS-IS

Default:

- Server: `rotate.aprs2.net`
- Port: `14580`

APRS-IS loginは以下の形式です。

`user CALL-SSID pass PASSCODE vers APRS-iGate-C3 0.3`

サーバーから `verified` が返るまでRFパケットは送信しません。

本機はRFへ送信できないRX-only iGateなので、RF→APRS-ISでは `qAO,IGATECALL` を付加します。

## Rev0.3 の保守的なゲート条件

以下はAPRS-ISへ送りません。

- AX.25 UI / PID F0 以外
- pathに `TCPIP` / `TCPXX` / `NOGATE` / `RFONLY` を含むもの
- generic query `?`
- third-party packet `}`

third-party packetは本来条件によりunwrap可能ですが、Rev0.3ではループ/誤変換防止のため全てdropします。後続版で実装候補です。

## 長期運用

- Wi-Fi切断時: 15秒周期で再接続
- 初回接続に60秒失敗: Setup APを自動起動
- 長時間通信不能: Wi-Fi stackを再初期化
- APRS-ISへ1時間到達不能: ESP32を再起動
- APRS-ISがTCP接続済みだがpasscodeがunverifiedの場合は、再起動理由にしません
- 受信ログ/packet countをNVSへ逐次書き込みません

## AFSK decoder

Rev0.3のAFSK decoderはこのプロジェクト用の独自実装です。

- Fs = 26,400 Hz
- Mark = 1,200 Hz
- Space = 2,200 Hz
- 1,200 baud
- 22 samples/bit
- complex correlation + DPLL
- NRZI
- HDLC flag detection
- bit unstuff
- CRC-16/X.25

PC上の合成波形および提供された試験WAVでCRC通過を確認し、FIX5実機でも復調2件・APRS-IS書込み1件・重複除外1件を確認しました。任意の他局・弱信号・各種歪みに対する受信性能を保証するものではありません。

受信率が低い場合、最初に疑う順序：

1. `AF CK` のDC中心が約1.65 Vか
2. AFSK受信時のADC振幅が小さすぎ/大きすぎないか
3. DRA818Vのfilter commandの挙動
4. demodulatorのenergy threshold / DPLL係数

## DRA818V

UART: 9600 8N1

起動時に主に以下を送ります。

- `AT+DMOCONNECT`
- `AT+DMOSETGROUP=0,144.6400,144.6400,0000,0,0000`
- `AT+DMOSETVOLUME=5`
- `AT+SETFILTER=1,1,1`

DRA818Vは電源OFFで設定を失うため、毎起動時に設定します。

## Rev0.3でまだ行わないこと

- INET→RF gateway
- Digipeater
- Tracker / beacon送信
- RF送信
- received packetのFlash保存
- third-party packet unwrap
- OTA update

PTTはハードウェアで受信固定しているため、ファームウェアの不具合で送信する構造にはしていません。

## 実機試験でまず見るもの

1. 3.9V CK: DRA818V電源
2. 3.3V CK: XIAOロジック電源
3. 5V CK: VUSB
4. AF CK: 約1.65V中心 + 受信AF
5. Web: DRA / ADC / Wi-Fi / APRS-IS status
6. Serial Monitor: USB CDC有効時のみdebugを表示



## v0.3 LED表示

正常接続中は起動後のmillis()を基準とする30秒周期です。

| 周期内の時間 | Wi-Fi LED | APRS LED |
| --- | --- | --- |
| 0秒以上10秒未満 | 点灯 | 消灯 |
| 10秒以上20秒未満 | 消灯 | 点灯 |
| 20秒以上30秒未満 | 消灯 | 消灯 |

Wi-Fi未接続時は400 msごと、APRS未認証時は700 msごとの切替点滅を維持。
各LEDはそれぞれの接続状態に従います。両方の正常時に上表の表示になります。
受信時のAPRS LEDの短い消灯表示も維持します（もともと消灯中なら見た目は変わりません）。
delay()を追加せず、受信・転送・Web・LCDはそのまま動作します。
約49.7日でmillis()が周回した際は周期の位相がリセットされます。
