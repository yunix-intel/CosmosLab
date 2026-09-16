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
inline constexpr double kSunDistFromCenterLy = 26582.0;  // Reid 2019: R0 = 8.15 kpc
inline constexpr double kSunOrbitSpeedKms    = 236.0;    // Reid 2019: Θ0 = 236±7 km/s
inline constexpr double kGalacticYearMyr     = 225.0;    // 银河年 2.25 亿年
inline constexpr double kSunHeightFromDiskLy = 18.0;     // Reid 2019: 偏北银极约 5.5 pc
                                                          // (旧值 55 ly 无依据)

// ---------------------------------------------------------------------------
//  旋臂几何 —— 参数取自 Reid et al. 2019 (ApJ 885, 131) Table 2
//
//  ★ 该论文用 BeSSeL 巡天 (VLBA) 与日本 VERA 测得约 200 个大质量恒星
//    形成区脉泽的**三角视差**直接测距 (精度典型 ±0.02 mas, 距离误差 <10%),
//    是当前银臂结构最可靠的实测约束。原文结论:
//      "strongly suggest that the Milky Way is a four-arm spiral,
//       with some extra arm segments and spurs"
//
//  ★ 关键点: **每条臂的螺距角不同** (9°~19°), 且多数臂有**折点 (kink)** ——
//    折点两侧螺距角不同。用"统一螺距角"是常见的过度简化, 会传递错误印象。
//
//  ★ 对数螺旋 + 折点:
//      ln(R / Rk) = -(β - βk) · tan(ψ)
//    即 R(β) = Rk · exp(-(β - βk) · tan(ψ))
//    β 为银心方位角, 以**太阳方向为 0°**, 从北银极看顺时针增大。
//    β < βk 用 ψ前, β > βk 用 ψ后。
//
//  ★ 星系的缠绕方向: 银河系是**后随螺旋 (trailing)** ——
//    从北银极看顺时针, 半径递减。代入 Reid 参数可验证:
//      英仙臂 β=0 时 R≈10.1 kpc (在太阳外侧 ✓)
//      人马臂 β=0 时 R≈6.9 kpc  (在太阳内侧 ✓)
// ---------------------------------------------------------------------------
struct ArmSpiral
{
    const char *nameCn;
    const char *nameEn;
    double betaBeginDeg;   // 该段臂的 β 起点
    double betaEndDeg;     // β 终点
    double betaKinkDeg;    // 折点处的 β
    double rKinkLy;        // 折点处的银心距
    double pitchPreDeg;    // 折点内侧螺距角
    double pitchPostDeg;   // 折点外侧螺距角
    double widthLy;        // 臂的径向宽度 (含约 90% 的示踪物)
    bool   isMajor;        // 是否为主臂
    const char *note;      // 教学要点
};

//                中文名        英文名                     β起    β终   βk     Rk(ly)  ψ前    ψ后   宽(ly) 主臂
inline const ArmSpiral kArmSpiral[] = {
  { "矩尺臂",     "Norma Arm",                 5.0,  54.0, 18.0,  14547.0, 19.5, 19.5,  1600.0, true,
    "紧贴中央棒的主臂, 靠近银心处有强烈的恒星形成区。它向外延伸后成为外臂 (Outer Arm)。" },

  { "盾牌-半人马臂", "Scutum-Centaurus Arm",    0.0, 104.0, 23.0,  16014.0, 14.1, 12.1,  1800.0, true,
    "银河系两条最长的旋臂之一, 从中央棒一端几乎延伸到盘缘。Spitzer 红外巡天中它最为显著。" },

  { "人马-船底臂", "Sagittarius-Carina Arm",   2.0,  97.0, 24.0,  19700.0, 17.1, 17.1,  1900.0, true,
    "位于太阳轨道内侧。银心方向上最显眼的旋臂, 那里是银河系恒星最密集的区域。" },

  { "英仙臂",     "Perseus Arm",            -23.0, 115.0, 40.0,  28930.0, 10.3,  8.7,  1100.0, true,
    "位于太阳轨道外侧的两大主臂之一, 恒星形成活动活跃。Reid 2019 测得其折点最为显著。" },

  { "外臂",       "Outer Arm",             -16.0,  71.0, 18.0,  39921.0,  9.4,  9.4,  1700.0, false,
    "银盘最外侧的一条臂, 与矩尺臂相连。距银心约 4 万光年。" },

  { "猎户支",     "Local Arm (Orion Spur)", -8.0,  34.0,  9.0,  26941.0, 11.4, 11.4,   900.0, false,
    "★ 太阳所在。它是一条**次级结构 (支/spur)**, 不是主旋臂 —— "
    "位于人马臂与英仙臂之间, 长度远短于主臂。把它标成主旋臂是常见科普错误。" },
};
inline constexpr int kArmSpiralCount = int(sizeof(kArmSpiral) / sizeof(kArmSpiral[0]));

// 保留旧的名称表 (供 UI 标注使用)
struct ArmInfo
{
    const char *nameCn;
    const char *nameEn;
    double      startAngleDeg;
    bool        isMajor;
    const char *note;
};
inline const ArmInfo kArms[] = {
    { "英仙臂",   "Perseus Arm",      0.0,   true,  "太阳轨道外侧的主臂, 螺距角约 10°" },
    { "矩尺臂",   "Norma Arm",      270.0,  true,  "紧贴中央棒, 螺距角约 19.5° (最陡)" },
    { "盾牌-半人马臂", "Scutum-Centaurus Arm", 180.0, true, "最长的主臂之一, 螺距角约 14°" },
    { "人马-船底臂", "Sagittarius-Carina Arm", 90.0, true, "太阳轨道内侧, 螺距角约 17°" },
};
inline constexpr int kArmInfoCount = int(sizeof(kArms) / sizeof(kArms[0]));

// 太阳所在的猎户支 (Local Arm)
// ★ Reid 2019: R0 = 8.15 kpc = 26,582 ly; 太阳在银道面北侧约 5.5 pc
inline constexpr double kOrionSpurDistLy   = 26582.0;
inline constexpr double kOrionSpurLengthLy = 6000.0;
inline constexpr double kOrionSpurAngleDeg = 90.0;   // 由 NASA/JPL 官方图实测定标

// 银心黑洞
inline constexpr double kSgrAStarRA  = 266.41683;   // 赤经 (度)
inline constexpr double kSgrAStarDec = -29.00781;   // 赤纬 (度)

// 盘的空间范围 (用于标尺与相机夹取)
inline constexpr double kSceneHaloRadius = kHaloRadiusLy / kLyPerUnit;

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
    "银盘直径约 10.6 万光年, 但薄盘厚度仅约 1000 光年 —— 直径是厚度的 100 多倍。"
    "这种“极薄”是银河系最反直觉的特征, 也是它看起来像一条亮带的原因。",

    "太阳位于**猎户支**内侧, 距银心约 2.658 万光年 (8.15 kpc), "
    "以约 236 km/s 绕银心运行。"
    "★ 猎户支并非主旋臂, 而是一条长约 6000 光年的次级结构 (支/spur), "
    "夹在英仙臂与人马臂之间。把它称作“猎户臂”是常见的科普错误。",

    "一个银河年约 2.25 亿年 —— 太阳至今已绕行约 20 圈。",

    "银心的**人马座 A\* (Sgr A\*)** 是超大质量黑洞, 质量约 430 万倍太阳质量, "
    "2022 年由事件视界望远镜首次成像。",

    "银河系与仙女座星系正以约 110 km/s 相互接近, 预计约 45 亿年后并合。",

    "★ 旋臂结构数据来自 **Reid et al. 2019 (ApJ 885, 131)** —— "
    "BeSSeL 巡天用 VLBA 测得约 200 个大质量恒星形成区脉泽的三角视差。"
    "结论是四条主臂 (英仙臂、人马-船底臂、盾牌-半人马臂、矩尺臂) 加若干臂段与支。",

    "★ 每条臂的螺距角**各不相同** (8.7°~19.5°), 且多数臂带“折点”: "
    "折点两侧螺距角不同。用统一的俯仰角描述是过度简化。",

    "★ 关于图层: 画面中**半透明的背景图**是 NASA/JPL 发布的银河系结构"
    "科学插画 (作者 Robert Hurt, 依据 Spitzer 红外与 CO 观测绘制), "
    "**不是照片** —— 我们身处银盘内部, 外部全景在物理上无法拍到。"
    "其上的**粒子**是按 Reid 2019 参数生成的 3D 示意模型。",

    "★ 两臂 vs 四臂之争: Spitzer 红外 (红巨星计数) 支持两条主臂, "
    "射电 21cm 原子氢与脉泽视差支持四条臂。这不矛盾 —— "
    "不同波段看到的是不同成分。本视图采用四臂模型 (脉泽视差证据更强)。",

    nullptr,
};

} // namespace gx
