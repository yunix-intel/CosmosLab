"""图像亮度分析 —— 量化过曝程度。

用法:
    python analyze.py <图1> [图2] ...

输出每张图的:
  * 平均亮度 / 中位亮度
  * 纯白(>=250)像素占比  —— 过曝的硬指标
  * 高光(>=240)像素占比
  * 中心 40% 区域的纯白占比  —— 排除 UI 面板干扰, 只看画面主体
"""
import sys
from PIL import Image, ImageStat


def analyze(path: str) -> None:
    im = Image.open(path).convert("RGB")
    w, h = im.size

    g = im.convert("L")
    stat = ImageStat.Stat(g)
    mean = stat.mean[0]
    median = sorted(g.getdata())[len(g.getdata()) // 2]

    hist = g.histogram()
    total = w * h
    white = sum(hist[250:]) / total * 100.0
    bright = sum(hist[240:]) / total * 100.0

    # 中心 40% 区域 (画面主体, 避开左上/右下 UI 面板)
    cx0, cy0 = int(w * 0.30), int(h * 0.20)
    cx1, cy1 = int(w * 0.70), int(h * 0.80)
    cg = im.crop((cx0, cy0, cx1, cy1)).convert("L")
    chist = cg.histogram()
    ctotal = cg.size[0] * cg.size[1]
    cwhite = sum(chist[250:]) / ctotal * 100.0
    cbright = sum(chist[240:]) / ctotal * 100.0

    print(f"{path}")
    print(f"  尺寸 {w}x{h}   平均亮度 {mean:6.2f}   中位 {median}")
    print(f"  全图:  纯白(>=250) {white:5.2f}%   高光(>=240) {bright:5.2f}%")
    print(f"  中心:  纯白(>=250) {cwhite:5.2f}%   高光(>=240) {cbright:5.2f}%")
    print()


if __name__ == "__main__":
    for p in sys.argv[1:]:
        try:
            analyze(p)
        except Exception as e:
            print(f"{p}: 失败 {e}\n")
