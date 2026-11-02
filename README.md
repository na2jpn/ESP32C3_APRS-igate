# APRS iGate ESP32C3 v1.2

秋葉原技研 / PS Ham-ware。XIAO ESP32C3 + DRA818Vによる受信専用APRS iGate。
**8周波数プリセット / 初期値144.660MHz / Bell 202 AFSK 1200bps / AX.25 / APRS-IS qAO**。
管理画面は日本語・英語に対応し、日本語が初期表示です。

## 初期設定・操作説明

[初期設定・運用ガイドPDF（日本語）](Initial_Setup_Guide_PDF/APRS_iGate_C3_v1_2_Initial_Setup_Guide_JA.pdf)を参照してください。
詳しい診断仕様・OTA途中再開の条件は[ファームREADME（日本語）](ESP32C3/APRS_iGate_C3_Rev1_2/README_JA.md)、
海外向けの概要は[English guide](ESP32C3/APRS_iGate_C3_Rev1_2/README_EN.md)にあります。

## v1.2の変更

管理画面の「受信周波数」で次の8種類を選択できます。
「保存して再起動」でNVSへ保存し、再起動後のDRA818V、Web、LCD、起動ログへ反映します。
未設定時は144.660MHz。v1.1で保存済みの144.640/144.660MHzや他の設定は引き継ぎます。

| MHz | 表示 / Region |
| --- | --- |
| 144.640 | — |
| 144.660 | JA・初期値 / default |
| 144.390 | US |
| 144.800 | EU |
| 145.175 | AU |
| 144.575 | NZ |
| 144.930 | AR |
| 145.570 | BR |

**全プリセットで1200bps AFSKです。** 地域表示は代表例です。日本の144.640MHzは一般に9600bps用ですが、
本機は9600bps復調に対応しません。UART制御の9600bpsとは別です。受信感度・AFゲインの変更はありません。

各ページ上部の「日本語 / English」で、設定、音声入力モニター、受信変化の記録、本体ログ、OTA画面を切り替えます。
言語はURLの `?lang=en` / `?lang=ja` で指定し、ページ間の移動・フォーム・再起動後の戻り先に引き継ぎます。
言語指定のないアクセスは日本語。設定保存や再起動は不要です。生ログ、CSV列名、APIキーやユーザー入力は翻訳しません。
編集中の未保存入力は言語切り替え時に保持しないので、先に保存してください。OTA実行中の言語切り替えは抑止します。

## フォルダー構成

| 場所 | 内容 |
| --- | --- |
| ESP32C3/APRS_iGate_C3_Rev1_2/ | Arduinoスケッチ、全ヘッダー、日英README、テスト、更新履歴 |
| ESP32C3/APRS_iGate_C3_Rev1_2/firmware/ | OTA用アプリ単体.bin |
| Initial_Setup_Guide_PDF/ | 初期設定・運用ガイドv1.2 |
| LICENSE.md | ライセンス |

既存v0.9以降からはOTAで `firmware/APRS_iGate_C3_v1.2_OTA.bin` を指定します。
ZIPはアップロードしません。保存済みWi-Fi・コールサイン・ビーコン・OTA設定を引き継ぎます。
更新後はRev1.2、選択したRF周波数、1200bps、APRS-IS VERIFIED、Samples増加を確認してください。

v0.12実機では試験用パケットの受信・復調・APRS-IS転送・aprs.fi位置表示まで確認済み。
v1.2はESP32C3ビルドとホストテストを実施。新しい周波数選択・各地域での実RF受信・長時間安定性は実機で確認してください。

[GitHub](https://github.com/na2jpn/ESP32C3_APRS-igate) / [プロジェクトページ](https://2jp.org/js1yjy/esp32_aprs_igate/)
