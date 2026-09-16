"""通过 Commons API 下载指定宽度的缩略图。

Special:FilePath 的 ?width= 对超大原图经常返回错误页 (2 KB 的 HTML),
改用 action=query&prop=imageinfo&iiurlwidth=N 拿缩略图直链, 稳定得多。

用法:
    python dl_commons.py <输出目录>
"""
import json
import os
import sys
import urllib.parse
import urllib.request

API = "https://commons.wikimedia.org/w/api.php"
UA = {"User-Agent": "SolarSystemEdu/1.0 (educational)"}

# 天体 id -> (Commons 文件名, 目标宽度, 输出文件名)
# 全部为 NASA/USGS/ESA 的公有领域影像, 且宽高比 = 2:1 (等距柱状标准)
ITEMS = {
    "ganymede":  ("Ganymede map by Askaniy.png",            4096, "real_ganymede.jpg"),
    "pluto":     ("Pluto color mapmosaic.jpg",              4096, "real_pluto.jpg"),
    "enceladus": ("Enceladus Color Map.jpg",                4096, "real_enceladus.jpg"),
    "callisto":  ("Callisto USGS global small.jpg",         1024, "real_callisto.jpg"),
    "europa":    ("Europa USGS 2024.png",                   4096, "real_europa.jpg"),
    "titan":     ("PIA19658-SaturnMoon-TitanGlobalMap-June2015.jpg", 3200, "real_titan.jpg"),
    "io":        ("Io map projection PIA00319.jpg",         2048, "real_io.jpg"),
    "triton":    ("PIA18668 Map of Triton.jpg",             4096, "real_triton.jpg"),
}


def thumb_url(title: str, width: int) -> str | None:
    # ★ Commons API 的 titles 必须带 "File:" 命名空间前缀。
    #   漏了不会报错, 只是返回 "missing" —— 表现为静默拿不到 URL。
    if not title.startswith("File:"):
        title = "File:" + title
    d = json.load(urllib.request.urlopen(urllib.request.Request(
        API + "?" + urllib.parse.urlencode({
            "action": "query", "format": "json", "prop": "imageinfo",
            "iiprop": "url|size", "iiurlwidth": width, "titles": title,
        }), headers=UA), timeout=60))
    for p in d.get("query", {}).get("pages", {}).values():
        ii = p.get("imageinfo", [{}])[0]
        # thumburl 是缩略图; 当原图小于请求宽度时可能只给 url
        u = ii.get("thumburl") or ii.get("url")
        if u:
            # 去掉 API 附加的跟踪参数, 避免部分 CDN 节点拒绝
            return u.split("?")[0]
    return None


def fetch(url: str, out: str) -> bool:
    try:
        with urllib.request.urlopen(
                urllib.request.Request(url, headers=UA), timeout=300) as r:
            data = r.read()
        if len(data) < 20000:
            print(f"   内容过小 ({len(data)} B) —— 可能是错误页")
            return False
        with open(out, "wb") as f:
            f.write(data)
        return True
    except Exception as e:
        print(f"   下载失败: {e}")
        return False


def main() -> None:
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)

    for body, (title, width, fname) in ITEMS.items():
        out = os.path.join(outdir, fname)
        print(f"--- {body}")
        url = thumb_url(title, width)
        if not url:
            print("   取不到 URL")
            continue
        if fetch(url, out):
            print(f"   -> {fname}  {os.path.getsize(out) / 1024:.0f} KB")


if __name__ == "__main__":
    main()
