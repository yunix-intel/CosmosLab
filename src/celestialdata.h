// ============================================================================
//  celestialdata.h —— 天体数据与轨道根数
//
//  数据本身在 celestialdata.cpp 中, 由 tools/gen_data.py 从 Python 版
//  (D:\tmp\solar-system-qt\core\data.py) 自动生成 —— 保证两个版本永远一致,
//  也省去手抄 18 个天体 x 25 个字段的出错风险。
// ============================================================================

#pragma once

// ---------------------------------------------------------------------------
//  轨道根数 (Standish / JPL 近似根数)
//
//  精度: 1800-2050 年约 1 角分。对可视化完全够用, 且不需要外部星历文件。
//  所有角度单位为度, 半长轴 a 单位为 AU。
// ---------------------------------------------------------------------------
struct OrbitalElements
{
    const char *id;
    double a;      // 半长轴 (AU)
    double e;      // 偏心率
    double inc;    // 轨道倾角 (度)
    double L;      // 平黄经 (度)
    double peri;   // 近日点黄经 (度)
    double node;   // 升交点黄经 (度)
    // ---- 每儒略世纪的变化率 ----
    double ra, re, ri, rL, rperi, rnode;
};

// ---------------------------------------------------------------------------
//  天体
// ---------------------------------------------------------------------------
struct BodyData
{
    const char *id;
    const char *name;          // 中文名
    const char *en;            // 英文名
    double radiusKm;           // 平均半径 (km)
    double massKg;
    double rotHours;           // 自转周期 (恒星日); 负值 = 逆行
    double poleRa, poleDec;    // IAU J2000 极轴 (度)
    double periodDays;         // 公转周期 (天)
    double gravity;            // 表面重力 (m/s^2)
    double escapeKms;          // 逃逸速度 (km/s)
    double density;            // 平均密度 (g/cm^3)
    double tempC;              // 平均表面温度 (°C)
    double albedo;             // 反照率
    int    moonCount;          // 卫星数量
    float  color[3];           // UI 与回退渲染用的基色
    const char *kind;          // star / rock / cratered / earth / gas / ice / moon
    const char *texture;       // 纹理名 (不含 albedo_ 前缀与 .png)
    double rotationOffset;     // 自转相位初值 (度)
    const char *parent;        // 母星 id; nullptr = 无 (太阳/行星)

    double orbitAu;            // 轨道半径 (AU), 仅用于 UI 显示

    // ---- 环系 ----
    bool   hasRings;
    double ringInner;          // 相对行星半径 (内缘)
    double ringOuter;          // 相对行星半径 (外缘)
    double ringOpacity;
    float  ringColor[3];

    // ---- 大气 ----
    bool   hasAtmo;
    float  atmoColor[3];
    double atmoOpacity;
    double atmoPower;

    // ---- 卫星轨道 (相对母星的圆轨道) ----
    double semiMajorKm;
    double ecc;
    double inc;

    const char *desc;
};

// ---------------------------------------------------------------------------
//  表 (定义在 celestialdata.cpp)
// ---------------------------------------------------------------------------
extern const OrbitalElements ORBITAL_ELEMENTS[];
extern const int             ORBITAL_ELEMENTS_COUNT;

extern const BodyData ALL_BODIES[];
extern const int      ALL_BODIES_COUNT;

extern const double AU_KM;    // 天文单位 (km) = 149597870.7
extern const double GM_SUN;   // 太阳引力常数 (km^3/s^2)
extern const double J2000;    // J2000 儒略日 = 2451545.0

// ---------------------------------------------------------------------------
//  查表辅助
// ---------------------------------------------------------------------------
const OrbitalElements *findOrbitalElements(const char *id);
const BodyData        *findBody(const char *id);
