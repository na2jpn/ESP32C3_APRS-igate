from pathlib import Path
import json
r=Path(__file__).resolve().parent.parent
texts=json.loads((r/'ui_texts.json').read_text())
assert len(texts)==len(set(texts))
out=['#pragma once','#include <cstring>','struct UiText { const char *key; const char *marker; const char *ja; const char *en; };','static const UiText UI_TEXTS[] = {']
for k,v in texts.items():
    assert v['ja'] and v['en']
    out.append('  {'+', '.join(json.dumps(s,ensure_ascii=False) for s in (k,'{{'+k+'}}',v['ja'],v['en']))+'},')
out+=['};','static inline const char *uiText(const char *key, bool english) {','  for (const auto &t : UI_TEXTS) if (std::strcmp(t.key, key) == 0) return english ? t.en : t.ja;','  return key;','}','static inline String uiLocalize(String page, bool english) {','  for (const auto &t : UI_TEXTS) page.replace(t.marker, english ? t.en : t.ja);','  return page;','}']
(r/'ui_texts.h').write_text('\n'.join(out)+'\n')
