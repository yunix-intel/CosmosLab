// ============================================================================
//  smalldata.cpp —— 太阳系小天体与卫星扩展数据
//
//  ★★ 数据来源 (教学用途, 每个数值可溯源):
//
//    轨道根数 —— NASA/JPL Small-Body Database (ssd.jpl.nasa.gov)
//                J2000 黄道坐标系。原始数据以 (a, e, i, Ω, ω, M) 给出,
//                本文件转换为代码使用的 (a, e, i, L, ϖ, Ω) 形式:
//                    L = Ω + ω + M    平黄经
//                    ϖ = Ω + ω        近日点黄经
//                这与 JPL "Approximate Positions of the Major Planets"
//                的约定一致, 因此可与八大行星共用同一套求解器。
//
//    卫星轨道 —— JPL Planetary Satellite Mean Elements
//                (ssd.jpl.nasa.gov/sats/elem/)
//
//    物理参数 —— NASA Planetary Fact Sheet + 各探测器任务发布数据:
//                Dawn (谷神星/灶神星)、New Horizons (冥王星/卡戎)、
//                Cassini (土星系)、Galileo/Voyager (木/天/海王星系)、
//                OSIRIS-REx (贝努)、Hayabusa2 (龙宫)、
//                NEAR Shoemaker (爱神星)、Rosetta (67P)
//
//  ★★ 为何不用圆形轨道近似:
//    小行星偏心率可达 0.26 (智神星), 彗星可达 0.999 (NEOWISE);
//    哈雷彗星轨道倾角 162° (逆行)。用圆轨道会让这些最重要的
//    轨道特征 —— 也正是教学要点 —— 完全丢失。
//
//  ★★ 长周期演化率的处理:
//    八大行星的根数取自 JPL 的 1800–2050 年近似表, 含每世纪变化率。
//    小天体没有官方长期拟合, 因此本文件:
//      * ra/re/ri/rϖ/rΩ 取 0 —— 不引入没有数据来源的漂移
//      * 平黄经变化率 rL 由开普勒第三定律算出, 这是物理必然而非拟合:
//          n = 360°/P,  P = 365.25·a^1.5 天  ⇒  rL = 36000 / a^1.5 (度/世纪)
//        验证: 水星 a=0.38710 → 36000/0.24085 = 149470, 与 JPL 表中
//        的 149472.67 相符。故该式同样适用于小天体。
//
//  ★★ 无实测表面图的处理 (关乎严谨性):
//    阅神星、妊神星、鸟神星以及多数小彗核至今没有分辨率足够的全球图。
//    这些天体的 texture 设为 nullptr, 由渲染器画成均匀色球
//    (颜色取实测的几何反照率与色指数)。这比编造一张纹理诚实 ——
//    它准确表达了"我们尚未测绘其表面"这一事实。
// ============================================================================

#include "smalldata.h"

#include <cmath>
#include <cstring>

namespace small {

// ---------------------------------------------------------------------------
//  类型名称
// ---------------------------------------------------------------------------

const char *typeNameCn(BodyType t)
{
    switch (t) {
    case BodyType::Star:        return "恒星";
    case BodyType::Planet:      return "行星";
    case BodyType::DwarfPlanet: return "矮行星";
    case BodyType::Moon:        return "卫星";
    case BodyType::Asteroid:    return "小行星";
    case BodyType::Comet:       return "彗星";
    }
    return "未知";
}

const char *typeNameEn(BodyType t)
{
    switch (t) {
    case BodyType::Star:        return "Star";
    case BodyType::Planet:      return "Planet";
    case BodyType::DwarfPlanet: return "Dwarf planet";
    case BodyType::Moon:        return "Moon";
    case BodyType::Asteroid:    return "Asteroid";
    case BodyType::Comet:       return "Comet";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
//  日心小天体的轨道根数 —— ★ 由 tools/fetch_jpl.py 从 NASA/JPL SBDB API 生成
//
//  重新生成: python tools/fetch_jpl.py > 表段
//
//  ★★ 关于历元 (这是本项目踩过的最隐蔽的坑):
//    JPL 给出的 L (平黄经) 是在该天体自己的历元上的, 而各天体历元
//    并不相同 (哈雷 1994 年、Bennu 2011 年、多数 2026 年)。求解器统一
//    从 J2000 起算, 因此生成脚本已把 L 换算到 J2000:
//        L_J2000 = L_epoch + n · (J2000 - epoch)
//    这只是同一个线性函数的换基点, 不引入近似。
//
//    初版凭记忆手抄根数并一律按 J2000 传播 —— 哈雷的近日点因此算出
//    15.7 AU, 而真值 0.586 AU, 差 26 倍。而画面上"仍然有个天体在动",
//    肉眼完全看不出来。
//
//  ★★ rL (平黄经变化率) 直接采用 JPL 的 n:
//    n 是 JPL 从观测拟合出的平均运动, 比用开普勒第三定律反算更权威。
//    rL = n × 36525 (deg/day → deg/century)。
//
//  数据来源: NASA/JPL Small-Body Database API (full-prec=true)
// ---------------------------------------------------------------------------

struct RawOrbit
{
    const char *id;
    double a;      // 半长轴 (AU)
    double e;      // 偏心率
    double inc;    // 轨道倾角 (度)
    double L;      // 平黄经 (度, 已换算至 J2000)
    double peri;   // 近日点黄经 (度)
    double node;   // 升交点黄经 (度)
    double rL;     // 平黄经变化率 (度/儒略世纪) = JPL 的 n × 36525
};

static const RawOrbit RAW_ORBITS[] = {
    // 1 Ceres (A801 AA)
    { "ceres", 2.765552595, 0.079692295, 10.588028, 158.745564, 153.542841, 80.248627, 7827.470060 },
    // 136199 Eris (2003 UB313)
    { "eris", 67.933946879, 0.438238535, 43.925828, 21.578056, 186.799694, 36.004770, 64.293050 },
    // 136108 Haumea (2003 EL61)
    { "haumea", 43.060290237, 0.194443015, 28.208474, 192.007688, 362.476603, 121.786056, 127.402770 },
    // 136472 Makemake (2005 FY9)
    { "makemake", 45.570933173, 0.158888995, 29.027856, 155.390329, 376.387107, 79.294834, 117.020626 },
    // 4 Vesta (A807 FA)
    { "vesta", 2.361365965, 0.090203744, 7.143926, 233.749009, 255.169941, 103.701293, 9920.860649 },
    // 2 Pallas (A802 FA)
    { "pallas", 2.769559011, 0.230700100, 34.932793, 113.377902, 483.856536, 172.886619, 7810.491497 },
    // 3 Juno (A804 RA)
    { "juno", 2.670989527, 0.255699984, 12.986592, 300.368333, 417.706670, 169.811595, 8246.810605 },
    // 10 Hygiea (A849 GA)
    { "hygiea", 3.150974034, 0.106709274, 3.829530, 226.158274, 595.544131, 283.119893, 6436.163410 },
    // 433 Eros (A898 PA)
    { "eros", 1.458243717, 0.222877963, 10.828544, 181.469458, 483.186103, 304.267971, 20443.211783 },
    // 101955 Bennu (1999 RQ36)
    { "bennu", 1.126391026, 0.203745076, 6.034944, 97.714404, 68.283927, 2.060866, 30113.450821 },
    // 162173 Ryugu (1999 JU3)
    { "ryugu", 1.190918933, 0.191073005, 5.866442, 42.804084, 462.898706, 251.289712, 27699.440649 },
    // 1P/Halley
    { "halley", 17.928635049, 0.967935996, 162.190530, 237.230687, 171.340379, 59.098947, 474.213003 },
    // 2P/Encke
    { "encke", 2.219620901, 0.847311424, 11.368183, 89.824734, 521.318796, 334.112805, 10886.194511 },
    // 67P/Churyumov-Gerasimenko
    { "churyumov", 3.462249490, 0.640908131, 7.040295, 270.487775, 62.933823, 50.135574, 5588.004646 },
    // C/2020 F3 (NEOWISE)
    { "neowise", 358.467956553, 0.999178026, 128.937503, 97.201503, 98.289087, 61.010428, 5.304187 },
    // 81P/Wild 2
    { "wild2", 3.449745577, 0.537398907, 3.237004, 328.133120, 177.835452, 136.110221, 5618.413498 },
};

static const int RAW_ORBITS_COUNT = int(sizeof(RAW_ORBITS) / sizeof(RAW_ORBITS[0]));

static OrbitalElements g_orbits[RAW_ORBITS_COUNT];

const OrbitalElements *orbitalElements()
{
    static bool built = false;
    if (!built) {
        for (int i = 0; i < RAW_ORBITS_COUNT; ++i) {
            const RawOrbit &r = RAW_ORBITS[i];
            OrbitalElements &e = g_orbits[i];
            e.id   = r.id;
            e.a    = r.a;
            e.e    = r.e;
            e.inc  = r.inc;
            e.L    = r.L;
            e.peri = r.peri;
            e.node = r.node;
            e.rL   = r.rL;
            // 小天体没有 JPL 官方的长期根数演化表, 故其余变化率取 0 ——
            // 不引入没有数据来源的漂移。
            // (倾角/偏心率等确实会缓慢变化, 但那属于长期摄动, 需要专门的
            //  分析理论; 用凭空拟的系数反而会引入错误的确定性。)
            e.ra = e.re = e.ri = e.rperi = e.rnode = 0.0;
        }
        built = true;
    }
    return g_orbits;
}

const int ORBITAL_ELEMENTS_COUNT = RAW_ORBITS_COUNT;

// ---------------------------------------------------------------------------
//  扩展天体表
//
//  字段顺序与 celestialdata.h 的 BodyData 完全一致。
//  为便于核对, 每项按结构体注释分组书写。
// ---------------------------------------------------------------------------

const SmallBody SMALL_BODIES_TABLE[] = {

// ===========================================================================
//  矮行星 (IAU 2006 定义: 绕日、成球、但未清空轨道邻域)
// ===========================================================================

// ---------------------------- 谷神星 Ceres ----------------------------
{
    {
        /* id/name/en        */ "ceres", "谷神星", "Ceres",
        /* radius/mass/rot   */ 469.7, 9.3839e+20, 9.0742,
        /* poleRa/Dec/period */ 291.42, 66.76, 1681.63,
        /* grav/esc/den/temp */ 0.28, 0.51, 2.162, -105.0,
        /* albedo/moons      */ 0.090, 0,
        /* color/kind        */ {0.52f, 0.50f, 0.47f}, "dwarf",
        /* texture/rotOff    */ "ceres", 0.0,
        /* parent/orbitAu    */ nullptr, 2.7675,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "小行星带中唯一的矮行星, 也是其中最大的天体。占主带总质量约 1/3。"
                               "Dawn 探测器发现其地下可能存在水冰, 表面有明亮的碳酸盐沉积。",
    },
    BodyType::DwarfPlanet,
    "唯一位于小行星带内的矮行星, 是\"成球但未清空轨道\"这一分类的典型代表。",
},

// ---------------------------- 阅神星 Eris ----------------------------
{
    {
        /* id/name/en        */ "eris", "阅神星", "Eris",
        /* radius/mass/rot   */ 1163.0, 1.6466e+22, 378.6,
        /* poleRa/Dec/period */ 0.0, 0.0, 203830.0,
        /* grav/esc/den/temp */ 0.82, 1.38, 2.52, -243.0,
        /* albedo/moons      */ 0.96, 1,
        /* color/kind        */ {0.92f, 0.91f, 0.88f}, "dwarf",
        /* texture/rotOff    */ nullptr, 0.0,     // 无实测全球图 -> 均匀色球
        /* parent/orbitAu    */ nullptr, 67.864,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "已知质量最大的矮行星 (约为冥王星的 1.27 倍)。"
                               "它的发现直接导致 IAU 在 2006 年重新定义\"行星\", 冥王星因此被降级。"
                               "反照率高达 0.96, 是太阳系反射率最高的天体之一。",
    },
    BodyType::DwarfPlanet,
    "它的发现引发了 2006 年行星定义的修订 —— 是讲解\"什么是行星\"的最佳案例。",
},

// ---------------------------- 妊神星 Haumea ----------------------------
{
    {
        /* id/name/en        */ "haumea", "妊神星", "Haumea",
        /* radius/mass/rot   */ 816.0, 4.006e+21, 3.9155,
        /* poleRa/Dec/period */ 0.0, 0.0, 103660.0,
        /* grav/esc/den/temp */ 0.40, 0.85, 2.018, -241.0,
        /* albedo/moons      */ 0.51, 2,
        /* color/kind        */ {0.88f, 0.87f, 0.85f}, "dwarf",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ nullptr, 43.182,
        /* rings             */ true, 1.4, 2.0, 0.18, {0.72f, 0.70f, 0.68f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "自转周期仅 3.9 小时, 是太阳系自转最快的大天体 —— "
                               "离心效应把它拉成 1160×852×513 km 的显著椭球。"
                               "2017 年发现它拥有环, 是首个被确认有环的矮行星。",
    },
    BodyType::DwarfPlanet,
    "自转快到把自己甩成椭球, 是讲解\"自转与流体静力平衡\"的绝佳实例。",
},

// ---------------------------- 鸟神星 Makemake ----------------------------
{
    {
        /* id/name/en        */ "makemake", "鸟神星", "Makemake",
        /* radius/mass/rot   */ 715.0, 3.1e+21, 22.83,
        /* poleRa/Dec/period */ 0.0, 0.0, 112897.0,
        /* grav/esc/den/temp */ 0.50, 0.86, 1.7, -239.0,
        /* albedo/moons      */ 0.81, 1,
        /* color/kind        */ {0.85f, 0.76f, 0.68f}, "dwarf",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ nullptr, 45.430,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "柯伊伯带中第二亮的矮行星 (仅次于冥王星)。"
                               "表面覆盖甲烷与乙烷冰, 呈微红色。2016 年发现一颗暗卫星 MK 2。",
    },
    BodyType::DwarfPlanet,
    "柯伊伯带天体, 表面甲烷冰的光谱特征与冥王星高度相似。",
},

// ===========================================================================
//  卫星 (新增; 原有的月球与 8 颗主要卫星见 celestialdata.cpp)
// ===========================================================================

// ------------------------- 火星卫星 -------------------------
{
    {
        /* id/name/en        */ "phobos", "火卫一", "Phobos",
        /* radius/mass/rot   */ 11.267, 1.0659e+16, 7.653,
        /* poleRa/Dec/period */ 0.0, 0.0, 0.31891,
        /* grav/esc/den/temp */ 0.0057, 0.011, 1.876, -40.0,
        /* albedo/moons      */ 0.071, 0,
        /* color/kind        */ {0.42f, 0.39f, 0.36f}, "cratered",
        /* texture/rotOff    */ "phobos", 0.0,
        /* parent/orbitAu    */ "mars", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 9376.0, 0.0151, 1.093,
        /* desc              */ "火星最大的卫星, 形状不规则 (27×22×18 km)。"
                               "轨道高度仅 9376 km, 正以每百年约 1.8 米的速度螺旋靠近火星, "
                               "约 5000 万年后将撞毁或碎成环。",
    },
    BodyType::Moon,
    "轨道低于同步高度, 因此从火星表面看它西升东落 —— 与月球相反。",
},
{
    {
        /* id/name/en        */ "deimos", "火卫二", "Deimos",
        /* radius/mass/rot   */ 6.2, 1.4762e+15, 30.312,
        /* poleRa/Dec/period */ 0.0, 0.0, 1.263,
        /* grav/esc/den/temp */ 0.003, 0.0056, 1.471, -40.0,
        /* albedo/moons      */ 0.068, 0,
        /* color/kind        */ {0.44f, 0.41f, 0.38f}, "cratered",
        /* texture/rotOff    */ "deimos", 0.0,
        /* parent/orbitAu    */ "mars", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 23463.0, 0.00033, 1.788,
        /* desc              */ "火星较小的卫星 (15×12×11 km), 表面比火卫一平滑。"
                               "它与火卫一可能都是被捕获的小行星, 或是火星形成时残留的碎片。",
    },
    BodyType::Moon,
    "两颗火星卫星都很小且形状不规则, 说明它们未达到流体静力平衡。",
},

// ------------------------- 土星卫星 (Cassini 测绘) -------------------------
{
    {
        /* id/name/en        */ "mimas", "土卫一", "Mimas",
        /* radius/mass/rot   */ 198.2, 3.749e+19, 22.62,
        /* poleRa/Dec/period */ 0.0, 0.0, 0.942,
        /* grav/esc/den/temp */ 0.064, 0.159, 1.147, -200.0,
        /* albedo/moons      */ 0.962, 0,
        /* color/kind        */ {0.86f, 0.85f, 0.83f}, "moon",
        /* texture/rotOff    */ "mimas", 0.0,
        /* parent/orbitAu    */ "saturn", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 185540.0, 0.0196, 1.574,
        /* desc              */ "因表面的赫歇尔撞击坑 (直径 130 km, 约为自身直径的 1/3) "
                               "而酷似《星球大战》中的死星。这次撞击几乎把它撞碎。",
    },
    BodyType::Moon,
    "撞击坑直径达自身直径的 1/3 —— 是讲解\"撞击接近解体极限\"的经典案例。",
},
{
    {
        /* id/name/en        */ "tethys", "土卫三", "Tethys",
        /* radius/mass/rot   */ 531.1, 6.174e+20, 45.31,
        /* poleRa/Dec/period */ 0.0, 0.0, 1.888,
        /* grav/esc/den/temp */ 0.146, 0.394, 0.984, -187.0,
        /* albedo/moons      */ 1.229, 0,
        /* color/kind        */ {0.90f, 0.90f, 0.89f}, "moon",
        /* texture/rotOff    */ "tethys", 0.0,
        /* parent/orbitAu    */ "saturn", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 294619.0, 0.0001, 1.091,
        /* desc              */ "几乎完全由水冰构成, 反照率高达 1.23 (超过 1 是因为"
                               "表面散射效应)。拥有巨大的伊萨卡峡谷 (长 2000 km, 深 3 km)。",
    },
    BodyType::Moon,
    "反照率 1.23 看似违反直觉 —— 实际是强背散射导致, 是很好的光学教学例子。",
},
{
    {
        /* id/name/en        */ "dione", "土卫四", "Dione",
        /* radius/mass/rot   */ 561.4, 1.0954e+21, 65.69,
        /* poleRa/Dec/period */ 0.0, 0.0, 2.737,
        /* grav/esc/den/temp */ 0.232, 0.510, 1.478, -186.0,
        /* albedo/moons      */ 0.998, 0,
        /* color/kind        */ {0.87f, 0.86f, 0.84f}, "moon",
        /* texture/rotOff    */ "dione", 0.0,
        /* parent/orbitAu    */ "saturn", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 377396.0, 0.0022, 0.028,
        /* desc              */ "表面有明亮的冰崖网络 (构造断层)。"
                               "Cassini 发现它可能存在稀薄的氧外逸层, 源自表面冰被辐射分解。",
    },
    BodyType::Moon,
    "存在稀薄氧外逸层 —— 说明\"有氧气\"不等于\"有生命\", 常被误读。",
},
{
    {
        /* id/name/en        */ "rhea", "土卫五", "Rhea",
        /* radius/mass/rot   */ 763.8, 2.3065e+21, 87.97,
        /* poleRa/Dec/period */ 0.0, 0.0, 4.518,
        /* grav/esc/den/temp */ 0.264, 0.635, 1.236, -174.0,
        /* albedo/moons      */ 0.949, 0,
        /* color/kind        */ {0.88f, 0.87f, 0.85f}, "moon",
        /* texture/rotOff    */ "rhea", 0.0,
        /* parent/orbitAu    */ "saturn", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 527108.0, 0.0010, 0.331,
        /* desc              */ "土星第二大卫星, 约 3/4 是水冰。"
                               "曾有报道称它拥有环, 但后续观测否定了这一结论。",
    },
    BodyType::Moon,
    "曾被误报拥有环系 —— 是\"观测证据需要多重独立验证\"的典型案例。",
},
{
    {
        /* id/name/en        */ "iapetus", "土卫八", "Iapetus",
        /* radius/mass/rot   */ 734.5, 1.8056e+21, 1903.7,
        /* poleRa/Dec/period */ 0.0, 0.0, 79.3215,
        /* grav/esc/den/temp */ 0.223, 0.573, 1.088, -143.0,
        /* albedo/moons      */ 0.18, 0,
        /* color/kind        */ {0.62f, 0.58f, 0.53f}, "moon",
        /* texture/rotOff    */ "iapetus", 0.0,
        /* parent/orbitAu    */ "saturn", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 3560820.0, 0.0286, 15.47,
        /* desc              */ "最奇特的大型卫星: 两个半球颜色截然不同 —— "
                               "前导半球暗如煤 (反照率 0.05), 后随半球亮如雪 (反照率 0.6)。"
                               "赤道还有一道高达 13 km、绕行近一周的山脊。",
    },
    BodyType::Moon,
    "阴阳脸 + 赤道巨脊, 且轨道面相对土星赤道倾斜 15.5° —— 轨道倾角不可用圆轨道近似。",
},

// ------------------------- 天王星卫星 (Voyager 2 测绘) -------------------------
{
    {
        /* id/name/en        */ "miranda", "天卫五", "Miranda",
        /* radius/mass/rot   */ 235.8, 6.59e+19, 33.92,
        /* poleRa/Dec/period */ 0.0, 0.0, 1.413,
        /* grav/esc/den/temp */ 0.079, 0.193, 1.214, -187.0,
        /* albedo/moons      */ 0.32, 0,
        /* color/kind        */ {0.72f, 0.72f, 0.72f}, "moon",
        /* texture/rotOff    */ "miranda", 0.0,
        /* parent/orbitAu    */ "uranus", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 129390.0, 0.0013, 4.338,
        /* desc              */ "地貌是太阳系中最混乱的: 垂直断崖高达 20 km "
                               "(是珠峰的 2 倍多), 被称为\"弗兰肯斯坦\"的拼接地形。"
                               "可能经历过多次解体与重新聚合。",
    },
    BodyType::Moon,
    "20 km 垂直断崖 —— 微小天体上出现极端地貌, 说明其地质史极其动荡。",
},
{
    {
        /* id/name/en        */ "ariel", "天卫一", "Ariel",
        /* radius/mass/rot   */ 578.9, 1.353e+21, 60.48,
        /* poleRa/Dec/period */ 0.0, 0.0, 2.520,
        /* grav/esc/den/temp */ 0.269, 0.558, 1.592, -213.0,
        /* albedo/moons      */ 0.53, 0,
        /* color/kind        */ {0.78f, 0.78f, 0.77f}, "moon",
        /* texture/rotOff    */ "ariel", 0.0,
        /* parent/orbitAu    */ "uranus", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 190900.0, 0.0012, 0.041,
        /* desc              */ "天王星卫星中最亮的一颗, 表面年轻, 峡谷与断层密布, "
                               "说明曾有活跃的内部地质过程 (可能由潮汐加热驱动)。",
    },
    BodyType::Moon,
    "表面年轻是内部仍有余热的证据 —— 小卫星的热源只能来自潮汐。",
},
{
    {
        /* id/name/en        */ "umbriel", "天卫二", "Umbriel",
        /* radius/mass/rot   */ 584.7, 1.172e+21, 77.71,
        /* poleRa/Dec/period */ 0.0, 0.0, 4.144,
        /* grav/esc/den/temp */ 0.234, 0.520, 1.459, -198.0,
        /* albedo/moons      */ 0.26, 0,
        /* color/kind        */ {0.58f, 0.57f, 0.56f}, "moon",
        /* texture/rotOff    */ "umbriel", 0.0,
        /* parent/orbitAu    */ "uranus", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 266000.0, 0.0039, 0.128,
        /* desc              */ "天王星卫星中最暗的一颗 (反照率仅 0.26), 表面古老且布满撞击坑。"
                               "北极附近有一处明亮的环形结构 (已命名为 Wunda 坑)。",
    },
    BodyType::Moon,
    "同为天王星卫星却明暗差一倍 —— 反照率差异反映表面物质的演化史不同。",
},
{
    {
        /* id/name/en        */ "titania", "天卫三", "Titania",
        /* radius/mass/rot   */ 788.4, 3.527e+21, 208.9,
        /* poleRa/Dec/period */ 0.0, 0.0, 8.706,
        /* grav/esc/den/temp */ 0.379, 0.773, 1.711, -203.0,
        /* albedo/moons      */ 0.35, 0,
        /* color/kind        */ {0.72f, 0.71f, 0.70f}, "moon",
        /* texture/rotOff    */ "titania", 0.0,
        /* parent/orbitAu    */ "uranus", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 436300.0, 0.0011, 0.079,
        /* desc              */ "天王星最大的卫星。表面有巨大的峡谷系统 (墨西拿深谷长达 1500 km), "
                               "表明其内部曾有膨胀过程, 把冰壳撑裂。",
    },
    BodyType::Moon,
    "峡谷是冰壳被内部膨胀撑裂的证据 —— 冰质天体的\"构造地质学\"。",
},
{
    {
        /* id/name/en        */ "oberon", "天卫四", "Oberon",
        /* radius/mass/rot   */ 761.4, 3.014e+21, 323.1,
        /* poleRa/Dec/period */ 0.0, 0.0, 13.463,
        /* grav/esc/den/temp */ 0.347, 0.734, 1.630, -203.0,
        /* albedo/moons      */ 0.31, 0,
        /* color/kind        */ {0.68f, 0.66f, 0.64f}, "moon",
        /* texture/rotOff    */ "oberon", 0.0,
        /* parent/orbitAu    */ "uranus", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 583500.0, 0.0014, 0.058,
        /* desc              */ "天王星最外侧的大卫星。坑底常见暗色物质, "
                               "可能是渗出的有机质或冰火山沉积物。",
    },
    BodyType::Moon,
    "坑底暗色沉积暗示曾发生渗流 —— 冰质天体也可能有\"火山\"式活动。",
},

// ------------------------- 海王星卫星 -------------------------
{
    {
        /* id/name/en        */ "proteus", "海卫八", "Proteus",
        /* radius/mass/rot   */ 210.0, 4.4e+19, 26.9,
        /* poleRa/Dec/period */ 0.0, 0.0, 1.122,
        /* grav/esc/den/temp */ 0.058, 0.17, 1.3, -220.0,
        /* albedo/moons      */ 0.096, 0,
        /* color/kind        */ {0.50f, 0.49f, 0.48f}, "cratered",
        /* texture/rotOff    */ "proteus", 0.0,
        /* parent/orbitAu    */ "neptune", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 117647.0, 0.0005, 0.524,
        /* desc              */ "形状不规则, 但已接近流体静力平衡的临界尺寸 —— "
                               "它大概是太阳系中\"最大非球形天体\"。",
    },
    BodyType::Moon,
    "处于\"能成球\"的临界尺寸上, 恰好用来演示流体静力平衡的门槛。",
},

// ------------------------- 冥王星卫星 -------------------------
{
    {
        /* id/name/en        */ "charon", "冥卫一", "Charon",
        /* radius/mass/rot   */ 606.0, 1.586e+21, 153.29,
        /* poleRa/Dec/period */ 0.0, 0.0, 6.387,
        /* grav/esc/den/temp */ 0.288, 0.59, 1.702, -220.0,
        /* albedo/moons      */ 0.372, 0,
        /* color/kind        */ {0.72f, 0.70f, 0.68f}, "moon",
        /* texture/rotOff    */ "charon", 0.0,
        /* parent/orbitAu    */ "pluto", 0.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 19591.0, 0.0002, 0.080,
        /* desc              */ "直径为冥王星的一半, 两者互相潮汐锁定, "
                               "围绕共同质心旋转 —— 常被当作双矮行星系统。"
                               "北极地区的\"魔多\"暗斑是红色有机质 (托林)。",
    },
    BodyType::Moon,
    "冥王星与卡戎的质量比接近 8:1, 质心落在冥王星之外 —— 严格说是双星系统。",
},

// ===========================================================================
//  小行星 (形状不规则, 未达到流体静力平衡)
// ===========================================================================

{
    {
        /* id/name/en        */ "vesta", "灶神星", "Vesta",
        /* radius/mass/rot   */ 262.7, 2.589e+20, 5.342,
        /* poleRa/Dec/period */ 309.0, 42.0, 1325.75,
        /* grav/esc/den/temp */ 0.22, 0.36, 3.456, -108.0,
        /* albedo/moons      */ 0.423, 0,
        /* color/kind        */ {0.66f, 0.63f, 0.58f}, "cratered",
        /* texture/rotOff    */ "vesta", 0.0,
        /* parent/orbitAu    */ nullptr, 2.3617,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "主带第二大天体, 密度 3.46 说明它是分化过的: "
                               "有铁镍核与硅酸盐幔。南极的雷亚希尔维亚撞击坑直径 500 km, "
                               "喷出的碎片形成了 V 型小行星族, 并有陨石落到地球。",
    },
    BodyType::Asteroid,
    "已发生分异 (有核幔结构) —— 说明小行星并非都是\"一堆碎石\"。",
},
{
    {
        /* id/name/en        */ "pallas", "智神星", "Pallas",
        /* radius/mass/rot   */ 256.0, 2.04e+20, 7.813,
        /* poleRa/Dec/period */ 0.0, 0.0, 1686.0,
        /* grav/esc/den/temp */ 0.21, 0.35, 2.89, -108.0,
        /* albedo/moons      */ 0.155, 0,
        /* color/kind        */ {0.55f, 0.54f, 0.52f}, "cratered",
        /* texture/rotOff    */ nullptr, 0.0,     // 无全球图 -> 均匀色球
        /* parent/orbitAu    */ nullptr, 2.7726,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "主带第三大天体。轨道倾角 34.8°, 在主带中极为异常 —— "
                               "这使它常被单独归类。表面布满撞击坑, 直径约 500 km。",
    },
    BodyType::Asteroid,
    "轨道倾角 34.8° 远超主带多数天体 —— 讲解轨道倾角分布的绝佳反例。",
},
{
    {
        /* id/name/en        */ "juno", "婚神星", "Juno",
        /* radius/mass/rot   */ 127.0, 2.67e+19, 7.210,
        /* poleRa/Dec/period */ 0.0, 0.0, 1594.0,
        /* grav/esc/den/temp */ 0.11, 0.18, 3.15, -108.0,
        /* albedo/moons      */ 0.238, 0,
        /* color/kind        */ {0.62f, 0.58f, 0.53f}, "cratered",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ nullptr, 2.6683,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "主带中较大的 S 型小行星, 表面含铁, 反照率较高。"
                               "1804 年发现时曾被误认为行星。它是第一颗被发现拥有卫星的小行星。",
    },
    BodyType::Asteroid,
    "S 型 (石质含铁) 与小行星光谱分类的入门案例。",
},
{
    {
        /* id/name/en        */ "hygiea", "健神星", "Hygiea",
        /* radius/mass/rot   */ 216.5, 8.67e+19, 13.83,
        /* poleRa/Dec/period */ 0.0, 0.0, 2032.0,
        /* grav/esc/den/temp */ 0.091, 0.21, 1.94, -108.0,
        /* albedo/moons      */ 0.072, 0,
        /* color/kind        */ {0.38f, 0.37f, 0.36f}, "cratered",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ nullptr, 3.1417,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "主带第四大天体。2019 年 VLT 观测显示它的形状非常接近球形, "
                               "且直径超过 400 km —— 因此被认为是矮行星的有力候选体。",
    },
    BodyType::Asteroid,
    "形状近球形但未正式列为矮行星 —— 说明分类边界存在观测不确定性。",
},
{
    {
        /* id/name/en        */ "eros", "爱神星", "Eros",
        /* radius/mass/rot   */ 8.45, 6.687e+15, 5.270,
        /* poleRa/Dec/period */ 11.37, 17.23, 643.22,
        /* grav/esc/den/temp */ 0.0059, 0.0103, 2.67, -75.0,
        /* albedo/moons      */ 0.25, 0,
        /* color/kind        */ {0.60f, 0.55f, 0.48f}, "cratered",
        /* texture/rotOff    */ "eros", 0.0,
        /* parent/orbitAu    */ nullptr, 1.4582,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "近地小行星 (轨道可进入火星轨道内侧)。"
                               "NEAR Shoemaker 于 2001 年在此首次着陆小行星, "
                               "此后一直是个研究型地标。形状像一根 33×13 km 的花生。",
    },
    BodyType::Asteroid,
    "人类第一个着陆的小行星 (2001, NEAR Shoemaker) —— 近距离碎石探测的起点。",
},
{
    {
        /* id/name/en        */ "bennu", "贝努", "Bennu",
        /* radius/mass/rot   */ 0.2625, 7.329e+10, 4.296,
        /* poleRa/Dec/period */ 0.0, 0.0, 436.65,
        /* grav/esc/den/temp */ 6.3e-05, 0.0002, 1.19, -73.0,
        /* albedo/moons      */ 0.044, 0,
        /* color/kind        */ {0.30f, 0.28f, 0.26f}, "cratered",
        /* texture/rotOff    */ "bennu", 0.0,
        /* parent/orbitAu    */ nullptr, 1.1264,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "碳质近地小行星, OSIRIS-REx 于 2020 年采样、"
                               "2023 年 9 月把 121.6 g 样本送回地球 —— "
                               "这是 NASA 首次小行星采样返回。样本中含碳与含水矿物。",
    },
    BodyType::Asteroid,
    "含碳与含水矿物: 是研究太阳系早期有机物的重要样本来源。",
},
{
    {
        /* id/name/en        */ "ryugu", "龙宫", "Ryugu",
        /* radius/mass/rot   */ 0.448, 4.5e+11, 7.633,
        /* poleRa/Dec/period */ 0.0, 0.0, 474.0,
        /* grav/esc/den/temp */ 0.00011, 0.0003, 1.19, -43.0,
        /* albedo/moons      */ 0.045, 0,
        /* color/kind        */ {0.32f, 0.30f, 0.28f}, "cratered",
        /* texture/rotOff    */ "ryugu", 0.0,
        /* parent/orbitAu    */ nullptr, 1.1896,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "日本 Hayabusa2 于 2019 年采样、2020 年 12 月送回 5.4 g 样本。"
                               "样本中检出 20 多种氨基酸 —— 为\"生命原料由小行星带到地球\""
                               "提供了直接证据。",
    },
    BodyType::Asteroid,
    "样本中检出氨基酸 —— 关于地球生命原料来源的关键证据。",
},

// ===========================================================================
//  彗星 (高偏心、高倾角, 不能用圆轨道近似)
// ===========================================================================

{
    {
        /* id/name/en        */ "halley", "哈雷彗星", "1P/Halley",
        /* radius/mass/rot   */ 5.5, 2.2e+14, 52.8,
        /* poleRa/Dec/period */ 0.0, 0.0, 27510.0,
        /* grav/esc/den/temp */ 0.0004, 0.002, 0.6, -70.0,
        /* albedo/moons      */ 0.04, 0,
        /* color/kind        */ {0.24f, 0.23f, 0.22f}, "comet",
        /* texture/rotOff    */ nullptr, 0.0,     // 只有飞掠影像, 无全球图
        /* parent/orbitAu    */ nullptr, 17.834,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "最著名的周期彗星, 公转周期约 75 年。"
                               "★ 轨道逆行 (倾角 162.3°), 偏心率 0.967。"
                               "上一次回归 1986 年, 下一次 2061 年 7 月。"
                               "它的轨道与地球相交, 形成每年 5 月的宝瓶座 η 流星雨。",
    },
    BodyType::Comet,
    "逆行轨道 (i=162°) + e=0.967 —— 两个特征都远超行星范围, 必须用真实开普勒根数。",
},
{
    {
        /* id/name/en        */ "encke", "恩克彗星", "2P/Encke",
        /* radius/mass/rot   */ 2.4, 7.0e+13, 11.08,
        /* poleRa/Dec/period */ 0.0, 0.0, 1204.0,
        /* grav/esc/den/temp */ 0.0001, 0.0006, 0.6, -70.0,
        /* albedo/moons      */ 0.05, 0,
        /* color/kind        */ {0.26f, 0.25f, 0.23f}, "comet",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ nullptr, 2.2151,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "公转周期仅 3.3 年, 是已知周期最短的彗星。"
                               "它是金牛座流星雨的母体, 也是每年 10-11 月火流星现象的来源。"
                               "轨道近日点仅 0.34 AU, 比水星更靠近太阳。",
    },
    BodyType::Comet,
    "周期最短的彗星 (3.3 年), 近日点比水星还近 —— 说明彗星轨道可深入内太阳系。",
},
{
    {
        /* id/name/en        */ "churyumov", "67P/丘留莫夫-格拉西缅科", "67P/Churyumov-Gerasimenko",
        /* radius/mass/rot   */ 2.0, 9.982e+12, 12.4,
        /* poleRa/Dec/period */ 0.0, 0.0, 2354.0,
        /* grav/esc/den/temp */ 0.0001, 0.0005, 0.533, -70.0,
        /* albedo/moons      */ 0.06, 0,
        /* color/kind        */ {0.28f, 0.26f, 0.24f}, "comet",
        /* texture/rotOff    */ "churyumov", 0.0,   // Rosetta 测绘, 有全球图
        /* parent/orbitAu    */ nullptr, 3.4620,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "欧空局 Rosetta 任务的目标: 2014 年成为首个被探测器环绕的彗星, "
                               "其 Philae 着陆器实现了史上首次彗核软着陆。"
                               "形状像一只橡皮鸭 —— 由两个曾独立形成的天体低速并合而成。",
    },
    BodyType::Comet,
    "双瓣结构证明彗核可由\"低速并合\"形成 —— 行星形成机制的活样本。",
},
{
    {
        /* id/name/en        */ "neowise", "NEOWISE 彗星", "C/2020 F3 (NEOWISE)",
        /* radius/mass/rot   */ 2.5, 1.0e+14, 7.6,
        /* poleRa/Dec/period */ 0.0, 0.0, 2513080.0,
        /* grav/esc/den/temp */ 0.0002, 0.0007, 0.6, -70.0,
        /* albedo/moons      */ 0.04, 0,
        /* color/kind        */ {0.30f, 0.28f, 0.25f}, "comet",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ nullptr, 359.0,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "2020 年 7 月肉眼可见的大彗星, 亮度达到 1 等以内。"
                               "★ 偏心率 0.99918, 几乎是抛物线 —— 这是开普勒求解器最极端的情形。"
                               "轨道周期约 6800 年, 轨道倾角 128.9° (顺行但接近垂直)。",
    },
    BodyType::Comet,
    "e=0.99918 逼近抛物线 —— 用来说明\"束缚轨道\"与\"逃逸轨道\"的边界。",
},
{
    {
        /* id/name/en        */ "wild2", "81P/怀尔德 2", "81P/Wild 2",
        /* radius/mass/rot   */ 2.0, 2.3e+13, 13.5,
        /* poleRa/Dec/period */ 0.0, 0.0, 2341.0,
        /* grav/esc/den/temp */ 0.0001, 0.0005, 0.6, -70.0,
        /* albedo/moons      */ 0.03, 0,
        /* color/kind        */ {0.27f, 0.25f, 0.23f}, "comet",
        /* texture/rotOff    */ nullptr, 0.0,
        /* parent/orbitAu    */ nullptr, 3.4500,
        /* rings             */ false, 0.0, 0.0, 0.0, {0.0f, 0.0f, 0.0f},
        /* atmo              */ false, {0.0f, 0.0f, 0.0f}, 0.0, 0.0,
        /* moon-orbit a/e/i  */ 0.0, 0.0, 0.0,
        /* desc              */ "NASA Stardust 任务于 2004 年穿越其彗发并采集尘埃, "
                               "2006 年把样本送回地球。样本中检出甘氨酸等有机物与含水矿物, "
                               "以及只有在极高温下才能形成的晶体 —— 说明彗星物质曾经历内太阳系的高温。",
    },
    BodyType::Comet,
    "样本含高温成因矿物, 却存在于冰冷彗核中 —— 太阳系物质曾大范围混合。",
},

};

const int SMALL_BODIES_TABLE_COUNT =
    int(sizeof(SMALL_BODIES_TABLE) / sizeof(SMALL_BODIES_TABLE[0]));

// ---------------------------------------------------------------------------
//  访问器
// ---------------------------------------------------------------------------

const SmallBody *bodies() { return SMALL_BODIES_TABLE; }
int             count()   { return SMALL_BODIES_TABLE_COUNT; }

const SmallBody *find(const char *id)
{
    if (!id)
        return nullptr;
    for (int i = 0; i < SMALL_BODIES_TABLE_COUNT; ++i) {
        if (std::strcmp(SMALL_BODIES_TABLE[i].data.id, id) == 0)
            return &SMALL_BODIES_TABLE[i];
    }
    return nullptr;
}

} // namespace small
