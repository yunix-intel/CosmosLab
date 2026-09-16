"""生成缩略图, 便于快速查看 (原图 2880x1800 太大)。

用法:
    python thumbs.py <输出宽> <图1> [图2] ...
输出: 同目录下 <原名>_s.png
"""
import os
import sys
from PIL import Image


def main() -> None:
    w = int(sys.argv[1])
    for p in sys.argv[2:]:
        if not os.path.isfile(p):
            print(f"缺失 {p}")
            continue
        im = Image.open(p).convert("RGB")
        h = int(im.size[1] * w / im.size[0])
        im2 = im.resize((w, h), Image.LANCZOS)
        stem, _ = os.path.splitext(p)
        out = stem + "_s.png"
        im2.save(out)
        print(f"{out}  {w}x{h}")


if __name__ == "__main__":
    main()
