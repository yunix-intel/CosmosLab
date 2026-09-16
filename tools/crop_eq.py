"""精确裁剪 USGS 海报的底部主图。

诊断发现: 海报类文件的底部主图区域, 每行会有一条**很长的连续内容段**
(宽 3400~3600 px); 上方的极区圆图与文字区则只有零散短段。
用「最长连续段宽度」作判据比「非白像素密度」可靠得多。

对每个文件:
  1. 逐行算最长连续内容段宽度
  2. 取 > 阈值(默认 2500px) 的连续行区间 = 主图上下边界
  3. 在该区间内取所有长段的最小左端/最大右端 = 左右边界
  4. 裁出并按需缩放到 2:1 (等距柱状的标准比例)

用法:
    python crop_eq.py <文件...> [--ratio 2.0]
"""
import os
import sys

import numpy as np
from PIL import Image

NONWHITE = 235
MIN_SEG = 2500


def segment_widths(mask: np.ndarray) -> np.ndarray:
    """返回每一行中最长连续 True 段的宽度。"""
    h = mask.shape[0]
    out = np.zeros(h, dtype=np.int32)
    for y in range(h):
        idx = np.where(mask[y])[0]
        if len(idx) == 0:
            continue
        gaps = np.where(np.diff(idx) > 8)[0]
        segs = np.split(idx, gaps + 1)
        out[y] = max(len(s) for s in segs)
    return out


def crop_one(path: str, ratio_target: float) -> None:
    im = Image.open(path).convert("RGB")
    a = np.asarray(im)
    h, w = a.shape[:2]

    widths = segment_widths(a.min(axis=2) < NONWHITE)
    rows = np.where(widths > MIN_SEG)[0]
    if len(rows) == 0:
        print(f"  {os.path.basename(path)}: 未找到主图区 (无超长连续段)")
        return

    y0, y1 = int(rows[0]), int(rows[-1]) + 1
    band = a[y0:y1].min(axis=2) < NONWHITE          # 只在这个带状区域找左右边界
    cols = np.where(band.mean(axis=0) > 0.5)[0]
    if len(cols) == 0:
        print(f"  {os.path.basename(path)}: 未能定位左右边界")
        return
    x0, x1 = int(cols[0]), int(cols[-1]) + 1

    cw, ch = x1 - x0, y1 - y0
    print(f"  {os.path.basename(path)}: {w}x{h}")
    print(f"     主图 ({x0},{y0})-({x1},{y1}) = {cw}x{ch} ratio={cw/ch:.3f}")

    # 按目标比例微调: 以宽度为准反推应有的高度, 从中心上下对称扩/裁。
    # 直接缩放会拉伸地貌, 而 2:1 是等距柱状投影的硬要求 (否则球面会形变)。
    want_h = int(round(cw / ratio_target))
    if want_h <= ch:
        center = (y0 + y1) // 2
        ny0 = center - want_h // 2
        ny1 = ny0 + want_h
        # 越界时贴边
        if ny0 < 0:
            ny0, ny1 = 0, want_h
        if ny1 > h:
            ny1, ny0 = h, h - want_h
    else:
        # 高度不足, 反而要以高度为准收窄宽度
        want_w = int(round(ch * ratio_target))
        center = (x0 + x1) // 2
        nx0 = max(0, center - want_w // 2)
        nx1 = min(w, nx0 + want_w)
        x0, x1 = nx0, nx1
        ny0, ny1 = y0, y1
        print(f"     高度不足, 改为收窄宽度 -> {want_w}px")

    out = os.path.splitext(path)[0] + "_eq.png"
    im.crop((x0, ny0, x1, ny1)).save(out)
    r = Image.open(out)
    print(f"     -> {os.path.basename(out)}  {r.size[0]}x{r.size[1]} "
          f"ratio={r.size[0]/r.size[1]:.3f}")


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    ratio = 2.0
    if "--ratio" in sys.argv:
        ratio = float(sys.argv[sys.argv.index("--ratio") + 1])
        args = [a for a in args if a != str(ratio)]

    for p in args:
        if os.path.isfile(p):
            crop_one(p, ratio)


if __name__ == "__main__":
    main()
