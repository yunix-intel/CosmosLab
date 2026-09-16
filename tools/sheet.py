"""把多张渲染图拼成一张联系表 (contact sheet), 便于一次性总览。

用法:
    python sheet.py <输出> <列数> <每格宽> <图1> [图2] ...
每格下方标注文件名 (去掉扩展名)。
"""
import os
import sys
from PIL import Image, ImageDraw

BG = (16, 16, 20)
FG = (200, 200, 210)


def main() -> None:
    out = sys.argv[1]
    cols = int(sys.argv[2])
    cell_w = int(sys.argv[3])
    paths = sys.argv[4:]

    cells = []
    for p in paths:
        if not os.path.isfile(p):
            print(f"缺失 {p}")
            continue
        im = Image.open(p).convert("RGB")
        ch = int(im.size[1] * cell_w / im.size[0])
        cells.append((os.path.splitext(os.path.basename(p))[0],
                      im.resize((cell_w, ch), Image.LANCZOS), ch))

    if not cells:
        print("没有可用图像")
        return

    cell_h = cells[0][2]
    label_h = 22
    rows = (len(cells) + cols - 1) // cols
    pad = 10

    W = cols * cell_w + (cols + 1) * pad
    H = rows * (cell_h + label_h) + (rows + 1) * pad
    sheet = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(sheet)

    for i, (name, im, ch) in enumerate(cells):
        r, c = divmod(i, cols)
        x = pad + c * (cell_w + pad)
        y = pad + r * (cell_h + label_h + pad)
        sheet.paste(im, (x, y))
        d.text((x + 4, y + ch + 4), name, fill=FG)

    sheet.save(out)
    print(f"{out}  {W}x{H}  共 {len(cells)} 格")


if __name__ == "__main__":
    main()
