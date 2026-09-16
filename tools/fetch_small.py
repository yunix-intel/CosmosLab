"""为新增的卫星 / 矮行星 / 小行星 / 彗核下载 NASA/USGS 真实全球图。

复用第二轮验证过的流程:
  Commons API 搜索 -> 按 (比例=2:1, 关键词得分) 挑选 -> 下载缩略图
  -> 若不是 2:1 则交给 crop_eq.py 裁出等距柱状主图

用法:
    python fetch_small.py <输出目录>
"""
import json
import os
import sys
import urllib.parse
import urllib.error
import urllib.request

API = "https://commons.wikimedia.org/w/api.php"
UA = {"User-Agent": "SolarSystemEdu/1.0 (educational)"}

# 天体 id -> 候选搜索词 (按优先级)
TARGETS = {
    # ---- 矮行星 ----
    "ceres":     ["Ceres global map Dawn", "Ceres map USGS"],
    # ---- 火星卫星 ----
    "phobos":    ["Phobos global map", "Phobos map mosaic"],
    "deimos":    ["Deimos global map", "Deimos map mosaic"],
    # ---- 土星卫星 (Cassini / USGS) ----
    "mimas":     ["Mimas global map USGS", "Mimas map Cassini"],
    "tethys":    ["Tethys global map USGS", "Tethys map Cassini"],
    "dione":     ["Dione global map USGS", "Dione map Cassini"],
    "rhea":      ["Rhea global map USGS", "Rhea map Cassini"],
    "iapetus":   ["Iapetus global map USGS", "Iapetus map Cassini"],
    # ---- 天王星卫星 (Voyager 2 / USGS) ----
    "miranda":   ["Miranda global map USGS", "Miranda map Voyager"],
    "ariel":     ["Ariel global map USGS", "Ariel map Voyager"],
    "umbriel":   ["Umbriel global map USGS", "Umbriel map Voyager"],
    "titania":   ["Titania global map USGS", "Titania map Voyager"],
    "oberon":    ["Oberon global map USGS", "Oberon map Voyager"],
    # ---- 海王星卫星 ----
    "proteus":   ["Proteus global map", "Proteus map Voyager"],
    # ---- 冥王星卫星 (New Horizons) ----
    "charon":    ["Charon global map New Horizons", "Charon map mosaic"],
    # ---- 小行星 ----
    "vesta":     ["Vesta global map Dawn", "Vesta map USGS"],
    "eros":      ["Eros global map NEAR", "Eros map mosaic"],
    "bennu":     ["Bennu global map OSIRIS-REx", "Bennu map mosaic"],
    "ryugu":     ["Ryugu global map Hayabusa2", "Ryugu map mosaic"],
    # ---- 彗核 ----
    "churyumov": ["67P global map Rosetta", "Churyumov-Gerasimenko map"],
}

# 排除明显的非全球图
BAD = ("diagram", ".svg", ".pdf", "pole", "quadrangle", "geologic",
       "topographic", "shaded relief", "cylindrical", "simple cylindrical projection legend",
       # ★ 必须排除艺术品/考古: "Eros" 既是小行星也是古希腊爱神,
       #   搜索 "Eros map" 会命中意大利古罗马马赛克照片 (实测踩到)。
       "villa romana", "mosaic - ", "archaeolog", "museum", "statue",
       "painting", "ancient", "roman", "greek", "sculpture", "fresco")

GOOD_KW = ("map", "mosaic", "global", "usgs", "color", "cylindrical")

# ★ Commons 对匿名高频请求会返回 429。每次 API 调用之间留间隔,
#   并在 429 时按指数退避重试 —— 否则整批任务会在中途全部失败。
import time

_LAST_CALL = [0.0]
_MIN_INTERVAL = 0.8      # 秒


def _throttle() -> None:
    dt = time.time() - _LAST_CALL[0]
    if dt < _MIN_INTERVAL:
        time.sleep(_MIN_INTERVAL - dt)
    _LAST_CALL[0] = time.time()


def api_get(params: dict, retries: int = 5) -> dict:
    url = API + "?" + urllib.parse.urlencode(params)
    delay = 2.0
    for attempt in range(retries):
        _throttle()
        try:
            req = urllib.request.Request(url, headers=UA)
            with urllib.request.urlopen(req, timeout=45) as r:
                return json.load(r)
        except urllib.error.HTTPError as e:
            if e.code == 429 and attempt < retries - 1:
                time.sleep(delay)
                delay *= 2
                continue
            raise
        except Exception:
            if attempt < retries - 1:
                time.sleep(delay)
                delay *= 2
                continue
            raise
    raise RuntimeError("重试次数用尽")


def search(query: str, limit: int = 10) -> list:
    try:
        d = api_get({"action": "query", "format": "json", "list": "search",
                     "srnamespace": 6, "srlimit": limit, "srsearch": query})
        return [x["title"] for x in d.get("query", {}).get("search", [])]
    except Exception as e:
        print(f"     搜索失败: {e}")
        return []


def info_batch(titles: list) -> dict:
    """一次取多个文件的尺寸, 用于按 2:1 比例筛选。"""
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


def pick(cands: list) -> str | None:
    """优先挑尺寸接近 2:1 的全球图。"""
    inf = info_batch(cands)
    scored = []
    for t in cands:
        low = t.lower()
        if any(b in low for b in BAD):
            continue
        if not low.endswith((".jpg", ".jpeg", ".png")):
            continue
        w, h = inf.get(t, (0, 0))
        if w < 800:
            continue
        ratio = w / h if h else 0
        s = 0.0
        # 比例越接近 2:1 越好 (等距柱状投影的硬要求)
        s -= abs(ratio - 2.0) * 12.0
        for k in GOOD_KW:
            if k in low:
                s += 2.0
        if "usgs" in low:
            s += 2.0
        scored.append((s, t, w, h, ratio))
    if not scored:
        return None
    scored.sort(reverse=True)
    best = scored[0]
    print(f"     候选 {len(scored)} 个, 选中: {best[1][5:]}  "
          f"({best[2]}x{best[3]} r={best[4]:.2f})")
    return best[1]


def thumb_url(title: str, width: int) -> str | None:
    if not title.startswith("File:"):
        title = "File:" + title
    try:
        d = api_get({"action": "query", "format": "json", "prop": "imageinfo",
                     "iiprop": "url", "iiurlwidth": width, "titles": title})
        for p in d.get("query", {}).get("pages", {}).values():
            ii = p.get("imageinfo", [{}])[0]
            u = ii.get("thumburl") or ii.get("url")
            return u.split("?")[0] if u else None
    except Exception:
        pass
    return None


def fetch(url: str, out: str) -> bool:
    try:
        with urllib.request.urlopen(
                urllib.request.Request(url, headers=UA), timeout=300) as r:
            data = r.read()
        if len(data) < 15000:
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

    ok, fail = 0, []
    for body, queries in TARGETS.items():
        out = os.path.join(outdir, f"real_{body}.jpg")
        print(f"--- {body}")
        if os.path.isfile(out):
            print("   已存在, 跳过")
            ok += 1
            continue

        chosen = None
        for q in queries:
            cands = search(q)
            if not cands:
                continue
            chosen = pick(cands)
            if chosen:
                break
        if not chosen:
            print("   * 未找到合适全球图")
            fail.append(body)
            continue

        url = thumb_url(chosen, 2048)
        if not url:
            print("   取不到 URL")
            fail.append(body)
            continue
        if fetch(url, out):
            print(f"   -> real_{body}.jpg  {os.path.getsize(out) / 1024:.0f} KB")
            ok += 1
        else:
            fail.append(body)

    print(f"\n成功 {ok} / 共 {len(TARGETS)}")
    if fail:
        print("未获取:", ", ".join(fail))


if __name__ == "__main__":
    main()
