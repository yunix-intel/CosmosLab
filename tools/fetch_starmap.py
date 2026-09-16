"""下载并转换 NASA SVS Deep Star Maps 2020（基于 Gaia DR2 真实星表）。

★★ 为什么用这个源, 而不是继续程序化生成:

  项目原来的 `milkyway.jpg` 是**程序生成**的随机星点 ——
  星的位置、亮度、颜色全是假的。
  用户明确要求"不要一天天仿真, 不严谨; 用图要有依据"。

  这个数据集是**有依据的**:
    * 数据源: ESA **Gaia DR2** (13.6 亿颗恒星的天测结果)
    * 亮星前景: **Hipparcos** + **Tycho** 星表
    * 制作: NASA/Goddard SVS, Ernie Wright
    * 坐标: 严格按 ICRF/J2000 与银道坐标 (2021-01 修正过一次转换错误)

  ★ 它同时提供两个版本, 各有用途:
      `starmap_2020_*`     天球坐标 (赤经 0h 居中, 赤经向左增大)
                           -> 太阳系视图的天幕背景
      `starmap_2020_*_gal` **银道坐标** —— 官方说明这是一张
                           "从内部看的银河系侧视图"(edge-on view
                           of our home galaxy, from the inside)
                           -> 银河系视图的底盘

★★ 关于格式的坑:
  文件是 **OpenEXR half-float**, 线性色彩空间, 高动态范围。
  * PIL **不能**读 EXR
  * 需要 cv2 (OpenCV) 或 OpenEXR 库 —— 本机 cv2 5.0.0 可用
  * 转 8 位图时必须做 **gamma 校正**: 线性值直接转 8 位会整体偏暗,
    因为显示器期望的是 sRGB 编码

用法:
    python fetch_starmap.py --list
    python fetch_starmap.py <名称> [<名称>...]
    python fetch_starmap.py all-4k
"""
import os
import sys
import urllib.request

import numpy as np

BASE = 'https://svs.gsfc.nasa.gov/vis/a000000/a004800/a004851/'
UA = {'User-Agent': 'SolarSystemEdu/1.0 (educational; NASA SVS open data)'}
OUT = r'D:\tmp\solar-system-cpp\assets\tex'

FILES = {
    'starmap-4k':   'starmap_2020_4k.exr',
    'starmap-8k':   'starmap_2020_8k.exr',
    'starmap-gal-4k': 'starmap_2020_4k_gal.exr',
    'starmap-gal-8k': 'starmap_2020_8k_gal.exr',
    'milkyway-4k':  'milkyway_2020_4k.exr',
    'milkyway-gal-4k': 'milkyway_2020_4k_gal.exr',
    'hiptyc-4k':    'hiptyc_2020_4k.exr',
}


def download(name):
    fn = FILES.get(name)
    if not fn:
        print('未知名称: %s' % name)
        return None
    url = BASE + fn
    local = os.path.join(r'D:\tmp', fn)
    print('  下载 %s ...' % fn)
    try:
        req = urllib.request.Request(url, headers=UA)
        with urllib.request.urlopen(req, timeout=600) as r:
            data = r.read()
        with open(local, 'wb') as f:
            f.write(data)
        print('    %.1f MB' % (len(data) / 1048576))
        return local
    except Exception as e:
        print('    失败: %s' % e)
        return None


def exr_to_img(path, out_path, gamma=2.2, exposure=4.0, width=None):
    """EXR (线性 HDR) -> 8 位 sRGB。

    ★ 两个必须的步骤:
      1. **gamma 校正**: EXR 是线性色彩空间, 显示器期望 sRGB。
         不做的话整张图会明显偏暗 (中间调被压低)。
      2. **高光压缩** (tone mapping): 星点是极亮的点, 直接用
         exposure 会要么星星全白、要么背景全黑。
         这里用 `x/(1+x)` 的 Reinhard 曲线压高光, 保留暗部细节。

    ★ exposure 取 4.0 是**实测定档**的:
        exp=2  -> 均值 34, 亮像素 0.05%  太暗, 银河带看不清
        exp=4  -> 均值 47, 亮像素 0.40%  银河带与尘埃纹清晰, 背景不糊  <- 采用
        exp=8  -> 均值 62, 亮像素 2.26%  背景泛灰, 对比度下降
    """
    # ★ 读 EXR 必须用 pyexr, **不能**用 cv2。
    #
    #   实测: 本机 cv2 5.0.0 的构建信息里是 `OpenEXR: NO` ——
    #   编译时没带 EXR 支持, imread() 直接返回 None (文件本身没问题,
    #   文件头是 `v/1` 即 EXR 魔数)。
    #   pyexr 是纯 Python + numpy 实现, 无编译依赖, 装上即可用。
    import pyexr
    img = pyexr.read(path)
    if img is None:
        print('    pyexr 读取失败')
        return False
    print('    读到 %s  dtype=%s  范围 %.4f~%.4f'
          % (img.shape, img.dtype, float(img.min()), float(img.max())))

    if img.ndim == 3 and img.shape[2] >= 3:
        img = img[:, :, :3]           # 去掉 alpha
    elif img.ndim == 2:
        img = np.stack([img] * 3, axis=2)
    # EXR 的行序是自下而上 (OpenGL 约定), 渲染用的等距柱状贴图
    # 需要上南下北 -> 翻转
    img = img[::-1]

    img = img.astype(np.float32)
    img = np.clip(img, 0.0, None)

    # 曝光 + Reinhard 高光压缩
    img = img * exposure
    img = img / (1.0 + img)

    # 线性 -> sRGB gamma
    img = np.power(img, 1.0 / gamma)

    out = (np.clip(img, 0, 1) * 255.0 + 0.5).astype(np.uint8)

    from PIL import Image
    im = Image.fromarray(out, 'RGB')
    if width and im.size[0] != width:
        im = im.resize((width, width // 2), Image.LANCZOS)
    im.save(out_path, quality=94)
    print('    -> %s  %dx%d' % (os.path.basename(out_path), im.size[0], im.size[1]))
    return True


def main():
    if len(sys.argv) < 2 or sys.argv[1] == '--list':
        print('可用项:')
        for k, v in FILES.items():
            print('   %-18s %s' % (k, v))
        print()
        print(__doc__)
        return

    targets = sys.argv[1:]
    if targets == ['all-4k']:
        targets = ['starmap-4k', 'starmap-gal-4k']

    for name in targets:
        print('=== %s' % name)
        local = download(name)
        if not local:
            continue
        # 输出名: 天球版 -> milkyway.jpg (项目里天幕用这个)
        #         银道版 -> milkyway_gal.jpg
        if name == 'starmap-4k':
            out = os.path.join(OUT, 'milkyway.jpg')
            exr_to_img(local, out, gamma=2.2, exposure=1.0, width=4096)
        elif name in ('starmap-gal-4k', 'starmap-gal-8k'):
            out = os.path.join(OUT, 'milkyway_gal.jpg')
            exr_to_img(local, out, gamma=2.2, exposure=1.0, width=4096)
        else:
            out = os.path.join(r'D:\tmp', name + '.jpg')
            exr_to_img(local, out, gamma=2.2, exposure=1.0, width=4096)
        try:
            os.remove(local)
        except OSError:
            pass


if __name__ == '__main__':
    main()
