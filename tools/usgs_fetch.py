"""从 USGS Astropedia 批量获取官方全球图。

★★ 本文件记录了一条重要的方法论转变。

之前的三条失败路径 (全部浪费了大量时间):
  1. **猜 Commons 分类名** —— "Maps of Ceres (dwarf planet)" 之类,
     全猜错, 脚本报"分类不存在", 于是误判成"该天体没有全球图"。
  2. **猜文件名** —— 按 USGS 命名习惯组合, 全部 404。
  3. **关键词搜索 + 启发式过滤** —— 能搜到东西, 但混入大量
     局部图 / 带标注图 / 伪彩图 / 艺术品, 靠黑名单和白名单
     永远穷举不完, 而且有些"看着像全球图"的其实是椭圆投影。

正确的路径:
  **找到原始发布方的目录**。行星全球图的原始发布方是
  USGS Astrogeology 的 Astropedia:
    * 它有服务端渲染的**完整目录** (1000 条/页, 可枚举)
    * 命名规范, 且**产品名里直接写明投影**
      (如 `callisto_galileo_voyager_simple_cylindrical_global_map`
        —— "simple cylindrical" 就是等距柱状)
    * 每个数据集页面给出 CKAN 直链, 可直接下载大图

教训: 找不到资源时, 不要继续优化"搜索算法", 而应该
      **去找这个领域的数据权威机构, 从它的目录入手**。

用法:
    python usgs_fetch.py --probe            解析全部目标数据集, 列出直链
    python usgs_fetch.py --download         下载 (存到 D:\\tmp\\usgs)
"""
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request

BASE = 'https://astrogeology.usgs.gov'
UA = {'User-Agent': 'SolarSystemEdu/1.0 (educational; planetary cartography)'}
OUTDIR = r'D:\tmp\usgs'

# 目标天体 -> USGS 数据集名
# 依据: 从 usgs_catalog.py --global 输出的 84 条全球图产品中挑选
TARGETS = {
    'europa':   'europa_voyager_galileo_ssi_global_mosaic_500m',
    'ganymede': 'ganymede_controlled_color_photomosaic_map',
    'callisto': 'callisto_galileo_voyager_simple_cylindrical_global_map',
    'io':       'io_voyager_galileo_image_mosaic_globe',
    'triton':   'triton_controlled_photomosaic',
    'phobos':   'phobos_viking_global_mosaic_5m',
    'eros':     'near_msi_albedo_mosaics',
    'vesta':    'vesta_dawn_fc_hamo_global_mosaic_60m',
    'tethys':   'tethys_pictorial_map_and_controlled_photomosaic',
    'dione':    'dione_pictorial_map_and_controlled_photomosaic',
    'iapetus':  'iapetus_cassini_voyager_global_mosaic_803m',
    'rhea':     'rhea_cassini_voyager_global_mosaic_417m',
    'enceladus': 'enceladus_image_mosaic',
    'titan':    'titan_cassini_iss_near_global_mosaic_450m',
}

# 每个天体优先选用的文件名关键词 (按优先级)
PREFER = {
    'europa':   ['global_mosaic', 'mosaic', 'full'],
    'ganymede': ['ganymede', 'full'],
    'callisto': ['simp', 'full'],
    'io':       ['globe', 'full'],
    'triton':   ['full', 'triton'],
    'phobos':   ['full', 'phobos'],
    'eros':     ['full', 'albedo'],
    'vesta':    ['mosaic', 'full'],
}


def get(url, retries=4, timeout=70):
    for a in range(retries):
        try:
            req = urllib.request.Request(url, headers=UA)
            with urllib.request.urlopen(req, timeout=timeout) as r:
                return r.read()
        except urllib.error.HTTPError as e:
            if e.code in (429, 503) and a < retries - 1:
                time.sleep(2 * (a + 1)); continue
            return None
        except Exception:
            if a < retries - 1:
                time.sleep(2 * (a + 1)); continue
            return None
    return None


def parse_links(dataset):
    html = get('%s/search/map/%s' % (BASE, dataset))
    if not html:
        return []
    txt = html.decode('utf-8', 'ignore')
    return sorted(set(re.findall(
        r'https://astrogeology\.usgs\.gov/ckan/dataset/[a-f0-9-]+/resource/'
        r'[a-f0-9-]+/download/[^"\']+', txt)))


def choose(links, body):
    """按优先级挑主文件。

    ★★ USGS 的命名约定 (实测总结):
        <name>_100.jpg     低分辨率预览
        <name>_512.jpg     中分辨率
        <name>_1024.jpg    较高分辨率
        <name>_1km.jpg     实际产品分辨率 (可能非常大, 如 15559px)
        full.jpg           拼版海报 (常含多张子图 + 文字, **不是**单张全球图)
        browse.jpg         目录预览图
        thumb.png          缩略图

    初版把 'full' 放进 PREFER 首位, 于是多个天体都拿到了**海报拼版**
    —— 它们比例五花八门 (0.9 / 1.1 / 3.3), 因为那是多张图排在一起。
    正确做法: **先按"最像单张全球图"排序** (文件名含数字分辨率后缀),
    再考虑天体专属关键词。

    另外 `_100` 也不能要 —— 那是 100 像素宽的预览。取最大的数字后缀。
    """
    imgs = [l for l in links
            if l.lower().endswith(('.jpg', '.jpeg', '.png'))]
    if not imgs:
        return None

    def score(l):
        fn = l.split('/download/')[-1].lower()
        if any(k in fn for k in ('thumb', 'browse', 'preview')):
            return -1e9
        # full.jpg 是海报拼版 -> 重罚
        if fn.startswith('full') or '_full' in fn:
            return -500
        sc = 0.0
        # 数字分辨率后缀越大越好
        m = re.findall(r'_(\d{2,5})\.(?:jpg|jpeg|png)$', fn)
        if m:
            sc += float(m[0])
        # 含 km / m 这种实际分辨率单位的更可能是真产品
        if re.search(r'_\d+(?:km|m)\.', fn):
            sc += 3000
        # 天体专属关键词加分
        for k in PREFER.get(body, []):
            if k in fn:
                sc += 400
        return sc

    ranked = sorted(imgs, key=score, reverse=True)
    return ranked[0] if ranked and score(ranked[0]) > -1e8 else None


def check_ratio(data, tol=0.22):
    """检查图像是否接近 2:1 等距柱状。返回 (ok, w, h)。"""
    try:
        import io
        from PIL import Image
        Image.MAX_IMAGE_PIXELS = None
        im = Image.open(io.BytesIO(data))
        w, h = im.size
        if w < 800:
            return (False, w, h)
        return (abs(w / h - 2.0) <= tol, w, h)
    except Exception:
        return (False, 0, 0)


def rank_candidates(links, body):
    """按"最可能是单张全球图"排序。"""
    imgs = [l for l in links
            if l.lower().endswith(('.jpg', '.jpeg', '.png'))]

    def score(l):
        fn = l.split('/download/')[-1].lower()
        if any(k in fn for k in ('thumb', 'browse', 'preview')):
            return -1e9
        sc = 0.0
        # 分辨率后缀越大越好
        m = re.findall(r'_(\d{2,5})\.(?:jpg|jpeg|png)$', fn)
        if m:
            sc += float(m[0])
        # 带实际分辨率单位 (如 _500m / _1km) 的最可能是真产品
        if re.search(r'_\d+(?:km|m)(?:_|\.)', fn):
            sc += 5000
        # 天体关键词
        for k in PREFER.get(body, []):
            if k in fn:
                sc += 400
        # 海报/拼版降权 (但保留, 因为它们有时是唯一可用的)
        if fn.startswith('full') or '_full' in fn:
            sc -= 800
        return sc

    return sorted(imgs, key=score, reverse=True)


def main():
    os.makedirs(OUTDIR, exist_ok=True)
    do_dl = '--download' in sys.argv
    only = [a for a in sys.argv[1:] if not a.startswith('--')]

    report = {}
    for body, ds in TARGETS.items():
        if only and body not in only:
            continue
        print('=== %-10s %s' % (body, ds))
        links = parse_links(ds)
        if not links:
            print('   页面无链接 (数据集名可能不对)')
            report[body] = {'ok': False, 'why': 'no links'}
            continue

        # ★★ 逐个候选**下载后验比例**, 而不是先选一个就下。
        #
        #   原因: 同一数据集里的文件形形色色 —— 有海报拼版 (比例 0.9~3.3)、
        #   有极区图 (接近 1:1)、有真的全球图 (2:1)。光看文件名无法确定,
        #   必须看实际像素尺寸。
        #
        #   代价是要多下载几个, 但 USGS 不限流, 且每个文件通常在
        #   100KB–2MB 之间, 完全可接受。
        ranked = rank_candidates(links, body)
        print('   候选 %d 个, 依次检验:' % len(ranked))
        report[body] = {'dataset': ds,
                        'files': [l.split('/download/')[-1] for l in links]}

        picked = None
        for l in ranked[:6]:
            fn = l.split('/download/')[-1]
            if not do_dl:
                picked = l
                print('      (探测模式) %s' % fn[:60])
                break
            data = get(l, timeout=500)
            if not data or len(data) < 30000:
                print('      %-52s 太小/失败' % fn[:52])
                continue
            ok, w, h = check_ratio(data)
            if not ok:
                print('      %-52s %dx%d 比例%.2f 不符' % (fn[:52], w, h, (w/h if h else 0)))
                continue
            picked = l
            ext = os.path.splitext(fn)[1] or '.jpg'
            outp = os.path.join(OUTDIR, body + ext)
            with open(outp, 'wb') as f:
                f.write(data)
            print('      ✓ %-52s %dx%d  %.0f KB' % (fn[:52], w, h, len(data) / 1024))
            break

        report[body]['pick'] = picked
        report[body]['ok'] = picked is not None and do_dl
        time.sleep(0.8)

    with open(os.path.join(OUTDIR, 'report.json'), 'w', encoding='utf-8') as f:
        json.dump(report, f, ensure_ascii=False, indent=2)

    okn = sum(1 for v in report.values() if v.get('ok'))
    print()
    print('成功 %d / 目标 %d' % (okn, len(report)))


if __name__ == '__main__':
    main()
