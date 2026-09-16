"""用 \\uXXXX 转义重写 cosmos 三个函数，从结构上排除「中文引号误用」问题。

为什么要用 \\uXXXX 而不是直接写中文:

  直接写中文时，文本里的引号极易误用 ASCII 的 " —— 而它在 C++ 字符串
  内部会**提前结束字面量**，报出 "unable to find string literal operator"
  这类极难定位的错。改成 \\uXXXX 后源码里没有非 ASCII 字符，
  这类错误从根上不可能再发生。

构建方式:

  用 esc() 逐字符转义，再用字符串拼接组装代码行 —— **不用 % 格式化**，
  因为生成的内容里含 %1 / % 这类 C++ 占位符，会与 Python 的 % 冲突。
"""
import os

SRC = os.path.join('D:', os.sep, 'tmp', 'solar-system-cpp', 'src', 'sceneitem.cpp')

Q = chr(34)      # 双引号
AP = chr(39)     # 单引号
NL = chr(10)     # 换行


def esc(s):
    """非 ASCII 字符 -> \\uXXXX"""
    out = []
    for ch in s:
        out.append(ch if ord(ch) < 128 else '\\u%04x' % ord(ch))
    return ''.join(out)


NOTES = [
    '★ 距离为对数映射: 远处的间隔被压缩了, 标注中的数字才是真实距离。'
    '这是为了把 6 个数量级的尺度放进同一画面必须付出的代价。',

    '宇宙在大于约 3 亿光年的尺度上才表现出均匀性, 这就是宇宙学原理。'
    '更小的尺度上, 星系呈纤维状成团分布, 中间是巨大的空洞。',

    '★ 暗能量占 68.5%, 暗物质与普通物质合计仅 31.5%。'
    '我们熟悉的物质只占宇宙的不到 5% —— 这是当代宇宙学最反直觉的结论。',

    '斯隆巨壁长约 13.8 亿光年, 光穿越它需要 13.8 亿年 —— '
    '约为宇宙年龄 (137.97 亿年) 的十分之一。',

    '宇宙微波背景 (CMB) 是大爆炸后 38 万年的光, 温度 2.7255 K, '
    '红移 z 约 1090。它是我们能看到的宇宙最古老的照片。',

    '本星系群正以约 185 km/s 朝室女座星系团坠落; 而更大的尺度上, '
    '整个拉尼亚凯亚超星系团都朝巨引源流动 —— 说明运动是分层的。',
]

HIER = [
    ('行星系',    '~10^-4 光年'),
    ('恒星系',    '~1 光年'),
    ('星系',      '10 万光年'),
    ('星系群/团', '1000 万光年'),
    ('超星系团',  '5 亿光年'),
    ('宇宙网',    '> 100 亿光年'),
]


def L(*parts):
    """拼接一行代码"""
    return ''.join(parts)


def q(s):
    """加双引号"""
    return Q + s + Q


def build_block():
    out = []

    # ---------------- cosmosInfo ----------------
    out.append('QVariantMap SolarScene::cosmosInfo() const')
    out.append('{')
    out.append('    QVariantMap m;')
    out.append('')
    out.append('    // 标题')
    out.append('    m[' + q('title') + ']       = QStringLiteral(' + q(esc('宇宙大尺度结构')) + ');')
    out.append('    m[' + q('titleEn') + ']     = QStringLiteral(' + q('Large-Scale Structure') + ');')
    out.append('')
    out.append('    // 宇宙学参数 (Planck 2018) —— 当代宇宙学的定量基础')
    out.append('    m[' + q('age') + ']         = QStringLiteral(' + q('%1 ' + esc('亿年')) + ')')
    out.append('                           .arg(cosmo::kAgeGyr * 10.0, 0, ' + AP + 'f' + AP + ', 1);')
    out.append('    m[' + q('h0') + ']          = QStringLiteral(' + q('%1 km/s/Mpc') + ').arg(cosmo::kH0, 0, ' + AP + 'f' + AP + ', 1);')
    out.append('    m[' + q('omegaM') + ']      = QStringLiteral(' + q('%1 %') + ').arg(cosmo::kOmegaM * 100.0, 0, ' + AP + 'f' + AP + ', 1);')
    out.append('    m[' + q('omegaLambda') + '] = QStringLiteral(' + q('%1 %') + ').arg(cosmo::kOmegaLambda * 100.0, 0, ' + AP + 'f' + AP + ', 1);')
    out.append('    m[' + q('omegaB') + ']      = QStringLiteral(' + q('%1 %') + ').arg(cosmo::kOmegaB * 100.0, 0, ' + AP + 'f' + AP + ', 1);')
    out.append('    m[' + q('cmb') + ']         = QStringLiteral(' + q('%1 K') + ').arg(cosmo::kCMBTempK, 0, ' + AP + 'f' + AP + ', 4);')
    out.append('    m[' + q('cmbZ') + ']        = QStringLiteral(' + q('z = %1') + ').arg(cosmo::kCMBRedshift, 0, ' + AP + 'f' + AP + ', 0);')
    out.append('    m[' + q('obsRadius') + ']   = QStringLiteral(' + q(esc('465 亿光年')) + ');')
    out.append('    m[' + q('obsDia') + ']      = QStringLiteral(' + q(esc('930 亿光年')) + ');')
    out.append('    m[' + q('recombT') + ']     = QStringLiteral(' + q(esc('大爆炸后 38 万年')) + ');')
    out.append('    m[' + q('galaxies') + ']    = QStringLiteral(' + q(esc('约 2 万亿个')) + ');')
    out.append('')
    out.append('    // 结构层级 —— 逐级放大, 教学上最直观的切入方式')
    out.append('    m[' + q('hierarchy') + '] = QVariantList{')
    for lvl, size in HIER:
        out.append('        QVariantMap{{' + q('lvl') + ', QStringLiteral(' + q(esc(lvl)) + ')},')
        out.append('                    {' + q('size') + ', QStringLiteral(' + q(esc(size)) + ')}},')
    out.append('    };')
    out.append('    return m;')
    out.append('}')
    out.append('')

    # ---------------- cosmosNotes ----------------
    out.append('QVariantList SolarScene::cosmosNotes() const')
    out.append('{')
    out.append('    QVariantList out;')
    out.append('    const char *notes[] = {')
    for n in NOTES:
        out.append('        ' + q(esc(n)) + ',')
    out.append('    };')
    out.append('    for (const char *n : notes)')
    out.append('        out.append(QString::fromUtf8(n));')
    out.append('    return out;')
    out.append('}')
    out.append('')

    # ---------------- cosmosStructures ----------------
    out.append('QVariantList SolarScene::cosmosStructures() const')
    out.append('{')
    out.append('    QVariantList out;')
    out.append('    for (int i = 0; i < LARGE_STRUCTURES_COUNT; ++i) {')
    out.append('        const LargeStructure &s = LARGE_STRUCTURES[i];')
    out.append('        QVariantMap m;')
    out.append('        m[' + q('name') + '] = QString::fromUtf8(s.nameCn);')
    out.append('        m[' + q('en') + ']   = QString::fromUtf8(s.nameEn);')
    out.append('        m[' + q('dist') + '] = s.distanceFromEarthMly < 1.0')
    out.append('                    ? QStringLiteral(' + q(esc('本星系群')) + ')')
    out.append('                    : (s.distanceFromEarthMly >= 1000.0')
    out.append('                       ? QStringLiteral(' + q('%1 ' + esc('亿光年')) + ')')
    out.append('                             .arg(s.distanceFromEarthMly / 100.0, 0, ' + AP + 'f' + AP + ', 0)')
    out.append('                       : QStringLiteral(' + q('%1 ' + esc('百万光年')) + ')')
    out.append('                             .arg(s.distanceFromEarthMly, 0, ' + AP + 'f' + AP + ', 0));')
    out.append('        m[' + q('size') + '] = s.sizeMly >= 1000.0')
    out.append('                    ? QStringLiteral(' + q('%1 ' + esc('亿光年')) + ')')
    out.append('                          .arg(s.sizeMly / 100.0, 0, ' + AP + 'f' + AP + ', 0)')
    out.append('                    : QStringLiteral(' + q('%1 ' + esc('百万光年')) + ')')
    out.append('                          .arg(s.sizeMly, 0, ' + AP + 'f' + AP + ', 0);')
    out.append('        m[' + q('kind') + '] = s.kind;')
    out.append('        m[' + q('desc') + '] = QString::fromUtf8(s.desc);')
    out.append('        out.append(m);')
    out.append('    }')
    out.append('    return out;')
    out.append('}')
    out.append('')

    return NL.join(out)


def main():
    src = open(SRC, encoding='utf-8').read()
    start = src.index('QVariantMap SolarScene::cosmosInfo() const')
    end = src.index('QVariantMap SolarScene::galaxyInfo() const')

    blk = build_block()
    open(SRC, 'w', encoding='utf-8').write(src[:start] + blk + src[end:])

    bad = [c for c in blk if ord(c) > 127]
    print('重写完成，块长度 %d 字符' % len(blk))
    print('块内非 ASCII 字符: %d 个%s' % (len(bad), '' if not bad else ' !! ' + repr(bad[:10])))


if __name__ == '__main__':
    main()
