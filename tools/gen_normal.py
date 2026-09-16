"""从反照率贴图生成法线贴图（当没有真实法线图时）。

★★ 为什么需要:
  项目里 34 个 albedo 贴图只有 12 个配套的 normal 贴图。缺法线图的天体
  在渲染时表面是**完全平的** —— 即使贴图里有清晰的撞击坑与沟槽,
  光照上去也没有起伏感, 看起来像"贴了一层纸"。
  实测 Ariel 的峡谷系统就是这样"平"着的。

★ 原理:
  把亮度当成**高程**来求梯度 (Sobel), 再由梯度算法线:
      n = normalize(-dL/dx * strength, -dL/dy * strength, 1)
  这是"假高程"法 —— 亮度不等于高程, 但在地质影像里
  亮处通常是向阳坡/新鲜物质, 暗处是阴影/低洼, 二者**统计相关**,
  足以产生可信的凹凸感。

★ 与传统"凹凸贴图"的区别:
  这是**从影像推导**的法线, 不是实测地形。对教学软件而言,
  它带来的立体感远胜于"平脸", 而且不会伪造任何**结构**
  (只是让已有的明暗关系参与光照)。
  在文档/UI 里应说明这一点。

★ 强度控制:
  太强会出现"浮雕"感 (像橡皮泥), 太弱没效果。
  实测 0.8~1.5 之间比较自然, 默认 1.1。

用法:
    python gen_normal.py --check <目录>
    python gen_normal.py <目录>           为每个 albedo 生成 normal_
    python gen_normal.py <目录> --only ariel oberon
"""
import glob
import os
import sys

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None

BLACK = 20


def make_normal(a, strength=1.1, blur=1):
    """由亮度梯度生成法线贴图 (RGB 编码)。"""
    # 转灰度, 归一化
    lum = (0.299 * a[:, :, 0] + 0.587 * a[:, :, 1] + 0.114 * a[:, :, 2])
    lum = lum.astype(np.float64) / 255.0

    # ★ 排除 NoData 黑区 —— 否则数据边界会生成一条假的"悬崖"法线,
    #   渲染时边界上会出现一圈异常高光。
    valid = ~((a[:, :, 0] < BLACK) & (a[:, :, 1] < BLACK) & (a[:, :, 2] < BLACK))
    lum_filled = lum.copy()
    if (~valid).any():
        # 用有效区的均值填掉黑区 (不参与梯度计算)
        lum_filled[~valid] = lum[valid].mean() if valid.any() else 0.5

    # 轻微高斯模糊, 避免单像素噪声被放大成麻点
    if blur > 0:
        from PIL import ImageFilter
        li = Image.fromarray((lum_filled * 255).astype(np.uint8), 'L')
        li = li.filter(ImageFilter.GaussianBlur(blur))
        lum_filled = np.asarray(li, dtype=np.float64) / 255.0

    # Sobel 梯度 (注意 y 方向: 图像 y 向下, 法线的 y 要取反)
    gx = np.zeros_like(lum_filled)
    gy = np.zeros_like(lum_filled)
    gx[:, 1:-1] = (lum_filled[:, 2:] - lum_filled[:, :-2]) * 0.5
    gy[1:-1, :] = (lum_filled[2:, :] - lum_filled[:-2, :]) * 0.5

    nx = -gx * strength * 8.0
    ny = gy * strength * 8.0        # 取反: 贴图 y 向下
    nz = np.ones_like(nx)

    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    nx /= ln
    ny /= ln
    nz /= ln

    # 编码到 0..255
    out = np.zeros((a.shape[0], a.shape[1], 3), dtype=np.uint8)
    out[:, :, 0] = np.clip((nx * 0.5 + 0.5) * 255, 0, 255).astype(np.uint8)
    out[:, :, 1] = np.clip((ny * 0.5 + 0.5) * 255, 0, 255).astype(np.uint8)
    out[:, :, 2] = np.clip((nz * 0.5 + 0.5) * 255, 0, 255).astype(np.uint8)

    # 黑区不产生法线扰动 (保持"平坦", 与均匀底色一致)
    out[~valid] = (128, 128, 255)
    return out


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    args = [x for x in sys.argv[1:] if not x.startswith('--')]
    dry = '--check' in sys.argv
    only = []
    if '--only' in sys.argv:
        i = sys.argv.index('--only')
        only = sys.argv[i + 1:]
        args = [a for a in args if a not in only]

    root = args[0] if args else 'assets/tex'
    strength = 1.1
    for a in sys.argv:
        if a.startswith('--strength='):
            strength = float(a.split('=')[1])

    tex_dir = root
    normal_dir = os.path.join(os.path.dirname(root), 'normal')

    albedos = sorted(glob.glob(os.path.join(tex_dir, 'albedo_*')))
    if not albedos:
        print('未找到 albedo 文件于 %s' % tex_dir)
        return

    # 输出目录: 与 tex 同级
    out_dir = os.path.join(os.path.dirname(os.path.abspath(root)), 'tex')
    if not os.path.isdir(out_dir):
        out_dir = root

    n = 0
    for p in albedos:
        name = os.path.basename(p)[7:].rsplit('.', 1)[0]
        if only and name not in only:
            continue
        dst = os.path.join(out_dir, 'normal_%s.jpg' % name)
        if os.path.isfile(dst):
            continue
        if dry:
            print('  %-14s 需生成 (无现有法线图)' % name)
            n += 1
            continue
        try:
            im = Image.open(p).convert('RGB')
            # 法线贴图不需要和 albedo 同分辨率 —— 一半即可, 省显存
            w, h = im.size
            if w > 1024:
                im = im.resize((1024, 512), Image.LANCZOS)
            arr = np.array(im)
            nrm = make_normal(arr, strength)
            Image.fromarray(nrm).save(dst, quality=92)
            print('  ✓ normal_%s.jpg  %dx%d' % (name, nrm.shape[1], nrm.shape[0]))
            n += 1
        except Exception as e:
            print('  ✗ %s: %s' % (name, e))

    print()
    print('共 %d 个' % n)


if __name__ == '__main__':
    main()
