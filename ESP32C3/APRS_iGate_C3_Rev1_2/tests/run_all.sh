#!/bin/sh
set -eu
cd "$(dirname "$0")"
python3 build_ui_texts.py
python3 build_ota_page.py
python3 build_audio_scope_page.py
bash run_host.sh
python3 test_led_cycle.py
python3 test_lcd_cycle.py
bin08=$(mktemp)
trap 'rm -f "$bin08"' EXIT
c++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -DCONFIG_IDF_TARGET_ESP32C3 -I stubs -I .. test_v08.cpp -o "$bin08"
"$bin08"
python3 test_v08_http.py
python3 - <<'PY'
from pathlib import Path
import re
s=Path('/tmp/igate-ota09.html').read_text()
Path('/tmp/igate-ota09.js').write_text(re.search(r'<script>(.*?)</script>',s,re.S).group(1))
PY
node --check /tmp/igate-ota09.js
node test_ota_ui.js
python3 test_ota_wifi.py

bin10=$(mktemp)
trap 'rm -f "$bin08" "$bin10"' EXIT
c++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -DCONFIG_IDF_TARGET_ESP32C3 -I stubs -I .. test_recovery.cpp -o "$bin10"
"$bin10"

python3 test_v010_services.py

bin11=$(mktemp)
trap 'rm -f "$bin08" "$bin10" "$bin11"' EXIT
c++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -DCONFIG_IDF_TARGET_ESP32C3 -I stubs -I .. test_scope.cpp -o "$bin11"
"$bin11"
python3 test_scope_http.py
node --check ../audio_scope_ui.js
node test_scope_ui.js

python3 test_rf_settings.py

UI_TEST_LANG=en node test_ota_ui.js
UI_TEST_LANG=en node test_scope_ui.js
python3 test_management_i18n.py
