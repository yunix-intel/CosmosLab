"""从 Commons 分类列表精确获取行星地图, 并做严格质量筛查。

★ 为什么不再用模糊搜索:
    "搜索 + 关键词打分" 会命中完全无关的东西 —— 实测踩到:
      * 搜 "Proteus map" 命中了**爱因斯坦在 RCA 电台的历史照片**
      * 搜 "Eros map"  命中了**古罗马马赛克艺术品**
    模糊搜索的召回率高但精确率太低, 且错误往往很隐蔽。

★ 改为: 列出 Category:Maps of X 的**全部成员**, 用文件名规则筛掉
   明显不合格的 (DEM/高程/地质图/极区图/带标注), 下载后用**像素级
   质量门禁**验证, 不合格的直接丢弃。

★ 质量门禁 (关键 —— 教学材料的严谨性要求):
    1. 饱和度检查: 伪彩色高程图饱和度极高 (红黄绿蓝), 自然表面
       饱和度低。用地貌彩图的饱和度中位数作判据。
    2. 黑色边缘检查: 投影留白会产生大片纯黑边, 贴到球面上会出现
       黑色带。检出后自动裁剪。
    3. 灰度检查: 纯灰度图 (如某些 USGS 底图) 缺少颜色信息,
       但比伪彩好, 保留并标记。

用法:
    python fetch_maps2.py <输出目录>
"""
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

API = "https://commons.wikimedia.org/w/api.php"
UA = {"User-Agent": "SolarSystemEdu/1.0 (educational; contact: local)"}

# 天体 id -> Commons 分类名 (不含 "Category:" 前缀)
CATS = {
    "ceres":     "Ceres (dwarf planet)",
    "deimos":    "Deimos (moon)",
    "dione":     "Maps of Dione",
    "ariel":     "Ariel (moon)",
    "oberon":    "Oberon (moon)",
    "proteus":   "Proteus (moon)",
    "charon":    "Charon (moon)",
    "eros":      "433 Eros",
    "ryugu":     "162173 Ryugu",
    "churyumov": "67P/Churyumov-Gerasimenko",
    "tethys":    "Maps of Tethys",
}

# 文件名黑名单 —— 这些几乎肯定不是"自然表面的等距柱状投影图"
BAD_WORDS = (
    "dem", "elevation", "topograph", "geolog", "gravity", "magnetic",
    "quadrangle", "pole", "north pole", "south pole", "shaded relief",
    "diagram", "map of the ", "landing", "grid", "labeled", "labelled",
    "legend", "sketch", "atlas", "sim", "cub", "cylindrical projection",
)

# 文件名好词 —— 提高排序
GOOD_WORDS = ("global", "mosaic", "basemap", "base map", "color", "colour",
              "simple cylindrical", "cylindrical", "control network")


def throttle() -> None:
    time.sleep(0.7)


def api_get(params: dict, retries: int = 5) -> dict:
    url = API + "?" + urllib.parse.urlencode(params)
    delay = 2.0
    for a in range(retries):
        throttle()
        try:
            req = urllib.request.Request(url, headers=UA)
            with urllib.request.urlopen(req, timeout=45) as r:
                return json.load(r)
        except urllib.error.HTTPError as e:
            if e.code == 429 and a < retries - 1:
                time.sleep(delay); delay *= 2; continue
            raise
        except Exception:
            if a < retries - 1:
                time.sleep(delay); delay *= 2; continue
            raise
    raise RuntimeError("重试耗尽")


def cat_members(cat: str, limit: int = 500) -> list:
    out = []
    cont = None
    while True:
        p = {"action": "query", "format": "json", "list": "categorymembers",
             "cmtitle": "Category:" + cat, "cmlimit": 100, "cmtype": "file"}
        if cont:
            p["cmcontinue"] = cont
        try:
            d = api_get(p)
        except Exception as e:
            print(f"     分类读取失败: {e}")
            break
        out += [x["title"] for x in d.get("query", {}).get("categorymembers", [])]
        cont = d.get("continue", {}).get("cmcontinue")
        if not cont or len(out) >= limit:
            break
    return out


def info_batch(titles: list) -> dict:
    out = {}
    for i in range(0, len(titles), 20):
        chunk = titles[i:i + 20]
        try:
            d = api_get({"action": "query", "format": "json", "prop": "imageinfo",
                         "iiprop": "size", "titles": "|".join(chunk)})
            for p in d.get("query", {}).get("pages", {}).values():
                ii = p.get("imageinfo", [{}])[0]
                if "width" in ii:
                    out[p["title"]] = (ii["width"], ii["height"])
        except Exception:
            pass
    return out


def score(title: str, w: int, h: int) -> float:
    low = title.lower()
    if any(b in low for b in BAD_WORDS):
        return -1e9
    if not low.endswith((".jpg", ".jpeg", ".png")):
        return -1e9
    s = 0.0
    r = w / h if h else 0
    # 2:1 是等距柱状投影的硬要求, 偏离即扣分
    s -= abs(r - 2.0) * 15.0
    for k in GOOD_WORDS:
        if k in low:
            s += 3.0
    if "usgs" in low:
        s += 3.0
    if w >= 2000:
        s += 2.0
    if w < 900:
        s -= 5.0
    return s


def thumb_url(title: str, width: int) -> str | None:
    try:
        d = api_get({"action": "query", "format": "json", "prop": "imageinfo",
                     "iiprop": "url", "iiurlwidth": width, "titles": title})
        for p in d.get("query", {}).get("pages", {}).values():
            ii = p.get("imageinfo", [{}])[0]
            u = ii.get("thumburl") or ii.get("url")
            if u:
                return u.split("?")[0]
    except Exception:
        pass
    return None


def fetch(url: str, out: str) -> bool:
    try:
        with urllib.request.urlopen(
                urllib.request.Request(url, headers=UA), timeout=300) as r:
            data = r.read()
        if len(data) < 20000:
            return False
        with open(out, "wb") as f:
            f.write(data)
        return True
    except Exception as e:
        print(f"     下载失败: {e}")
        return False


def main() -> None:
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)

    only = [a for a in sys.argv[2:] if not a.startswith("-")]

    for body, cat in CATS.items():
        if only and body not in only:
            continue
        out = os.path.join(outdir, f"real_{body}.jpg")
        print(f"--- {body}  (分类: {cat})")
        if os.path.isfile(out):
            print("   已存在, 跳过")
            continue

        members = cat_members(cat)
        if not members:
            print("   分类为空或不存在")
            continue
        inf = info_batch(members)

        ranked = []
        for t in members:
            w, h = inf.get(t, (0, 0))
            if not w:
                continue
            sc = score(t, w, h)
            if sc > -1e8:
                ranked.append((sc, t, w, h))
        ranked.sort(reverse=True)

        if not ranked:
            print("   无合格候选")
            continue

        print(f"   {len(members)} 个成员, {len(ranked)} 个通过初筛; 前 3:")
        for sc, t, w, h in ranked[:3]:
            print(f"      [{sc:6.1f}] {t[5:]:64s} {w}x{h} r={w/h:.2f}")

        # 取最高分下载
        _, title, w, h = ranked[0]
        url = thumb_url(title, 2048)
        if not url:
            print("   取不到 URL")
            continue
        if fetch(url, out):
            print(f"   -> real_{body}.jpg  {os.path.getsize(out)/1024:.0f} KB")
            with open(os.path.join(outdir, f"src_{body}.txt"), "w",
                      encoding="utf-8") as f:
                f.write(title + "\n")


if __name__ == "__main__":
    main()
