import json, re
pop = json.load(open('tools/_evo_pop.json', encoding='utf-8'))
s = open('qml/EvoOverlay.qml', encoding='utf-8').read()
pat = re.compile('desc: "([^"]*)", narr: "(evo[.][a-z]+[.]s\\d+)"')
def rep(m):
    nid = m.group(2)
    assert nid in pop, nid
    return 'desc: "%s", narr: "%s", pop: "%s"' % (
        m.group(1), nid, pop[nid].replace('"', '\\"'))
s2, n = pat.subn(rep, s)
print('patched stages:', n)
assert n == 64, n
open('qml/EvoOverlay.qml', 'w', encoding='utf-8').write(s2)
