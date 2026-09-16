// ============================================================================
//  galaxydata.h —— 银河系参考数据
//
//  用于软件的「银河系」尺度视图。数值取自近年天文测量, 并标注主要来源,
//  因为这是教学用途 —— 每个数字都应该可查证。
//
//  主要来源:
//    * GRAVITY Collaboration (2019, 2022) —— 太阳距银心距离
//    * Reid et al. (2019) —— 太阳绕银速度
//    * Gaia DR3 (2022) —— 银盘尺度与旋臂结构
// ============================================================================

#pragma once

namespace gx {

// ---------------------------------------------------------------------------
//  场景尺度换算
//
//  真实比例: 直径 105700 ly, 薄盘厚仅 1000 ly —— 直径是厚度的 105 倍。
//  这个"薄"是银河系最反直觉的特征之一, 教学上必须如实呈现, 所以这里
//  **不做任何艺术夸张**, 用户看到的就是真实比例。
// ---------------------------------------------------------------------------
inline constexpr double kDiskRadiusLy = 52850.0;   // 银盘半径 ≈ 直径的一半
inline constexpr double kSceneRadius  = 100.0;     // 银盘半径对应的场景单位数
inline constexpr double kLyPerUnit    = kDiskRadiusLy / kSceneRadius;  // 528.5 ly/单位

// ---------------------------------------------------------------------------
//  尺寸 (光年)
// ---------------------------------------------------------------------------
inline constexpr double kDiameterLy     = 105700.0;  // 银盘直径
inline constexpr double kThinDiskLy     = 1000.0;    // 薄盘厚度
inline constexpr double kThickDiskLy    = 2500.0;    // 厚盘厚度
inline constexpr double kBulgeRadiusLy  = 5000.0;    // 核球半径
inline constexpr double kBarHalfLenLy   = 13500.0;   // 中央棒半长
inline constexpr double kHaloRadiusLy   = 150000.0;  // 银晕半径 (远超银盘)

// ---------------------------------------------------------------------------
//  太阳的位置与运动
// ---------------------------------------------------------------------------
inline constexpr double kSunDistFromCenterLy = 26000.0;  // 距银心 2.6 万光年
inline constexpr double kSunOrbitSpeedKms    = 230.0;    // 绕银心速度 km/s
inline constexpr double kGalacticYearMyr     = 225.0;    // 银河年 2.25 亿年
inline constexpr double kSunHeightFromDiskLy = 55.0;     // 距银道面高度

// ---------------------------------------------------------------------------
//  旋臂几何
//
//  银盘旋臂用对数螺旋描述: r = r0 · e^(b·θ), b = tan(俯仰角)。
//  银河系 4 条主旋臂的俯仰角约 12°, 缠绕约 1 圈。
//
//  ★ 旋臂起点必须落在**棒的末端** —— 这是棒旋星系的定义特征:
//    旋臂从中央棒的两端延伸出去, 而不是从核球外围凭空开始。
//    早期误取 25000 ly, 于是在棒端 (13500 ly) 与旋臂起点之间留下一圈
//    密度空洞, 画面上表现为银心旁一个明显的黑椭圆。
// ---------------------------------------------------------------------------
inline constexpr double kArmPitchDeg  = 12.0;
inline constexpr int    kArmCount     = 4;
inline constexpr double kArmStartLy   = 13500.0;   // = 棒半长, 从棒端伸出
inline constexpr double kArmEndLy     = 50000.0;   // 旋臂终点 (接近盘缘)

// 盘的指数标长。银河系的实测值约 2.5–3.5 kpc (8000–11000 ly),
// 而非可随意取的经验值 —— 它决定银盘亮度沿半径的衰减快慢。
inline constexpr double kDiskScaleLengthLy = 10000.0;

// ---------------------------------------------------------------------------
//  组成
// ---------------------------------------------------------------------------
inline constexpr double kStarCount    = 2.5e11;   // 恒星数 (1000–4000 亿的中值)
inline constexpr double kMassSolar    = 1.5e12;   // 总质量 (太阳质量, 含暗物质)
inline constexpr double kBlackHoleMass= 4.3e6;    // 人马座 A* 质量 (太阳质量)

// ---------------------------------------------------------------------------
//  邻近星系 (用于拉远后的本星系群视图)
// ---------------------------------------------------------------------------
struct NeighborGalaxy
{
    const char *name;
    const char *nameEn;
    double distanceLy;        // 距银河系中心
    double radiusLy;          // 半长轴
    double angleDeg;          // 在天空中的方位
    double elevDeg;           // 相对银道面的高度角
    double axisRatio;         // 短轴/长轴
    double tiltDeg;           // 视角倾角
    float  color[3];
};

inline const NeighborGalaxy NEIGHBORS[] = {
    // 名称           英文                  距离ly   半径ly  方位  高度  轴比  倾角  颜色
    {"仙女座星系",  "Andromeda (M31)",   2.537e6, 110000,  38.0,  -22.0, 0.32, 62.0, {0.92f, 0.90f, 0.86f}},
    {"三角座星系",  "Triangulum (M33)",  2.730e6,  30000, -54.0,   16.0, 0.42, 48.0, {0.86f, 0.89f, 0.94f}},
    {"大麦哲伦云",  "Large Magellanic Cloud", 1.63e5, 7000, -118.0, -34.0, 0.55, 25.0, {0.90f, 0.88f, 0.90f}},
    {"小麦哲伦云",  "Small Magellanic Cloud", 2.00e5, 3500, -124.0, -40.0, 0.62, 30.0, {0.88f, 0.87f, 0.92f}},
};

// ---------------------------------------------------------------------------
//  教学文本
// ---------------------------------------------------------------------------
inline const char *kName   = "银河系";
inline const char *kNameEn = "Milky Way";
inline const char *kType   = "棒旋星系 (SBbc)";

inline const char *kNotes[] = {
    "银盘直径约 10.6 万光年, 但薄盘厚度仅约 1000 光年 —— 直径是厚度的 100 多倍。",
    "太阳位于猎户臂内侧, 距银心约 2.6 万光年, 以约 230 km/s 绕银心运行。",
    "一个银河年约 2.25 亿年 —— 太阳至今已绕行约 20 圈。",
    "银心的人马座 A* 是超大质量黑洞, 质量约 430 万倍太阳质量。",
    "银河系与仙女座星系正以约 110 km/s 相互接近, 预计约 45 亿年后并合。",
    nullptr,
};

} // namespace gx
