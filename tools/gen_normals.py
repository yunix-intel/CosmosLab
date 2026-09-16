"""从真实反照率贴图推导法线贴图。

真实照片（NASA/USGS 影像）没有附带法线数据, 但环形山、山脊在反照率上
本来就有明暗差异。用亮度梯度近似高度场梯度, 可以得到与地貌**对得上**的
凹凸感 —— 比旧的程序化法线贴图更准确（那批是按随机种子生成的, 形状与
照片里的真实环形山毫无关系）。

两个关键处理:

1. **按纬度校正水平梯度**
   等距柱状投影在两极附近水平方向被极度压缩: 同样的角距, 在极区对应的
   实际距离趋近于 0。不校正的话极点会算出一片噪点法线。
   校正因子 = 1/cos(lat)。

2. **先轻度模糊再求梯度**
   照片里的传感器噪声和压缩伪影会被梯度算子放大成"沙粒"。先做一次
   高斯模糊, 只保留地貌尺度的起伏。

用法:
    python gen_normals.py <资产目录>
"""
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

# 需要法线贴图的天体: 有实体地表的岩石/冰质天体。
# 气体巨行星与太阳不加 —— 它们的可见表面是云顶, 加凹凸反而不真实。
ROCKY = ["mercury", "venus", "earth", "mars", "moon", "pluto",
         "io", "europa", "ganymede", "callisto", "enceladus", "triton"]

STRENGTH = 1.4       # 梯度放大倍数。真实照片的亮度梯度本就比程序化平涂
                     # 贴图强得多, 放大过头会让月球变成"浮雕硬币"。
                     # 最终由 shader 的 uNormalScale 再做总控。
BLUR = 1.8           # 预模糊半径 (pixel)


def to_normal(height: np.ndarray) -> np.ndarray:
    """高度场 -> 切线空间法线贴图 (RGB 8bit)。"""
    h, w = height.shape

    # 纬度 (行 0 = 北极), 用于水平梯度校正
    lat = np.deg2rad(90.0 - (np.arange(h) + 0.5) * 180.0 / h)
    coslat = np.cos(lat)
    coslat = np.maximum(coslat, 0.08)          # 限制极点放大, 避免爆炸

    # 中心差分求梯度
    gy, gx = np.gradient(height)
    gx = gx / coslat[:, None] ** 0.65          # 部分校正 (全量会过强)

    nx = -gx * STRENGTH * w / 2048.0
    ny = gy * STRENGTH * h / 1024.0
    nz = np.ones_like(nx)

    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    nx, ny, nz = nx / ln, ny / ln, nz / ln

    # 编码到 [0,1]: n*0.5+0.5
    rgb = np.stack([nx * 0.5 + 0.5, ny * 0.5 + 0.5, nz * 0.5 + 0.5], axis=-1)
    return np.clip(rgb * 255.0, 0, 255).astype(np.uint8)


def process(path: str, out: str) -> None:
    im = Image.open(path).convert("L")
    # 预模糊: 去掉传感器噪声与 JPEG 伪影
    im = im.filter(ImageFilter.GaussianBlur(BLUR))

    a = np.asarray(im).astype(np.float64) / 255.0
    rgb = to_normal(a)
    Image.fromarray(rgb, "RGB").save(out, "JPEG", quality=90, optimize=True)
    print(f"  {os.path.basename(out):28s} "
          f"{os.path.getsize(out) / 1024:7.0f} KB")


def main() -> None:
    d = sys.argv[1] if len(sys.argv) > 1 else "assets/tex"
    n = 0
    for body in ROCKY:
        src = os.path.join(d, f"albedo_{body}.jpg")
        if not os.path.isfile(src):
            src = os.path.join(d, f"albedo_{body}.png")
        if not os.path.isfile(src):
            print(f"  {body}: 无反照率贴图, 跳过")
            continue
        process(src, os.path.join(d, f"normal_{body}.jpg"))
        n += 1
    print(f"\n生成 {n} 张法线贴图")


if __name__ == "__main__":
    main()
