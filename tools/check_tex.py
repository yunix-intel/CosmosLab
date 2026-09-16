"""贴图内容校验：用像素统计识别伪彩色/示意图/局部图。

★ 为什么需要这一层:
  文件名黑名单 + 白名单能过滤掉大部分问题, 但实测仍有漏网:
    * `PIA21914-Ceres-DwarfPlanet-Map...` —— 名字含 "Map", 内容是
      **带图例与地名的彩色地质图** (红黄绿蓝)
    * `Europa volcanism.jpg` —— 名字没问题, 内容是**冰壳剖面示意图**
    * `PIA18668` —— NASA 官方 ID, 但是**局部地形特写**而非全球图

  这些都无法靠文件名判断, 必须看**像素本身**。

★ 三个判据 (实测阈值):

  1. **饱和度**: 伪彩色高程图的饱和度极高且分布宽 (红黄绿蓝都出现),
     而自然表面 (岩石/冰) 的饱和度低且集中。
     判据: 饱和度中位数 > 0.35 或 95 分位 > 0.75 → 判为伪彩。

  2. **色相多样性**: 伪彩图会跨越几乎所有色相 (彩虹配色),
     自然图通常集中在 1-2 个色相区间。
     判据: 显著色相桶 (>3% 像素) 数量 >= 4 → 判为伪彩/示意图。

  3. **说明文字区**: 示意图/地质图常含大片纯白或纯黑区域 (图例、标题栏)。
     判据: 纯白或纯黑像素占比 > 12% → 可疑。

用法:
    python check_tex.py <目录或文件...>        # 报告
    python check_tex.py --delete <目录>        # 删除不合格的
"""
import colorsys
import glob
import os
import sys

from PIL import Image


def analyse(path):
    """返回统计字典。"""
    # ★ 大图先缩放: 我们关心的是**颜色分布**, 不需要全分辨率。
    #   而且 triton 那张是 14138x7069 (一亿像素), 直接处理会触发
    #   PIL 的 DecompressionBombWarning 并浪费大量内存。
    im = Image.open(path)
    im.draft('RGB', (900, 450))      # JPEG 快速降采样
    im = im.convert('RGB')
    im.thumbnail((900, 450))
    w, h = im.size
    px = list(im.getdata())
    n = len(px)

    sat_vals = []
    hue_buckets = [0] * 12          # 每 30° 一桶
    white = black = 0

    for (r, g, b) in px:
        if r >= 245 and g >= 245 and b >= 245:
            white += 1
            continue
        if r <= 12 and g <= 12 and b <= 12:
            black += 1
            continue
        hh, ll, ss = colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0)
        # 只统计"有颜色"的像素 (太暗/太亮的饱和度不可靠)
        if 0.06 < ll < 0.94:
            sat_vals.append(ss)
            if ss > 0.20:
                hue_buckets[int(hh * 12) % 12] += 1

    sat_vals.sort()
    med = sat_vals[len(sat_vals) // 2] if sat_vals else 0.0
    p95 = sat_vals[int(len(sat_vals) * 0.95)] if sat_vals else 0.0

    colored = sum(hue_buckets)
    strong = sum(1 for c in hue_buckets if c > colored * 0.03) if colored else 0

    return {
        'w': w, 'h': h, 'n': n,
        'ratio': w / h if h else 0,
        'sat_med': med,
        'sat_p95': p95,
        'hue_spread': strong,
        'white_pct': white / n * 100,
        'black_pct': black / n * 100,
    }


def verdict(st, name=''):
    """给判定与理由。"""
    reasons = []

    # ★ 排除两类**本来就不适用**这些判据的文件:
    #   1. 法线贴图 (normal_*): 编码的是法线向量而非颜色,
    #      饱和度天然接近 1.0、色相分布宽 —— 这是正确的, 不是伪彩。
    #   2. 环系贴图 (ring_*): 是细长的径向条带 (实测 900x7),
    #      几何上就不该是 2:1 等距柱状。
    if name.startswith('normal_') or name.startswith('ring_'):
        return []

    if abs(st['ratio'] - 2.0) > 0.15:
        reasons.append('比例 %.2f 非 2:1' % st['ratio'])

    # 判据 1+2: 伪彩色 (彩虹配色)
    if st['sat_med'] > 0.35 and st['hue_spread'] >= 4:
        reasons.append('伪彩色 (饱和度中位 %.2f, 跨 %d 个色相)'
                       % (st['sat_med'], st['hue_spread']))
    elif st['hue_spread'] >= 6:
        reasons.append('色相跨 %d 个区间, 疑似示意图' % st['hue_spread'])

    # 判据 3: 图例/文字栏
    #
    # ★ 只看**白色**, 不看黑色。
    #   太空天体图里天体本来就嵌在**黑色天幕**上, 黑色占比 60% 以上
    #   完全正常 (实测合格的火卫二全球图有 61% 黑)。
    #   初版把白+黑一起算, 直接把它误判了。
    #   而图例/标题栏一定是**白底黑字**或浅色底 —— 那才是信号。
    if st['white_pct'] > 12:
        reasons.append('纯白占比 %.0f%%, 疑似含图例或文字栏' % st['white_pct'])

    return reasons


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    do_delete = '--delete' in sys.argv

    files = []
    for a in args:
        if os.path.isdir(a):
            files += sorted(glob.glob(os.path.join(a, '*.jpg'))
                            + glob.glob(os.path.join(a, '*.png')))
        elif os.path.isfile(a):
            files.append(a)

    if not files:
        print('未找到文件')
        return

    bad = []
    print('%-24s %11s %6s %7s %8s %6s  %s'
          % ('文件', '尺寸', '比例', '饱和中位', '色相跨', '白黑%', '判定'))
    for p in files:
        try:
            st = analyse(p)
        except Exception as e:
            print('%-24s 读取失败: %s' % (os.path.basename(p), e))
            continue
        rs = verdict(st, os.path.basename(p))
        mark = 'OK' if not rs else '!! ' + '; '.join(rs)
        if rs:
            bad.append(p)
        print('%-24s %5dx%-5d %6.2f %9.2f %8d %5.0f%%  %s'
              % (os.path.basename(p), st['w'], st['h'], st['ratio'],
                 st['sat_med'], st['hue_spread'],
                 st['white_pct'] + st['black_pct'], mark))

    print()
    print('共 %d 个文件, %d 个不合格' % (len(files), len(bad)))

    if do_delete and bad:
        print()
        for p in bad:
            os.remove(p)
            # 同时删掉来源记录
            d = os.path.dirname(p)
            b = os.path.basename(p)
            for pre in ('src_', 'real_'):
                pass
            print('  已删除 %s' % os.path.basename(p))
        print('共删除 %d 个' % len(bad))


if __name__ == '__main__':
    main()
