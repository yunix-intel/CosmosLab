"""按 Reid et al. 2019 的实测参数重写旋臂模型（替换"统一12°"的简化）。

★★ 权威依据 —— Reid, M. J. et al. 2019, ApJ 885, 131
   "Trigonometric Parallaxes of High-mass Star-forming Regions:
    Our View of the Milky Way"

   观测: **BeSSeL 巡天** (VLBA) + 日本 VERA, 约 200 个大质量恒星形成区
         脉泽的**三角视差**直接测距, 精度典型 ±0.02 mas (距离误差 <10%)
   结论: 原文明确 —— "strongly suggest that the Milky Way is a
         **four-arm spiral**, with some extra arm segments and spurs"

★ 为什么必须换掉旧参数:
   旧代码用"4 条等权螺旋, 统一螺距 12°, 起始相位 0/90/180/270°"。
   而实测的每条臂螺距角各不相同 (9°~19°), 且多数臂有**折点 (kink)** ——
   折点两侧的螺距角不同。这是 Reid 2019 的核心发现之一。
   用统一螺距在教学上会传递错误印象。

★ 参数表 (Reid et al. 2019 Table 2; kpc 按 1 kpc = 3261.56 ly 换算)
   臂名                β范围(°)    βk(°)  Rk(kpc)  Rk(ly)   ψ前(°)  ψ后(°)
   Sagittarius-Carina   2 → 97      24     6.04    19700    17.1    17.1
   Scutum-Centaurus     0 → 104     23     4.91    16014    14.1    12.1
   Perseus            −23 → 115     40     8.87    28930    10.3     8.7
   Norma                5 → 54      18     4.46    14547    19.5    19.5
   Local (猎户支)      −8 → 34      9      8.26    26941    11.4    11.4
   Outer              −16 → 71      18    12.24    39921     9.4     9.4

★ 坐标约定:
   Reid 的银心方位角 β 以**太阳方向为 0°**, 从北银极看顺时针增大。
   本项目场景中方位角 φ = atan2(z, x) 同样是从北看顺时针,
   故 **φ_点 = φ_太阳 + β**。

★ 其他关键参数 (Reid 2019 正文):
   R0 = 8.15 kpc = 26,582 ly       (太阳距银心)
   Θ0 = 236 ± 7 km/s               (太阳处的圆轨道速度)
   ω0 = 30.3 ± 0.5 km/s/kpc        (角速度)
   太阳偏离银道面约 5.5 pc (偏北银极方向)
   7 kpc 内年轻大质量恒星的标高仅 19 pc (故能良好定义银道面)
"""
import re

H = r'D:\tmp\solar-system-cpp\src\galaxydata.h'
C = r'D:\tmp\solar-system-cpp\src\galaxy.cpp'

NEW_HEADER = r'''// ---------------------------------------------------------------------------
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
inline constexpr double kOrionSpurAngleDeg = 90.0;   // 由 NASA/JPL 官方图实测定标'''

NEW_SUN = r'''QVector3D Galaxy::sunPosition()
{
    // ★ 太阳位置 —— 按天文观测确定, 不是"为了好看"随便放的。
    //
    //  ★★ 这里修正了一个**科学错误**:
    //     旧代码写的是 `const float ang = 1.15f;` 并注明
    //     "方位角取一条旋臂附近, 视觉上'落在臂上'更有说服力"。
    //     这是错的 —— 太阳**不在任何主旋臂上**, 而在人马臂与英仙臂
    //     之间的**猎户支 (Orion Spur / Local Arm)**, 属次级结构。
    //
    //     两重证据:
    //       1. NASA/JPL 官方图 (R. Hurt) 把 Sun 圆圈明确画在两臂之间的
    //          低亮度区, 标注为 "Orion Spur"
    //       2. 对 NASA 图做径向亮度剖面: 从太阳指向银心方向亮度单调上升
    //          (147→246), 背离银心方向单调下降 (147→42) —— 正是"身处
    //          臂间低密度区"的特征
    //
    //  ★ 方位角由 NASA/JPL 官方图标定:
    //     图上银心在 (1000,1000), 太阳圈在 (985,1372) 像素
    //     => 太阳位于银心的**下方**, 即图像方位角 ≈ 90°。
    //     场景方位角 φ = atan2(z, x) 与图像方位角一一对应
    //     (图像 +y 向下 ↔ 场景 +z; 均为从北银极看的顺时针),
    //     故太阳场景方位角取 90°。
    //
    //  ★ 距离取 Reid et al. 2019 的 R0 = 8.15 kpc = 26,582 ly
    //     (GRAVITY 合作组 2019 用 VLTI 测 Sgr A* 视差得 8.178 kpc,
    //      两者在误差内一致; 本项目采用结构研究的常用值 8.15 kpc)
    const float d = float(gx::kSunDistFromCenterLy / gx::kLyPerUnit);
    const float h = float(gx::kSunHeightFromDiskLy / gx::kLyPerUnit);
    const float ang = float(gx::kOrionSpurAngleDeg * M_PI / 180.0);
    return QVector3D(d * std::cos(ang), h, d * std::sin(ang));
}'''


def main():
    # ---- 1) 更新头文件 ----
    s = open(H, encoding='utf-8').read()
    start = s.index('// ---------------------------------------------------------------------------\n//  旋臂几何')
    end = s.index('// 银心黑洞')
    s = s[:start] + NEW_HEADER + '\n\n' + s[end:]

    # 更新太阳距银心距离为 Reid 2019
    s = s.replace('inline constexpr double kSunDistFromCenterLy = 26000.0;  // 距银心 2.6 万光年',
                  'inline constexpr double kSunDistFromCenterLy = 26582.0;  // Reid 2019: R0 = 8.15 kpc')
    # 太阳标高: Reid 2019 说偏北银极 5.5 pc
    s = s.replace('inline constexpr double kSunHeightFromDiskLy = 55.0;     // 距银道面高度',
                  'inline constexpr double kSunHeightFromDiskLy = 18.0;     // Reid 2019: 偏北银极约 5.5 pc\n'
                  '                                                          // (旧值 55 ly 无依据)')
    # 轨道速度: Reid 2019 给 236 km/s
    s = s.replace('inline constexpr double kSunOrbitSpeedKms    = 230.0;    // 绕银心速度 km/s',
                  'inline constexpr double kSunOrbitSpeedKms    = 236.0;    // Reid 2019: Θ0 = 236±7 km/s')
    open(H, 'w', encoding='utf-8').write(s)
    print('galaxydata.h 已更新 (Reid 2019 参数)')

    # ---- 2) 更新 sunPosition ----
    c = open(C, encoding='utf-8').read()
    a = c.index('QVector3D Galaxy::sunPosition()')
    b = c.index('\n}', a) + 2
    c = c[:a] + NEW_SUN + '\n' + c[b:]
    open(C, 'w', encoding='utf-8').write(c)
    print('galaxy.cpp sunPosition 已修正')


if __name__ == '__main__':
    main()
