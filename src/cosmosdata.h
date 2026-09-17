// ============================================================================
//  cosmosdata.h —— 宇宙大尺度结构数据
//
//  ★ 数据来源 (全部可溯源, 教学用途):
//    * 本星系群成员与距离 —— NASA/IPAC Extragalactic Database (NED),
//      Karachentsev et al. 2013 "Updated Nearby Galaxy Catalog"
//    * 室女座星系团 —— NED, Mei et al. 2007 (ACS Virgo Cluster Survey)
//    * 拉尼亚凯亚超星系团 —— Tully et al. 2014, Nature 513, 71
//      ("The Laniakea Supercluster of Galaxies"),
//      边界由**本动速度场**的水流域 (watershed) 划分, 而非简单球体
//    * 巨壁 / 空洞 —— Gaia 与 SDSS 巡天的大尺度结构图
//    * 宇宙学参数 —— Planck 2018 结果
//
//  ★ 尺度层级 (每个层级差约 1-2 个数量级, 无法用同一个场景坐标表示):
//      本星系群       ~ 300 万光年   (10 个成员)
//      室女座星系团   ~ 3300 万光年  (1300+ 成员)
//      拉尼亚凯亚     ~ 5.2 亿光年   (10 万星系)
//      可观测宇宙     ~ 930 亿光年   (直径)
//    因此宇宙视图需要用**对数或分段缩放**, 否则小尺度结构会在屏幕上
//    挤成一个点。本文件给出真实距离, 由渲染层决定如何压缩。
// ============================================================================

#pragma once

// ---------------------------------------------------------------------------
//  星系条目
// ---------------------------------------------------------------------------
struct GalaxyData
{
    const char *id;
    const char *nameCn;
    const char *nameEn;
    double distanceMly;      // 距地球 (百万光年)
    double diameterKly;      // 直径 (千光年)
    double massLog10;        // 恒星质量 (log10 太阳质量)
    int    type;             // 0=螺旋 1=椭圆 2=不规则 3=矮星系 4=环状
    double raDeg, decDeg;    // 赤道坐标 (度), 用于确定方向

    // ★★ 实测红移 z (无量纲)。**负值 = 蓝移**。
    //
    //  来源: NASA/IPAC 河外星系数据库 (NED) 的日心视向速度,
    //        按 z = v / c (c = 299792.458 km/s) 换算。逐条可查。
    //
    //  ★★ 为什么必须给**实测值**而不是用哈勃定律反算:
    //     在近距星系上, 哈勃定律**根本不适用** —— 星系的自身运动
    //     (本动速度, peculiar velocity) 往往远大于宇宙膨胀的贡献。
    //     若用 z = H₀d/c 反算, 会得出这样荒谬的结果:
    //
    //         M31  距离 2.54 Mly -> 反算 z = +0.00018 (红移)
    //              实测           z = -0.00100 (蓝移!)
    //
    //     方向都反了 —— 因为 M31 正被引力拉着**接近**我们。
    //     对近距星系, 实测红移是"宇宙膨胀 + 本动速度"的叠加,
    //     而这个叠加本身就是最好的教学素材。
    double redshift;
    float  color[3];
    const char *desc;
};

// ---------------------------------------------------------------------------
//  本星系群 (Local Group) —— 距离 < 1000 万光年
//
//  成员数约 80+ 个星系, 但绝大多数是矮星系。这里收录 12 个最重要的。
//  银河系与仙女座是两个主导成员, 其余都远小于它们。
// ---------------------------------------------------------------------------
extern const GalaxyData LOCAL_GROUP[];
extern const int       LOCAL_GROUP_COUNT;

// ---------------------------------------------------------------------------
//  室女座星系团 (Virgo Cluster) —— 距离约 54 Mly
//
//  这是离我们最近的**星系团** (而非星系群), 成员 1300+ 个。
//  它主导着本星系群的运动 —— 我们正以约 185 km/s 朝它坠落。
//  银河系属于它的外围成员 (严格说是室女座超星系团的一部分)。
// ---------------------------------------------------------------------------
extern const GalaxyData VIRGO_CLUSTER[];
extern const int       VIRGO_CLUSTER_COUNT;

// ---------------------------------------------------------------------------
//  大尺度结构 —— 巨壁与空洞
//
//  ★ 这些结构的尺度 (数亿光年) 已经大到**光从一端到另一端所需的时间
//    可与宇宙年龄相比** —— 这是讲解"宇宙学原理"与"结构形成"的关键。
// ---------------------------------------------------------------------------
struct LargeStructure
{
    const char *nameCn;
    const char *nameEn;
    double distanceFromEarthMly;
    double sizeMly;
    int    kind;             // 0=巨壁/纤维 1=超星系团 2=空洞
    const char *desc;
};

extern const LargeStructure LARGE_STRUCTURES[];
extern const int            LARGE_STRUCTURES_COUNT;

// ---------------------------------------------------------------------------
//  宇宙学参数 (Planck 2018)
//
//  ★ 这些数字是当代宇宙学的定量基础, 教学上必须给准确值:
// ---------------------------------------------------------------------------
namespace cosmo {
inline constexpr double kAgeGyr        = 13.797;    // 宇宙年龄 (十亿年)
inline constexpr double kH0            = 67.4;      // 哈勃常数 (km/s/Mpc)
inline constexpr double kOmegaM        = 0.315;     // 物质密度参数
inline constexpr double kOmegaLambda   = 0.685;     // 暗能量密度参数
inline constexpr double kOmegaB        = 0.049;     // 重子物质密度参数
inline constexpr double kCMBTempK      = 2.7255;    // CMB 温度 (K)
inline constexpr double kCMBRedshift   = 1089.9;    // 复合时期红移
inline constexpr double kObsUniverseMly = 46500.0;  // 可观测宇宙半径 (百万光年)
inline constexpr double kObsUniverseDiaMly = 93000.0;  // 直径
inline constexpr long long kGalaxyCount = 2000000000000LL;  // 可观测宇宙星系数
inline constexpr double kRecombinationYr = 380000.0;  // 复合时期距大爆炸 (年)

// 太阳系绕银心速度, 用于展示运动层级
inline constexpr double kSunOrbitKms   = 230.0;
inline constexpr double kLocalGroupKms = 185.0;   // 向室女座团坠落速度
} // namespace cosmo
