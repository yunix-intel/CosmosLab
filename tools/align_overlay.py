"""计算 NASA 图与粒子模型之间的最佳旋转角，并生成叠加用贴图。

★★ 问题: 叠加时必须让两个旋臂图案重合, 否则会出现"双份旋臂"。

★ 做法:
  1. 粒子模型的旋臂方位可解析计算:
         θ(r) = θ_start + ln(r / r0) / tan(pitch)
     θ_start = 0/90/180/270°, r0 = 13500 ly, pitch = 12°
  2. NASA 图的旋臂方位由 measure_arms.py 在多个半径上实测得到
  3. 求一个全局旋转 δ, 使"模型臂方位 + δ"与"实测臂方位"最接近

★ 关于"图里几条臂":
  实测最强的两个峰总是相距约 180°, 说明这张图**强调两条主臂**
  (Hurt 的插画按 Spitzer 红外结论把英仙臂与盾牌-半人马臂画得更亮更连续)。
  模型画的是四条等权臂, 因此对齐后必然有两条更贴合、两条落在臂间。
  这是"两臂 vs 四臂"这一真实科学争议的体现, 不是 bug —— 会写进 UI 说明。

用法:
    python align_overlay.py --solve      只求旋转角
    python python align_overlay.py --build  求角 + 生成叠加贴图
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

SRC = r'D:\tmp\galaxy_hurt_2000.jpg'
OUT_TEX = r'D:\tmp\solar-system-cpp\assets\tex\galaxy_overlay.jpg'
OUT_META = r'D:\tmp\solar-system-cpp\assets\tex\galaxy_overlay.txt'

LY_PER_PX = 68.0
CENTER_PX = (1000.0, 1000.0)
SUN_PX = (995.0, 1380.0)

# ---- 粒子模型参数 (与 galaxydata.h 一致) ----
ARM_THETA0 = [0.0, 90.0, 180.0, 270.0]   # 度
ARM_R0_LY = 13500.0
ARM_PITCH_DEG = 12.0

# ---- measure_arms.py 的实测结果 (半径 ly -> [(方位角°, 强度)]) ----
MEASURED = {
    20000: [(230, 1.00), (53, 0.91), (137, 0.82), (102, 0.62), (311, 0.50)],
    25000: [(180, 1.00), (2, 0.86), (145, 0.56), (69, 0.44), (253, 0.36)],
    30000: [(126, 1.00), (318, 0.88), (82, 0.55), (203, 0.46), (36, 0.41)],
    35000: [(288, 1.00), (80, 0.98), (115, 0.73), (355, 0.52), (253, 0.38)],
    40000: [(257, 1.00), (58, 0.90), (135, 0.45), (222, 0.43)],
    45000: [(224, 1.00), (38, 0.88), (188, 0.36)],
}


def model_arm_angles(r_ly):
    """模型在半径 r 处的四条臂方位角 (度)。"""
    out = []
    for t0 in ARM_THETA0:
        th = t0 + math.degrees(
            math.log(r_ly / ARM_R0_LY) / math.tan(math.radians(ARM_PITCH_DEG)))
        out.append(th % 360.0)
    return out


def ang_diff(a, b):
    """最小角差, 归一到 ±180。"""
    d = (a - b + 180.0) % 360.0 - 180.0
    return d


def score(delta):
    """给定旋转 δ, 计算模型臂与实测峰的加权失配。越小越好。"""
    total = 0.0
    weight = 0.0
    for r, peaks in MEASURED.items():
        arms = [a + delta for a in model_arm_angles(r)]
        for pang, pw in peaks:
            # 每个实测峰取其到最近模型臂的距离, 按强度加权
            d = min(abs(ang_diff(pang, a)) for a in arms)
            total += d * pw
            weight += pw
    return total / max(weight, 1e-9)


def solve():
    best, bd = None, 1e9
    for delta in np.arange(-180.0, 180.0, 0.5):
        s = score(delta)
        if s < bd:
            bd, best = s, float(delta)
    # 细化
    for delta in np.arange(best - 1.0, best + 1.0, 0.05):
        s = score(delta)
        if s < bd:
            bd, best = s, float(delta)
    return best, bd


def build_overlay(rotate_deg):
    """生成叠加贴图。

    ★ 要清掉的东西:
      * 白色文字 (Sun / 臂名 / 距离标签) —— 与 app 自己的 3D 标注冲突
      * 细网格线与距离环 —— 叠加后在 3D 里会显得杂乱
      做法: 先按"纯白"掩掉文字, 再用中值滤波抹掉细线
      (线条宽 2-3 px, 中值窗口 9 足够; 文字高约 20 px 不会被中值吃掉,
       所以必须先掩文字)。
    """
    im = Image.open(SRC).convert('RGB')
    a = np.array(im).astype(np.float32)
    r, g, b = a[:, :, 0], a[:, :, 1], a[:, :, 2]

    # 1) 掩白色文字
    white = (r > 205) & (g > 205) & (b > 205) \
            & (np.abs(r - g) < 18) & (np.abs(g - b) < 18)
    # 取亮度图做中值, 同时给出"非白"区域的可靠亮度
    lum = 0.299 * r + 0.587 * g + 0.114 * b

    print('  掩掉白字像素 %.2f%%' % (white.mean() * 100))

    # 2) 中值滤波抹细线 + 填白洞
    #    用 PIL 的 MedianFilter (C 实现, 比 numpy 滑动窗快得多)
    li = Image.fromarray(np.clip(lum, 0, 255).astype(np.uint8), 'L')
    med = li.filter(ImageFilter.MedianFilter(size=9))
    lum2 = np.array(med).astype(np.float32)
    # 白字位置用中值结果填
    lum2 = np.where(white, lum2, lum)

    # 3) 保留颜色: 用原始彩色除以原始亮度得到色度, 再乘以清理后的亮度
    eps = 1e-3
    scale = lum2 / np.maximum(lum, eps)
    scale = np.clip(scale, 0.0, 3.0)
    out = a * scale[:, :, None]

    # 4) 旋转对齐
    img = Image.fromarray(np.clip(out, 0, 255).astype(np.uint8))
    # PIL 的 rotate 是逆时针为正; 图像坐标 y 向下, 故角度取反
    img = img.rotate(-rotate_deg, resample=Image.BICUBIC,
                     center=CENTER_PX, fillcolor=(0, 0, 0))
    img.save(OUT_TEX, quality=93)
    print('  已写 %s  %dx%d' % (OUT_TEX, img.size[0], img.size[1]))

    # 5) 元数据: 供 C++ 计算贴图在场景里的尺寸与偏移
    h, w = img.size[1], img.size[0]
    meta = {
        'imagePx': w,
        'lyPerPx': LY_PER_PX,
        'sceneUnitsPerPx': LY_PER_PX / 528.5,
        'sunPx': SUN_PX,
        'centerPx': CENTER_PX,
        'rotateDeg': rotate_deg,
        'source': 'NASA/JPL-Caltech/ESO/R. Hurt, PIA/STScI-01H8PJ6083PCF4KJ1AYKVVRX8Q',
    }
    with open(OUT_META, 'w', encoding='utf-8') as f:
        for k, v in meta.items():
            f.write('%s = %s\n' % (k, v))
    print('  已写 %s' % OUT_META)
    return meta


def main():
    delta, err = solve()
    print('=== 最佳旋转角 δ = %.2f°  (平均失配 %.1f°)' % (delta, err))
    print()
    print('  各半径处的对齐情况:')
    for r in sorted(MEASURED):
        arms = [(a + delta) % 360 for a in model_arm_angles(r)]
        peaks = MEASURED[r]
        pairs = []
        for pang, pw in peaks[:3]:
            d = min(abs(ang_diff(pang, a)) for a in arms)
            pairs.append('%.0f°→偏%.0f°' % (pang, d))
        print('    r=%5d ly  模型臂 %s'
              % (r, ' '.join('%.0f' % a for a in arms)))
        print('              实测峰 %s' % '  '.join(pairs))

    if '--build' in sys.argv:
        print()
        print('=== 生成叠加贴图')
        build_overlay(delta)


if __name__ == '__main__':
    main()
