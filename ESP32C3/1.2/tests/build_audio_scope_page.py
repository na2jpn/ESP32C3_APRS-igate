from pathlib import Path
r=Path(__file__).resolve().parent.parent
s=(r/'audio_scope_template.html').read_text()+(r/'audio_scope_ui.js').read_text()+'\n</script></body></html>\n'
(r/'audio_scope_page.h').write_text('#pragma once\nstatic const char AUDIO_SCOPE_PAGE[] PROGMEM=R"SCOPE('+s+')SCOPE";\n')
