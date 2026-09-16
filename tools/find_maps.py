"""用 Commons 的文件搜索 (namespace 6) 找全球图，并按质量门禁筛选。

★ 与前几版脚本的关键差别:
  之前用「分类列表」和「猜文件名」，两者都不稳:
    * 分类名靠猜 -> 全猜错, 误判成"没有图"
    * 文件名靠猜 -> 全不存在

  改用 `list=search&srnamespace=6` 直接搜**文件**, 这是最可靠的入口。
  搜索结果按相关度排序, 再叠加质量门禁筛选, 并且**逐个下载后做像素校验**
  (文件名骗得过规则, 骗不过像素)。

用法:
    python find_maps.py <天体id> <查询串> [--dl]
    python find_maps.py --batch          # 批量处理缺口清单
"""
import io
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

API = 'https://commons.wikimedia.org/w/api.php'
UA = {'User-Agent': 'SolarSystemEdu/1.0 (educational)'}
OUT = r'D:\tmp\smalltex'

_last = [0.0]


def q(params, retries=4):
    url = API + '?' + urllib.parse.urlencode(params)
    delay = 2.0
    for a in range(retries):
        dt = time.time() - _last[0]
        if dt < 1.1:
            time.sleep(1.1 - dt)
        _last[0] = time.time()
        try:
            with urllib.request.urlopen(
                    urllib.request.Request(url, headers=UA), timeout=50) as r:
                return json.load(r)
        except urllib.error.HTTPError as e:
            if e.code == 429 and a < retries - 1:
                time.sleep(delay); delay *= 2; continue
            raise
    return {}


def search_files(query, limit=30):
    d = q({'action': 'query', 'format': 'json', 'list': 'search',
           'srnamespace': 6, 'srlimit': limit, 'srsearch': query})
    return [x['title'] for x in d.get('query', {}).get('search', [])]


def sizes(titles):
    out = {}
    for i in range(0, len(titles), 20):
        d = q({'action': 'query', 'format': 'json', 'prop': 'imageinfo',
               'iiprop': 'size|url', 'titles': '|'.join(titles[i:i + 20])})
        for p in d.get('query', {}).get('pages', {}).values():
            ii = p.get('imageinfo', [{}])[0]
            if 'width' in ii:
                out[p['title']] = (ii['width'], ii['height'])
    return out


def thumb(title, width=2048):
    d = q({'action': 'query', 'format': 'json', 'prop': 'imageinfo',
           'iiprop': 'url', 'iiurlwidth': width, 'titles': title})
    for p in d.get('query', {}).get('pages', {}).values():
        ii = p.get('imageinfo', [{}])[0]
        u = ii.get('thumburl') or ii.get('url')
        if u:
            return u.split('?')[0]
    return None


def get(url):
    try:
        with urllib.request.urlopen(
                urllib.request.Request(url, headers=UA), timeout=180) as r:
            return r.read()
    except Exception:
        return None


# ---- 质量门禁 (文件名层) ----
BAD = (
    'dem', 'elevation', 'topograph', 'geolog', 'gravity', 'magnetic',
    'quadrangle', 'pole', 'shaded relief', 'mercator', 'regio',
    'annotated', 'false color', 'false-color', 'enhanced color',
    'color-coded', 'legend', 'diagram', 'sketch', 'schematic',
    'orbit', 'trajectory', 'timeline', 'chart',
    'artistic', 'artist', 'concept', 'illustration', 'poster', 'rendering',
    'impression', 'painting', 'drawing', 'circa',
    'cratername', 'crater name', 'nomenclature', 'graticule',
    'villa romana', 'archaeolog', 'statue', 'ancient', 'museum',
    'einstein', 'sky', 'closeup', 'close-up', 'detail of', 'portion',
    'limb', 'horizon', 'simulat', 'reconstruct',
)
GOOD = ('map', 'mosaic', 'global', 'basemap', 'base map', 'cylindrical',
        'usgs', 'controlled', 'color', 'colour')


def has_bad(low):
    """★★ 黑名单匹配必须区分 "annotated" 与 "unannotated"。

    初版用 `b in low` 简单子串匹配, 于是黑名单里的 'annotated'
    把 'Unannotated' 也一起拦掉了 —— 而 Unannotated 恰恰是我们**要**的:
    NASA 对每张全球图通常同时发布 Annotated(带地名/图例) 与
    Unannotated(纯净) 两个版本。

    'unannotated' 里包含 'annotated' 这个子串, 简单 `in` 无法区分。
    解法: 对每个黑名单词, 检查它是否作为**独立片段**出现
    (即不被字母直接黏连成另一个词)。
    """
    for b in BAD:
        i = low.find(b)
        while i >= 0:
            # 若该词前面紧跟字母(如 un+annotated), 视为另一个词, 跳过
            if i == 0 or not low[i - 1].isalpha():
                return True
            i = low.find(b, i + 1)
    return False


def name_score(title, w, h):
    low = title.lower()
    if has_bad(low):
        return -1e9
    if not low.endswith(('.jpg', '.jpeg', '.png')):
        return -1e9
    if abs(w / h - 2.0) > 0.15:
        return -1e9
    if w < 1000:
        return -1e9
    if not any(g in low for g in GOOD):
        return -1e9
    s = 0.0
    r = w / h
    s -= abs(r - 2.0) * 12.0
    for g in GOOD:
        if g in low:
            s += 2.0
    if 'usgs' in low or 'jpl' in low or 'nasa' in low:
        s += 3.0
    if w >= 2000:
        s += 2.0
    return s


# ---- 质量门禁 (像素层) ----
def pixel_ok(data):
    import colorsys
    try:
        from PIL import Image
        im = Image.open(io.BytesIO(data))
        im.draft('RGB', (900, 450))
        im = im.convert('RGB')
        im.thumbnail((900, 450))
    except Exception:
        return (False, '无法解码')
    px = list(im.getdata())
    n = max(len(px), 1)
    sat, hue, white = [], [0] * 12, 0
    for (r, g, b) in px:
        if r >= 245 and g >= 245 and b >= 245:
            white += 1
            continue
        hh, ll, ss = colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0)
        if 0.06 < ll < 0.94:
            sat.append(ss)
            if ss > 0.20:
                hue[int(hh * 12) % 12] += 1
    sat.sort()
    med = sat[len(sat) // 2] if sat else 0.0
    colored = sum(hue)
    spread = sum(1 for c in hue if c > colored * 0.03) if colored else 0
    if med > 0.35 and spread >= 4:
        return (False, '伪彩 (饱和%.2f 跨%d色相)' % (med, spread))
    if spread >= 6:
        return (False, '色相跨%d区间' % spread)
    if white / n * 100 > 12:
        return (False, '纯白%.0f%%' % (white / n * 100))
    hi = sum(1 for s in sat if s > 0.62) / n * 100
    if hi > 2.5 and med > 0.18:
        return (False, '高饱和%.1f%% 疑文字标注' % hi)
    return (True, '')


def try_body(body, queries, rank=0):
    """为某天体找图。rank 用于取第 N 个候选 (跳过不合格的)。"""
    cands = []
    for query in queries:
        try:
            cands += search_files(query)
        except Exception:
            pass
    cands = list(dict.fromkeys(cands))
    if not cands:
        print('   搜索无结果')
        return False

    inf = sizes(cands)
    ranked = []
    for t in cands:
        w, h = inf.get(t, (0, 0))
        if not w:
            continue
        sc = name_score(t, w, h)
        if sc > -1e8:
            ranked.append((sc, t, w, h))
    ranked.sort(reverse=True)
    # 附加: 也保留一批"比例合格但缺关键词"的候选作兜底
    fallback = []
    for t in cands:
        w, h = inf.get(t, (0, 0))
        if w and abs(w / h - 2.0) < 0.15 and w >= 1000:
            low = t.lower()
            if not has_bad(low) and low.endswith(('.jpg', '.jpeg', '.png')):
                fallback.append((0.0, t, w, h))
    allc = ranked + [f for f in fallback if f[1] not in {r[1] for r in ranked}]

    print('   候选 %d 个 (含兜底)' % len(allc))
    for sc, t, w, h in allc[:8]:
        print('      [%6.1f] %-58s %dx%d' % (sc, t[5:63], w, h))

    for sc, t, w, h in allc:
        url = thumb(t, 2048)
        if not url:
            continue
        data = get(url)
        if not data or len(data) < 40000:
            continue
        ok, why = pixel_ok(data)
        if not ok:
            print('      弃用 %s -- %s' % (t[5:55], why))
            continue
        out = os.path.join(OUT, 'real_%s.jpg' % body)
        with open(out, 'wb') as f:
            f.write(data)
        with open(os.path.join(OUT, 'src_%s.txt' % body), 'w',
                  encoding='utf-8') as f:
            f.write('Wikimedia Commons: %s\n' % t)
        print('      ✓ 采用 %s  %dx%d  %.0f KB'
              % (t[5:60], w, h, len(data) / 1024))
        return True
    return False


BATCH = {
    'ceres':  ['Ceres Dawn global color map', 'Ceres global mosaic Dawn',
               'Ceres atlas Dawn'],
    'europa': ['Europa global color map', 'Europa mosaic Galileo USGS',
               'Europa cylindrical map'],
    'eros':   ['Eros global map NEAR', '433 Eros map cylindrical',
               'Eros mosaic NEAR Shoemaker'],
    'proteus': ['Proteus global mosaic Voyager', 'Proteus cylindrical map',
                'Proteus NEptune moon map'],
    'ryugu':  ['Ryugu global map Hayabusa2', 'Ryugu shape model map',
               '162173 Ryugu map'],
    'io':     ['Io global color map USGS', 'Io Galileo mosaic map',
               'Io cylindrical map'],
    'ganymede': ['Ganymede global color map USGS', 'Ganymede mosaic map',
                 'Ganymede cylindrical map'],
    'triton': ['Triton global color map USGS', 'Triton Voyager mosaic map',
               'Triton cylindrical map'],
    'callisto': ['Callisto global color map USGS', 'Callisto mosaic map'],
}


def main():
    if '--batch' in sys.argv:
        only = [a for a in sys.argv[1:] if not a.startswith('--')]
        for body, qs in BATCH.items():
            if only and body not in only:
                continue
            print('=== %s' % body)
            try:
                ok = try_body(body, qs)
                print('   => %s' % ('成功' if ok else '未取到'))
            except Exception as e:
                print('   异常: %s' % e)
        return

    if len(sys.argv) < 3:
        print(__doc__)
        return
    body = sys.argv[1]
    queries = [sys.argv[2]]
    print('=== %s' % body)
    print('   => %s' % ('成功' if try_body(body, queries) else '未取到'))


if __name__ == '__main__':
    main()
