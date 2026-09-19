# -*- coding: utf-8 -*-
# 把 _evo_pop.json 全量同步到 EvoOverlay.qml 各 stage 的 pop 字段
import json, re
pop = json.load(open('tools/_evo_pop.json', encoding='utf-8'))
p = 'qml/EvoOverlay.qml'
s = open(p, encoding='utf-8').read()
n = 0
def rep(m):
    global n
    nid = m.group(1)
    assert nid in pop, nid
    n += 1
    return 'narr: "%s", pop: "%s"' % (nid, pop[nid].replace('"', '\\"'))
s2 = re.sub(r'narr: "(evo\.[a-z]+\.s\d+)", pop: "[^"]*"', rep, s)
print('synced stages:', n)
assert n == 64, n
open(p, 'w', encoding='utf-8').write(s2)
