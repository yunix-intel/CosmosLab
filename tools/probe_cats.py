"""探测 Commons 上真实存在的分类名（避免靠猜）。

★ 之前的教训:
  我一直在**猜**分类名 ("Maps of Ceres (dwarf planet)"), 全猜错了,
  于是脚本报告"分类为空或不存在", 我就误判成"该天体没有全球图"。

  正确做法: 用 allcategories 的**前缀搜索**列出实际存在的分类名,
  再从中挑。这类"先枚举再选择"的思路对任何有层级的目录都适用。

用法:
    python probe_cats.py <关键词> [更多关键词...]
"""
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

API = 'https://commons.wikimedia.org/w/api.php'
UA = {'User-Agent': 'SolarSystemEdu/1.0'}

_last = [0.0]


def q(params, retries=4):
    """带限流的请求。Commons 对高频访问会返 429。"""
    url = API + '?' + urllib.parse.urlencode(params)
    delay = 2.0
    for a in range(retries):
        dt = time.time() - _last[0]
        if dt < 1.1:
            time.sleep(1.1 - dt)
        _last[0] = time.time()
        try:
            with urllib.request.urlopen(
                    urllib.request.Request(url, headers=UA), timeout=45) as r:
                return json.load(r)
        except urllib.error.HTTPError as e:
            if e.code == 429 and a < retries - 1:
                time.sleep(delay); delay *= 2; continue
            raise
    raise RuntimeError('重试耗尽')


def allcats(prefix_search):
    """用 acsearch 模糊查分类名。"""
    d = q({'action': 'query', 'format': 'json', 'list': 'allcategories',
           'aclimit': 30, 'acmin': 1, 'acsearch': prefix_search})
    return [c['*'] for c in d.get('query', {}).get('allcategories', [])]


def cat_members(cat, limit=60):
    d = q({'action': 'query', 'format': 'json', 'list': 'categorymembers',
           'cmtitle': 'Category:' + cat, 'cmlimit': limit, 'cmtype': 'file'})
    return [x['title'] for x in d.get('query', {}).get('categorymembers', [])]


def main():
    kws = sys.argv[1:]
    if not kws:
        kws = ['Ceres', 'Europa', 'Eros', 'Proteus', 'Ryugu']

    for kw in kws:
        print(f'===== {kw}')
        try:
            cats = allcats(kw + ' map')
        except Exception as e:
            print(f'   查询失败: {e}')
            continue
        # 只保留与"地图/影像"相关的
        rel = [c for c in cats
               if any(k in c.lower() for k in ('map', 'mosaic', 'image', 'photo'))]
        if not rel:
            print('   (无明显相关分类)')
            continue
        for c in rel[:6]:
            print(f'   {c}')
            try:
                mem = cat_members(c, 40)
                if mem:
                    for t in mem[:6]:
                        print(f'        {t[5:74]}')
                    if len(mem) > 6:
                        print(f'        ... 共 {len(mem)} 个文件')
            except Exception as e:
                print(f'        读取失败: {e}')


if __name__ == '__main__':
    main()
