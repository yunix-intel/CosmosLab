"""测量 NASA/JPL 银河系图里旋臂的方位角，用于校准粒子模型。

★★ 为什么必须先测:

  用户选择"3D 粒子 + 半透明叠加 NASA 图"。
  但如果粒子的旋臂相位与图上画的不一致, 叠加后会出现**双份旋臂** ——
  比不叠加还难看。

  所以要先从图里测出**旋臂在给定半径处的方位角**,
  再据此把粒子模型旋转对齐 (或反过来旋转贴图)。

★ 测量原理:
  以银心为圆心、取若干固定半径画圆, 沿圆周采样亮度。
  旋臂处是亮带 -> 圆周亮度曲线上的峰就是旋臂的位置。
  对亮度做较强的角度平滑以压掉恒星/星云的高频纹理,
  再找显著峰。

★ 要排除的干扰:
  1. 白色文字 (Sun, Arm 名, 距离标签) —— 用"纯白且三通道接近"掩掉
  2. 距离环线 (细的亮弧) —— 半径方向的窄带, 沿圆周方向是连续的,
     会整体抬高基线但不形成角度上的峰, 影响可控
  3. 中央核球 —— 半径太小时整圈都亮, 没有可辨的峰。故测量半径
     从棒端附近 (约 13,500 ly ≈ 198 px) 之外开始

用法:
    python measure_arms.py
"""
import math

import numpy as np
from PIL import Image

SRC = r'D:\tmp\galaxy_hurt_2000.jpg'
# 由三条距离环 + 太阳-银心距离交叉标定得到 (见 calibration 注释)
LY_PER_PX = 68.0
SUN_PX = (995.0, 1380.0)      # 太阳在图中的位置
CENTER_PX = (1000.0, 1000.0)  # 银心 = 图心


def load_masked(path):
    """读图并掩掉纯白文字。"""
    a = np.array(Image.open(path).convert('RGB')).astype(np.float32)
    r, g, b = a[:, :, 0], a[:, :, 1], a[:, :, 2]
    white = (r > 210) & (g > 210) & (b > 210) \
            & (np.abs(r - g) < 16) & (np.abs(g - b) < 16)
    lum = 0.299 * r + 0.587 * g + 0.114 * b
    # 用大窗口中值替换白字, 避免在圆采样时打到 255
    from numpy.lib.stride_tricks import sliding_window_view
    pad = 6
    p = np.pad(lum, pad, mode='edge')
    win = sliding_window_view(p, (2 * pad + 1, 2 * pad + 1))
    med = np.median(win, axis=(2, 3))
    lum = np.where(white, med, lum)
    return lum


def circle_profile(lum, cx, cy, r_px, n=1440):
    """沿半径 r_px 的整圆采样。"""
    h, w = lum.shape
    th = np.linspace(0, 2 * math.pi, n, endpoint=False)
    xs = (cx + r_px * np.cos(th)).astype(int)
    ys = (cy + r_px * np.sin(th)).astype(int)
    ok = (xs >= 0) & (xs < w) & (ys >= 0) & (ys < h)
    v = np.full(n, np.nan)
    v[ok] = lum[ys[ok], xs[ok]]
    return th, v


def circ_smooth(v, k):
    n = len(v)
    k = max(3, k | 1)
    ker = np.ones(k) / k
    idx = np.arange(n)
    ok = ~np.isnan(v)
    if ok.sum() < 10:
        return v
    v = np.interp(idx, idx[ok], v[ok], period=n)
    pad = np.concatenate([v[-(k // 2):], v, v[:k // 2]])
    return np.convolve(pad, ker, mode='same')[k // 2:-(k // 2)]


def find_peaks(v, th, min_sep_deg=40.0, top=8):
    """找显著峰。返回 [(方位角°, 相对强度), ...]。"""
    n = len(v)
    sep = int(min_sep_deg / 360.0 * n)
    v = np.asarray(v, dtype=np.float64)
    lo, hi = np.nanmin(v), np.nanmax(v)
    if hi - lo < 1e-6:
        return []
    norm = (v - lo) / (hi - lo)
    order = np.argsort(norm)[::-1]
    picked = []
    for i in order:
        if norm[i] < 0.25:
            break
        if all(min(abs(i - j), n - abs(i - j)) >= sep for j in picked):
            picked.append(i)
        if len(picked) >= top:
            break
    picked.sort(key=lambda i: -norm[i])
    return [(float(math.degrees(th[i])), float(norm[i])) for i in picked]


def main():
    print('=== 载入并掩掉文字')
    lum = load_masked(SRC)
    cx, cy = CENTER_PX
    sun_x, sun_y = SUN_PX

    # 太阳的方位角 (图像坐标 y 向下, 故用 atan2(dy, dx))
    sun_ang = math.degrees(math.atan2(sun_y - cy, sun_x - cx)) % 360
    sun_r_px = math.hypot(sun_x - cx, sun_y - cy)
    print('  太阳: 距银心 %.0f px = %.0f ly  方位角 %.1f°'
          % (sun_r_px, sun_r_px * LY_PER_PX, sun_ang))

    print()
    print('=== 逐半径测旋臂方位')
    print('  %-10s %-9s %s' % ('半径(ly)', 'px', '检测到的旋臂方位角(相对强度)'))

    rows = []
    for r_ly in (15000, 20000, 25000, 30000, 35000, 40000, 45000):
        r_px = r_ly / LY_PER_PX
        if r_px > min(cx, cy) - 20:
            continue
        th, v = circle_profile(lum, cx, cy, r_px)
        vs = circ_smooth(v, 61)          # 约 15° 平滑
        peaks = find_peaks(vs, th, min_sep_deg=35, top=8)
        rows.append((r_ly, peaks))
        txt = '  '.join('%.0f°(%.2f)' % (a, s) for a, s in peaks[:6])
        print('  %-10d %-9.0f %s' % (r_ly, r_px, txt))

    print()
    print('=== 与项目粒子模型的对比')
    # 项目模型: 4 条臂, 螺距 12°, 从 13500 ly 起
    # 对数螺旋 theta(r) = theta0 + ln(r/r0)/tan(pitch)
    pitch = math.radians(12.0)
    r0 = 13500.0
    print('  模型螺距角 12°, 起点半径 %.0f ly' % r0)
    print('  模型在半径 r 处的方位角间隔 = %.1f° (相邻臂)'
          % (360.0 / 4))
    print()
    print('  ★ 结论需要人工比对: 若图的臂数/螺距与模型差异大,')
    print('    则不应把两者硬叠, 而应改用"图驱动粒子"的方式。')


if __name__ == '__main__':
    main()
