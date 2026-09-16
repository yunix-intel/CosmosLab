"""清理并修复贴图：删除非地图、填补 NoData 黑区。

★★ 三类问题与对应处置（本脚本的核心逻辑）:

  **类型 1: 根本不是表面图** -> 删除
    实测踩到的：
      * albedo_churyumov.jpg —— "Rosetta's Comet" 实为**发现照片**
        （左图彗星是个小点，右图是星场位置示意）
      * albedo_deimos.jpg（旧）—— **两张半球照 + 比例尺**的拼图
    判据：黑色占比 > 55% 且有效像素呈**多个分离块**（不是单一条带）。

  **类型 2: 条带型圆柱地图**（JPL/USGS 系列）-> 边缘延拓
    影像只覆盖部分纬度（探测器只拍了被照亮的半球），
    上下是**真实的未测绘区**，呈纯黑。
    地理上正确，只是缺数据 -> 用最近有效行延拓填满。
    判据：黑区集中在顶部和/或底部，有效区是**单块连续的横向条带**。

  **类型 3: 标准等距柱状** -> 无需处理

★ 如何区分类型 1 与 2（这是关键）:
  看**有效像素的连通性**。条带型只有 1 个连通块（从最左到最右连续）；
  拼图型有 2 个以上分离的块（两张照片各算一块）。
  用"每行有效像素占比"的分布判断：条带型在中间行接近 100%，
  拼图型任何一行都到不了 100%（两张图之间有间隔）。

用法:
    python clean_textures.py --check <目录>   只报告分类
    python clean_textures.py <目录>           执行（删除类 1，延拓类 2）
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


def classify(a, name=''):
    """返回 (类别, 黑区占比, 说明)。"""
    h, w = a.shape[:2]
    m = black_mask(a)
    frac = m.mean()

    if frac < 0.005:
        return (3, frac, '标准等距柱状')

    # ★★ 先用**文件名白名单**排除已知的正确来源。
    #
    #   初版纯靠像素统计判断，把 Ariel/Oberon/Titania/Umbriel/Miranda
    #   全判成"拼图"要删除 —— 但它们是 **JPL/USGS 的圆柱地图**，
    #   黑带是真实未测绘纬度，地理上是正确的（已目视确认）。
    #
    #   教训：像素统计无法区分"未测绘黑带"与"照片拼图的间隙"，
    #   因为它们看起来都是黑。**来源比像素更可靠** —— 已知的
    #   制图机构系列产品直接放行。
    KNOWN_MAPS = ('map jpl usgs', 'jpl usgs', 'viking', 'usgs',
                  'controlled', 'photomosaic', 'mosaic', 'simple_cyl',
                  'simplecyl', 'cylindrical')
    low = name.lower()
    if any(k in low for k in KNOWN_MAPS):
        return (2, frac, '已知制图机构的圆柱地图')

    # 每行有效像素占比
    rowfill = 1.0 - m.mean(axis=1)
    # 条带型: 中间区域应接近满行
    mid = rowfill[h // 3: 2 * h // 3]
    mid_max = mid.max() if mid.size else 0.0
    mid_mean = mid.mean() if mid.size else 0.0

    # 黑区是否集中在上下两端?
    prof = m.mean(axis=1)
    top_heavy = prof[:h // 3].mean()
    bot_heavy = prof[-h // 3:].mean()
    mid_black = prof[h // 3: 2 * h // 3].mean()
    edge_concentrated = (top_heavy + bot_heavy) / 2.0 > 0.25 and mid_black < 0.10

    if mid_max > 0.985 and mid_mean > 0.85:
        return (2, frac, '条带型圆柱地图 (有未测绘黑带)')
    if edge_concentrated:
        return (2, frac, '条带型 (黑区在上下)')
    return (1, frac, '非表面图 (拼图/照片)')


def extend(a):
    """逐列向上/向下延伸最近的有效像素。"""
    h, w = a.shape[:2]
    out = a.copy()

    m = black_mask(out)
    for x in range(w):
        col = m[:, x]
        if not col.any():
            continue
        top = 0
        while top < h and col[top]:
            top += 1
        if 0 < top < h:
            out[:top, x] = a[top, x]

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

    # 残余的内部小洞: 横向延拓
    m3 = black_mask(out)
    if m3.any():
        for y in range(h):
            row = m3[y]
            if not row.any() or row.all():
                continue
            idx = np.where(~row)[0]
            first, last = idx[0], idx[-1]
            out[y, :first] = out[y, first]
            out[y, last + 1:] = out[y, last]

    return out


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

    rm, fx = [], []
    for p in files:
        if not os.path.isfile(p) or p.endswith('.bak'):
            continue
        try:
            a = np.array(Image.open(p).convert('RGB'))
        except Exception as e:
            print('%-24s 读取失败: %s' % (os.path.basename(p), e))
            continue
        kind, frac, desc = classify(a, os.path.basename(p))
        if kind == 1:
            print('%-24s 黑 %5.1f%%  [删除] %s' % (os.path.basename(p), frac * 100, desc))
            rm.append(p)
        elif kind == 2:
            print('%-24s 黑 %5.1f%%  [延拓] %s' % (os.path.basename(p), frac * 100, desc))
            fx.append(p)

    print()
    print('待删除 %d 个, 待延拓 %d 个' % (len(rm), len(fx)))

    if dry:
        return

    for p in rm:
        os.remove(p)
        print('  已删除 %s' % os.path.basename(p))

    for p in fx:
        bak = p + '.bak'
        if not os.path.isfile(bak):
            shutil.copy(p, bak)
        a = np.array(Image.open(p).convert('RGB'))
        out = extend(a)
        Image.fromarray(out).save(p, quality=94)
        print('  已延拓 %s' % os.path.basename(p))


if __name__ == '__main__':
    main()
