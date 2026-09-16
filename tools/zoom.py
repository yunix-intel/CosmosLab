"""裁剪图片的指定区域并放大, 用于诊断渲染细节。

用法:
    python zoom.py <图片> <x> <y> <w> <h> [放大倍数]
输出: <图片>_zoom.png
"""
import os
import sys

from PIL import Image


def main() -> None:
    p = sys.argv[1]
    x, y, w, h = (int(v) for v in sys.argv[2:6])
    scale = int(sys.argv[6]) if len(sys.argv) > 6 else 3

    im = Image.open(p).convert("RGB")
    W, H = im.size
    box = (max(0, x), max(0, y), min(W, x + w), min(H, y + h))
    crop = im.crop(box)
    cw, ch = crop.size
    crop = crop.resize((cw * scale, ch * scale), Image.NEAREST)

    stem, _ = os.path.splitext(p)
    out = f"{stem}_zoom.png"
    crop.save(out)
    print(f"{out}  源区域 {box}  ->  {crop.size[0]}x{crop.size[1]}")


if __name__ == "__main__":
    main()
