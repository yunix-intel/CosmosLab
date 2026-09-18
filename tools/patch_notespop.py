import json

notes = json.load(open('tools/_notes_pop.json', encoding='utf-8'))
s = open('src/sceneitem.cpp', encoding='utf-8').read()

def esc(t):
    out = []
    for c in t:
        o = ord(c)
        if o > 127:
            out.append('\\u%04x' % o)
        else:
            out.append(c)
    return ''.join(out)

def func(name, items):
    lines = ['QVariantList SolarScene::%s() const' % name, '{',
             '    QVariantList out;']
    for t in items:
        lines.append('    out.append(QString::fromUtf8("%s"));' % esc(t))
    lines += ['    return out;', '}', '']
    return '\n'.join(lines)

block = ('// ---------------------------------------------------------------------------\n'
         '//  通俗版教学要点 (选项一, 与专业版一一对应, QML 按 proMode 选择)\n'
         '// ---------------------------------------------------------------------------\n'
         + func('cosmosNotesPop', notes['cosmos'])
         + func('galaxyNotesPop', notes['galaxy']))

anchor = 'QVariantList SolarScene::cosmosStructures() const'
assert anchor in s
assert 'cosmosNotesPop' not in s
s = s.replace(anchor, block + anchor, 1)
open('src/sceneitem.cpp', 'w', encoding='utf-8').write(s)
print('inserted cosmosPop', len(notes['cosmos']), 'galaxyPop', len(notes['galaxy']))
