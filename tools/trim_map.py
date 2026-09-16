"""裁剪地图边缘的无效数据条带（彩色填充 / 空白）。

★ 为什么需要:
  NASA/USGS 发布的全球图常在地图范围外填充**彩色条纹**表示"无数据"
  (实测 Ceres 的 Mercator 版上下有绿/黄条纹)。
  直接贴到球面上会把这些条纹也画出地球 —— 必须裁掉。

★ 判据: 双色性 (bi-modality)
  正常的地表图, 每一行的颜色分布是**连续且低频差异**的;
  而填充条纹是**高饱和 + 与主体色截然不同**的窄带。
  实测: 条纹区的"高饱和像素占比"远高于地表区。

  所以逐行统计高饱和像素占比, 超过阈值的行判为填充带。
  从上下两端向中间扫描, 遇到第一行"干净"的即停止。

用法:
    python trim_map.py <输入> <输出> [--sat 0.55] [--frac 0.25]
"""
import sys

from PIL import Image


def row_stats(im, sat_thresh=0.55):
    """返回每行的高饱和像素占比。"""
    import colorsys
    w, h = im.size
    px = im.load()
    # 抽样: 每行取 120 个采样点即可判断 (全扫太慢)
    step_x = max(1, w // 120)
    out = []
    for y in range(h):
        hi = 0
        cnt = 0
        for x in range(0, w, step_x):
            r, g, b = px[x, y][:3]
            cnt += 1
            _, ll, ss = colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0)
            if ss > sat_thresh and 0.10 < ll < 0.92:
                hi += 1
        out.append(hi / max(cnt, 1))
    return out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return
    src, dst = sys.argv[1], sys.argv[2]

    sat = 0.55
    frac = 0.25
    for a in sys.argv[3:]:
        if a.startswith('--sat='):
            sat = float(a.split('=')[1])
        if a.startswith('--frac='):
            frac = float(a.split('=')[1])

    im = Image.open(src).convert('RGB')
    w, h = im.size
    rs = row_stats(im, sat)

    # 从上往下找第一条干净行
    top = 0
    while top < h and rs[top] > frac:
        top += 1
    # 从下往上找
    bot = h - 1
    while bot > top and rs[bot] > frac:
        bot -= 1

    if top == 0 and bot == h - 1:
        print('没有检测到边缘填充带 (未裁剪)')
        im.save(dst, quality=92)
    else:
        # 上下各留 1 行余量, 避免残留
        t = max(0, top)
        b = min(h, bot + 1)
        im.crop((0, t, w, b)).save(dst, quality=92)
        print('裁剪: 上 %d 行, 下 %d 行 -> %dx%d' % (t, h - b, w, b - t))

    out = Image.open(dst)
    print('输出 %s  %dx%d  比例 %.3f'
          % (dst, out.size[0], out.size[1], out.size[0] / out.size[1]))


if __name__ == '__main__':
    main()
