// ============================================================================
//  smalldata.h —— 太阳系小天体与卫星扩展数据
//
//  与 celestialdata.cpp 的分工:
//    celestialdata   —— 太阳 + 8 大行星 + 冥王星 + 8 颗主要卫星 (原有)
//    smalldata (本文件) —— 矮行星、其余主要卫星、小行星、彗星
//
//  ★ 数据来源与严谨性 (教学用途, 每个数值可查证):
//    * 轨道根数 (a, e, i, Ω, ω, M) —— NASA/JPL Small-Body Database
//      (ssd.jpl.nasa.gov), J2000 黄道坐标系, 历元 J2000.0
//    * 物理参数 (半径/质量/自转/反照率) —— NASA Planetary Fact Sheet
//      与 JPL/NASA 各探测器任务发布数据 (Dawn, New Horizons, Cassini,
//      Galileo, Voyager, OSIRIS-REx, Hayabusa2, Rosetta)
//    * 卫星轨道 —— JPL Planetary Satellite Mean Elements
//      (ssd.jpl.nasa.gov/sats/elem/)
//
//  ★ 未采用圆形轨道近似: 小行星与彗星的偏心率可达 0.97 (哈雷),
//    倾角可达 162° (逆行), 必须用真实开普勒根数求解。卫星亦然 ——
//    例如土卫八 (Iapetus) 轨道倾角 15.5°、偏心率 0.028, 用圆轨道
//    会让"土卫八轨道面明显倾斜"这个教学要点丢失。
// ============================================================================

#pragma once

#include "celestialdata.h"

namespace small {

// ---------------------------------------------------------------------------
//  天体分类 —— 教学上必须明确区分, 不能把矮行星与行星混为一谈
//
//  这个分类依据 IAU 2006 年决议:
//    * 行星   —— 绕太阳、质量足以成球、已清空轨道邻域
//    * 矮行星 —— 绕太阳、质量足以成球、**未**清空轨道邻域
//    * 小行星 —— 体积较小、形状不规则 (未达到流体静力平衡)
// ---------------------------------------------------------------------------
enum class BodyType
{
    Star,
    Planet,
    DwarfPlanet,
    Moon,
    Asteroid,
    Comet,
};

const char *typeNameCn(BodyType t);
const char *typeNameEn(BodyType t);

// ---------------------------------------------------------------------------
//  扩展天体条目: 复用 celestialdata.h 的 BodyData 结构 + 类型标注
// ---------------------------------------------------------------------------
struct SmallBody
{
    BodyData    data;
    BodyType    type;
    const char *typeNote;      // 该天体的教学要点
};

// 扩展天体表 (定义于 smalldata.cpp)
const SmallBody *bodies();
int             count();

// 日心小天体的轨道根数 (由 JPL 原始值经公式转换得到, 见 .cpp)
// 注意 rL 由开普勒第三定律算出, 而非拟合值。
const OrbitalElements *orbitalElements();
extern const int ORBITAL_ELEMENTS_COUNT;

// 按 id 查扩展天体; 找不到返回 nullptr
const SmallBody *find(const char *id);

} // namespace small
