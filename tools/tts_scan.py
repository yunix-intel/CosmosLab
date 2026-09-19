# -*- coding: utf-8 -*-
# 扫描 narration.json zh/zh_pop 中 TTS 不友好的符号, 输出转写清单
import json, re, collections
n = json.load(open('assets/evo/narration.json', encoding='utf-8'))
tok = collections.Counter()
locs = {}
pat = re.compile(r'[A-Za-zΑ-Ωα-ω☉₀-₉⁰-⁹⁵⁶Δχ²ΩΛσμ×÷±°′″~‐‑‒–—/=+%．]')
for k, v in n['text'].items():
    for lang in ('zh', 'zh_pop'):
        t = v[lang]
        # 找出含拉丁/希腊/上标/特殊符号的词片段
        for m in pat.finditer(t):
            s = max(0, m.start()-8); e = min(len(t), m.end()+8)
            frag = t[s:e]
            tok[m.group(0)] += 1
            locs.setdefault(m.group(0), []).append(k + '/' + lang + ': ' + frag)
print('=== 符号频次 ===')
for ch, c in tok.most_common(60):
    import unicodedata
    print(repr(ch), unicodedata.name(ch, '?'), c)
print()
print('=== 全部上下文(截断) ===')
for ch in sorted(locs):
    print('---', repr(ch))
    for l in locs[ch][:6]:
        print('   ', l)
