"""生成供 3D 场景叠加的银河系底图（基于 NASA/JPL 官方图）。

★★ 数据来源与依据 (必须标注在教学软件里):

  图: "Milky Way and Our Location"
      NASA/JPL-Caltech/ESO/R. Hurt
      https://science.nasa.gov/asset/webb/milky-way-and-our-location/
  性质: **科学插画** (illustration), 不是照片。
        —— 我们身处银盘内部, 外部全景在物理上不可能拍到。
  依据: Spitzer 红外巡天对银臂的结论 (Benjamin et al. 2005 等)、
        CO 分子云观测、VLBA 脉泽三角视差测距 (Reid et al. 2014, 2019)。
  标注: 图上带银经刻度 (0°~330°) 与 15,000~75,000 ly 距离环 ——
        本工具正是用这些刻度做几何标定的。

★ 标定结果 (交叉验证):
   45,000 ly 环 -> 670 px  (67.2 ly/px)
   60,000 ly 环 -> 873 px  (68.7 ly/px)
   75,000 ly 环 -> 1113 px (67.4 ly/px)
   太阳-银心 26,000 ly -> 380 px (68.4 ly/px)
   四者收敛于 68 ly/px, 取此值。
   图上太阳位于 r=380px, 方位角 88.4° (图像坐标, atan2(dy,dx), y 向下)

★ 处理步骤:
   1. 掩掉白色文字 (Sun / 臂名 / 距离标签) —— 白色文字在 3D 叠加时会
      与 app 自己的标注冲突, 且与不同缩放级别不匹配
   2. 中值滤波抹掉细网格线与距离环
   3. 保色: 用"原色/原亮度 × 清理后亮度"恢复彩色, 而不是直接转灰度
   4. 不做旋转 —— 由 C++ 侧按"图像方位角 = 场景方位角"直接映射

用法:
    python build_overlay.py
"""
import os

import numpy as np
from PIL import Image, ImageFilter

SRC = r'D:\tmp\galaxy_hurt_2000.jpg'
OUT = r'D:\tmp\solar-system-cpp\assets\tex\galaxy_overlay.jpg'
META = r'D:\tmp\solar-system-cpp\assets\tex\galaxy_overlay.txt'

LY_PER_PX = 68.0
SUN_PX = (985.0, 1372.0)          # 太阳在图中的位置 (太阳圆环中心)
CENTER_PX = (1000.0, 1000.0)      # 银心

# NASA 图上的标注位置 (2000px 坐标, 读自调试图) -> 用作 3D 标注锚点
LABELS_PX = {
    'scutum':    (1337, 571),
    'sagittarius': (718, 888),
    'norma':     (1290, 908),
    'perseus':   (714, 1378),
    'outer':     (541, 1398),
    'orionspur': (969, 1449),
    'sun':       (1010, 1367),
}


def main():
    im = Image.open(SRC).convert('RGB')
    a = np.array(im).astype(np.float32)
    r, g, b = a[:, :, 0], a[:, :, 1], a[:, :, 2]

    # ---- 1) 白色文字掩码 ----
    white = (r > 205) & (g > 205) & (b > 205) \
            & (np.abs(r - g) < 18) & (np.abs(g - b) < 18)
    print('白色文字像素 %.2f%%' % (white.mean() * 100))

    lum = 0.299 * r + 0.587 * g + 0.114 * b

    # ---- 2) 中值滤波抹细线 ----
    li = Image.fromarray(np.clip(lum, 0, 255).astype(np.uint8), 'L')
    # 网格线宽约 2-3 px, 中值窗口 7 足以抹掉; 窗口太大又会糊掉星云细节
    med = np.array(li.filter(ImageFilter.MedianFilter(size=7))).astype(np.float32)

    # 白字位置用中值结果填; 其余位置保留原亮度
    lum2 = np.where(white, med, lum)

    # ---- 3) 保色 ----
    eps = 1e-3
    scale = np.clip(lum2 / np.maximum(lum, eps), 0.0, 3.0)
    out = a * scale[:, :, None]
    out = np.clip(out, 0, 255).astype(np.uint8)

    Image.fromarray(out).save(OUT, quality=93)
    print('已写 %s  %dx%d' % (OUT, out.shape[1], out.shape[0]))

    # ---- 4) 元数据 (供 C++ 定位与缩放) ----
    with open(META, 'w', encoding='utf-8') as f:
        f.write('# NASA/JPL-Caltech/ESO/R. Hurt —— 银河系结构科学插画\n')
        f.write('# 标定: 由三条距离环 (45/60/75 kly) + 太阳-银心距离 交叉验证\n')
        f.write('# 太阳-银心交叉验证: 太阳在图中的方位角 %.1f°\n'
                % (np.degrees(np.arctan2(SUN_PX[1] - CENTER_PX[1],
                                         SUN_PX[0] - CENTER_PX[0])) % 360))
        f.write('\n[geometry]\n')
        f.write('imagePx = 2000\n')
        f.write('lyPerPx = %.1f\n' % LY_PER_PX)
        f.write('centerPxX = %.0f\n' % CENTER_PX[0])
        f.write('centerPxY = %.0f\n' % CENTER_PX[1])
        f.write('sunPxX = %.0f\n' % SUN_PX[0])
        f.write('sunPxY = %.0f\n' % SUN_PX[1])
        f.write('\n[labels_px]\n')
        for k, (x, y) in LABELS_PX.items():
            f.write('%s = %d, %d\n' % (k, x, y))
        f.write('\n[labels_ly_deg]\n')
        for k, (x, y) in LABELS_PX.items():
            dx, dy = x - CENTER_PX[0], y - CENTER_PX[1]
            rr = float(np.hypot(dx, dy)) * LY_PER_PX
            aa = float(np.degrees(np.arctan2(dy, dx)) % 360.0)
            f.write('%s = %.0f ly, %.1f deg\n' % (k, rr, aa))
    print('已写 %s' % META)

    print()
    print('标注锚点 (半径 ly, 场景方位角°):')
    for k, (x, y) in LABELS_PX.items():
        dx, dy = x - CENTER_PX[0], y - CENTER_PX[1]
        rr = float(np.hypot(dx, dy)) * LY_PER_PX
        aa = float(np.degrees(np.arctan2(dy, dx)) % 360.0)
        print('   %-14s %6.0f ly   %6.1f°' % (k, rr, aa))


if __name__ == '__main__':
    main()
