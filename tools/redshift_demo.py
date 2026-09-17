"""红移的可视化演示图 —— 用于教学与 UI 设计验证。

★★ 为什么要专门画这张图:

  用户问"红移能不能科学且直观表现出来"。答案是能, 但**必须先区分
  两种不同的东西**, 否则会做出一个看着漂亮但科学上错误的图:

    (1) **把星系按红移着色** —— 这是**数据编码**, 不是物理模拟。
        真实的星系颜色由它的恒星族决定 (老星系偏红、年轻星系偏蓝),
        与红移是两回事。但把 z 映射成颜色是**发表级论文里通行的
        可视化手法** (SDSS 自己的图就这么做), 只要明确标注
        "按红移着色" 就没有问题。

    (2) **光谱线的红移** —— 这才是红移的**定义本身**, 也是最直观的
        科学表达: 把同一套特征谱线 (Ca II K、Hβ、Mg、Na D、Hα)
        在不同 z 下画出来, 让"谱线整体向长波移动"这件事直接可见。

  本工具画的是 (2)。它同时能解释一个**真实的天文事实**:
  在 z≈0.74 时, Hα (656.3 nm) 已经移到 1141 nm —— **进入红外**。
  这正是 SDSS 的 LRG 样本为什么是"亮红星系": 高红移下,
  可见光波段里剩下的主要是长波端。

★ 关于常见误解 (必须写进教学说明):
  宇宙学红移**不是多普勒效应**, 而是空间膨胀。
  z=1 时若按 v = cz 理解会得出"以光速远离", 这在相对论里不可能 ——
  正确说法是"星系本身没动, 是它们之间的空间在增加"。

用法:
    python redshift_demo.py
"""
import math
import os

from PIL import Image, ImageDraw, ImageFont

OUT = r'D:\tmp\solar-system-cpp\assets\lss\redshift_demo.png'

# ---- 宇宙学参数 (Planck 2018) ----
H0, OM0, C_KMS = 67.4, 0.315, 299792.458

# 1 Mpc = 3.26156 Mly
#
# ★ 踩过的坑: 初版写成 306.601 (= 1/3.26156 × 1000), 系数倒置,
#   所有距离大了 94 倍 —— z=1 本该 104 亿光年却算成 1042 亿,
#   直接超过可观测宇宙半径 (465 亿光年), 属于"一眼可辨的荒谬值"。
#   教训: 算出天文距离后先做**量级自检** —— z=1 应在 100 亿光年量级。
MPC_TO_MLY = 3.26156


def comoving_mly(z, n=256):
    """红移 -> 共动距离 (百万光年)。Simpson 积分。"""
    if z <= 0:
        return 0.0
    dz = z / n
    tot = 0.0
    for i in range(n + 1):
        zz = i * dz
        e = math.sqrt(OM0 * (1 + zz) ** 3 + (1 - OM0))
        w = 1.0 if i in (0, n) else (4.0 if i % 2 else 2.0)
        tot += w / e
    return (C_KMS / H0) * (dz / 3.0) * tot * MPC_TO_MLY


# ---- 波长 -> 近似 RGB (CIE 近似, 用于画可见光色带) ----
def wl_to_rgb(wl):
    """波长 (nm) -> (r,g,b) 0..255。400~700 之外返回暗色。

    ★ 这只是"肉眼看到的颜色"的近似。物理上眼睛对 400nm 以下是不可见的,
      所以图上 700nm 之外画成暗灰 —— 这个视觉断点本身就是教学内容。
    """
    if wl < 380 or wl > 780:
        return (46, 48, 56)
    if wl < 440:
        r = -(wl - 440) / 60.0
        g, b = 0.0, 1.0
    elif wl < 490:
        r, g = 0.0, (wl - 440) / 50.0
        b = 1.0
    elif wl < 510:
        r, g, b = 0.0, 1.0, -(wl - 510) / 20.0
    elif wl < 580:
        r, g, b = (wl - 510) / 70.0, 1.0, 0.0
    elif wl < 645:
        r, g, b = 1.0, -(wl - 645) / 65.0, 0.0
    else:
        r, g, b = 1.0, 0.0, 0.0
    # 边缘衰减: 让可见光两端自然变暗
    f = 1.0
    if wl < 420:
        f = 0.3 + 0.7 * (wl - 380) / 40.0
    elif wl > 700:
        f = 0.3 + 0.7 * (780 - wl) / 80.0
    return tuple(int(255 * c * f) for c in (max(r, 0), max(g, 0), max(b, 0)))


# ---- 真实光谱特征线 (静止波长 nm) ----
LINES = [
    ('Ca II K', 393.4),
    ('Ca II H', 396.8),
    ('Hδ',      410.2),
    ('Hγ',      434.0),
    ('Hβ',      486.1),
    ('Mg I',    517.5),
    ('Na I D',  589.0),
    ('Hα',      656.3),
]


def main():
    W, H = 1500, 1000
    bg = (5, 7, 13)
    im = Image.new('RGB', (W, H), bg)
    d = ImageDraw.Draw(im)

    def font(sz):
        for name in ('msyh.ttc', 'msyh.ttf', 'simhei.ttf'):
            p = os.path.join(r'C:\Windows\Fonts', name)
            if os.path.isfile(p):
                try:
                    return ImageFont.truetype(p, sz)
                except Exception:
                    pass
        return ImageFont.load_default()

    f_title = font(30)
    f_mid = font(19)
    f_sm = font(15)
    f_tiny = font(13)

    d.text((50, 32), '红移的可视化：同一套谱线在不同红移下的位置',
           font=f_title, fill=(220, 232, 255))
    d.text((50, 74),
           '谱线整体向长波移动 —— 这是红移的定义本身，'
           '也是天文学家测量它的方式',
           font=f_sm, fill=(150, 168, 195))

    # ---- 光谱区 ----
    X0, X1 = 210, W - 70
    WL_MIN, WL_MAX = 380, 1350        # 覆盖到近红外, 让"移出可见"可见
    y0 = 150
    row_h = 150
    bar_h = 62

    zs = [0.0, 0.30, 0.738, 1.00]

    def wl2x(wl):
        return X0 + (wl - WL_MIN) / (WL_MAX - WL_MIN) * (X1 - X0)

    for ri, z in enumerate(zs):
        ytop = y0 + ri * row_h
        ybar = ytop + 44

        # 每行的说明
        dist = comoving_mly(z)
        if z == 0:
            label = 'z = 0    静止参考系'
            sub = '（实验室测得的波长）'
        elif z == 0.738:
            label = 'z = 0.738'
            sub = '本样本中位  距离 %.2f 亿光年' % (dist / 100.0)
        else:
            label = 'z = %.3f' % z
            sub = '距离 %.2f 亿光年' % (dist / 100.0)
        d.text((50, ytop + 8), label, font=f_mid, fill=(190, 208, 235))
        d.text((50, ytop + 32), sub, font=f_tiny, fill=(120, 136, 160))

        # 色带背景 (逐像素画波长对应颜色)
        for x in range(X0, X1):
            wl = WL_MIN + (x - X0) / (X1 - X0) * (WL_MAX - WL_MIN)
            c = wl_to_rgb(wl)
            d.line([(x, ybar), (x, ybar + bar_h)], fill=c)

        # 可见光边界 (700nm 右侧开始变暗)
        xvis = wl2x(700)
        if xvis < X1:
            d.line([(xvis, ybar - 10), (xvis, ybar + bar_h + 10)],
                   fill=(120, 130, 145), width=1)
            d.text((xvis + 4, ybar - 26), '可见光边界 700nm',
                   font=f_tiny, fill=(120, 130, 145))

        # 谱线 (画成暗线, 模拟吸收线)
        li = 0
        for name, wl0 in LINES:
            wl = wl0 * (1.0 + z)
            x = wl2x(wl)
            if x < X0 or x > X1:
                continue
            d.line([(x, ybar), (x, ybar + bar_h)], fill=(10, 10, 14), width=3)
            # ★ 只在可见光范围内标名字 —— 移出可见的线标出来会误导
            #
            # ★★ 标签必须**交错两行**: 初版全部放在同一行, 而
            #    380~450nm 内有 4 条线 (Ca II K/H、Hδ、Hγ) 挤在一起,
            #    标签糊成一团完全看不清。按出现顺序奇偶交错即可分开。
            if wl <= 700:
                dy = 6 if li % 2 == 0 else 25
                d.text((x - 15, ybar + bar_h + dy), name,
                       font=f_tiny, fill=(178, 194, 218))
                li += 1

        # 该行小结: 有几条线还在可见光内
        vis = sum(1 for _, w in LINES if w * (1 + z) <= 700)
        d.text((X1 - 200, ytop + 8),
               '可见光内 %d / %d 条' % (vis, len(LINES)),
               font=f_sm, fill=(150, 200, 160) if vis >= 4 else (210, 180, 130))

    # ---- 底部教学说明 ----
    yb = y0 + len(zs) * row_h + 20
    d.line([(50, yb), (W - 50, yb)], fill=(40, 50, 70))
    yb += 18

    notes = [
        ('波段范围延伸至 1350nm（近红外），因此可以看到谱线"移出可见光"的过程。', (160, 176, 200)),
        ('在 z = 0.738 时，Hα（656nm）移到 1141nm —— 完全进入红外；'
         '同时 4000Å 断裂移到 695nm。', (210, 200, 150)),
        ('这正是 SDSS 高红移样本多为"亮红星系"的原因：可见光波段里剩下的'
         '主要是长波端，短波特征谱线早已移出。', (210, 200, 150)),
        ('', (0, 0, 0)),
        ('★ 宇宙学红移不是多普勒效应，而是空间膨胀。'
         '若按 v = cz 理解，z = 1 的星系就是"以光速远离"，', (200, 160, 160)),
        ('  这在相对论里不可能。正确说法是：星系本身没怎么动，'
         '是它们之间的空间在增加。', (200, 160, 160)),
        ('★ 红移与颜色的关系容易误解：星系"看起来红"多半是因为它本身'
         '由老年恒星组成（SED 偏红），红移只是叠加在上面的一层效应。',
         (170, 180, 200)),
    ]
    for txt, col in notes:
        if txt:
            d.text((50, yb), txt, font=f_sm, fill=col)
        yb += 26

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    im.save(OUT, quality=94)
    print('已写 %s  (%dx%d)' % (OUT, W, H))

    # ---- 同时打印数据表, 便于写进 UI 与文档 ----
    print()
    print('=== 红移 -> 共动距离 (Planck 2018) ===')
    for z in [0.1, 0.3, 0.5, 0.6, 0.738, 0.9, 1.0, 1.5, 2.0, 6.0, 1100.0]:
        dm = comoving_mly(z)
        print('  z = %-7.3f  %8.1f Mly   %6.2f Gly' % (z, dm, dm / 1000.0))

    print()
    print('=== 谱线位移 (nm) ===')
    print('  %-10s %8s' % ('线', '静止'), end='')
    for z in zs:
        print(' %10s' % ('z=%.3f' % z), end='')
    print()
    for name, wl0 in LINES:
        print('  %-10s %8.1f' % (name, wl0), end='')
        for z in zs:
            print(' %10.1f' % (wl0 * (1 + z)), end='')
        print()

    print()
    print('=== 常见误解的量化 ===')
    for z in [0.5, 1.0, 1.5]:
        naive = z                           # v = cz, 以 c 为单位
        print('  z=%.1f: 按 v=cz 得 %.2fc %s'
              % (z, naive, '← 超过光速, 说明该公式在此失效' if naive >= 1.0 else ''))


if __name__ == '__main__':
    main()
