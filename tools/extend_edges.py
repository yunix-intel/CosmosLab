"""给圆柱投影条带贴图做"边缘延拓"，填掉未测绘纬度的黑带。

★★ 适用对象 —— 必须先分清两种情况:

  情况 A: **条带型圆柱地图**（JPL/USGS 系列，如 Ariel/Oberon/Titania 等）
    影像只覆盖了部分纬度（Voyager 2 只拍了被照亮的那一极），
    上下是**真实的未测绘区**，呈纯黑。
    这类图**地理上是正确的**，只是缺数据。
    -> 处理: 用最近的有效行**边缘延拓**填满，让球面上不出现黑帽。
       这是"把已知纬度圈的外观向未知极区延续"，是制图上的常规做法
       （比涂成随机噪声诚实，也比留黑帽好看）。

  情况 B: **拼图型**（如旧版 Deimos 是"两张半球照 + 比例尺"）
    根本不是地图，无法修复。
    -> 处理: 换掉，不用本工具。

★ 为什么不能用"逐行重建极区"的老办法:
  老办法假定坏带只在最上/最下若干行。但条带图的黑带**上下深度不同**，
  而且中间可能有零散小洞。必须**逐列**扫描。

★ 与 fix_poles.py 的区别:
  fix_poles 做的是"符合极区物理的收敛"（向整圈均值收敛，避免放射条纹）。
  本工具做的是**纯边缘延拓**（直接复制最近有效行），
  适合"未测绘"这种"不知道"的情况 —— 不臆造纬度变化。

用法:
    python extend_edges.py --check <目录>   报告黑带深度
    python extend_edges.py <目录>           处理（自动备份 .bak）
"""
import glob
import os
import shutil
import sys

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None

BLACK = 16


def black_mask(a):
    return (a[:, :, 0] < BLACK) & (a[:, :, 1] < BLACK) & (a[:, :, 2] < BLACK)


def extend(a):
    """逐列向上/向下延伸最近的有效像素。"""
    h, w = a.shape[:2]
    m = black_mask(a)
    out = a.copy()

    # ---- 向上延伸 ----
    for x in range(w):
        col = m[:, x]
        if not col.any():
            continue
        # 找到该列顶部连续黑区
        top = 0
        while top < h and col[top]:
            top += 1
        if 0 < top < h:
            out[:top, x] = a[top, x]

    # ---- 向下延伸 (用更新后的 out 判定, 避免顶部延伸影响) ----
    m2 = black_mask(out)
    for x in range(w):
        col = m2[:, x]
        if not col.any():
            continue
        bot = 0
        while bot < h and col[h - 1 - bot]:
            bot += 1
        if 0 < bot < h:
            out[h - bot:, x] = out[h - 1 - bot, x]

    return out


def process(path, dry=False):
    im = Image.open(path).convert('RGB')
    a = np.array(im)
    m = black_mask(a)
    frac = m.mean()
    if frac < 0.005:
        return (path, frac, '无需处理', False)
    if dry:
        return (path, frac, '需延拓', True)

    bak = path + '.bak'
    if not os.path.isfile(bak):
        shutil.copy(path, bak)
    out = extend(a)
    Image.fromarray(out).save(path, quality=94)
    return (path, frac, '已延拓', True)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    args = [x for x in sys.argv[1:] if not x.startswith('--')]
    dry = '--check' in sys.argv

    files = []
    for x in args:
        if os.path.isdir(x):
            files += sorted(glob.glob(os.path.join(x, 'albedo_*')))
        else:
            files.append(x)

    n = 0
    for p in files:
        if not os.path.isfile(p) or p.endswith('.bak'):
            continue
        try:
            path, frac, msg, changed = process(p, dry)
        except Exception as e:
            print('%-24s 失败: %s' % (os.path.basename(p), e))
            continue
        if msg != '无需处理':
            print('%-24s 黑区 %5.1f%%  %s' % (os.path.basename(p), frac * 100, msg))
            if changed:
                n += 1
    print()
    print('共 %d 个文件%s' % (n, ' 需要处理' if dry else ' 已处理'))


if __name__ == '__main__':
    main()
