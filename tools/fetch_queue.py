"""多源行星地图采集器 —— 带限流、断点续传、多来源回退。

★ 为什么要多源:
    单一来源总有盲区。Commons 分类偶尔不全, NASA 图像库对某些小天体也
    没有全球图。多源回退能显著提高覆盖率。

来源优先级 (按"权威性 + 可用性"排序):
    1. NASA Images API  (images-api.nasa.gov) —— NASA 官方图像库,
       覆盖 photojournal / 各任务发布, 返回干净的 JSON, 限流宽松。
    2. Wikimedia Commons 分类 (maps of X) —— 聚合了 USGS/ESA/JAXA 的
       官方地图, 授权明确。
    3. Commons 搜索 —— 兜底。

★ 限流策略 (这是本文件的核心约束):
    * 每个 host 独立的最小请求间隔 (Commons 1.2s / NASA 0.5s)
    * 遇 429 指数退避 (2s → 4s → 8s → 16s), 最多 5 次
    * 每轮只处理 MAX_PER_RUN 个天体 —— 便于"隔一段时间分开取",
      避免长时间高频请求给服务器造成压力
    * 状态写入 state.json, 重复运行会跳过已完成的

★ 质量门禁:
    等距柱状投影要求比例严格 2:1。比例偏离的图贴到球面上会横竖不等变。
    门禁: |ratio - 2| > 0.15 直接弃用, 宁可留空用均匀色球。

用法:
    python fetch_queue.py <输出目录> [--max 3] [--list]
"""
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

UA = {"User-Agent": "SolarSystemEdu/1.0 (educational use; polite crawler)"}

# ---- 每个 host 的最小请求间隔 (秒) ----
HOST_INTERVAL = {
    "commons.wikimedia.org": 1.2,
    "images-api.nasa.gov": 0.5,
    "photojournal.jpl.nasa.gov": 1.0,
}
DEFAULT_INTERVAL = 1.0

_last_call = {}      # host -> 上次请求时间
MAX_ATTEMPTS = 5


def _throttle(url: str) -> None:
    host = urllib.parse.urlparse(url).netloc
    iv = HOST_INTERVAL.get(host, DEFAULT_INTERVAL)
    dt = time.time() - _last_call.get(host, 0.0)
    if dt < iv:
        time.sleep(iv - dt)
    _last_call[host] = time.time()


def http_json(url: str) -> dict | None:
    """带限流与指数退避的 JSON 请求。"""
    delay = 2.0
    for attempt in range(MAX_ATTEMPTS):
        _throttle(url)
        try:
            req = urllib.request.Request(url, headers=UA)
            with urllib.request.urlopen(req, timeout=60) as r:
                return json.load(r)
        except urllib.error.HTTPError as e:
            if e.code in (429, 503) and attempt < MAX_ATTEMPTS - 1:
                time.sleep(delay)
                delay *= 2
                continue
            return None
        except Exception:
            if attempt < MAX_ATTEMPTS - 1:
                time.sleep(delay)
                delay *= 2
                continue
            return None
    return None


def http_bytes(url: str) -> bytes | None:
    delay = 2.0
    for attempt in range(MAX_ATTEMPTS):
        _throttle(url)
        try:
            req = urllib.request.Request(url, headers=UA)
            with urllib.request.urlopen(req, timeout=300) as r:
                return r.read()
        except urllib.error.HTTPError as e:
            if e.code in (429, 503) and attempt < MAX_ATTEMPTS - 1:
                time.sleep(delay); delay *= 2; continue
            return None
        except Exception:
            if attempt < MAX_ATTEMPTS - 1:
                time.sleep(delay); delay *= 2; continue
            return None
    return None


# ---------------------------------------------------------------------------
#  来源 1: NASA Images API (官方图像库)
# ---------------------------------------------------------------------------

def nasa_search(query: str, limit: int = 40) -> list:
    url = ("https://images-api.nasa.gov/search?"
           + urllib.parse.urlencode({"q": query, "media_type": "image",
                                     "page_size": limit}))
    d = http_json(url)
    if not d:
        return []
    out = []
    for it in d.get("collection", {}).get("items", []):
        data = it.get("data", [{}])[0]
        links = it.get("links", [{}])
        thumb = links[0].get("href") if links else None
        out.append({
            "title": data.get("title", ""),
            "nasa_id": data.get("nasa_id", ""),
            "thumb": thumb,
            "desc": (data.get("description") or "")[:200],
        })
    return out


def nasa_asset_url(nasa_id: str) -> str | None:
    """取该条目的原始文件 URL (优先 jpg 大图)。"""
    d = http_json("https://images-api.nasa.gov/asset/" + urllib.parse.quote(nasa_id))
    if not d:
        return None
    best = None
    for it in d.get("collection", {}).get("items", []):
        href = it.get("href", "")
        low = href.lower()
        if not low.endswith((".jpg", ".jpeg", ".png")):
            continue
        if "thumb" in low or "~small" in low:
            continue
        # 优先 orig / large / medium
        rank = 0
        if "orig" in low: rank = 3
        elif "large" in low: rank = 2
        elif "medium" in low: rank = 1
        if best is None or rank > best[0]:
            best = (rank, href)
    return best[1] if best else None


# ---------------------------------------------------------------------------
#  来源 2/3: Wikimedia Commons
# ---------------------------------------------------------------------------

COMMONS_API = "https://commons.wikimedia.org/w/api.php"


def commons_cat_members(cat: str, limit: int = 300) -> list:
    out, cont = [], None
    while True:
        p = {"action": "query", "format": "json", "list": "categorymembers",
             "cmtitle": "Category:" + cat, "cmlimit": 100, "cmtype": "file"}
        if cont:
            p["cmcontinue"] = cont
        d = http_json(COMMONS_API + "?" + urllib.parse.urlencode(p))
        if not d:
            break
        out += [x["title"] for x in d.get("query", {}).get("categorymembers", [])]
        cont = d.get("continue", {}).get("cmcontinue")
        if not cont or len(out) >= limit:
            break
    return out


def commons_sizes(titles: list) -> dict:
    out = {}
    for i in range(0, len(titles), 20):
        chunk = titles[i:i + 20]
        d = http_json(COMMONS_API + "?" + urllib.parse.urlencode({
            "action": "query", "format": "json", "prop": "imageinfo",
            "iiprop": "size", "titles": "|".join(chunk)}))
        if not d:
            continue
        for p in d.get("query", {}).get("pages", {}).values():
            ii = p.get("imageinfo", [{}])[0]
            if "width" in ii:
                out[p["title"]] = (ii["width"], ii["height"])
    return out


def commons_thumb(title: str, width: int = 2048) -> str | None:
    d = http_json(COMMONS_API + "?" + urllib.parse.urlencode({
        "action": "query", "format": "json", "prop": "imageinfo",
        "iiprop": "url", "iiurlwidth": width, "titles": title}))
    if not d:
        return None
    for p in d.get("query", {}).get("pages", {}).values():
        ii = p.get("imageinfo", [{}])[0]
        u = ii.get("thumburl") or ii.get("url")
        if u:
            return u.split("?")[0]
    return None


# ---------------------------------------------------------------------------
#  质量门禁
# ---------------------------------------------------------------------------

GOOD_KW = ("global", "mosaic", "basemap", "base map", "color map", "colour map",
           "map", "cylindrical", "usgs", "controlled")
BAD_KW = ("dem", "elevation", "topograph", "geolog", "gravity", "magnetic",
          "quadrangle", "pole", "shaded relief", "diagram", "sketch", "legend",
          "labeled", "labelled", "artistic", "artist", "concept", "poster",
          "villa romana", "archaeolog", "statue", "painting", "ancient",
          "museum", "roman", "greek", "einstein")


def name_score(title: str, w: int, h: int) -> float:
    low = title.lower()
    if any(b in low for b in BAD_KW):
        return -1e9
    if not low.endswith((".jpg", ".jpeg", ".png")):
        return -1e9
    r = w / h if h else 0
    if abs(r - 2.0) > 0.15:        # 硬门禁: 等距柱状投影必须 2:1
        return -1e9
    s = 0.0
    for k in GOOD_KW:
        if k in low:
            s += 2.5
    if w >= 2000:
        s += 2.0
    if w < 1000:
        s -= 4.0
    return s


# ---------------------------------------------------------------------------
#  队列定义 —— 每个天体给出多来源候选
# ---------------------------------------------------------------------------

QUEUE = {
    "ariel": {
        "label": "天卫一 Ariel",
        "nasa": ["Ariel Uranus moon map"],
        "commons_cat": ["Ariel (moon)", "Maps of Ariel"],
    },
    "oberon": {
        "label": "天卫四 Oberon",
        "nasa": ["Oberon Uranus moon map"],
        "commons_cat": ["Oberon (moon)", "Maps of Oberon"],
    },
    "eros": {
        "label": "爱神星 Eros",
        "nasa": ["Eros asteroid global mosaic NEAR"],
        "commons_cat": ["433 Eros", "Maps of 433 Eros"],
    },
    "ryugu": {
        "label": "龙宫 Ryugu",
        "nasa": ["Ryugu asteroid Hayabusa2"],
        "commons_cat": ["162173 Ryugu", "Maps of 162173 Ryugu"],
    },
    "churyumov": {
        "label": "67P 彗核",
        "nasa": ["67P Churyumov-Gerasimenko comet nucleus"],
        "commons_cat": ["67P/Churyumov-Gerasimenko", "Maps of 67P"],
    },
    "ceres": {
        "label": "谷神星 Ceres",
        "nasa": ["Ceres global map Dawn"],
        "commons_cat": ["Ceres (dwarf planet)", "Maps of Ceres"],
    },
    "proteus": {
        "label": "海卫八 Proteus",
        "nasa": ["Proteus Neptune moon"],
        "commons_cat": ["Proteus (moon)"],
    },
    "deimos": {
        "label": "火卫二 Deimos",
        "nasa": ["Deimos Mars moon map"],
        "commons_cat": ["Deimos (moon)"],
    },
    "triton": {
        "label": "海卫一 Triton",
        "nasa": ["Triton Neptune moon map"],
        "commons_cat": ["Triton (moon)"],
    },
    "enceladus": {
        "label": "土卫二 Enceladus",
        "nasa": ["Enceladus global map Cassini"],
        "commons_cat": ["Enceladus (moon)"],
    },
    "ganymede": {
        "label": "木卫三 Ganymede",
        "nasa": ["Ganymede global map USGS"],
        "commons_cat": ["Ganymede (moon)"],
    },
    "callisto": {
        "label": "木卫四 Callisto",
        "nasa": ["Callisto global map USGS"],
        "commons_cat": ["Callisto (moon)"],
    },
    "europa": {
        "label": "木卫二 Europa",
        "nasa": ["Europa global map USGS"],
        "commons_cat": ["Europa (moon)"],
    },
    "io": {
        "label": "木卫一 Io",
        "nasa": ["Io global map USGS"],
        "commons_cat": ["Io (moon)"],
    },
    "titan": {
        "label": "土卫六 Titan",
        "nasa": ["Titan global map Cassini"],
        "commons_cat": ["Titan (moon)"],
    },
}


# ---------------------------------------------------------------------------

def load_state(path: str) -> dict:
    if os.path.isfile(path):
        try:
            return json.load(open(path, encoding="utf-8"))
        except Exception:
            pass
    return {}


def save_state(path: str, st: dict) -> None:
    with open(path, "w", encoding="utf-8") as f:
        json.dump(st, f, ensure_ascii=False, indent=2)


def image_ok(data: bytes) -> tuple:
    """校验比例 2:1。返回 (ok, w, h)。"""
    try:
        import io
        from PIL import Image
        im = Image.open(io.BytesIO(data))
        w, h = im.size
        if w < 900:
            return (False, w, h)
        if abs(w / h - 2.0) > 0.15:
            return (False, w, h)
        return (True, w, h)
    except Exception:
        return (False, 0, 0)


def try_commons(body: str, spec: dict, outdir: str) -> bool:
    cands = []
    for cat in spec.get("commons_cat", []):
        members = commons_cat_members(cat)
        if members:
            cands += members
    if not cands:
        return False
    sizes = commons_sizes(cands)
    ranked = []
    for t in dict.fromkeys(cands):
        w, h = sizes.get(t, (0, 0))
        if not w:
            continue
        sc = name_score(t, w, h)
        if sc > -1e8:
            ranked.append((sc, t, w, h))
    ranked.sort(reverse=True)
    for sc, title, w, h in ranked[:6]:
        url = commons_thumb(title, 2048)
        if not url:
            continue
        data = http_bytes(url)
        if not data:
            continue
        ok, gw, gh = image_ok(data)
        if not ok:
            print(f"     弃用 {title[5:50]} ({gw}x{gh} 比例不符)")
            continue
        out = os.path.join(outdir, f"real_{body}.jpg")
        with open(out, "wb") as f:
            f.write(data)
        print(f"     ✓ Commons: {title[5:56]}  {gw}x{gh}  {len(data)/1024:.0f} KB")
        with open(os.path.join(outdir, f"src_{body}.txt"), "w",
                  encoding="utf-8") as f:
            f.write(f"Wikimedia Commons: {title}\n")
        return True
    return False


def try_nasa(body: str, spec: dict, outdir: str) -> bool:
    for q in spec.get("nasa", []):
        items = nasa_search(q)
        for it in items[:12]:
            nid = it.get("nasa_id")
            if not nid:
                continue
            url = nasa_asset_url(nid)
            if not url:
                continue
            data = http_bytes(url)
            if not data or len(data) < 40000:
                continue
            ok, w, h = image_ok(data)
            if not ok:
                continue
            out = os.path.join(outdir, f"real_{body}.jpg")
            with open(out, "wb") as f:
                f.write(data)
            print(f"     ✓ NASA: {nid}  {w}x{h}  {len(data)/1024:.0f} KB")
            with open(os.path.join(outdir, f"src_{body}.txt"), "w",
                      encoding="utf-8") as f:
                f.write(f"NASA Images API: {nid}\n"
                        f"https://images.nasa.gov/details/{nid}\n")
            return True
    return False


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    outdir = args[0] if args else "."

    max_per_run = 3
    for a in sys.argv[1:]:
        if a.startswith("--max="):
            max_per_run = int(a.split("=")[1])

    if "--list" in sys.argv:
        print("队列:")
        for b, s in QUEUE.items():
            print(f"  {b:12s} {s['label']}")
        return

    os.makedirs(outdir, exist_ok=True)
    state_path = os.path.join(outdir, "state.json")
    st = load_state(state_path)

    done = 0
    for body, spec in QUEUE.items():
        if done >= max_per_run:
            print(f"\n本轮已达上限 {max_per_run} 个, 其余留待下次 (避免高频请求)")
            break

        rec = st.get(body, {})
        if rec.get("ok"):
            continue

        print(f"--- {body}  ({spec['label']})")
        ok = False
        # 先 Commons (权威地图多), 再 NASA
        try:
            ok = try_commons(body, spec, outdir)
        except Exception as e:
            print(f"     Commons 异常: {e}")
        if not ok:
            try:
                ok = try_nasa(body, spec, outdir)
            except Exception as e:
                print(f"     NASA 异常: {e}")

        st[body] = {"ok": ok, "ts": time.strftime("%Y-%m-%d %H:%M:%S")}
        save_state(state_path, st)

        if ok:
            done += 1
        else:
            print("     未取到合格图")

    ok_n = sum(1 for v in st.values() if v.get("ok"))
    print(f"\n累计成功 {ok_n} / 队列 {len(QUEUE)}")


if __name__ == "__main__":
    main()
