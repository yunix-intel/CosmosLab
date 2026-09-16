"""把各种行星地图投影统一重采样为标准等距柱状（equirectangular）。

★ 为什么需要这个工具:
  NASA/USGS/JAXA 发布的"全球图"用的是多种投影, 直接当等距柱状贴到
  球面上会出现系统性的畸变:
    * **椭圆投影 (Elliptical / HAMO)** —— 极区被压扁, 贴图后两极
      会出现明显的拉伸与错位
    * **墨卡托 (Mercator)** —— 高纬度被极度放大 (墨卡托的固有特性),
      两极区域几乎无法表示
    * **正射/半球图 (Orthographic / Hemispherical Globes)** —— 只有
      半球可见, 另一半球是空白

  这些图**内容是真实的**, 只是**投影不对**。重采样后即可正确使用 ——
  这比放弃它们或编造纹理都更符合"只做能确定真实的"原则。

★ 重采样的数学:
  目标图 (等距柱状) 上每个像素 (λ, φ) 对应源图上的一点 (x_src, y_src)。
  用**反向映射 + 双线性插值**避免前向映射产生空洞。

    等距柱状:  x = (λ + 180) / 360 · W
               y = (90 - φ) / 180 · H

    墨卡托:    x = W/2 + W · λ / (2π)
               y = H/2 - H · ln(tan(π/4 + φ/2)) / (2π)

    椭圆投影 (HAMO): 由 MITgcm/NASA 用的椭圆投影, 近似为
               x = W/2 · (1 + λ/π) · cos(φ_ell)
               y = H/2 · (1 - φ/φ_max)
              其中 φ_ell = asin(...) —— 见下方实现 (用数值反演)

★ 自动裁边:
  源图常在地图范围外填充彩色条纹或白边。逐行/逐列统计"高饱和占比"
  与"纯白占比", 从四边向中间扫描, 遇到第一行/列干净内容即停止。

用法:
    python reproject.py list                        # 列出支持的投影
    python reproject.py <输入> <输出> <投影> [选项]
    python reproject.py <输入> <输出> auto          # 自动判断 + 裁边

选项:
    --trim          只裁边, 不重投影
    --no-trim       只重投影, 不裁边
    --phi-max=90    椭圆投影的纬度上限 (HAMO 常用 72)
"""
import math
import sys

from PIL import Image


# ---------------------------------------------------------------------------
#  投影定义: 目标 (等距柱状) 坐标 -> 源图归一化坐标
#  返回 (u, v) 且 0<=u<=1, 0<=v<=1; 超出范围返回 None
# ---------------------------------------------------------------------------

def eq_from_equirect(lam, phi):
    """源图本身就是等距柱状"""
    return (lam / (2 * math.pi) + 0.5,
            (0.5 - phi / math.pi))


def eq_from_mercator(lam, phi):
    """墨卡托 -> 等距柱状

    墨卡托: y = H/2 - H·ln(tan(π/4 + φ/2)) / (2π)
    反解出在源图中的归一化 v 即为 (0.5 - ln(tan(π/4+φ/2))/(2π))
    """
    if abs(phi) >= math.pi / 2 - 1e-9:
        return None
    v = 0.5 - math.log(math.tan(math.pi / 4 + phi / 2)) / (2 * math.pi)
    return (lam / (2 * math.pi) + 0.5, v)


def eq_from_elliptical(lam, phi, phi_max_deg=90.0):
    """椭圆投影 (HAMO) -> 等距柱状

    ★ 椭圆投影的形态: 整张图是个椭圆, 极区被"收拢"。
    常见的近似把经度按 cos(缩放) 压缩:
        u = 0.5 + 0.5 · (λ/π) · k(φ)
        v = 0.5 - (φ/π) · (π/φ_max) · 0.5
    其中 k(φ) 是纬度处的横向缩放因子。

    对 HAMO 这类图, k(φ) = sqrt(1 - (φ/φ_max)²) 是常用近似
    (椭圆的横向半宽随纬度按椭圆规律收缩)。
    """
    pm = math.radians(phi_max_deg)
    if abs(phi) > pm:
        return None
    t = phi / pm                        # -1..1
    k = math.sqrt(max(0.0, 1.0 - t * t))  # 椭圆横向收缩
    u = 0.5 + 0.5 * (lam / math.pi) * k
    v = 0.5 - 0.5 * t
    return (u, v)


PROJ = {
    'equirect':  ('等距柱状 (无需重投影)', eq_from_equirect),
    'cylindrical': ('等距柱状 (无需重投影)', eq_from_equirect),
    'mercator':  ('墨卡托 -> 等距柱状', eq_from_mercator),
    'elliptical': ('椭圆投影 (HAMO) -> 等距柱状', eq_from_elliptical),
}


def guess_projection(name):
    low = name.lower()
    if 'mercator' in low:
        return 'mercator'
    if 'elliptical' in low or 'hamo' in low:
        return 'elliptical'
    if 'cylindrical' in low or 'equirect' in low:
        return 'equirect'
    return 'equirect'


# ---------------------------------------------------------------------------
#  边缘裁切
# ---------------------------------------------------------------------------

def detect_trim(im, sat_thresh=0.55, hi_frac=0.22, white_frac=0.35):
    """检测并返回应保留的 (left, top, right, bottom)。"""
    import colorsys
    w, h = im.size
    px = im.load()
    sx = max(1, w // 140)
    sy = max(1, h // 140)

    def bad_row(y):
        hi = wh = cnt = 0
        for x in range(0, w, sx):
            r, g, b = px[x, y][:3]
            cnt += 1
            if r >= 245 and g >= 245 and b >= 245:
                wh += 1
                continue
            _, ll, ss = colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0)
            if ss > sat_thresh and 0.10 < ll < 0.92:
                hi += 1
        return (hi / cnt > hi_frac) or (wh / cnt > white_frac)

    def bad_col(x):
        hi = wh = cnt = 0
        for y in range(0, h, sy):
            r, g, b = px[x, y][:3]
            cnt += 1
            if r >= 245 and g >= 245 and b >= 245:
                wh += 1
                continue
            _, ll, ss = colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0)
            if ss > sat_thresh and 0.10 < ll < 0.92:
                hi += 1
        return (hi / cnt > hi_frac) or (wh / cnt > white_frac)

    top = 0
    while top < h - 1 and bad_row(top):
        top += 1
    bot = h
    while bot > top + 1 and bad_row(bot - 1):
        bot -= 1
    left = 0
    while left < w - 1 and bad_col(left):
        left += 1
    right = w
    while right > left + 1 and bad_col(right - 1):
        right -= 1
    return (left, top, right, bot)


# ---------------------------------------------------------------------------
#  重投影
# ---------------------------------------------------------------------------

def reproject(im, proj_name, phi_max=90.0, out_w=4096):
    """反向映射 + 双线性插值。"""
    fn = PROJ[proj_name][1]
    src = im.convert('RGB')
    sw, sh = src.size
    spx = src.load()

    out_h = out_w // 2                    # 等距柱状恒为 2:1
    dst = Image.new('RGB', (out_w, out_h), (0, 0, 0))
    dpx = dst.load()

    for j in range(out_h):
        # v 是等距柱状的行 -> 纬度
        phi = math.pi * (0.5 - (j + 0.5) / out_h)
        for i in range(out_w):
            lam = 2 * math.pi * ((i + 0.5) / out_w - 0.5)
            if proj_name in ('equirect', 'cylindrical'):
                uv = fn(lam, phi)
            else:
                uv = fn(lam, phi, phi_max) if proj_name == 'elliptical' else fn(lam, phi)
            if uv is None:
                continue
            u, v = uv
            if not (0.0 <= u <= 1.0 and 0.0 <= v <= 1.0):
                continue
            fx = u * (sw - 1)
            fy = v * (sh - 1)
            x0, y0 = int(fx), int(fy)
            x1, y1 = min(x0 + 1, sw - 1), min(y0 + 1, sh - 1)
            dx, dy = fx - x0, fy - y0
            c00 = spx[x0, y0]
            c10 = spx[x1, y0]
            c01 = spx[x0, y1]
            c11 = spx[x1, y1]
            r = (c00[0] * (1 - dx) * (1 - dy) + c10[0] * dx * (1 - dy)
                 + c01[0] * (1 - dx) * dy + c11[0] * dx * dy)
            g = (c00[1] * (1 - dx) * (1 - dy) + c10[1] * dx * (1 - dy)
                 + c01[1] * (1 - dx) * dy + c11[1] * dx * dy)
            b = (c00[2] * (1 - dx) * (1 - dy) + c10[2] * dx * (1 - dy)
                 + c01[2] * (1 - dx) * dy + c11[2] * dx * dy)
            dpx[i, j] = (int(r), int(g), int(b))
    return dst


def main():
    if len(sys.argv) < 2 or sys.argv[1] == 'list':
        print('支持的投影:')
        for k, (desc, _) in PROJ.items():
            print('   %-14s %s' % (k, desc))
        return

    if len(sys.argv) < 4:
        print(__doc__)
        return

    src_path, dst_path = sys.argv[1], sys.argv[2]
    proj = sys.argv[3]
    if proj == 'auto':
        proj = guess_projection(src_path)

    phi_max = 90.0
    do_trim = '--no-trim' not in sys.argv
    only_trim = '--trim' in sys.argv
    for a in sys.argv[4:]:
        if a.startswith('--phi-max='):
            phi_max = float(a.split('=')[1])

    im = Image.open(src_path).convert('RGB')
    print('源图 %dx%d  比例 %.3f' % (im.size[0], im.size[1], im.size[0] / im.size[1]))

    if do_trim:
        l, t, r, b = detect_trim(im)
        if (l, t, r, b) != (0, 0, im.size[0], im.size[1]):
            im = im.crop((l, t, r, b))
            print('裁边: 左%d 上%d 右%d 下%d -> %dx%d'
                  % (l, t, im.size[0] - (r - l) + l, b,
                     im.size[0], im.size[1]))
        else:
            print('未检测到边缘填充带')

    print('投影: %s' % PROJ[proj][0])
    if proj in ('equirect', 'cylindrical') or only_trim:
        out = im
    else:
        out = reproject(im, proj, phi_max)
        print('重投影完成 -> %dx%d' % out.size)

    out.save(dst_path, quality=93)
    print('输出 %s  %dx%d  比例 %.3f'
          % (dst_path, out.size[0], out.size[1], out.size[0] / out.size[1]))


if __name__ == '__main__':
    main()
