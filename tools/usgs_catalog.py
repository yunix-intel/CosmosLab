"""枚举 USGS Astropedia 的全部数据集，找出"全球图"级别的产品。

★★ 这是本任务真正的突破口。

之前的失败路径:
  1. 猜 Commons 分类名        -> 全猜错, 误判成"没有图"
  2. 猜文件名                 -> 全不存在
  3. 关键词搜索 + 启发式过滤   -> 命中的多是局部图/带标注图/伪彩图

正确的路径:
  **USGS Astrogeology 的 Astropedia 是这些全球图的原始发布方**,
  它有服务端渲染的完整目录, 而且每个数据集页面里都给出
  CKAN 直链:
      https://astrogeology.usgs.gov/ckan/dataset/<uuid>/resource/<uuid>/download/<file>

  先用 /search/results 枚举全部数据集 (带分页), 再按名字筛出
  "global" 类产品, 最后解析页面拿直链。

★ 为什么优先找 USGS 而不是 Commons:
  Commons 是二手聚合, 命名混乱、版本繁杂 (Annotated/Unannotated/
  Mercator/Elliptical/...), 且没有统一的"这是什么投影"的元数据。
  USGS 是原始发布方, 命名规范且产品定义清晰。

用法:
    python usgs_catalog.py --list            列出全部数据集名
    python usgs_catalog.py --global          只列出 global/mosaic 类
    python usgs_catalog.py --page <name>     解析某数据集页面的直链
"""
import json
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

BASE = 'https://astrogeology.usgs.gov'
UA = {'User-Agent': 'SolarSystemEdu/1.0 (educational; planetary map study)'}
CACHE = r'D:\tmp\usgs_catalog.json'


def get(url, retries=4):
    for a in range(retries):
        try:
            req = urllib.request.Request(url, headers=UA)
            with urllib.request.urlopen(req, timeout=70) as r:
                return r.read().decode('utf-8', 'ignore')
        except urllib.error.HTTPError as e:
            if e.code in (429, 503) and a < retries - 1:
                time.sleep(2 * (a + 1)); continue
            return ''
        except Exception:
            if a < retries - 1:
                time.sleep(2 * (a + 1)); continue
            return ''
    return ''


def crawl(max_pages=40):
    """枚举全部数据集名。分页参数是 'page'。"""
    names = set()
    for pg in range(1, max_pages + 1):
        url = '%s/search/results?q=x&page=%d' % (BASE, pg)
        html = get(url)
        if not html:
            break
        found = set(re.findall(r'/search/map/([a-z0-9/_-]+)', html))
        new = found - names
        names |= found
        print('  第 %2d 页: +%d (累计 %d)' % (pg, len(new), len(names)), flush=True)
        if not new:
            break
        time.sleep(0.7)
    return sorted(names)


def parse_dataset(name):
    """解析数据集页面, 提取标题与 CKAN 下载直链。"""
    url = '%s/search/map/%s' % (BASE, name)
    html = get(url)
    if not html:
        return None
    title = ''
    m = re.search(r'<title>(.*?)</title>', html, re.S)
    if m:
        title = re.sub(r'\s+', ' ', m.group(1)).strip()
        title = title.replace('Astropedia Lunar and Planetary Catalog', '').strip(' -')
    links = re.findall(
        r'https://astrogeology\.usgs\.gov/ckan/dataset/[a-f0-9-]+/resource/[a-f0-9-]+/download/[^"\']+',
        html)
    return {'name': name, 'title': title, 'links': sorted(set(links))}


def main():
    if '--list' in sys.argv or '--global' in sys.argv:
        import os
        if os.path.isfile(CACHE):
            names = json.load(open(CACHE, encoding='utf-8'))
            print('从缓存读取 %d 条' % len(names))
        else:
            print('枚举中 (每页约 0.7s)...')
            names = crawl()
            json.dump(names, open(CACHE, 'w', encoding='utf-8'),
                      ensure_ascii=False, indent=1)
            print('已缓存到 %s' % CACHE)

        if '--global' in sys.argv:
            # "全球图"级产品: global / mosaic / atlas 等
            key = ('global', 'mosaic', 'atlas', 'controlled_photomosaic',
                   'controlled_color')
            hit = [n for n in names
                   if any(k in n.lower() for k in key)
                   and 'quadrangle' not in n.lower()
                   and 'region' not in n.lower()
                   and 'area' not in n.lower()]
            print()
            print('=== 全球图级产品 (%d 条) ===' % len(hit))
            for h in hit:
                print('   ', h)
        else:
            print()
            for n in names:
                print(n)
        return

    if '--page' in sys.argv:
        i = sys.argv.index('--page')
        if i + 1 >= len(sys.argv):
            print('需要数据集名')
            return
        r = parse_dataset(sys.argv[i + 1])
        if not r:
            print('解析失败')
            return
        print('标题: %s' % r['title'])
        print('直链 %d 个:' % len(r['links']))
        for l in r['links']:
            print('   ', l.split('/download/')[-1], '   <-', l[:110])
        return

    print(__doc__)


if __name__ == '__main__':
    main()
