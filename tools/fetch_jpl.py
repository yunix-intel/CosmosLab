"""从 NASA/JPL Small-Body Database API 取权威轨道根数 (全精度)。

★ 为什么必须用 API 而不是手抄:
    手抄根数时还得记住**历元**(epoch) —— 平近点角 M 是历元的函数, 换历元
    M 就变。凭记忆写历元极易出错, 而且错得极隐蔽: 位置会整体偏移, 但
    画面上"仍然有个天体在动", 肉眼发现不了。
    (本项目踩过: 哈雷用了 1994 历元的根数却按 J2000 传播,
     近日点位置算出 15.7 AU, 真值 0.586 AU —— 差 26 倍。)

★ 必须加 full-prec=true:
    不加时 API 只返回 3 位有效数字 (a=2.77), 对教学不够;
    加上后返回完整精度 (a=2.7675185...)。

★ API 还直接给出 n (平均运动) 与 tp (过近日点时刻):
    n  比用开普勒第三定律反算更权威, 故直接采用;
    tp 用于自检 —— 由 (a, e, M, epoch) 反推的近日点时刻应与 tp 一致。

API 文档: https://ssd-api.jpl.nasa.gov/doc/sbdb.html

用法:
    python fetch_jpl.py           # 输出 C++ 表
    python fetch_jpl.py --json    # 存原始数据
"""
import json
import sys
import urllib.parse
import urllib.request

API = "https://ssd-api.jpl.nasa.gov/sbdb.api"

BODIES = {
    "ceres":     "Ceres",
    "eris":      "136199",        # Eris
    "haumea":    "136108",        # Haumea
    "makemake":  "136472",        # Makemake
    "vesta":     "Vesta",
    "pallas":    "Pallas",
    "juno":      "Juno",
    "hygiea":    "Hygiea",
    "eros":      "Eros",
    "bennu":     "Bennu",
    "ryugu":     "Ryugu",
    "halley":    "1P",
    "encke":     "2P",
    "churyumov": "67P",
    "neowise":   "C/2020 F3",
    "wild2":     "81P",
}


def fetch(name: str) -> dict | None:
    params = {"sstr": name, "full-prec": "true"}
    url = API + "?" + urllib.parse.urlencode(params)
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "SolarSystemEdu/1.0"})
        with urllib.request.urlopen(req, timeout=60) as r:
            return json.load(r)
    except Exception as e:
        print(f"  {name}: 请求失败 {e}", file=sys.stderr)
        return None


def main() -> None:
    dump = "--json" in sys.argv
    out = {}

    print("// 由 tools/fetch_jpl.py 从 NASA/JPL SBDB API 生成 (full-prec)",
          file=sys.stderr)

    for bid, query in BODIES.items():
        d = fetch(query)
        if not d or "orbit" not in d:
            print(f"  {bid}: 无轨道数据", file=sys.stderr)
            continue

        o = d["orbit"]
        el = {e["name"]: e["value"] for e in o["elements"]}

        a   = float(el["a"])
        e   = float(el["e"])
        inc = float(el["i"])
        om  = float(el["om"])
        w   = float(el["w"])
        ma  = float(el["ma"])
        n   = float(el["n"])          # 平均运动 deg/day (权威值)
        tp  = float(el["tp"])         # 过近日点时刻 (TDB)
        jd  = float(o["epoch"])

        L    = om + w + ma
        peri = om + w

        out[bid] = {
            "a": a, "e": e, "i": inc, "om": om, "w": w, "ma": ma,
            "n": n, "tp": tp, "epochJd": jd,
            "L": L, "peri": peri,
            # JPL 直接给出的近日点/远日点距离 —— 作为自检基准,
            # 比引用文献里的旧解更权威 (旧解会被后续观测修正)。
            "q": float(el.get("q", a * (1 - e))),
            "Q": float(el.get("ad", a * (1 + e))),
            "fullname": d.get("object", {}).get("fullname", ""),
        }

        print(f"  {bid:10s} a={a:<14.8f} e={e:<12.9f} i={inc:<10.5f} "
              f"n={n:<12.8f} epoch=JD{jd:.1f}", file=sys.stderr)

    if dump:
        with open("jpl_elements.json", "w", encoding="utf-8") as f:
            json.dump(out, f, ensure_ascii=False, indent=2)
        print("\n已写入 jpl_elements.json", file=sys.stderr)
        return

    # ------------------------------------------------------------------
    #  ★★ 历元转换 (这是整个流程里最容易出错的一步)
    #
    #  JPL 给出的 L (平黄经) 是**在该天体自己的历元上**的, 而各天体的
    #  历元并不相同 (哈雷 1994 年、Bennu 2011 年、多数 2026 年)。
    #  求解器统一从 J2000 起算, 因此必须把 L 换算到 J2000:
    #
    #      L_J2000 = L_epoch + n · (J2000 - epoch)
    #
    #  这里的换算不引入近似 —— 平近点角随时间线性变化, 所以这只是
    #  同一个线性函数的换基点。
    #
    #  验证方法: 由 (M_epoch, n, epoch) 反推过近日点时刻
    #      t_peri = epoch - M_epoch / n        (M = 0 时过近日点)
    #  它应与 JPL 直接给出的 tp 一致。脚本自己完成这项校验 ——
    #  转换写错时会立刻暴露, 不会等到渲染出来才发现。
    # ------------------------------------------------------------------
    J2000 = 2451545.0

    print("// 数据来源: NASA/JPL Small-Body Database API (full-prec=true)")
    print("// ★ L 已由各自历元换算至 J2000 (见 tools/fetch_jpl.py 的说明)")
    print("// 字段: id, a, e, i, L(J2000), peri, node, rL(度/世纪), epochJd")
    print("static const RawOrbit RAW_ORBITS[] = {")

    problems = []
    for bid, v in out.items():
        n = v["n"]                       # deg/day
        rl = n * 36525.0                 # deg/century
        L2000 = v["L"] + n * (J2000 - v["epochJd"])

        # 自检: 在 JPL 给出的过近日点时刻 tp, 平近点角应为 0 (mod 360)。
        #
        # ★ 关键: 判据要用 M(tp) ≡ 0 (mod 360), 而不是"反推的 tp 数值相等"。
        #   起初写成 t_peri = epoch - M/n 与 JPL 的 tp 直接比, 结果 8 项
        #   全部"失败" —— 但差值恰好都精确等于一个周期 (帕拉斯 1683.5 d
        #   对周期 1680 d; 哈雷 27728.05 d 对周期 27728.1 d)。
        #   原因是 JPL 的 tp 指向历元后的**下一次**近日点, 而 epoch - M/n
        #   给的是**上一次**。取模后两者等价。
        #   教训: 校验公式本身也要先验证, 否则会误报数据有问题。
        M_at_tp = v["ma"] + n * (v["tp"] - v["epochJd"])
        residual = abs((M_at_tp + 180.0) % 360.0 - 180.0)   # 归一化到 ±180
        if residual > 0.01:                                 # 0.01° 容差
            problems.append((bid, residual))

        print(f'    // {v["fullname"]}')
        print(f'    {{ "{bid}", {v["a"]:.9f}, {v["e"]:.9f}, {v["i"]:.6f}, '
              f'{L2000:.6f}, {v["peri"]:.6f}, {v["om"]:.6f}, '
              f'{rl:.6f} }},')
    print("};")

    # ------------------------------------------------------------------
    #  自检基准表 —— 供 src/orbtest.cpp 使用
    #
    #  ★ 为什么要自动生成而不是手写:
    #    手写的文献值会过时。哈雷的近日点距离在 1994 年解里是 0.58598 AU,
    #    但 JPL 当前解 (含更多观测) 给出 0.574842 AU —— 差了 1.9%。
    #    自检若对着旧值比, 会把**正确的**计算判成错误。
    #    故基准值一律从同一个 API 实时取, 保证与代码用的是同一版解。
    # ------------------------------------------------------------------
    print("", file=sys.stderr)
    print("// ---- 自检基准 (由 tools/fetch_jpl.py 从同一 API 生成) ----",
          file=sys.stderr)
    print("const PeriRef PERI_REFS[] = {", file=sys.stderr)
    for bid, v in out.items():
        print(f'    {{ "{bid}", {v["q"]:.6f}, {v["Q"]:.6f}, {v["tp"]:.2f} }},   '
              f'// {v["fullname"]}', file=sys.stderr)
    print("};", file=sys.stderr)

    print("", file=sys.stderr)
    if problems:
        print("!! 近日点时刻自检未通过:", file=sys.stderr)
        for bid, resid in problems:
            print(f"   {bid}: 残差 {resid:.4f}°", file=sys.stderr)
    else:
        print(f"近期点时刻自检: 全部 {len(out)} 项通过 "
              f"(反推值与 JPL tp 一致)", file=sys.stderr)


if __name__ == "__main__":
    main()
