"""从 NASA 官方银河系图中提取**旋臂名称标签的位置**，作为 3D 标注的锚点。

★★ 为什么要用图上的标签位置:

  图上的 "Perseus Arm" / "Sagittarius Arm" / ... 这些文字是
  Robert Hurt 亲手**放在对应旋臂上的** —— 这等于作者已经替我们
  标注好了每条臂的真实走向。

  直接采用这些位置作为 3D 标注锚点, 比用解析式 log(r) 螺旋去算
  更可靠: 因为真实的旋臂并非完美对数螺旋, 用公式算总会偏。

★ 天文标准提示:
  这张图按 Spitzer 红外结论**强调两条主臂**(英仙臂、盾牌-半人马臂),
  四条臂都画了但主次分明。项目 UI 必须说明这一点,
  否则学生会以为"银河系有四条等权的臂"。

用法:
    python find_labels.py            检测并输出标签框
    python find_labels.py --debug    另外输出带框的调试图
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

SRC = r'D:\tmp\galaxy_hurt_2000.jpg'
DEBUG = r'D:\tmp\label_debug.png'


def white_mask(path):
    a = np.array(Image.open(path).convert('RGB')).astype(np.float32)
    r, g, b = a[:, :, 0], a[:, :, 1], a[:, :, 2]
    return (r > 205) & (g > 205) & (b > 205) \
        & (np.abs(r - g) < 20) & (np.abs(g - b) < 20)


def clusters(mask, min_px=120):
    """纯 numpy 的连通域标记 (BFS), 返回 [(x0,y0,x1,y1,面积)]。"""
    from collections import deque
    h, w = mask.shape
    seen = np.zeros_like(mask, dtype=bool)
    out = []
    ys, xs = np.where(mask)
    for sy, sx in zip(ys[::7], xs[::7]):      # 稀疏起步, 加速
        if seen[sy, sx]:
            continue
        q = deque([(sy, sx)])
        seen[sy, sx] = True
        n = 0
        x0 = x1 = sx
        y0 = y1 = sy
        while q:
            y, x = q.popleft()
            n += 1
            if x < x0: x0 = x
            if x > x1: x1 = x
            if y < y0: y0 = y
            if y > y1: y1 = y
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    ny, nx = y + dy, x + dx
                    if 0 <= ny < h and 0 <= nx < w \
                            and mask[ny, nx] and not seen[ny, nx]:
                        seen[ny, nx] = True
                        q.append((ny, nx))
        if n >= min_px:
            out.append((x0, y0, x1, y1, n))
    return out


def merge_words(boxes, dx=70, dy=14):
    """把相邻的字符合并成词/短语。"""
    boxes = sorted(boxes, key=lambda b: (b[1] // 30, b[0]))
    changed = True
    while changed:
        changed = False
        out = []
        while boxes:
            b = boxes.pop(0)
            merged = False
            for i, c in enumerate(out):
                # 垂直重叠且水平接近 -> 合并
                vov = min(b[3], c[3]) - max(b[1], c[1])
                if vov > 0.4 * min(b[3] - b[1], c[3] - c[1]):
                    gap = max(b[0], c[0]) - min(b[2], c[2])
                    if gap < dx:
                        out[i] = (min(b[0], c[0]), min(b[1], c[1]),
                                  max(b[2], c[2]), max(b[3], c[3]),
                                  b[4] + c[4])
                        merged = True
                        changed = True
                        break
            if not merged:
                out.append(b)
        boxes = out
    return boxes


def main():
    if not os.path.isfile(SRC):
        print('缺少源图')
        return
    m = white_mask(SRC)
    print('白色像素 %.2f%%' % (m.mean() * 100))
    blobs = clusters(m, min_px=60)
    print('字符级连通域 %d 个' % len(blobs))
    words = merge_words(blobs)
    # 只要横向较长、高度像文字的
    words = [w for w in words if (w[2] - w[0]) > 40 and 8 < (w[3] - w[1]) < 60]
    words.sort(key=lambda w: -w[4])
    print('合并后文本块 %d 个 (按像素数排序):' % len(words))
    print('  %-24s %-10s %s' % ('包围盒(x0,y0,x1,y1)', '面积', '中心'))
    for w in words[:24]:
        cx = (w[0] + w[2]) // 2
        cy = (w[1] + w[3]) // 2
        print('  %-24s %-10d (%d,%d)'
              % ('(%d,%d,%d,%d)' % w[:4], w[4], cx, cy))

    if '--debug' in sys.argv:
        im = Image.open(SRC).convert('RGB')
        d = ImageDraw.Draw(im)
        for i, w in enumerate(words[:24]):
            d.rectangle(w[:4], outline=(255, 60, 60), width=3)
            d.text((w[0], max(0, w[1] - 18)), str(i), fill=(255, 220, 0))
        im.save(DEBUG)
        print()
        print('调试图 -> %s' % DEBUG)


if __name__ == '__main__':
    main()
