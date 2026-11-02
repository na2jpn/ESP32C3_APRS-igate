from pathlib import Path
root=Path(__file__).resolve().parent.parent
prefix='''<!doctype html><html lang='ja'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>APRS iGate OTA v1.2</title><style>body{font-family:system-ui;max-width:780px;margin:24px auto;padding:0 16px}button,input{margin:8px 0;padding:8px}progress{width:100%}pre{white-space:pre-wrap}</style><h1>{{ota.title}}</h1><p>{{ota.intro}}</p><p>{{ota.hint}}</p><input id='f' type='file' accept='.bin'><button id='go' onclick='go()'>{{ota.start}}</button><button id='cancel' onclick='cancel()' disabled>{{ota.cancel}}</button><progress id='p' max='100' value='0'></progress><pre id='s'>{{ota.wait}}</pre><p><a href='/logs'>{{ota.logs}}</a> / <a href='/'>{{ota.back}}</a></p><script>let token='''
# Token is hex-only and inserted by the server.
output='#pragma once\nstatic String otaUpdatePage(const String &token){\n  String page=R"OTA('+prefix+"')OTA\";\n  page+=token;\n  page+=R\"OTA(';\n"+(root/'ota_uploader.js').read_text()+'\n</script></html>)OTA";\n  return page;\n}\n'
(root/'ota_page.h').write_text(output)
