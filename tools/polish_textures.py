"""贴图最终修复：裁掉海报边框 + 边缘延拓填充未测绘区。

★★ 三类问题，三种处置:

  1. **带标题栏/坐标轴的海报** (如 Enceladus 的 USGS 图)
     -> 用"每行/每列有效像素占比"找出**主体图像区**并裁切。
        海报的特征: 四周有白/黑边框, 边框内有文字与刻度。
        主体区的判据: 连续一大片区域, 其每行有效占比接近满行。

  2. **圆柱条带地图的未测绘黑带** (Ariel/Oberon/Bennu/Vesta/...)
     -> 逐列**边缘延拓**: 复制最近的有效像素行。
        这是制图上的常规做法（把已知纬度圈的外观延续到未知极区），
        比留黑帽好看，也比臆造噪声诚实。

  3. **不规则镶嵌** (Charon)
     -> 先裁到紧致包围盒, 再边缘延拓 + 残余洞邻域填充。

★ 关于"是否应该删掉":
  只有**根本不是表面图**的才删（如 67P 的发现照片、旧 Deimos 的
  双半球拼图）。条带地图即使黑区占 60% 也**必须保留** ——
  黑区是真实的未测绘区，图本身是正确的地理数据。

用法:
    python polish_textures.py --check <目录>
    python polish_textures.py <目录>
"""
import glob
import os
import shutil
import sys

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None

BLACK = 18
WHITE = 236


def content_mask(a):
    """有效内容 = 非纯黑 且 非纯白。"""
    bl = (a[:, :, 0] < BLACK) & (a[:, :, 1] < BLACK) & (a[:, :, 2] < BLACK)
    wh = (a[:, :, 0] > WHITE) & (a[:, :, 1] > WHITE) & (a[:, :, 2] > WHITE)
    return ~(bl | wh)


def find_frame_crop(a):
    """找**海报边框**并返回应裁切的 (x0,y0,x1,y1)。

    ★★ 判据必须基于"有没有纯白"，而不是"有效像素占比低"。
       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    初版用"非纯黑即内容"的占比来找主体区，结果把**条带地图**裁坏了：
       * Bennu 有 17% 纯黑(未测绘纬度)，逐行占比低的区域被当成边框外，
         裁完只剩 173x706 (原 2400x1200)
       * Callisto / Vesta 的边缘有 1-2 像素黑边，也被"裁掉"，
         比例从 2.000 变成 2.178 / 2.428

    真正区分海报与条带地图的是**纯白**：
      * 海报的边框里一定有**白底黑字的标题栏 / 坐标刻度**
      * 条带地图的黑区只有纯黑，不会有成片纯白

    所以判据改为: 该行/列含纯白的比例 > 60%。只有海报的
    标题栏/刻度栏会这样。
    """
    h, w = a.shape[:2]
    wh = (a[:, :, 0] > WHITE) & (a[:, :, 1] > WHITE) & (a[:, :, 2] > WHITE)
    whrow = wh.mean(axis=1)
    whcol = wh.mean(axis=0)

    # 主体区 = 不含大片纯白的行/列
    rt = np.where(whrow < 0.60)[0]
    ct = np.where(whcol < 0.60)[0]
    if rt.size == 0 or ct.size == 0:
        return None

    # 主体必须是**连续的一大块**，且四周确实有白栏被切掉
    y0, y1 = int(rt[0]), int(rt[-1]) + 1
    x0, x1 = int(ct[0]), int(ct[-1]) + 1
    trimmed_y = (y1 - y0) < h * 0.97
    trimmed_x = (x1 - x0) < w * 0.97
    if not (trimmed_y or trimmed_x):
        return None

    # 保守: 四周各留 3 像素余量, 避免把紧邻的刻度线切进主体
    y0 = max(0, y0 + 3); x0 = max(0, x0 + 3)
    y1 = min(h, y1 - 3); x1 = min(w, x1 - 3)
    if y1 - y0 < 32 or x1 - x0 < 64:
        return None
    return (x0, y0, x1, y1)


def extend_edges(a):
    """逐列上下延拓 + 逐行左右延拓 + 残余洞邻域填充。"""
    h, w = a.shape[:2]
    out = a.copy()
    m = ~content_mask(out)        # 需要填的位置 (黑或白)

    # 上下延拓
    for x in range(w):
        col = m[:, x]
        if not col.any():
            continue
        top = 0
        while top < h and col[top]:
            top += 1
        if 0 < top < h:
            out[:top, x] = out[top, x]
    m = ~content_mask(out)
    for x in range(w):
        col = m[:, x]
        if not col.any():
            continue
        bot = 0
        while bot < h and col[h - 1 - bot]:
            bot += 1
        if 0 < bot < h:
            out[h - bot:, x] = out[h - 1 - bot, x]

    # 左右延拓 (处理内部洞的边界)
    m = ~content_mask(out)
    for y in range(h):
        row = m[y]
        if not row.any() or row.all():
            continue
        idx = np.where(~row)[0]
        if idx.size == 0:
            continue
        f, l = idx[0], idx[-1]
        out[y, :f] = out[y, f]
        out[y, l + 1:] = out[y, l]

    # 残余内部洞: 邻域扩散
    m = ~content_mask(out)
    if m.any():
        for _ in range(60):
            if not m.any():
                break
            acc = np.zeros_like(out, dtype=np.float64)
            cnt = np.zeros(m.shape, dtype=np.float64)
            for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                sv = np.roll(~m, (dy, dx), axis=(0, 1))
                sc = np.roll(out, (dy, dx), axis=(0, 1))
                acc += sc * sv[:, :, None]
                cnt += sv
            newly = m & (cnt > 0)
            if not newly.any():
                break
            out[newly] = (acc[newly] / cnt[newly][:, None]).astype(np.uint8)
            m &= ~newly

    return out


def process(path, dry=False):
    im = Image.open(path).convert('RGB')
    a = np.array(im)

    crop = find_frame_crop(a)
    if crop:
        x0, y0, x1, y1 = crop
        if not dry:
            bak = path + '.bak'
            if not os.path.isfile(bak):
                shutil.copy(path, bak)
            sub = a[y0:y1, x0:x1]
            out = extend_edges(sub)
            Image.fromarray(out).save(path, quality=94)
        return (path, '裁边框 %d,%d-%d,%d' % (x0, y0, x1, y1), True)

    m = ~content_mask(a)
    frac = m.mean()
    if frac < 0.004:
        return (path, '干净', False)

    if not dry:
        bak = path + '.bak'
        if not os.path.isfile(bak):
            shutil.copy(path, bak)
        out = extend_edges(a)
        Image.fromarray(out).save(path, quality=94)
    return (path, '延拓 %.1f%%' % (frac * 100), True)


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
            path, msg, changed = process(p, dry)
        except Exception as e:
            print('%-24s 失败: %s' % (os.path.basename(p), e))
            continue
        if changed:
            print('%-24s %s' % (os.path.basename(p), msg))
            n += 1
    print()
    print('共 %d 个文件%s' % (n, ' 需要处理' if dry else ' 已处理'))


if __name__ == '__main__':
    main()
