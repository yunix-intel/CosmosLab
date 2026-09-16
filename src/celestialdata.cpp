// ============================================================================
//  celestialdata.cpp —— 天体数据与轨道根数
//
//  ★ 本文件由 tools/gen_data.py 从 Python 版自动生成, 请勿手工编辑 ★
//
//  源: D:\tmp\solar-system-qt\core\data.py
//  重新生成: python tools/gen_data.py > src/celestialdata.cpp
// ============================================================================

#include "celestialdata.h"

#include <cstring>

// ---------------------------------------------------------------------------
//  轨道根数 (Standish / JPL 近似根数, 1800-2050 年精度约 1 角分)
//  a/e/i/L/peri/node 为 J2000 时刻值, r* 为每儒略世纪的变化率。
// ---------------------------------------------------------------------------

const OrbitalElements ORBITAL_ELEMENTS[] = {
    { "mercury",  0.38709927, 0.20563593,  7.00497902,    252.2503235,  77.45779628, 48.33076593,
           3.7e-07,    1.906e-05,  -0.00594749, 149472.67411175,      0.16047689,     -0.12534081 },
    { "venus",   0.72333566, 0.00677672,  3.39467605,    181.9790995, 131.60246718, 76.67984255,
           3.9e-06,   -4.107e-05,   -0.0007889,  58517.81538729,      0.00268329,     -0.27769418 },
    { "earth",   1.00000261, 0.01671123,  -1.531e-05,   100.46457166, 102.93768193,         0.0,
          5.62e-06,   -4.392e-05,  -0.01294668,  35999.37244981,      0.32327364,             0.0 },
    { "mars",    1.52371034,  0.0933941,  1.84969142,    -4.55343205, -23.94362959, 49.55953891,
         1.847e-05,    7.882e-05,  -0.00813131,  19140.30268499,      0.44441088,     -0.29257343 },
    { "jupiter",    5.202887, 0.04838624,  1.30439695,    34.39644051,  14.72847983, 100.47390909,
       -0.00011607,  -0.00013253,  -0.00183714,   3034.74612775,      0.21252668,      0.20469106 },
    { "saturn",  9.53667594, 0.05386179,  2.48599187,    49.95424423,  92.59887831, 113.66242448,
        -0.0012506,  -0.00050991,   0.00193609,   1222.49362201,     -0.41897216,     -0.28867794 },
    { "uranus", 19.18916464, 0.04725744,  0.77263783,   313.23810451,  170.9542763, 74.01692503,
       -0.00196176,   -4.397e-05,  -0.00242939,    428.48202785,      0.40805281,      0.04240589 },
    { "neptune", 30.06992276, 0.00859048,  1.77004347,   -55.12002969,  44.96476227, 131.78422574,
        0.00026291,    5.105e-05,   0.00035372,    218.45945325,     -0.32241464,     -0.00508664 },
    { "pluto",  39.48211675,  0.2488273, 17.14001206,   238.92903833, 224.06891629, 110.30393684,
       -0.00031596,     5.17e-05,    4.818e-05,    145.20780515,     -0.04062942,     -0.01183482 },
};

const int ORBITAL_ELEMENTS_COUNT =
    int(sizeof(ORBITAL_ELEMENTS) / sizeof(ORBITAL_ELEMENTS[0]));

// ---------------------------------------------------------------------------
//  天体表 —— 太阳 + 行星 + 卫星
// ---------------------------------------------------------------------------

const BodyData ALL_BODIES[] = {
    // ================= 太阳 / Sun =================
    {
        /* id/name/en        */ "sun", "太阳", "Sun",
        /* radius/mass/rot   */ 696340.0, 1.9885e+30, 609.12,
        /* poleRa/Dec/period */ 286.13, 63.87, 0.0,
        /* grav/esc/den/temp */ 274.0, 617.7, 1.408, 5505.0,
        /* albedo/moons      */ 1.0, 0,
        /* color/kind        */ {1.0f, 0.94f, 0.78f}, "star",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ nullptr, 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "太阳系的中心天体, 占系统总质量的 99.86%。核心以每秒约 6 亿吨氢的速率进行核聚变, 产生的能量需数万年才能从核心传到表面。",
    },
    // ================= 水星 / Mercury =================
    {
        /* id/name/en        */ "mercury", "水星", "Mercury",
        /* radius/mass/rot   */ 2439.7, 3.3011e+23, 1407.6,
        /* poleRa/Dec/period */ 281.01, 61.42, 87.9691,
        /* grav/esc/den/temp */ 3.7, 4.25, 5.427, 167.0,
        /* albedo/moons      */ 0.142, 0,
        /* color/kind        */ {0.61f, 0.56f, 0.53f}, "cratered",
        /* texture/rotOff    */ "mercury", 0.0,
        /* parent/orbitAu    */ nullptr, 0.387,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "太阳系最小的行星, 也是离太阳最近的行星。表面布满撞击坑, 昼夜温差为太阳系之最。",
    },
    // ================= 金星 / Venus =================
    {
        /* id/name/en        */ "venus", "金星", "Venus",
        /* radius/mass/rot   */ 6051.8, 4.8675e+24, -5832.5,
        /* poleRa/Dec/period */ 272.76, 67.16, 224.701,
        /* grav/esc/den/temp */ 8.87, 10.36, 5.243, 464.0,
        /* albedo/moons      */ 0.689, 0,
        /* color/kind        */ {0.93f, 0.85f, 0.65f}, "venus",
        /* texture/rotOff    */ "venus", 0.0,
        /* parent/orbitAu    */ nullptr, 0.723,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ true, {1.0f, 0.92f, 0.72f}, 1.35, 2.6,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "地球的\"姊妹星\", 却是太阳系最炽热的行星。浓密的二氧化碳大气造成失控温室效应, 地表气压是地球的 92 倍。",
    },
    // ================= 地球 / Earth =================
    {
        /* id/name/en        */ "earth", "地球", "Earth",
        /* radius/mass/rot   */ 6371.0, 5.97237e+24, 23.9345,
        /* poleRa/Dec/period */ 0.0, 90.0, 365.256,
        /* grav/esc/den/temp */ 9.807, 11.186, 5.514, 15.0,
        /* albedo/moons      */ 0.306, 1,
        /* color/kind        */ {0.18f, 0.43f, 0.77f}, "earth",
        /* texture/rotOff    */ "earth", 0.0,
        /* parent/orbitAu    */ nullptr, 1.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ true, {0.35f, 0.65f, 1.0f}, 1.7, 3.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "目前已知唯一存在生命的行星。液态水海洋覆盖 71% 的表面, 含氧大气与强磁场共同维持着宜居环境。",
    },
    // ================= 火星 / Mars =================
    {
        /* id/name/en        */ "mars", "火星", "Mars",
        /* radius/mass/rot   */ 3389.5, 6.4171e+23, 24.6229,
        /* poleRa/Dec/period */ 317.68, 52.89, 686.98,
        /* grav/esc/den/temp */ 3.721, 5.03, 3.9335, -65.0,
        /* albedo/moons      */ 0.25, 2,
        /* color/kind        */ {0.76f, 0.33f, 0.18f}, "mars",
        /* texture/rotOff    */ "mars", 0.0,
        /* parent/orbitAu    */ nullptr, 1.524,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ true, {1.0f, 0.62f, 0.42f}, 0.42, 4.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "红色星球。氧化铁尘土赋予它锈红色调, 拥有太阳系最高的火山奥林匹斯山和长达 4000 km 的水手号峡谷。",
    },
    // ================= 木星 / Jupiter =================
    {
        /* id/name/en        */ "jupiter", "木星", "Jupiter",
        /* radius/mass/rot   */ 69911.0, 1.8982e+27, 9.925,
        /* poleRa/Dec/period */ 268.06, 64.5, 4332.589,
        /* grav/esc/den/temp */ 24.79, 59.5, 1.326, -110.0,
        /* albedo/moons      */ 0.503, 95,
        /* color/kind        */ {0.79f, 0.66f, 0.51f}, "gas_giant",
        /* texture/rotOff    */ "jupiter", 0.0,
        /* parent/orbitAu    */ nullptr, 5.203,
        /* rings             */ true, 1.4, 1.81, 0.1, {0.54f, 0.48f, 0.4f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "太阳系最大的行星, 质量超过其余七大行星总和的 2.5 倍。剧烈的大气对流形成明暗相间的云带, 大红斑是持续数百年的风暴。",
    },
    // ================= 土星 / Saturn =================
    {
        /* id/name/en        */ "saturn", "土星", "Saturn",
        /* radius/mass/rot   */ 58232.0, 5.6834e+26, 10.656,
        /* poleRa/Dec/period */ 40.589, 83.537, 10759.22,
        /* grav/esc/den/temp */ 10.44, 35.5, 0.687, -140.0,
        /* albedo/moons      */ 0.342, 146,
        /* color/kind        */ {0.85f, 0.76f, 0.56f}, "gas_giant",
        /* texture/rotOff    */ "saturn", 0.0,
        /* parent/orbitAu    */ nullptr, 9.537,
        /* rings             */ true, 1.24, 2.35, 0.95, {0.82f, 0.75f, 0.62f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "以壮观的环系闻名。环主要由水冰颗粒构成, 厚度仅约 10 米而直径达 28 万公里。平均密度小于水。",
    },
    // ================= 天王星 / Uranus =================
    {
        /* id/name/en        */ "uranus", "天王星", "Uranus",
        /* radius/mass/rot   */ 25362.0, 8.681e+25, -17.24,
        /* poleRa/Dec/period */ 257.311, -15.175, 30688.5,
        /* grav/esc/den/temp */ 8.87, 21.3, 1.271, -195.0,
        /* albedo/moons      */ 0.3, 28,
        /* color/kind        */ {0.62f, 0.85f, 0.9f}, "ice_giant",
        /* texture/rotOff    */ "uranus", 0.0,
        /* parent/orbitAu    */ nullptr, 19.191,
        /* rings             */ true, 1.6, 2.0, 0.3, {0.55f, 0.62f, 0.68f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "自转轴倾斜 97.8°, 几乎\"躺着\"绕太阳公转, 导致极区会经历长达 42 年的极昼与极夜。",
    },
    // ================= 海王星 / Neptune =================
    {
        /* id/name/en        */ "neptune", "海王星", "Neptune",
        /* radius/mass/rot   */ 24622.0, 1.02413e+26, 16.11,
        /* poleRa/Dec/period */ 299.36, 43.46, 60195.0,
        /* grav/esc/den/temp */ 11.15, 23.5, 1.638, -200.0,
        /* albedo/moons      */ 0.29, 16,
        /* color/kind        */ {0.25f, 0.42f, 0.85f}, "ice_giant",
        /* texture/rotOff    */ "neptune", 0.0,
        /* parent/orbitAu    */ nullptr, 30.069,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "距太阳最远的行星。拥有太阳系最强烈的风暴, 风速可达 2100 km/h。通过数学计算预测位置后被观测证实。",
    },
    // ================= 冥王星 / Pluto =================
    {
        /* id/name/en        */ "pluto", "冥王星", "Pluto",
        /* radius/mass/rot   */ 1188.3, 1.303e+22, -153.3,
        /* poleRa/Dec/period */ 132.993, -6.163, 90560.0,
        /* grav/esc/den/temp */ 0.62, 1.21, 1.854, -229.0,
        /* albedo/moons      */ 0.52, 5,
        /* color/kind        */ {0.78f, 0.7f, 0.62f}, "dwarf",
        /* texture/rotOff    */ "pluto", 0.0,
        /* parent/orbitAu    */ nullptr, 39.482,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "2006 年被重新归类为矮行星。表面有由氮冰构成的\"心形\"斯普特尼克平原, 与冰山起伏的丘陵地形。",
    },
    // ================= 月球 / Moon =================
    {
        /* id/name/en        */ "moon", "月球", "Moon",
        /* radius/mass/rot   */ 1737.4, 7.342e+22, 655.7,
        /* poleRa/Dec/period */ 270.0, 66.54, 27.3217,
        /* grav/esc/den/temp */ 1.62, 2.38, 3.344, -20.0,
        /* albedo/moons      */ 0.136, 0,
        /* color/kind        */ {0.62f, 0.6f, 0.57f}, "moon",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ "earth", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 384400.0, 0.0549, 5.145,
        /* desc              */ "地球唯一的天然卫星, 也是太阳系第五大卫星。潮汐锁定使它始终以同一面朝向地球。",
    },
    // ================= 木卫一 / Io =================
    {
        /* id/name/en        */ "io", "木卫一", "Io",
        /* radius/mass/rot   */ 1821.6, 8.932e+22, 42.46,
        /* poleRa/Dec/period */ 268.06, 64.5, 1.769,
        /* grav/esc/den/temp */ 1.796, 2.56, 3.528, -143.0,
        /* albedo/moons      */ 0.63, 0,
        /* color/kind        */ {0.95f, 0.85f, 0.35f}, "moon",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ "jupiter", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 421700.0, 0.0041, 0.036,
        /* desc              */ "太阳系火山活动最剧烈的天体, 表面被硫与硅酸盐熔岩覆盖, 有超过 400 座活火山。",
    },
    // ================= 木卫二 / Europa =================
    {
        /* id/name/en        */ "europa", "木卫二", "Europa",
        /* radius/mass/rot   */ 1560.8, 4.8e+22, 85.23,
        /* poleRa/Dec/period */ 268.06, 64.5, 3.551,
        /* grav/esc/den/temp */ 1.314, 2.03, 3.013, -171.0,
        /* albedo/moons      */ 0.67, 0,
        /* color/kind        */ {0.86f, 0.82f, 0.74f}, "moon",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ "jupiter", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 671100.0, 0.009, 0.466,
        /* desc              */ "表面被冰层覆盖, 冰下可能存在深度达 100 km 的液态水海洋, 含水量约为地球海洋的两倍, 是地外生命探测的首要目标。",
    },
    // ================= 木卫三 / Ganymede =================
    {
        /* id/name/en        */ "ganymede", "木卫三", "Ganymede",
        /* radius/mass/rot   */ 2634.1, 1.4819e+23, 171.71,
        /* poleRa/Dec/period */ 268.06, 64.5, 7.155,
        /* grav/esc/den/temp */ 1.428, 2.74, 1.936, -163.0,
        /* albedo/moons      */ 0.43, 0,
        /* color/kind        */ {0.65f, 0.62f, 0.58f}, "moon",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ "jupiter", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 1070400.0, 0.0013, 0.177,
        /* desc              */ "太阳系最大的卫星, 比水星还大。是唯一拥有自身磁场的卫星, 也存在冰下海洋。",
    },
    // ================= 木卫四 / Callisto =================
    {
        /* id/name/en        */ "callisto", "木卫四", "Callisto",
        /* radius/mass/rot   */ 2410.3, 1.0759e+23, 400.54,
        /* poleRa/Dec/period */ 268.06, 64.5, 16.689,
        /* grav/esc/den/temp */ 1.235, 2.44, 1.834, -139.0,
        /* albedo/moons      */ 0.22, 0,
        /* color/kind        */ {0.48f, 0.45f, 0.42f}, "moon",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ "jupiter", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 1882700.0, 0.0074, 0.192,
        /* desc              */ "太阳系撞击坑密度最高的天体之一, 表面极为古老, 几乎没有地质活动的痕迹。",
    },
    // ================= 土卫六 / Titan =================
    {
        /* id/name/en        */ "titan", "土卫六", "Titan",
        /* radius/mass/rot   */ 2574.7, 1.3452e+23, 382.68,
        /* poleRa/Dec/period */ 40.589, 83.537, 15.945,
        /* grav/esc/den/temp */ 1.352, 2.64, 1.882, -179.0,
        /* albedo/moons      */ 0.22, 0,
        /* color/kind        */ {0.85f, 0.65f, 0.32f}, "moon",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ "saturn", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ true, {1.0f, 0.72f, 0.35f}, 2.2, 2.4,
        /* moon-orbit a/e/i  */ 1221870.0, 0.0288, 0.348,
        /* desc              */ "太阳系唯一拥有浓密大气的卫星, 地表存在液态甲烷的湖泊与河流, 是除地球外唯一有稳定地表液体的天体。",
    },
    // ================= 土卫二 / Enceladus =================
    {
        /* id/name/en        */ "enceladus", "土卫二", "Enceladus",
        /* radius/mass/rot   */ 252.1, 1.0802e+20, 32.885,
        /* poleRa/Dec/period */ 40.589, 83.537, 1.37,
        /* grav/esc/den/temp */ 0.113, 0.239, 1.609, -198.0,
        /* albedo/moons      */ 0.81, 0,
        /* color/kind        */ {0.94f, 0.95f, 0.96f}, "moon",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ "saturn", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 237948.0, 0.0047, 0.009,
        /* desc              */ "南极存在\"虎纹\"状裂缝, 持续喷发含盐冰粒与水汽的羽流, 证实冰下存在液态海洋, 是寻找地外生命的重点目标。",
    },
    // ================= 海卫一 / Triton =================
    {
        /* id/name/en        */ "triton", "海卫一", "Triton",
        /* radius/mass/rot   */ 1353.4, 2.139e+22, -141.05,
        /* poleRa/Dec/period */ 299.36, 43.46, -5.877,
        /* grav/esc/den/temp */ 0.779, 1.455, 2.061, -235.0,
        /* albedo/moons      */ 0.76, 0,
        /* color/kind        */ {0.86f, 0.84f, 0.82f}, "moon",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ "neptune", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 354759.0, 1.6e-05, 156.885,
        /* desc              */ "太阳系唯一大型逆行卫星, 推测是被海王星捕获的柯伊伯带天体。表面温度 -235°C, 是太阳系已测得最冷的天体之一。",
    },
};

const int ALL_BODIES_COUNT = int(sizeof(ALL_BODIES) / sizeof(ALL_BODIES[0]));

// ---------------------------------------------------------------------------
//  常量
// ---------------------------------------------------------------------------

const double AU_KM  = 149597870.7;   // 天文单位 (km)
const double GM_SUN = 132712440018.0;   // 太阳引力常数 (km^3/s^2)
const double J2000  = 2451545.0;   // J2000 儒略日

// ---------------------------------------------------------------------------
//  查表
// ---------------------------------------------------------------------------

const OrbitalElements *findOrbitalElements(const char *id)
{
    if (!id)
        return nullptr;
    for (int i = 0; i < ORBITAL_ELEMENTS_COUNT; ++i)
        if (std::strcmp(ORBITAL_ELEMENTS[i].id, id) == 0)
            return &ORBITAL_ELEMENTS[i];
    return nullptr;
}

const BodyData *findBody(const char *id)
{
    if (!id)
        return nullptr;
    for (int i = 0; i < ALL_BODIES_COUNT; ++i)
        if (std::strcmp(ALL_BODIES[i].id, id) == 0)
            return &ALL_BODIES[i];
    return nullptr;
}
