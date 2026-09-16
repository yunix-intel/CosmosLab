"""修复等距柱状贴图的极区坏行（白带 / 黑带 / NoData 填充）。

★★ 问题现象:
  渲染出的行星两极发白、发糊，像蒙了一层雾。
  根因是贴图最上/最下若干行是**无效数据**（白带或黑带），
  球面 UV 会把这些行集中到极点周围的小圆盘上，形成明显的亮斑/暗斑。

  实测: albedo_mars.jpg 最上行 100% 纯白；albedo_charon.jpg 顶部 85 行纯黑。

★ 为什么不能简单裁掉:
  等距柱状贴图必须覆盖完整 -90°..+90°。裁掉顶部等于把极区削平，
  极点贴图会塌陷，反而产生更明显的畸变。正确做法是**重建极点行**。

★★ 关键教训（第一版为什么失败）:
  第一版取"坏带之下 4 行的逐列均值"作参考。但那 4 行**本身仍带白带**
  —— 实测 Mars 在第 27 行仍有 57% 白像素。于是重建出来的极点行
  依然是白的，等于没修。

  正解：先用**逐列扫描**找到每列真正干净的第一行，再据此重建。
  因为白带的深度在不同经度上不一样（赤道附近窄、极区宽），
  用统一的行号必然取到脏数据。

★ 重建策略:
  1. 对每一列独立地找"该列第一个非坏像素"的行号 y_clean[x]
  2. 参考值取该列在 y_clean[x] 附近若干行的**中位数**（中值抗噪）
  3. 极点行向"该纬度圈的均值"收敛 —— 极点物理上是所有经度的汇聚点，
     保留列间差异会画出放射状条纹

用法:
    python fix_poles.py --check <目录或文件...>   只报告
    python fix_poles.py <目录或文件...>            就地修复（自动备份 .bak）
"""
import glob
import os
import shutil
import sys

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None

WHITE = 238
BLACK = 14
BAD_PIX = 0.65         # 单列判定"坏像素"的…不需要; 逐列按像素值判定
MAX_DEPTH = 0.08       # 单侧最多重建高度比例
NEED_BAD_FRAC = 0.50   # 一行中坏像素占比超过此值才算坏行


def bad_mask(a):
    """逐像素标记"极端值"(疑似 NoData)。"""
    r, g, b = a[:, :, 0], a[:, :, 1], a[:, :, 2]
    white = (r > WHITE) & (g > WHITE) & (b > WHITE)
    black = (r < BLACK) & (g < BLACK) & (b < BLACK)
    return white | black


def row_is_bad(mask, y):
    return mask[y].mean() > NEED_BAD_FRAC


def depth(mask):
    """返回 (顶部坏行数, 底部坏行数)。"""
    h = mask.shape[0]
    lim = max(1, int(h * MAX_DEPTH))
    top = 0
    while top < lim and row_is_bad(mask, top):
        top += 1
    bot = 0
    while bot < lim and row_is_bad(mask, h - 1 - bot):
        bot += 1
    return top, bot


def col_clean_row(mask, x, top_hint, from_top):
    """★ 逐列找该列第一个非坏像素的行号。"""
    h = mask.shape[0]
    if from_top:
        y = top_hint
        while y < h and mask[y, x]:
            y += 1
        return min(y, h - 1)
    y = h - 1 - top_hint
    while y >= 0 and mask[y, x]:
        y -= 1
    return max(y, 0)


def rebuild(a, top, bot, band=5):
    """逐列重建极区坏行。"""
    h, w = a.shape[:2]
    mask = bad_mask(a)
    out = a.copy()

    # ---- 顶部 ----
    if top > 0:
        ref = np.zeros((w, 3), dtype=np.float64)
        for x in range(w):
            y0 = col_clean_row(mask, x, top, True)
            lo = max(0, y0)
            hi = min(h, y0 + band)
            if hi <= lo:
                hi = min(h, lo + 1)
            # 中值抗噪（个别残留坏像素不会污染整列参考）
            ref[x] = np.median(a[lo:hi, x].astype(np.float64), axis=0)

        ring_mean = ref.mean(axis=0)
        for y in range(top):
            # t: 1 -> 贴着干净数据的边界; 0 -> 极点
            t = (y + 1) / float(top + 1)
            out[y] = ref * t + ring_mean * (1.0 - t)

    # ---- 底部 ----
    if bot > 0:
        ref = np.zeros((w, 3), dtype=np.float64)
        for x in range(w):
            y0 = col_clean_row(mask, x, bot, False)
            hi = min(h, y0 + 1)
            lo = max(0, hi - band)
            if hi <= lo:
                lo = max(0, hi - 1)
            ref[x] = np.median(a[lo:hi, x].astype(np.float64), axis=0)

        ring_mean = ref.mean(axis=0)
        for k in range(bot):
            y = h - 1 - k
            t = (k + 1) / float(bot + 1)
            out[y] = ref * t + ring_mean * (1.0 - t)

    return out


def process(path, dry=False):
    im = Image.open(path).convert('RGB')
    a = np.array(im)
    mask = bad_mask(a)
    top, bot = depth(mask)
    if top == 0 and bot == 0:
        return (path, 0, 0, False)

    if dry:
        return (path, top, bot, True)

    bak = path + '.bak'
    if not os.path.isfile(bak):
        shutil.copy(path, bak)

    out = rebuild(a, top, bot)
    Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).save(path, quality=94)
    return (path, top, bot, True)


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
            path, top, bot, changed = process(p, dry)
        except Exception as e:
            print('%-26s 失败: %s' % (os.path.basename(p), e))
            continue
        if changed:
            print('%-26s 上 %3d 下 %3d  %s'
                  % (os.path.basename(p), top, bot, '需修复' if dry else '已修复'))
            n += 1
    print()
    print('共 %d 个文件%s' % (n, ' 需要修复' if dry else ' 已修复'))


if __name__ == '__main__':
    main()
