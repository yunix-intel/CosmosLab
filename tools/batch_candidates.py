"""批量下载候选地图并生成对照表，供目视评估。

★ 为什么要目视:
  自动化的文件名规则与像素统计能筛掉大部分问题, 但**投影方式**和
  **内容是否真的是表面图**只有肉眼一看就清楚。
  实测教训: 像素检测放行过 "PIA20351-HAMO-EllipticalMap" ——
  它确实不是伪彩、不含标注, 是**真实的自然色全球图**,
  但它用的是**椭圆投影**, 直接贴到球面上会在极区畸变。

用法:
    python batch_candidates.py <天体id> <输出前缀> "<文件1>" "<文件2>" ...
"""
import io
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

API = 'https://commons.wikimedia.org/w/api.php'
UA = {'User-Agent': 'SolarSystemEdu/1.0 (educational)'}

_l = [0.0]


def q(params, retries=4):
    url = API + '?' + urllib.parse.urlencode(params)
    delay = 2.0
    for a in range(retries):
        dt = time.time() - _l[0]
        if dt < 1.1:
            time.sleep(1.1 - dt)
        _l[0] = time.time()
        try:
            with urllib.request.urlopen(
                    urllib.request.Request(url, headers=UA), timeout=50) as r:
                return json.load(r)
        except urllib.error.HTTPError as e:
            if e.code == 429 and a < retries - 1:
                time.sleep(delay); delay *= 2; continue
            raise
        except Exception:
            if a < retries - 1:
                time.sleep(delay); delay *= 2; continue
            raise
    return {}


def thumb(title, width=2560):
    d = q({'action': 'query', 'format': 'json', 'prop': 'imageinfo',
           'iiprop': 'url|size', 'iiurlwidth': width, 'titles': title})
    for p in d.get('query', {}).get('pages', {}).values():
        ii = p.get('imageinfo', [{}])[0]
        u = ii.get('thumburl') or ii.get('url')
        if u:
            return u.split('?')[0], ii.get('width'), ii.get('height')
    return None, 0, 0


def get(url):
    try:
        with urllib.request.urlopen(
                urllib.request.Request(url, headers=UA), timeout=240) as r:
            return r.read()
    except Exception as e:
        print('     下载失败: %s' % e)
        return None


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return
    body = sys.argv[1]
    outdir = sys.argv[2]
    titles = sys.argv[3:]

    os.makedirs(outdir, exist_ok=True)

    for i, t in enumerate(titles):
        if not t.startswith('File:'):
            t = 'File:' + t
        print('--- [%d] %s' % (i, t[5:70]))
        try:
            url, w, h = thumb(t, 2560)
        except Exception as e:
            print('     查询失败: %s' % e)
            continue
        if not url:
            print('     无 URL')
            continue
        data = get(url)
        if not data:
            continue
        ext = '.png' if url.lower().endswith('.png') else '.jpg'
        out = os.path.join(outdir, '%s_%d%s' % (body, i, ext))
        with open(out, 'wb') as f:
            f.write(data)
        print('     -> %s  %dx%d  %.0f KB'
              % (os.path.basename(out), w, h, len(data) / 1024))


if __name__ == '__main__':
    main()
