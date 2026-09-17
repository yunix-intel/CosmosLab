"""下载宇宙视图里具名天体的真实观测图像。

★★ 为什么需要:
  宇宙视图里的 10 个具名天体 (银河系、M31、M33、大小麦哲伦云、
  室女座团、M87/M86/M49/M104) 此前**没有任何图像** —— 画面上只有
  粒子点与文字标签。而这些天体都有权威的真实观测图像可配。

★ 图源优先级:
  1. **NASA Images API** (images-api.nasa.gov) —— 官方图像库,
     JSON 结构干净, 有稳定的 nasa_id 与多分辨率资产
  2. **ESO 公开图库** (cdn.eso.org) —— 南天体的最佳来源
     (LMC/SMC/M104 都在南天)

★ 关于"挑哪张":
  不用模糊搜索结果的第一条 —— 实测搜 "Sombrero galaxy" 会返回
  记录片《History of Hubble Space Telescope》这类无关项。
  改为**按天体逐个指定候选 nasa_id 并下载后目视确认**。
  每个天体的候选都写上挑选理由 (波段 / 探测器 / 是否为经典图)。

用法:
    python fetch_galaxies.py --probe        只列出候选
    python fetch_galaxies.py <id> [...]     下载指定天体
    python fetch_galaxies.py --all          下载全部
"""
import json
import os
import sys
import urllib.parse
import urllib.request

UA = {'User-Agent': 'SolarSystemEdu/1.0 (educational; NASA/ESA open data)'}
OUT = r'D:\tmp\solar-system-cpp\assets\galaxy'
META = r'D:\tmp\solar-system-cpp\assets\galaxy\SOURCES.txt'

# 天体 -> 候选列表。每项 (nasa_id, 说明)
# ★ 说明里记录为什么选它, 便于日后核对与替换。
CANDIDATES = {
    'andromeda': [
        ('PIA04921', 'GALEX 紫外全景 — 完整盘面, 适合展示星系形态'),
        ('GSFC_20171208_Archive_e000833', 'Hubble 高清全景 (PHAT 巡天)'),
    ],
    'triangulum': [
        ('PIA11969', 'Spitzer 红外 — 完整盘面, 尘埃结构清晰'),
        ('PIA03033', 'GALEX 紫外 — 恒星形成区分布'),
    ],
    'lmc': [
        ('PIA09071', 'Spitzer 红外 — 大麦哲伦云的恒星形成区'),
        ('GSFC_20171208_Archive_e001642', 'Hubble 局部特写'),
    ],
    'smc': [
        ('PIA09071', '（同 LMC, 待换）'),
    ],
    'm87': [
        ('PIA23122', '★ 事件视界望远镜 (EHT) 2019 — 人类首张黑洞照片'),
        ('PIA15418', 'Hubble — M87 相对论性喷流, 长约 5000 光年'),
    ],
    'm104': [
        ('PIA15226', 'Spitzer 红外 — 草帽星系的尘埃环'),
    ],
    'm86': [],
    'm49': [],
    'virgo_center': [],
}

# ESO 图库候选 (id -> (eso_id, 说明))
ESO_CANDIDATES = {
    'smc': ('eso0102a', 'ESO — 小麦哲伦云 (约 20 万光年外)'),
    'lmc': ('eso0101a', 'ESO — 大麦哲伦云'),
    'm104': ('eso0001a', 'ESO/VLT — 草帽星系'),
    'm87': ('eso1820a', 'ESO/VLT — M87 星系与喷流'),
    'm49': ('eso9905a', 'ESO — M49'),
}


def nasa_asset(nasa_id, width=1600):
    """取 NASA 图像的下载 URL。优先用 ~orig 或 ~large。"""
    url = 'https://images-api.nasa.gov/asset/' + urllib.parse.quote(nasa_id)
    try:
        d = json.load(urllib.request.urlopen(
            urllib.request.Request(url, headers=UA), timeout=45))
    except Exception as e:
        return None, 'asset 查询失败: %s' % str(e)[:50]
    items = d.get('collection', {}).get('items', [])
    if not items:
        return None, 'asset 为空'
    # 优先 orig, 其次 large, 再次任意 jpg
    pref = {'~orig': 0, '~large': 1, '~medium': 2, '~small': 3}
    best, bestr = None, 99
    for it in items:
        href = it.get('href', '')
        if not href.lower().endswith(('.jpg', '.jpeg', '.png')):
            continue
        for k, r in pref.items():
            if k in href and r < bestr:
                best, bestr = href, r
    if not best:
        best = items[-1].get('href')
    return best, ''


def down(url, out):
    try:
        with urllib.request.urlopen(
                urllib.request.Request(url, headers=UA), timeout=300) as r:
            d = r.read()
        if len(d) < 20000:
            return 0
        with open(out, 'wb') as f:
            f.write(d)
        return len(d)
    except Exception:
        return 0


def main():
    os.makedirs(OUT, exist_ok=True)
    only = [a for a in sys.argv[1:] if not a.startswith('--')]
    do_all = '--all' in sys.argv

    if '--probe' in sys.argv or (not only and not do_all):
        print('候选清单:')
        for body, cands in CANDIDATES.items():
            print('  %s:' % body)
            if not cands:
                print('     (无候选 — 待补)')
            for nid, why in cands:
                print('     %-32s %s' % (nid, why))
        return

    targets = list(CANDIDATES) if do_all else only
    log = []
    for body in targets:
        print('=== %s' % body)
        got = False
        for nid, why in CANDIDATES.get(body, []):
            url, err = nasa_asset(nid)
            if not url:
                print('   %-32s %s' % (nid, err))
                continue
            ext = os.path.splitext(url)[1].lower() or '.jpg'
            out = os.path.join(OUT, '%s%s' % (body, ext))
            n = down(url, out)
            if n:
                print('   ✓ %-32s %.0f KB  (%s)' % (nid, n / 1024, why[:30]))
                log.append('%s = NASA %s  (%s)' % (body, nid, why))
                got = True
                break
            print('   ✗ %-32s 下载失败' % nid)

        if not got and body in ESO_CANDIDATES:
            eso_id, why = ESO_CANDIDATES[body]
            url = 'https://cdn.eso.org/images/large/%s.jpg' % eso_id
            out = os.path.join(OUT, '%s.jpg' % body)
            n = down(url, out)
            if n:
                print('   ✓ ESO %-28s %.0f KB  (%s)' % (eso_id, n / 1024, why[:30]))
                log.append('%s = ESO %s  (%s)' % (body, eso_id, why))
                got = True

        if not got:
            print('   — 未取到')

    if log:
        with open(META, 'a', encoding='utf-8') as f:
            f.write('\n'.join(log) + '\n')
        print()
        print('来源已追加到 %s' % META)


if __name__ == '__main__':
    main()
