"""从 Wikimedia Commons 获取卫星与矮行星的真实影像 (NASA/USGS 公有领域)。

solarsystemscope 只覆盖 8 大行星 + 月球 + 太阳, 木卫/土卫/海卫/冥王星
需要另找来源。Commons 上的 USGS/NASA 全球地图是最合适的一类 —— 它们是
标准的等距柱状投影 (equirectangular), 可直接作为球面 albedo 贴图。

用法:
    python fetch_moons.py <输出目录>
"""
import json
import os
import sys
import urllib.parse
import urllib.request

API = "https://commons.wikimedia.org/w/api.php"
UA = "SolarSystemEdu/1.0 (educational; contact: local)"

# 天体 id -> Commons 搜索词。挑选时优先 "map" / "mosaic" (全球地图),
# 避开 "diagram" / "svg" / "from ... missions" (拼贴示意或非全球覆盖)。
TARGETS = {
    "pluto":     ["Pluto global map New Horizons", "Pluto color mapmosaic"],
    "io":        ["Io global map USGS", "Io Voyager Galileo map"],
    "europa":    ["Europa global map USGS", "Europa map mosaic"],
    "ganymede":  ["Ganymede USGS map", "Ganymede global map"],
    "callisto":  ["Callisto global map USGS", "Callisto map mosaic"],
    "titan":     ["Titan global map Cassini", "Titan map mosaic"],
    "enceladus": ["Enceladus global map Cassini", "Enceladus map"],
    "triton":    ["Triton global map Voyager", "Triton map mosaic"],
}

BAD = ("diagram", ".svg", "from galileo", "from voyager", "pole", "grid")


def api_get(params: dict) -> dict:
    url = API + "?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=40) as r:
        return json.load(r)


def search(query: str, limit: int = 8) -> list:
    try:
        d = api_get({
            "action": "query", "format": "json", "list": "search",
            "srnamespace": 6, "srlimit": limit, "srsearch": query,
        })
    except Exception as e:
        print(f"   搜索失败: {e}")
        return []
    return [x["title"] for x in d.get("query", {}).get("search", [])]


def ext_of(title: str) -> str:
    return os.path.splitext(title)[1].lower()


def pick(cands: list) -> str | None:
    """挑一个最像全球地图的候选。"""
    scored = []
    for t in cands:
        low = t.lower()
        if any(b in low for b in BAD):
            continue
        if ext_of(t) not in (".jpg", ".jpeg", ".png"):
            continue
        s = 0
        if "map" in low:
            s += 3
        if "mosaic" in low:
            s += 2
        if "global" in low:
            s += 2
        if "usgs" in low:
            s += 2
        if "color" in low:
            s += 1
        # 越简单的文件名往往越是标准地图 (PIA 编号类优先)
        if low.startswith("file:pia"):
            s += 1
        scored.append((s, t))
    if not scored:
        return None
    scored.sort(reverse=True)
    return scored[0][1]


def download(title: str, out_path: str, width: int = 2048) -> bool:
    """通过 Special:FilePath 取指定宽度的缩略图 (Commons 会自动转码)。"""
    name = title[len("File:"):]
    url = ("https://commons.wikimedia.org/wiki/Special:FilePath/"
           + urllib.parse.quote(name.replace(" ", "_")) + f"?width={width}")
    try:
        req = urllib.request.Request(url, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=120) as r:
            data = r.read()
        if len(data) < 5000:
            print(f"   内容过小 ({len(data)} B), 跳过")
            return False
        with open(out_path, "wb") as f:
            f.write(data)
        return True
    except Exception as e:
        print(f"   下载失败: {e}")
        return False


def main() -> None:
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)

    for body, queries in TARGETS.items():
        print(f"--- {body}")
        chosen = None
        for q in queries:
            cands = search(q)
            if not cands:
                continue
            chosen = pick(cands)
            if chosen:
                print(f"   候选: {cands[:3]}")
                print(f"   选中: {chosen}")
                break
        if not chosen:
            print("   * 未找到合适地图")
            continue

        ext = ext_of(chosen)
        out = os.path.join(outdir, f"real_{body}{ext}")
        if os.path.isfile(out):
            print(f"   已存在, 跳过 ({out})")
            continue
        if download(chosen, out):
            print(f"   -> {os.path.basename(out)} "
                  f"{os.path.getsize(out) / 1024:.0f} KB")


if __name__ == "__main__":
    main()
