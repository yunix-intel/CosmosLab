"""把下载到的原始贴图整理成应用使用的资产目录。

输入: D:/tmp/realtex 下的一堆原始文件 (solarsystemscope 的 2k_*.jpg,
      Commons 的 real_*.jpg)
输出: assets/tex/ 下按应用命名规范的文件 (albedo_<id>.jpg 等)

同时把所有贴图**规范到 2:1** —— 这是等距柱状投影的硬要求, 比例不对的话
贴到球面上会横竖不等比。差异 < 6% 的直接缩放 (形变不可见); 差异更大的
先裁到接近再缩放, 避免明显拉伸。

用法:
    python build_assets.py <输入目录> <输出目录>
"""
import os
import shutil
import sys

import numpy as np
from PIL import Image

# (输出名, 候选输入名列表) —— 按顺序取第一个存在的
MAP = [
    # ---- 8 大行星 + 太阳 (solarsystemscope, 基于 NASA 影像) ----
    ("albedo_sun.jpg",       ["2k_sun.jpg"]),
    ("albedo_mercury.jpg",   ["2k_mercury.jpg"]),
    ("albedo_venus.jpg",     ["2k_venus_surface.jpg"]),
    ("albedo_earth.jpg",     ["2k_earth_daymap.jpg"]),
    ("albedo_mars.jpg",      ["2k_mars.jpg"]),
    ("albedo_jupiter.jpg",   ["2k_jupiter.jpg"]),
    ("albedo_saturn.jpg",    ["2k_saturn.jpg"]),
    ("albedo_uranus.jpg",    ["2k_uranus.jpg"]),
    ("albedo_neptune.jpg",   ["2k_neptune.jpg"]),
    # ---- 月球 ----
    ("albedo_moon.jpg",      ["2k_moon.jpg"]),
    # ---- 矮行星 / 卫星 (Commons: NASA / USGS / ESA 公有领域) ----
    ("albedo_pluto.jpg",     ["real_pluto.jpg"]),
    ("albedo_io.jpg",        ["real_io.jpg"]),
    ("albedo_europa.jpg",    ["real_europa.jpg"]),
    ("albedo_ganymede.jpg",  ["real_ganymede.jpg"]),
    ("albedo_callisto.jpg",  ["real_callisto.jpg"]),
    ("albedo_titan.jpg",     ["real_titan.jpg"]),
    ("albedo_enceladus.jpg", ["real_enceladus.jpg"]),
    ("albedo_triton.jpg",    ["real_triton.jpg"]),
    # ---- 地球附加层 ----
    ("night_earth.jpg",      ["2k_earth_nightmap.jpg"]),
    ("clouds.jpg",           ["2k_earth_clouds.jpg"]),
    # ---- 银河背景 ----
    ("milkyway.jpg",         ["2k_stars_milky_way.jpg"]),
]

TARGET = 2.0
TOL_SOFT = 0.06          # 差异 < 6%: 直接缩放
MAX_W = 2048             # 输出统一宽度上限 (显存与性能的平衡)


def normalize(im: Image.Image) -> Image.Image:
    w, h = im.size
    r = w / h
    d = abs(r - TARGET) / TARGET

    if d < 0.005:
        pass                                   # 已经标准, 不动
    elif d < TOL_SOFT:
        # 轻微形变, 直接缩放 (5% 以内的不等比肉眼不可辨)
        im = im.resize((w, int(round(w / TARGET))), Image.LANCZOS)
    else:
        # 差异较大: 以短边为准裁成长条再缩放。
        # 对 r>2 (图太宽) 裁掉左右; 对 r<2 (图太高) 裁掉上下。
        if r > TARGET:
            want_w = int(round(h * TARGET))
            x0 = (w - want_w) // 2
            im = im.crop((x0, 0, x0 + want_w, h))
        else:
            want_h = int(round(w / TARGET))
            y0 = (h - want_h) // 2
            im = im.crop((0, y0, w, y0 + want_h))

    # 限制宽度 (性能)
    if im.size[0] > MAX_W:
        im = im.resize((MAX_W, MAX_W // 2), Image.LANCZOS)
    return im


def main() -> None:
    src = sys.argv[1] if len(sys.argv) > 1 else "."
    dst = sys.argv[2] if len(sys.argv) > 2 else "assets/tex"
    os.makedirs(dst, exist_ok=True)

    ok, miss = 0, []
    for out_name, cands in MAP:
        found = None
        for c in cands:
            p = os.path.join(src, c)
            if os.path.isfile(p):
                found = p
                break
        if not found:
            miss.append(out_name)
            continue

        im = Image.open(found).convert("RGB")
        before = im.size
        im = normalize(im)
        out = os.path.join(dst, out_name)
        # 统一存 PNG 也无必要 —— JPEG 体积小很多, 且这些贴图本身无透明通道
        im.save(out, "JPEG", quality=92, optimize=True)
        ok += 1
        print(f"{out_name:24s} {before[0]:5d}x{before[1]:<5d} -> "
              f"{im.size[0]:5d}x{im.size[1]:<5d}  "
              f"{os.path.getsize(out) / 1024:7.0f} KB")

    print(f"\n完成 {ok} 个, 缺失 {len(miss)} 个")
    if miss:
        print("缺失:", ", ".join(miss))

    # 环纹理: 沿用项目里已有的 (程序化生成的径向条纹更细腻,
    # 而 solarsystemscope 的 alpha 环只有 125px 高, 放大会糊)
    for ring in ("ring_saturn.png", "ring_jupiter.png", "ring_uranus.png"):
        p = os.path.join(src, ring)
        if os.path.isfile(p):
            shutil.copy2(p, os.path.join(dst, ring))
            print(f"{ring:24s} (复制)")


if __name__ == "__main__":
    main()
