# ESP32C3 APRS iGate

**XIAO ESP32C3とDRA818Vを使用した、144.640 MHz・受信専用のAPRS iGateです。**

無線で受信したBell 202 AFSK 1200 bpsのパケットをESP32C3で復調し、Wi-Fi経由でAPRS-ISへ転送します。通常運用にPCは必要ありません。1602 LCDとWeb画面で接続・受信状況を確認できます。

秋葉原技研 / PS Ham-ware - JH1HST

![ESP32C3 APRS iGate](APRS_igate_esp32c3.png)

## はじめに

**書き込み・初期設定・運用方法は、[初期設定・運用ガイド（PDFフォルダー）](Initial_Setup_Guide_PDF/)を参照してください。** 使用するファームウェアに対応した版のPDFを開いてください。

ファームウェアの`.ino`と`.h`、同梱のOTA用バイナリは、[ESP32C3フォルダー](ESP32C3/)内にあります。現行版は**v0.6**です。基板に印字されたRev.1とは別の版番号です。

## リポジトリの構成

| 場所 | 内容 |
| --- | --- |
| [ESP32C3/](ESP32C3/) | ファームウェアのソースコード（`.ino`・`.h`）、版ごとの説明・検証記録など |
| [Initial_Setup_Guide_PDF/](Initial_Setup_Guide_PDF/) | 初期設定・運用ガイド。書き込み、Web設定、位置ビーコン、OTA、表示、トラブル対応 |
| [APRS_igate_esp32c3.png](APRS_igate_esp32c3.png) | 本機の紹介画像 |
| [diagram.pdf](diagram.pdf) | 回路資料 |
| [LICENSE.md](LICENSE.md) | 利用条件 |

## 主な機能

- **144.640 MHz固定受信**：DRA818Vを起動時に自動設定。
- **Bell 202 AFSK 1200 bps復調・AX.25解析**：ADC連続DMA、NRZI、HDLC、ビットスタッフィング除去、CRC検証。
- **APRS-IS転送**：VERIFIED認証後、受信パケットを`qAO`付きで転送。同一パケットの30秒以内の再転送を抑制。
- **自局位置ビーコン**：Webで緯度・経度を設定し、有効にすると認証後初回・以後30分間隔でAPRS-ISへ送信。Commentを付加。
- **Web OTA更新**：専用パスワードで認証し、Wi-Fi経由でアプリケーションの`.bin`を更新。
- **受信診断**：AF RMS、約5秒ピーク、ADC Min/Max、1200 / 2200 Hz検出値、CRC bad、DMA overflowなど。
- **LCD・LED表示**：LCDは15分ごとに約24秒（3画面×4秒×2周）自動点灯。ボタン短押しでは60秒点灯。
- **設定保持・自動復旧**：NVSへ設定保存。Wi-Fi / APRS-IS再接続と、長時間通信不能時の再初期化・再起動。

**RF送信、デジピーター、INET→RF転送は行いません。** 自局位置ビーコンもインターネット経由の送信です。DRA818VのPTTはハードウェアで受信状態に固定します。

## ハードウェア・開発環境

| 項目 | 構成・確認環境 |
| --- | --- |
| MCU | Seeed Studio XIAO ESP32C3 |
| RF | DRA818V / 144.640 MHz / Bell 202 AFSK 1200 bps |
| LCD | 1602A + PCF8574 I²Cバックパック、3.3 V / 5 Vレベル変換 |
| ネットワーク | 2.4 GHz Wi-Fi |
| 電源 | USB 5 V、DRA818V用約4.0 V |
| 開発環境 | Arduino IDE 2.3.10 / ESP32 Core 3.3.12 |
| 書き込み設定 | XIAO_ESP32C3、Flash 4MB、USB CDC On Boot：Enabled |
| パーティション | Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)、OTA対応 |

ESP32コアに含まれるライブラリを使用します。追加の外部Arduinoライブラリは不要です。

## 導入時の要点

1. [PDFガイド](Initial_Setup_Guide_PDF/)を確認し、[ESP32C3/](ESP32C3/)内の使用版の`.ino`をArduino IDEで開きます。同じフォルダーの`.h`も揃えてください。
2. v0.2 / v0.3からの導入はUSBで書き込みます。v0.4以降でOTA設定済みならWeb更新できます。
3. 初回設定APは`APRS-iGate-XXXX`、パスワードは`aprssetup`、設定URLは`http://192.168.4.1/`です。
4. 通常時は同じLAN内から`http://aprs-igate.local/`、または本機のIPアドレスでWeb画面を開きます。

位置表示には、**緯度・経度の入力と「Send own position to APRS-IS」のチェック**が必要です。Gridは保存のみで、ビーコンの座標には使用しません。Commentは**半角英数字・記号で40文字以内**にしてください（保存上限40バイト）。

OTAパスワード設定後は、設定保存・Web再起動・初期化にもBasic認証を使用します。ユーザー名は**`admin`**、パスワードは設定した**OTA password**です。

v0.6では、設定保存・Web再起動・初期化後に待機案内を表示し、**60秒後にトップ画面へ自動で戻ります**。IPが変わった場合は、新しいアドレスで開き直してください。

詳しい手順、SETUPボタン、LED点灯周期、OTA用ファイルの選び方、USB復旧方法は[PDFガイド](Initial_Setup_Guide_PDF/)にまとめています。

## 開発・検証状況

実機ではFIX5の段階で、IC-7100のFM試験電波によるAFSK復調・AX.25解析、重複除外、APRS-IS認証とTCP書き込みまで確認しました。現在は設置場所での受信・連続運転を評価しています。

v0.6はESP32 Core 3.3.12 / XIAO ESP32C3向けコンパイルと、PC上での復調・ADC診断・LED・位置ビーコン・OTA・LCD制御テストを実施済みです。保存後の60秒遷移はソースを確認しています。新機能の実機動作・ブラウザーでの遷移、サーバーでの受理・aprs.fi表示は別途確認する項目です。版ごとの詳細は、ソースに同梱した`CHANGELOG.md`と`TEST_NOTES.md`を参照してください。

`Gated`と`Position beacon TCP writes`はTCP書き込み成功件数で、サーバー受理や地図表示を保証する値ではありません。CRC badは雑音でも増えます。受信診断枠以外の表示は、最新状況を確認する際にページを開き直してください。

## ライセンス・関連リンク

利用条件は[PS Ham-ware / AKIHABARA-GIKEN License](LICENSE.md)に従います。個人の学習・研究・アマチュア無線用途での製作・実行・改変が許可されています。営利利用、元データの再配布、改変物の公開・配布には事前の許可が必要です。詳細はライセンス本文を確認してください。

- [PS Ham-ware](https://2jp.org/ps/)
- [秋葉原技研](https://2jp.org/js1yjy/)
- [不具合報告・改善提案](https://github.com/na2jpn/ESP32C3_APRS-igate/issues)
