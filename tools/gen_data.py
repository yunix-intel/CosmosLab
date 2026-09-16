#!/usr/bin/env python3
"""从 Python 版 (solar-system-qt/core/data.py) 生成 C++ 数据表。

为什么要生成而不是手抄:
  data.py 里有 18 个天体、每个 25 个字段, 加上 9 组轨道根数共 9x12 个数。
  手抄一次必然出错, 且源数据以后再改就对不上了。生成器保证两边永远一致。

用法 (在 solar-system-cpp 目录下):
    <venv>/python.exe tools/gen_data.py > src/celestialdata.cpp
"""

from __future__ import annotations

import io
import os
import sys

# 让 core.data 可导入
SRC = r"D:\tmp\solar-system-qt"
if SRC not in sys.path:
    sys.path.insert(0, SRC)

from core.data import (  # noqa: E402
    ORBITAL_ELEMENTS, SUN, PLANETS, MOONS, ALL_BODIES,
    AU_KM, GM_SUN, J2000,
)

out = io.StringIO()
w = out.write


def f(v) -> str:
    """浮点数 -> C++ 字面量。"""
    if v is None:
        return "0.0"
    try:
        x = float(v)
    except (TypeError, ValueError):
        return "0.0"
    if x != x or x in (float("inf"), float("-inf")):
        return "0.0"
    if isinstance(v, int):
        return "%d.0" % v
    s = repr(x)
    if "e" not in s and "E" not in s and "." not in s:
        s += ".0"
    return s


def s(v) -> str:
    """字符串 -> C++ 字面量 (转义反斜杠、引号、换行)。"""
    if v is None:
        return "nullptr"
    t = (str(v).replace("\\", "\\\\").replace('"', '\\"')
         .replace("\n", "\\n").replace("\r", ""))
    return '"%s"' % t


def v3(c) -> str:
    if not c:
        return "{0.5f, 0.5f, 0.5f}"
    return "{%sf, %sf, %sf}" % (f(c[0]), f(c[1]), f(c[2]))


w("// ============================================================================\n")
w("//  celestialdata.cpp —— 天体数据与轨道根数\n")
w("//\n")
w("//  ★ 本文件由 tools/gen_data.py 从 Python 版自动生成, 请勿手工编辑 ★\n")
w("//\n")
w("//  源: D:\\tmp\\solar-system-qt\\core\\data.py\n")
w("//  重新生成: python tools/gen_data.py > src/celestialdata.cpp\n")
w("// ============================================================================\n\n")
w('#include "celestialdata.h"\n\n')
w("#include <cstring>\n\n")

# ---------------------------------------------------------------- 轨道根数
w("// ---------------------------------------------------------------------------\n")
w("//  轨道根数 (Standish / JPL 近似根数, 1800-2050 年精度约 1 角分)\n")
w("//  a/e/i/L/peri/node 为 J2000 时刻值, r* 为每儒略世纪的变化率。\n")
w("// ---------------------------------------------------------------------------\n\n")
w("const OrbitalElements ORBITAL_ELEMENTS[] = {\n")
for bid, el in ORBITAL_ELEMENTS.items():
    w("    { %-9s %11s, %10s, %11s, %14s, %12s, %11s,\n"
      % (s(bid) + ",", f(el['a']), f(el['e']), f(el['i']),
         f(el['L']), f(el['peri']), f(el['node'])))
    w("      %12s, %12s, %12s, %15s, %15s, %15s },\n"
      % (f(el['ra']), f(el['re']), f(el['ri']),
         f(el['rL']), f(el['rperi']), f(el['rnode'])))
w("};\n\n")
w("const int ORBITAL_ELEMENTS_COUNT =\n"
  "    int(sizeof(ORBITAL_ELEMENTS) / sizeof(ORBITAL_ELEMENTS[0]));\n\n")

# ---------------------------------------------------------------- 天体表
w("// ---------------------------------------------------------------------------\n")
w("//  天体表 —— 太阳 + 行星 + 卫星\n")
w("// ---------------------------------------------------------------------------\n\n")
w("const BodyData ALL_BODIES[] = {\n")

for b in ALL_BODIES:
    rings = getattr(b, 'rings', None) or {}
    atmo = getattr(b, 'atmo', None) or {}

    empty_r = "false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f}"
    if rings:
        ring_line = "%s, %s, %s, %s, %s" % (
            "true", f(rings.get('inner', 1.0)), f(rings.get('outer', 2.0)),
            f(rings.get('opacity', 1.0)), v3(rings.get('color')))
    else:
        ring_line = empty_r

    if atmo:
        atmo_line = "%s, %s, %s, %s" % (
            "true", v3(atmo.get('color')), f(atmo.get('opacity', 1.0)),
            f(atmo.get('power', 2.0)))
    else:
        atmo_line = "false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0"

    w("    // ================= %s / %s =================\n" % (b.name, b.en))
    w("    {\n")
    w("        /* id/name/en        */ %s, %s, %s,\n" % (s(b.id), s(b.name), s(b.en)))
    w("        /* radius/mass/rot   */ %s, %s, %s,\n"
      % (f(b.radius_km), f(b.mass_kg), f(b.rot_hours)))
    w("        /* poleRa/Dec/period */ %s, %s, %s,\n"
      % (f(b.pole_ra), f(b.pole_dec), f(b.period_days)))
    w("        /* grav/esc/den/temp */ %s, %s, %s, %s,\n"
      % (f(b.gravity), f(b.escape_kms), f(b.density), f(b.temp_c)))
    w("        /* albedo/moons      */ %s, %d,\n" % (f(b.albedo), b.moons))
    w("        /* color/kind        */ %s, %s,\n" % (v3(b.color), s(b.kind)))
    w("        /* texture/rotOff    */ %s, %s,\n"
      % (s(b.texture), f(b.rotation_offset)))
    w("        /* parent/orbitAu    */ %s, %s,\n" % (s(b.parent), f(b.orbit_au)))
    w("        /* rings             */ %s,\n" % ring_line)
    w("        /* atmo              */ %s,\n" % atmo_line)
    w("        /* moon-orbit a/e/i  */ %s, %s, %s,\n"
      % (f(b.semi_major_km), f(b.ecc), f(b.inc)))
    w("        /* desc              */ %s,\n" % s(b.desc))
    w("    },\n")
w("};\n\n")
w("const int ALL_BODIES_COUNT = int(sizeof(ALL_BODIES) / sizeof(ALL_BODIES[0]));\n\n")

# ---------------------------------------------------------------- 常量
w("// ---------------------------------------------------------------------------\n")
w("//  常量\n")
w("// ---------------------------------------------------------------------------\n\n")
w("const double AU_KM  = %s;   // 天文单位 (km)\n" % f(AU_KM))
w("const double GM_SUN = %s;   // 太阳引力常数 (km^3/s^2)\n" % f(GM_SUN))
w("const double J2000  = %s;   // J2000 儒略日\n\n" % f(J2000))

# ---------------------------------------------------------------- 查表
w("// ---------------------------------------------------------------------------\n")
w("//  查表\n")
w("// ---------------------------------------------------------------------------\n\n")
w("const OrbitalElements *findOrbitalElements(const char *id)\n")
w("{\n")
w("    if (!id)\n")
w("        return nullptr;\n")
w("    for (int i = 0; i < ORBITAL_ELEMENTS_COUNT; ++i)\n")
w("        if (std::strcmp(ORBITAL_ELEMENTS[i].id, id) == 0)\n")
w("            return &ORBITAL_ELEMENTS[i];\n")
w("    return nullptr;\n")
w("}\n\n")
w("const BodyData *findBody(const char *id)\n")
w("{\n")
w("    if (!id)\n")
w("        return nullptr;\n")
w("    for (int i = 0; i < ALL_BODIES_COUNT; ++i)\n")
w("        if (std::strcmp(ALL_BODIES[i].id, id) == 0)\n")
w("            return &ALL_BODIES[i];\n")
w("    return nullptr;\n")
w("}\n")

sys.stdout.write(out.getvalue())
