# ============================================================================
#  hubble_diagram.py —— 从真实超新星数据生成哈勃图
#
#  三块面板:
#    A) μ–z 图        : 1701 颗 Ia 型超新星 + 三条 ΛCDM 理论曲线
#    B) 残差图        : 相对最佳拟合的偏离 —— 减速宇宙的曲线会明显跑偏
#    C) χ²–Ωm         : 参数约束
#
#  ★ 面板 B 是这张图真正的价值所在:
#    看 μ–z 图时, 三条曲线在视觉上"差不多"; 只有把差异放大,
#    才能看出数据究竟支持哪一个。这正是 1998 年那两篇论文的做法。
# ============================================================================
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hubble_fit import dl_over_dh, chi2_over_C, load_pantheon, DATA   # noqa: E402

# ---- 调色 (与 redshift_demo.png 一致的暗色系) ----
BG      = (11, 15, 22)
GRID    = (30, 38, 52)
FRAME   = (58, 72, 94)
TX      = (222, 233, 248)
TXDIM   = (138, 156, 182)
C_DATA  = (120, 200, 255)      # 数据点 (密度图)
C_BEST  = (126, 226, 138)      # 最佳拟合
C_EDS   = (255, 118, 108)      # 爱因斯坦-德西特 (Ωm=1)
C_EMPTY = (255, 205, 112)      # 空宇宙 (Ωm=0)
C_PLANCK= (190, 150, 255)      # Planck 2018

W, H = 1700, 1660


def font(sz, bold=False):
    names = (['msyhbd.ttc', 'msyh.ttc'] if bold else ['msyh.ttc'])
    for n in names:
        p = os.path.join(r'C:\Windows\Fonts', n)
        if os.path.isfile(p):
            try:
                return ImageFont.truetype(p, sz)
            except Exception:
                pass
    return ImageFont.load_default()


def density_panel(arr, w, h, cmap_stops):
    """把累积计数数组映射成 RGB 图。

    ★ 为什么用密度累积而不是直接画点:
      1701 个点里有相当一部分挤在低红移区 (中位 z=0.16),
      直接画会糊成一片。累积到像素上再上色, 能看到真实的疏密结构。
    """
    a = arr[:h, :w]
    if a.max() <= 0:
        return np.zeros((h, w, 3), dtype=np.uint8)
    # 对数压缩 —— 点密度跨几个量级, 线性映射只能看到最密的几处
    v = np.log1p(a) / math.log1p(a.max())
    v = np.clip(v, 0.0, 1.0)
    stops = np.array(cmap_stops, dtype=float)      # [(pos, r,g,b), ...]
    pos = stops[:, 0]
    out = np.zeros((h, w, 3), dtype=float)
    for c in range(3):
        out[:, :, c] = np.interp(v, pos, stops[:, c + 1])
    return out.astype(np.uint8)


def main():
    z, mu, err = load_pantheon(DATA)
    n = len(z)
    print('载入 %d 颗超新星' % n)

    # ---- 拟合 ----
    oms = np.linspace(0.02, 0.98, 97)
    chis = np.array([chi2_over_C(z, mu, err, o, 1.0 - o)[0] for o in oms])
    i0 = int(np.argmin(chis))
    lo, hi = max(0.0, oms[i0] - 0.06), min(1.0, oms[i0] + 0.06)
    fine = np.linspace(lo, hi, 121)
    cf = np.array([chi2_over_C(z, mu, err, o, 1.0 - o)[0] for o in fine])
    jb = int(np.argmin(cf))
    OM = float(fine[jb])
    CHI_MIN = float(cf[jb])
    sig = fine[cf <= CHI_MIN + 1.0]
    OM_LO, OM_HI = float(sig.min()), float(sig.max())
    print('最佳拟合 Ωm = %.4f   (1σ: %.3f ~ %.3f)' % (OM, OM_LO, OM_HI))

    # 三种模型的 C (各自边缘化)
    models = []
    for label, om, ol, col in [
        ('最佳拟合  Ωm=%.3f' % OM, OM, 1.0 - OM, C_BEST),
        ('爱因斯坦-德西特  Ωm=1', 1.0, 0.0, C_EDS),
        ('空宇宙  Ωm=0', 0.0, 0.0, C_EMPTY),
        ('Planck 2018  Ωm=0.315', 0.315, 0.685, C_PLANCK),
    ]:
        c, cc = chi2_over_C(z, mu, err, om, ol)
        models.append((label, om, ol, col, cc, c))
        print('  %-26s χ² = %8.1f' % (label, c))

    # ======================= 画布 =======================
    im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(im)

    f_title = font(30, True)
    f_sub   = font(15)
    f_lab   = font(19, True)
    f_mid   = font(16)
    f_sm    = font(14)
    f_tiny  = font(12)
    f_mono  = ImageFont.truetype(r'C:\Windows\Fonts\consola.ttf', 14) \
        if os.path.isfile(r'C:\Windows\Fonts\consola.ttf') else f_tiny

    # ---- 标题 ----
    d.text((70, 42), '哈勃图 · Hubble Diagram', font=f_title, fill=TX)
    d.text((70, 84),
           'Pantheon+ 样本: %d 颗 Ia 型超新星 (2022) — 真实观测数据' % n,
           font=f_sub, fill=TXDIM)
    d.text((70, 106),
           '标准烛光: 距离由视亮度独立测定, 与红移无关 —— 因此可以检验 v–d 关系',
           font=f_tiny, fill=(104, 122, 148))

    LM, RM = 110, 60
    PW = W - LM - RM

    # ==================== 面板 A: μ vs z ====================
    AY, AH = 186, 420
    ZMIN, ZMAX = 0.01, 2.5
    MUMIN, MUMAX = 28.4, 47.6

    def zx(zz):
        t = (math.log10(max(zz, ZMIN)) - math.log10(ZMIN)) \
            / (math.log10(ZMAX) - math.log10(ZMIN))
        return LM + t * PW

    def muy(v):
        return AY + AH - (v - MUMIN) / (MUMAX - MUMIN) * AH

    d.text((LM, AY - 34), 'A  距离模数 μ 与红移 z', font=f_lab, fill=TX)
    d.text((LM + 300, AY - 30),
           'μ = 5·log₁₀(d_L / 10pc) — 纵轴越大 = 越远越暗',
           font=f_tiny, fill=TXDIM)

    # 网格
    for zz in [0.01, 0.03, 0.1, 0.3, 1.0, 2.5]:
        x = zx(zz)
        d.line([(x, AY), (x, AY + AH)], fill=GRID)
    for v in np.arange(30, 48, 2):
        y = muy(v)
        d.line([(LM, y), (LM + PW, y)], fill=GRID)

    # 数据密度图
    ACC = np.zeros((AH, PW), dtype=float)
    clipped = 0
    for zz, mm in zip(z, mu):
        x = int(zx(zz) - LM)
        y = int(muy(mm) - AY)
        if not (0 <= x < PW and 0 <= y < AH):
            clipped += 1
        if 0 <= x < PW and 0 <= y < AH:
            ACC[y, x] += 1.0
            # 轻微扩散, 让稀疏区的点也能看见
            for dy in (-1, 1):
                for dx in (-1, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < AH and 0 <= xx < PW:
                        ACC[yy, xx] += 0.22

    # ★★ 画完立刻核对: 坐标范围必须能装下**全部**数据点。
    #   实测踩到过一次 —— 纵轴下限写高了 3 个量级, 42 颗最近的低红移
    #   超新星被静默丢掉, 图上完全看不出来。
    if clipped:
        print('  ⚠ 有 %d 个点落在坐标范围外 (检查 MUMIN/ZMIN)' % clipped)
    else:
        print('  ✓ 全部 %d 个点都在范围内' % n)

    dens = density_panel(ACC, PW, AH, [
        (0.00, 11, 15, 22),
        (0.10, 30, 58, 96),
        (0.30, 40, 110, 175),
        (0.60, 110, 190, 245),
        (0.85, 210, 235, 255),
        (1.00, 255, 255, 255),
    ])
    px = dens.astype(Image.Image) if False else Image.fromarray(dens, 'RGB')
    im.paste(px, (LM, AY))

    # 理论曲线
    zs = np.exp(np.linspace(math.log(ZMIN), math.log(min(ZMAX, z.max() + 0.05)), 400))
    for label, om, ol, col, cc, chi in models:
        pts = []
        for zz in zs:
            v = 5.0 * math.log10(dl_over_dh(zz, om, ol)) + cc
            pts.append((zx(zz), muy(v)))
        d.line(pts, fill=col, width=3)

    # 边框
    d.rectangle([LM, AY, LM + PW, AY + AH], outline=FRAME, width=2)

    # 轴刻度
    for zz in [0.01, 0.03, 0.1, 0.3, 1.0, 2.5]:
        d.text((zx(zz) - 16, AY + AH + 10), ('%g' % zz), font=f_mono, fill=TXDIM)
    for v in np.arange(30, 48, 2):
        d.text((LM - 40, muy(v) - 8), '%g' % v, font=f_mono, fill=TXDIM)
    d.text((LM + PW - 60, AY + AH + 32), '红移 z', font=f_sm, fill=TXDIM)
    d.text((LM - 92, AY - 2), 'μ', font=f_mid, fill=TXDIM)

    # 图例
    # ★ 放**左下角**: 右上角正是三条曲线张开最大的位置, 图例在那儿会盖住
    #   最关键的"它们到底差多少"那一段。
    lx, ly = LM + 24, AY + AH - 146
    d.rectangle([lx - 12, ly - 10, lx + 350, ly + 96],
                fill=(14, 19, 28), outline=FRAME)
    for i, (label, om, ol, col, cc, chi) in enumerate(models):
        yy = ly + i * 24
        d.line([(lx, yy + 7), (lx + 34, yy + 7)], fill=col, width=3)
        d.text((lx + 44, yy), label, font=f_sm, fill=col)
    d.text((lx, ly + 100), '— 数据点为密度图 (越亮 = 该处超新星越多)',
           font=f_tiny, fill=TXDIM)

    # ==================== 面板 B: 残差 ====================
    BY, BH = 700, 400
    DMIN, DMAX = -0.7, 1.5

    def by(v):
        return BY + BH - (v - DMIN) / (DMAX - DMIN) * BH

    d.text((LM, BY - 38), 'B  残差 — 相对最佳拟合的偏离', font=f_lab, fill=TX)
    d.text((LM + 380, BY - 34),
           '★ 这一块才是判据: 若宇宙减速, 数据就该跟着红线上扬',
           font=f_tiny, fill=(255, 170, 150))

    for zz in [0.01, 0.03, 0.1, 0.3, 1.0, 2.5]:
        x = zx(zz)
        d.line([(x, BY), (x, BY + BH)], fill=GRID)
    for v in np.arange(-0.5, 1.6, 0.5):
        y = by(v)
        d.line([(LM, y), (LM + PW, y)], fill=GRID)

    # 零线
    d.line([(LM, by(0)), (LM + PW, by(0))], fill=(90, 108, 132), width=2)

    # 模型的残差曲线 (相对最佳拟合)
    _, _, _, _, cc_best, _ = models[0]
    for label, om, ol, col, cc, chi in models:
        if abs(om - OM) < 1e-9:
            continue
        pts = []
        for zz in zs:
            v = (5.0 * math.log10(dl_over_dh(zz, om, ol)) + cc) \
              - (5.0 * math.log10(dl_over_dh(zz, OM, 1.0 - OM)) + cc_best)
            pts.append((zx(zz), by(v)))
        d.line(pts, fill=col, width=3)

    # 数据残差 (分箱, 让趋势看得见)
    nb = 22
    edges = np.exp(np.linspace(math.log(ZMIN), math.log(ZMAX), nb + 1))
    for i in range(nb):
        m = (z >= edges[i]) & (z < edges[i + 1])
        if m.sum() < 3:
            continue
        zb = math.sqrt(edges[i] * edges[i + 1])     # 对数中点
        dm = np.mean([5.0 * math.log10(dl_over_dh(zz, OM, 1.0 - OM)) + cc_best
                      for zz in z[m]])
        r = float(np.mean(mu[m] - dm))
        e = float(np.sqrt(np.mean(err[m] ** 2)) / math.sqrt(m.sum()))
        x, y = zx(zb), by(r)
        d.line([(x, y - e / (DMAX - DMIN) * BH),
                (x, y + e / (DMAX - DMIN) * BH)], fill=(150, 200, 245), width=2)
        d.ellipse([x - 5, y - 5, x + 5, y + 5], fill=(225, 240, 255),
                  outline=(255, 255, 255))

    d.rectangle([LM, BY, LM + PW, BY + BH], outline=FRAME, width=2)
    for zz in [0.01, 0.03, 0.1, 0.3, 1.0, 2.5]:
        d.text((zx(zz) - 16, BY + BH + 10), ('%g' % zz), font=f_mono, fill=TXDIM)
    for v in np.arange(-0.5, 1.6, 0.5):
        d.text((LM - 46, by(v) - 8), '%+.1f' % v, font=f_mono, fill=TXDIM)
    d.text((LM + PW - 60, BY + BH + 32), '红移 z', font=f_sm, fill=TXDIM)
    d.text((LM - 96, BY - 2), 'Δμ', font=f_mid, fill=TXDIM)

    # 图例 B
    lx2, ly2 = LM + PW - 400, BY + 16
    d.rectangle([lx2 - 12, ly2 - 8, lx2 + 350, ly2 + 74],
                fill=(14, 19, 28), outline=FRAME)
    d.ellipse([lx2, ly2 + 2, lx2 + 10, ly2 + 12], fill=(225, 240, 255))
    d.text((lx2 + 20, ly2 - 2), '数据 (分箱均值 ± 标准误)', font=f_tiny, fill=TXDIM)
    for i, (label, om, ol, col, cc, chi) in enumerate(models):
        if abs(om - OM) < 1e-9:
            continue
        yy = ly2 + 22 + i * 20
        d.line([(lx2, yy + 6), (lx2 + 30, yy + 6)], fill=col, width=3)
        d.text((lx2 + 40, yy - 2), label.split('  ')[0], font=f_tiny, fill=col)

    # ==================== 面板 C: χ²–Ωm ====================
    CY, CH = 1186, 250
    CMIN = CHI_MIN
    d.text((LM, CY - 34), 'C  参数约束', font=f_lab, fill=TX)

    # 只画 Δχ² 到 40, 否则最差的点会把曲线压平
    dc = chis - CHI_MIN
    ymax = 60.0

    def cx(om):
        return LM + om * PW

    def cy(v):
        return CY + CH - min(v, ymax) / ymax * CH

    for v in np.arange(0, 61, 10):
        d.line([(LM, cy(v)), (LM + PW, cy(v))], fill=GRID)
    for om in np.arange(0.0, 1.01, 0.2):
        d.line([(cx(om), CY), (cx(om), CY + CH)], fill=GRID)

    # Δχ² 参考线
    for lvl, lab in [(1, '1σ'), (4, '2σ'), (9, '3σ')]:
        col = (70, 88, 112)
        d.line([(LM, cy(lvl)), (LM + PW, cy(lvl))], fill=col)
        d.text((LM + PW - 34, cy(lvl) - 16), lab, font=f_tiny, fill=col)

    pts = [(cx(o), cy(v)) for o, v in zip(oms, dc)]
    d.line(pts, fill=C_BEST, width=3)

    # 最佳点
    bx, byy = cx(OM), cy(0)
    d.line([(bx, CY), (bx, CY + CH)], fill=(90, 200, 110), width=2)
    d.ellipse([bx - 5, byy - 5, bx + 5, byy + 5], fill=(220, 255, 220))
    d.text((bx + 12, byy + 6), 'Ωm = %.3f' % OM, font=f_mid, fill=C_BEST)
    d.text((bx + 12, byy + 28),
           '(1σ: %.3f–%.3f)' % (OM_LO, OM_HI), font=f_tiny, fill=TXDIM)

    # 标注被排除的模型
    for lab, om, col in [('爱因斯坦-德西特', 1.0, C_EDS),
                         ('空宇宙', 0.0, C_EMPTY)]:
        v = float(np.interp(om, oms, dc))
        x = cx(om)
        if om >= 1.0:
            x = LM + PW - 6
        d.line([(x, cy(v)), (x, CY + CH)], fill=col, width=2)
        d.text((x - 110 if om > 0.5 else x + 8, CY + 8),
               '%s\nΔχ²=%.0f' % (lab, v), font=f_tiny, fill=col)

    d.rectangle([LM, CY, LM + PW, CY + CH], outline=FRAME, width=2)
    for om in np.arange(0.0, 1.01, 0.2):
        d.text((cx(om) - 14, CY + CH + 8), '%.1f' % om, font=f_mono, fill=TXDIM)
    d.text((LM + PW / 2 - 60, CY + CH + 32), 'Ωm (物质密度参数)',
           font=f_sm, fill=TXDIM)
    d.text((LM - 96, CY - 2), 'Δχ²', font=f_mid, fill=TXDIM)

    # ==================== 页脚 ====================
    fy = CY + CH + 66
    lines = [
        ('★ 结论', C_BEST, ''),
        ('  最佳拟合 Ωm = %.3f,  Planck 2018 (CMB) 给 0.315 —— 两条完全独立的观测途径一致。'
         % OM, TXDIM, ''),
        ('  减速宇宙 (Ωm=1) 的 Δχ² = %.0f —— 即"若数据符合减速宇宙, 这个值应为 0"。'
         % (float(np.interp(1.0, oms, dc))), TXDIM, ''),
        ('  这是宇宙加速膨胀的定量证据, 也是 2011 年诺贝尔物理学奖的依据。', TXDIM, ''),
        ('  注: 本拟合只用对角误差; 计入 Pantheon+ 的完整协方差矩阵会给出更紧的约束。',
         (108, 124, 150), ''),
    ]
    yy = fy
    for txt, col, _ in lines:
        d.text((70, yy), txt, font=f_sm if col != C_BEST else f_lab, fill=col)
        yy += 26 if col != C_BEST else 32

    out = r'D:\tmp\solar-system-cpp\assets\sn\hubble_diagram.png'
    os.makedirs(os.path.dirname(out), exist_ok=True)
    im.save(out)
    print('已保存 %s  (%dx%d)' % (out, W, H))
    return 0


if __name__ == '__main__':
    sys.exit(main())
