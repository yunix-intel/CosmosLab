"""把 PDS 的浮点/单波段科学数据产品转成可贴图的三通道图像。

★★ 为什么单独写这个工具:
  PDS 存档的全球图**不是**现成的可见光图片, 而是**辐射定标后的科学数据**:
    * `ceres_albedo_g.tif` —— mode='F' (32 位浮点), 存的是**反照率比值**
    * `ceres_bestmap_c.cub` —— PDS 的 ISIS cube 格式 (需专用库)

  直接 convert('RGB') 会得到全黑 —— 因为浮点值 0.2~1.66 被裁到了 0。

  要得到能看的图, 必须:
    1. **识别 NoData 哨兵值**并置黑
    2. **拉伸动态范围** —— 科学数据为了保真往往压得很窄
       (实测 Ceres 反照率 99% 的像素落在 0.45–0.56,
        不拉伸的话全图是一片均匀灰)
    3. 转成 8 位三通道

★ 关于拉伸方式的取舍:
  * 线性百分位拉伸 (1%–99%) —— 最保守, 不改变相对亮度关系,
    是科学可视化的标准做法。**默认用这个。**
  * 直方图均衡 —— 对比度最好看, 但**会改变相对亮度**,
    可能让暗区看起来和亮区一样亮 —— 在教学材料里这是误导, 故不用。

用法:
    python sci2img.py <输入> <输出> [--nodata 0.5] [--lo 1] [--hi 99]
    python sci2img.py --probe <输入>      只报告统计, 不输出
"""
import sys

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None


def probe(a):
    v = a[np.isfinite(a)]
    print('  形状 %s  dtype %s' % (a.shape, a.dtype))
    print('  范围 min=%.4f max=%.4f mean=%.4f' % (v.min(), v.max(), v.mean()))
    for q in (0.1, 1, 5, 50, 95, 99, 99.9):
        print('    %5.1f%% = %.4f' % (q, np.percentile(v, q)))
    # 检出可能的哨兵值 (某个值出现频率异常高)
    vals, cnts = np.unique(np.round(v, 4), return_counts=True)
    top = np.argsort(cnts)[::-1][:5]
    print('  最常见值:')
    for i in top:
        print('    %.4f  占比 %.2f%%' % (vals[i], cnts[i] / len(v) * 100))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return

    if sys.argv[1] == '--probe':
        a = np.array(Image.open(sys.argv[2]), dtype=np.float32)
        probe(a)
        return

    src, dst = sys.argv[1], sys.argv[2]
    nodata = None
    lo_q, hi_q = 1.0, 99.0
    for a in sys.argv[3:]:
        if a.startswith('--nodata='):
            nodata = float(a.split('=')[1])
        if a.startswith('--lo='):
            lo_q = float(a.split('=')[1])
        if a.startswith('--hi='):
            hi_q = float(a.split('=')[1])

    im = Image.open(src)
    if im.mode in ('F', 'I', 'I;16'):
        a = np.array(im, dtype=np.float32)
        valid = np.isfinite(a)
        if nodata is not None:
            # ★ 哨兵值处理: 用相对容差判断 (浮点比较不能用 ==)
            valid &= np.abs(a - nodata) > 1e-4
        if valid.sum() == 0:
            print('  无有效像素')
            return
        v = a[valid]
        lo = np.percentile(v, lo_q)
        hi = np.percentile(v, hi_q)
        if hi <= lo:
            hi = lo + 1e-6
        print('  拉伸区间 [%.4f, %.4f] (百分位 %.0f–%.0f)'
              % (lo, hi, lo_q, hi_q))
        norm = np.clip((a - lo) / (hi - lo), 0, 1)
        norm[~valid] = 0.0
        g = (norm * 255).astype(np.uint8)
        out = Image.fromarray(g, 'L').convert('RGB')
    else:
        out = im.convert('RGB')

    out.save(dst, quality=93)
    print('  输出 %s  %dx%d' % (dst, out.size[0], out.size[1]))


if __name__ == '__main__':
    main()
