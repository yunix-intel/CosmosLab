"""从 USGS/NASA 地图海报中裁剪出等距柱状投影主图。

USGS 的行星地图常以「海报」形式发布: 顶部是南北极圆图 + 文字说明,
底部才是标准等距柱状投影的全球图。直接当球面贴图用会混进图框和
文字。本脚本按「非白色像素密度」自动定位主图的边界框并裁出。

判据: 海报的图框区域是纯白 (255,255,255), 主图区域色彩丰富。
逐行/逐列统计"明显非白"的像素比例, 比例超过阈值的连续区间即主图。

用法:
    python crop_maps.py <目录> [--report]
"""
import os
import sys

import numpy as np
from PIL import Image

# 天体 -> 期望宽高比。裁完用它校验, 偏差大就说明定位错了。
EXPECT_RATIO = 2.0
NONWHITE = 235      # 低于此灰度算"有内容"
DENSITY = 0.55      # 一行/列中超过此比例的像素有内容, 才算落在主图内


def bbox_of_content(a: np.ndarray) -> tuple | None:
    """返回主图的 (x0, y0, x1, y1), 找不到返回 None。"""
    gray = a.min(axis=2)                     # 最暗通道, 抗白色背景
    mask = gray < NONWHITE

    rows = mask.mean(axis=1)
    cols = mask.mean(axis=0)

    def span(profile: np.ndarray) -> tuple | None:
        idx = np.where(profile > DENSITY)[0]
        if len(idx) == 0:
            return None
        return int(idx[0]), int(idx[-1]) + 1

    rs = span(rows)
    cs = span(cols)
    if rs is None or cs is None:
        return None
    return cs[0], rs[0], cs[1], rs[1]


def close_to(ratio: float, target: float, tol: float = 0.12) -> bool:
    return abs(ratio - target) / target < tol


def process(path: str, report_only: bool) -> None:
    im = Image.open(path).convert("RGB")
    a = np.asarray(im)
    h, w = a.shape[:2]

    bb = bbox_of_content(a)
    if bb is None:
        print(f"  {os.path.basename(path)}: 未找到主图")
        return

    x0, y0, x1, y1 = bb
    cw, ch = x1 - x0, y1 - y0
    ratio = cw / max(ch, 1)

    full_ratio = w / h
    status = "已达标" if close_to(full_ratio, EXPECT_RATIO) else "需裁剪"
    print(f"  {os.path.basename(path)}: {w}x{h} ratio={full_ratio:.2f} [{status}]")
    print(f"     裁剪框 ({x0},{y0})-({x1},{y1}) = {cw}x{ch} ratio={ratio:.2f}")

    if report_only:
        return

    if not close_to(ratio, EXPECT_RATIO, 0.15):
        print("     * 裁剪后比例仍不符, 跳过 (可能是极区圆图被误判)")
        return

    out = os.path.splitext(path)[0] + "_eq.png"
    im.crop((x0, y0, x1, y1)).save(out)
    print(f"     -> {os.path.basename(out)}")


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    report_only = "--report" in sys.argv
    d = args[0] if args else "."

    # 只处理比例不符的 (海报类)
    for name in sorted(os.listdir(d)):
        p = os.path.join(d, name)
        if not os.path.isfile(p) or "_eq" in name:
            continue
        if not name.lower().endswith((".jpg", ".jpeg", ".png")):
            continue
        if name.startswith("2k_"):
            continue                      # solarsystemscope 的已经是标准贴图
        try:
            process(p, report_only)
        except Exception as e:
            print(f"  {name}: 失败 {e}")


if __name__ == "__main__":
    main()
