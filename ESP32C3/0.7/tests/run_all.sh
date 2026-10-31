#!/bin/sh
set -eu
cd "$(dirname "$0")"
bash run_host.sh
python3 test_led_cycle.py
python3 test_lcd_cycle.py
python3 test_v07.py
python3 - <<'PY'
from pathlib import Path
import re
s=Path('/tmp/igate-ota07.html').read_text()
Path('/tmp/igate-ota07.js').write_text(re.search(r'<script>(.*?)</script>',s,re.S).group(1))
PY
node --check /tmp/igate-ota07.js
node test_ota_ui.js
