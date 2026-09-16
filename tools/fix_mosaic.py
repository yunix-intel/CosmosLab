"""把"带黑色背景的镶嵌图"转成可用的等距柱状贴图。

★★ 问题本质:
  部分天体的"全球图"其实是**航天器影像镶嵌图**，形状是**不规则的实测覆盖区**，
  四周（有时还是中间）是黑色背景。实测覆盖率只有 30–40%：
      天王星五颗大卫星 (Ariel/Miranda/Oberon/Titania/Umbriel)  57–62% 是黑
      Charon 33%、Pluto 30%、67P 69%、Deimos 61%

★ 之前试过的错误做法 —— 必须记下来:
  写了个"重建极区坏行"的工具，把大块黑区当成 NoData 逐行重建。
  结果对**标准等距柱状**的图（Mars、Enceladus）有效，
  但对**镶嵌图**完全跑偏：那些黑区不在极区，而在大片中低纬度，
  "重建"等于把黑色涂成从邻近行复制的颜色 —— 把真实的黑斑
  （如 Charon 的暗极）也一起抹掉了。**全部回退。**

★ 正确做法（本工具）:
  既然实测区是**不规则的一坨**，就把它**内缩并居中**，
  再用边缘延拓（edge-extend）填满整幅等距柱状。
  代价是贴图内容被放大了一点（相当于相机靠近了），
  但这比"球面上有一大块纯黑"或"把黑区涂成假颜色"都诚实得多。

  步骤:
    1. 求实测区的**紧致包围盒**（bounding box of non-NoData pixels）
    2. 裁剪到该包围盒
    3. 用缩放到目标尺寸（保持经度方向不变形：纵向拉伸到 2:1）
    4. 对残余的小块 NoData（包围盒内部的洞）做**邻域填充**
       —— 用最近有效像素扩散填补，避免留下黑斑

用法:
    python fix_mosaic.py --check <目录>     报告覆盖率
    python fix_mosaic.py <目录>             处理（自动备份 .bak）
"""
import glob
import os
import shutil
import sys

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None

# "无效像素"判据: 近黑 (NoData 背景)
BLACK = 16
COVER_MIN = 0.35        # 实测覆盖低于此比例则判定为"镶嵌图"


def valid_mask(a):
    """有效像素掩码 = 非近黑。"""
    return ~((a[:, :, 0] < BLACK) & (a[:, :, 1] < BLACK) & (a[:, :, 2] < BLACK))


def bbox_of(mask, pad=2):
    ys, xs = np.where(mask)
    if ys.size == 0:
        return None
    y0, y1 = max(0, ys.min() - pad), min(mask.shape[0], ys.max() + 1 + pad)
    x0, x1 = max(0, xs.min() - pad), min(mask.shape[1], xs.max() + 1 + pad)
    return (x0, y0, x1, y1)


def fill_holes(a, mask):
    """用最近有效像素的均值填补掩码内的洞（多次迭代扩散）。"""
    out = a.astype(np.float64).copy()
    m = mask.copy()
    for _ in range(40):
        if m.all():
            break
        # 4 邻域均值
        acc = np.zeros_like(out)
        cnt = np.zeros(m.shape, dtype=np.float64)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            src_v = np.roll(m, (dy, dx), axis=(0, 1))
            src_c = np.roll(out, (dy, dx), axis=(0, 1))
            acc += src_c * src_v[:, :, None]
            cnt += src_v
        newly = (~m) & (cnt > 0)
        if not newly.any():
            break
        out[newly] = acc[newly] / cnt[newly][:, None]
        m |= newly
    return out


def process(path, dry=False, target_w=2048):
    im = Image.open(path).convert('RGB')
    a = np.array(im)
    h, w = a.shape[:2]
    mask = valid_mask(a)
    cover = mask.mean()

    if cover >= 1.0 - 1e-9:
        return (path, cover, '无需处理', False)
    if cover < COVER_MIN:
        return (path, cover, '覆盖率过低', False)

    if dry:
        return (path, cover, '需处理', True)

    bak = path + '.bak'
    if not os.path.isfile(bak):
        shutil.copy(path, bak)

    bb = bbox_of(mask)
    x0, y0, x1, y1 = bb
    sub = a[y0:y1, x0:x1]
    sub_mask = mask[y0:y1, x0:x1]

    # 先用邻域扩散填补包围盒内的洞
    filled = fill_holes(sub, sub_mask)

    # 缩放到 2:1 的目标尺寸
    out_h = target_w // 2
    img = Image.fromarray(np.clip(filled, 0, 255).astype(np.uint8))
    img = img.resize((target_w, out_h), Image.LANCZOS)
    img.save(path, quality=94)

    return (path, cover, '已处理 -> %dx%d' % (target_w, out_h), True)


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
            path, cover, msg, changed = process(p, dry)
        except Exception as e:
            print('%-24s 失败: %s' % (os.path.basename(p), e))
            continue
        if msg != '无需处理':
            print('%-24s 覆盖 %5.1f%%  %s' % (os.path.basename(p), cover * 100, msg))
            if changed:
                n += 1
    print()
    print('共 %d 个文件%s' % (n, ' 需要处理' if dry else ' 已处理'))


if __name__ == '__main__':
    main()
