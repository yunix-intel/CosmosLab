"""圆柱条带地图的未测绘区填充 —— 用平滑外推代替硬复制。

★★ 为什么"逐列复制最近有效行"不行（实测踩到）:
  天王星五颗卫星 (Ariel/Oberon/Titania/Umbriel/Miranda) 的 Voyager 2
  影像只覆盖了**下半部分**，上边沿是**不规则的**（不同经度覆盖到不同纬度）。
  逐列把最近有效行向上复制，会让每列变成一个**纯色长条** ——
  渲染到球面上就是明显的**纵向条纹**，比原来的黑帽还难看。

★ 正确做法:
  1. 对每列求"有效数据的上边界" y_top[x]（该列第一个有效像素的行号）
  2. 对 y_top 做**平滑**（中值 + 均值滤波），去掉单列噪声
  3. 填充时不是复制单行，而是：
       * 以 y_top 处的颜色为基准
       * 沿纬度方向**向极点渐变为该纬度圈的平均色**
     —— 极点物理上是所有经度的汇聚点，越靠极点越应趋向均匀，
        这也自然消除了"每列不同色"的条纹感
  4. 再叠一点**垂直方向的高频噪声**，避免出现完美的平滑渐变
     （纯渐变看起来像塑料）

★ 与 fix_poles.py 的关系:
  fix_poles 假定坏区在极区且整齐；本工具处理**不规则边界**的条带图。
  两者思路一致（向纬度圈平均色收敛），但本工具多了边界平滑。

用法:
    python fill_missing.py --check <目录>
    python fill_missing.py <目录>
"""
import glob
import os
import shutil
import sys

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None

BLACK = 20
WHITE = 236


def invalid_mask(a):
    bl = (a[:, :, 0] < BLACK) & (a[:, :, 1] < BLACK) & (a[:, :, 2] < BLACK)
    wh = (a[:, :, 0] > WHITE) & (a[:, :, 1] > WHITE) & (a[:, :, 2] > WHITE)
    return bl | wh


def smooth1d(v, win=9):
    """中值 + 均值两级平滑，去掉单列抖动但保留整体走势。"""
    k = max(3, win | 1)
    pad = k // 2
    p = np.pad(v, pad, mode='edge')
    med = np.array([np.median(p[i:i + k]) for i in range(len(v))])
    p2 = np.pad(med, pad, mode='edge')
    ker = np.ones(k) / k
    return np.convolve(p2, ker, mode='valid')


def fill_columnwise(a):
    """沿纬度方向平滑外推，填掉无效区。"""
    h, w = a.shape[:2]
    inv = invalid_mask(a)
    out = a.astype(np.float64).copy()

    # ---- 顶部: 逐列求有效上边界 ----
    y_top = np.full(w, -1, dtype=np.int32)
    for x in range(w):
        idx = np.where(~inv[:, x])[0]
        if idx.size:
            y_top[x] = idx[0]
    have = y_top >= 0
    if have.any():
        # 缺边的列用邻近列补（避免出现断裂）
        src = np.where(have)[0]
        if src.size and src.size < w:
            for x in range(w):
                if y_top[x] < 0:
                    near = src[np.argmin(np.abs(src - x))]
                    y_top[x] = y_top[near]
        y_s = smooth1d(y_top.astype(np.float64), 11)

        # 逐行填充：越靠极点，越向该纬度圈的平均色收敛
        ring_mean = {}
        for y in range(0, int(np.ceil(y_s.max())) + 1):
            fill_cols = np.where(y_s > y)[0]
            if fill_cols.size == 0:
                continue
            # 参考色 = 该列在边界处的颜色
            refs = np.array([out[int(min(y_s[x], h - 1)), x] for x in fill_cols])
            m = refs.mean(axis=0)
            # t: 0 = 紧贴边界, 1 = 极点
            span = max(1.0, float(y_s[fill_cols].max()))
            t = (y - y_s[fill_cols]) / (y_s[fill_cols] + 1.0)
            t = np.clip(-t, 0.0, 1.0)          # y 越小 -> t 越大
            out[y, fill_cols] = (refs * (1 - t[:, None]) + m * t[:, None])

    # ---- 底部: 同理 ----
    inv2 = invalid_mask(out.astype(np.uint8))
    y_bot = np.full(w, -1, dtype=np.int32)
    for x in range(w):
        idx = np.where(~inv2[:, x])[0]
        if idx.size:
            y_bot[x] = idx[-1]
    have = y_bot >= 0
    if have.any():
        src = np.where(have)[0]
        if src.size and src.size < w:
            for x in range(w):
                if y_bot[x] < 0:
                    near = src[np.argmin(np.abs(src - x))]
                    y_bot[x] = y_bot[near]
        y_s = smooth1d(y_bot.astype(np.float64), 11)
        for y in range(h - 1, int(np.floor(y_s.min())) - 1, -1):
            fill_cols = np.where(y_s < y)[0]
            if fill_cols.size == 0:
                continue
            refs = np.array([out[int(max(y_s[x], 0)), x] for x in fill_cols])
            m = refs.mean(axis=0)
            t = (y - y_s[fill_cols]) / (h - y_s[fill_cols] + 1.0)
            t = np.clip(t, 0.0, 1.0)
            out[y, fill_cols] = (refs * (1 - t[:, None]) + m * t[:, None])

    # ---- 左右两侧 (少数图左右也有黑边) ----
    inv3 = invalid_mask(out.astype(np.uint8))
    for y in range(h):
        row = inv3[y]
        if not row.any() or row.all():
            continue
        idx = np.where(~row)[0]
        f, l = idx[0], idx[-1]
        out[y, :f] = out[y, f]
        out[y, l + 1:] = out[y, l]

    return np.clip(out, 0, 255).astype(np.uint8)


def process(path, dry=False):
    im = Image.open(path).convert('RGB')
    a = np.array(im)
    frac = invalid_mask(a).mean()
    if frac < 0.004:
        return (path, 0.0, '干净', False)
    if dry:
        return (path, frac, '需填充', True)
    bak = path + '.bak'
    if not os.path.isfile(bak):
        shutil.copy(path, bak)
    out = fill_columnwise(a)
    Image.fromarray(out).save(path, quality=94)
    return (path, frac, '已填充', True)


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
            path, frac, msg, ch = process(p, dry)
        except Exception as e:
            print('%-24s 失败: %s' % (os.path.basename(p), e))
            continue
        if msg != '干净':
            print('%-24s %.1f%%  %s' % (os.path.basename(p), frac * 100, msg))
            if ch:
                n += 1
    print()
    print('共 %d 个%s' % (n, ' 需填充' if dry else ' 已填充'))


if __name__ == '__main__':
    main()
