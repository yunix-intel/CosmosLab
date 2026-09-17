// ============================================================================
//  cosmos.cpp —— 宇宙大尺度结构渲染实现
// ============================================================================

#include "cosmos.h"
#include "cosmosdata.h"

#include <QDebug>
#include <QOpenGLFunctions_3_3_Core>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <QString>
#include <QIODevice>
#include <QFile>
#include <cstring>
#include <random>

namespace {

// 对数映射参数
constexpr double kD0      = 0.1;          // Mly, 让最近天体也有间距
constexpr double kDMax    = 46500.0;      // Mly, 可观测宇宙半径
constexpr float  kSceneR  = 100.0f;

// 粒子属性: 位置(3) + 亮度(1)
// ★ 顶点格式: aDir(3) + aDistMly(1) + aBright(1)
//   存**物理量**而非映射后的坐标 —— 切换映射只需改 uniform,
//   无需重建顶点缓冲。
constexpr int kFloats = 5;

const char *kVert = R"(
#version 330 core
layout(location = 0) in vec3  aDir;        // 单位方向
layout(location = 1) in float aDistMly;    // 真实距离 (百万光年)
layout(location = 2) in float aBright;

uniform mat4  uViewProj;
uniform float uPixelScale;
uniform float uSize;
uniform int   uMapMode;      // 0 = 对数压缩, 1 = 真实比例

out float vBright;

const float kD0     = 0.1;         // Mly, 让最近天体也有间距
const float kDMax   = 46500.0;     // Mly, 可观测宇宙半径
const float kSceneR = 100.0;

// 距离 -> 场景半径
//
// ★ 映射放在着色器而非 CPU: 切换模式只需改 uMapMode, 零成本。
//   放 CPU 则每次切换都要重建并重传整个顶点缓冲。
//
// ★ 本函数必须与 C++ 侧 Cosmos::sceneRadiusFromMly 保持**完全一致**,
//   否则标签位置会与粒子错开。
float mappedRadius(float mly) {
    if (uMapMode == 1) {
        return clamp(mly / kDMax, 0.0, 1.0) * kSceneR;      // 真实比例
    }
    return log(1.0 + mly / kD0) / log(1.0 + kDMax / kD0) * kSceneR;
}

void main() {
    vec3 pos = normalize(aDir) * mappedRadius(aDistMly);
    vec4 clip = uViewProj * vec4(pos, 1.0);
    gl_Position = clip;
    float sz = uSize * uPixelScale / max(clip.w, 0.001);
    gl_PointSize = clamp(sz, 0.7, 4.5);
    vBright = aBright;
}
)";

const char *kFrag = R"(
#version 330 core
in float vBright;
uniform float uAlpha;
uniform vec3  uColor;
out vec4 FragColor;

void main() {
    // 方形点在小尺寸下会看出马赛克, 用圆形软边
    vec2 d = gl_PointCoord - vec2(0.5);
    float r2 = dot(d, d) * 4.0;
    if (r2 > 1.0) discard;
    float a = (1.0 - r2) * uAlpha * vBright;
    FragColor = vec4(uColor * a, a);
}
)";

} // namespace

// ---------------------------------------------------------------------------
//  距离映射
// ---------------------------------------------------------------------------

float Cosmos::sceneRadiusFromMly(double mly, int mode)
{
    if (mode == MapLinear)
        return float(qBound(0.0, mly / kDMax, 1.0)) * kSceneR;
    const double num = std::log10(1.0 + mly / kD0);
    const double den = std::log10(1.0 + kDMax / kD0);
    return float(num / den) * kSceneR;
}

float Cosmos::distToScene(double mly)
{
    return sceneRadiusFromMly(mly, MapLog);       // 兼容旧调用
}

double Cosmos::mlyFromSceneRadius(double r)
{
    // ★ 对数映射的**反函数**, 用于把"旧的场景坐标"迁移成真实距离。
    //   现有粒子的场景坐标本就是对数映射的结果, 反推后**视觉完全不变**,
    //   但数据从此是物理量 (距离 Mly)。
    //     R = log10(1 + d/d0) / log10(1 + dMax/d0) * Rmax
    //  => d = d0 * (10^(R/Rmax * log10(1+dMax/d0)) - 1)
    if (r <= 0.0)
        return 0.0;
    const double den = std::log10(1.0 + kDMax / kD0);
    const double t = qBound(0.0, r / double(kSceneR), 1.0);
    return kD0 * (std::pow(10.0, t * den) - 1.0);
}

QVector<CosmosMarker> Cosmos::markers(int mode)
{
    // ★ 标签位置必须与粒子用**同一种映射** —— 否则切换模式后
    //   标签会飘到粒子之外。所以这里也接受 mode 参数。
    //   同时把真实距离填进 distMly, 供 UI 显示与二次计算。
    QVector<CosmosMarker> out;

    // ---- 本星系群成员 ----
    // ★ 本星系群整体在一个极小的半径内 (10 Mly), 对数映射后约在
    //   场景半径的 33%。为了让各成员在屏幕上可分辨, 用**真实方向**
    //   但人为拉开间距 —— 否则 0.16 到 2.5 Mly 的差异在对数下只有
    //   几个像素。这是有意的视觉放大, 标注里给出真实距离作补偿。
    for (int i = 0; i < LOCAL_GROUP_COUNT; ++i) {
        const GalaxyData &g = LOCAL_GROUP[i];
        const double r = sceneRadiusFromMly(g.distanceMly, mode);
        // 用赤经/赤纬定方向; 本星系群成员挤在一起, 加一圈人为的角度偏移
        const double ra  = g.raDeg  * M_PI / 180.0;
        const double dec = g.decDeg * M_PI / 180.0;
        // 银河系自身放在原点
        QVector3D p;
        if (g.distanceMly < 1e-9) {
            p = QVector3D(0.0f, 0.0f, 0.0f);
        } else {
            // 在真实方向上再加 15° 的分离, 让相邻成员不重叠
            const double sep = 0.26;   // 弧度
            p = QVector3D(float(std::cos(dec) * std::cos(ra + sep) * r),
                          float(std::sin(dec) * r),
                          float(std::cos(dec) * std::sin(ra + sep) * r));
        }
        out.append({ QString::fromUtf8(g.nameCn),
                     QString::fromUtf8(g.nameEn),
                     g.distanceMly < 1e-9
                        ? QStringLiteral("我们所在")
                        : QStringLiteral("%1 百万光年").arg(g.distanceMly, 0, 'f', 2),
                     p, 0, g.distanceMly });
    }

    // ---- 室女座星系团成员 ----
    for (int i = 0; i < VIRGO_CLUSTER_COUNT; ++i) {
        const GalaxyData &g = VIRGO_CLUSTER[i];
        const double r = sceneRadiusFromMly(g.distanceMly, mode);
        const double ra  = g.raDeg  * M_PI / 180.0;
        const double dec = g.decDeg * M_PI / 180.0;
        const QVector3D p(float(std::cos(dec) * std::cos(ra) * r),
                          float(std::sin(dec) * r),
                          float(std::cos(dec) * std::sin(ra) * r));
        out.append({ QString::fromUtf8(g.nameCn),
                     QString::fromUtf8(g.nameEn),
                     QStringLiteral("%1 百万光年").arg(g.distanceMly, 0, 'f', 1),
                     p, 1, g.distanceMly });
    }

    // ---- 大尺度结构 ----
    for (int i = 0; i < LARGE_STRUCTURES_COUNT; ++i) {
        const LargeStructure &s = LARGE_STRUCTURES[i];
        const double r = sceneRadiusFromMly(s.distanceFromEarthMly, mode);
        // 用结构名做稳定的方位角散列 (同名每次运行位置一致)
        uint h = 2166136261u;
        for (const char *c = s.nameEn; *c; ++c)
            h = (h ^ uint(*c)) * 16777619u;
        const double az = double(h % 3600) / 3600.0 * 2.0 * M_PI;
        const double el = (double((h / 3600) % 1000) / 1000.0 - 0.5) * M_PI * 0.9;
        const QVector3D p(float(std::cos(el) * std::cos(az) * r),
                          float(std::sin(el) * r),
                          float(std::cos(el) * std::sin(az) * r));
        out.append({ QString::fromUtf8(s.nameCn),
                     QString::fromUtf8(s.nameEn),
                     QStringLiteral("%1 百万光年").arg(s.distanceFromEarthMly, 0, 'f', 0),
                     p, s.kind == 2 ? 4 : (s.kind == 0 ? 3 : 2),
                     s.distanceFromEarthMly });
    }

    return out;
}

// ---------------------------------------------------------------------------

int Cosmos::s_lastBuilt = 0;
int Cosmos::s_lastBuiltSdss = 0;
int Cosmos::s_lastDrawn = 0;

Cosmos::Cosmos() = default;
Cosmos::~Cosmos() = default;

void Cosmos::destroy()
{
    if (m_vbo.isCreated())
        m_vbo.destroy();
    if (m_vao.isCreated())
        m_vao.destroy();
    delete m_prog;
    m_prog = nullptr;
    m_ready = false;
}

void Cosmos::init(QOpenGLFunctions_3_3_Core *f)
{
    if (m_ready || !f)
        return;
    m_f = f;

    auto *p = new QOpenGLShaderProgram;
    const bool ok = p->addShaderFromSourceCode(QOpenGLShader::Vertex, kVert)
                 && p->addShaderFromSourceCode(QOpenGLShader::Fragment, kFrag)
                 && p->link();
    if (!ok) {
        qWarning() << "[宇宙] 着色器失败:" << p->log();
        delete p;
        return;
    }
    m_prog = p;

    build();
    m_ready = m_prog && m_vbo.isCreated() && m_count > 0;

    qInfo() << "[宇宙] 初始化" << (m_ready ? "成功" : "失败")
            << " 星系点:" << m_count
            << " (纤维:" << m_filamentCount
            << " 空洞边缘:" << m_voidCount << ")";
}

// ---------------------------------------------------------------------------
//  生成宇宙网
//
//  ★ 三步模型 (对应真实的形成机制):
//      1. 撒节点 —— 代表星系团/超星系团 (暗物质晕的密集处)
//      2. 连纤维 —— 邻近节点之间的桥, 星系沿纤维分布
//      3. 挖空洞 —— 掏空球体内几乎不放星系
//
//    纯随机分布会得到均匀噪点, 完全不像宇宙网 —— 这一点实测对比很明显。
// ---------------------------------------------------------------------------

// 红移 -> 共动距离 (百万光年)。
//
// ★ 用 Simpson 积分算标准 ΛCDM 公式, 不用 z*c/H0 线性近似 ——
//   后者在 z=1 时低估约 30%, 会把远处的纤维"压扁"。
//   参数取 Planck 2018: H0=67.4 km/s/Mpc, Ωm=0.315。
static double comovingDistanceMly(double z)
{
    if (z <= 0.0)
        return 0.0;
    constexpr double H0 = 67.4, OM0 = 0.315, C_KMS = 299792.458;
    constexpr double MPC_TO_MLY = 3.26156;
    const int n = 128;
    const double dz = z / n;
    double tot = 0.0;
    for (int i = 0; i <= n; ++i) {
        const double zz = i * dz;
        const double e = std::sqrt(OM0 * std::pow(1.0 + zz, 3) + (1.0 - OM0));
        const double w = (i == 0 || i == n) ? 1.0 : (i % 2 ? 4.0 : 2.0);
        tot += w / e;
    }
    return (C_KMS / H0) * (dz / 3.0) * tot * MPC_TO_MLY;
}

// ---------------------------------------------------------------------------
//  SDSS 真实星系星表
//
//  ★ 数据来源: SDSS DR17 eBOSS LRG 聚类样本
//     https://data.sdss.org/sas/dr17/eboss/lss/catalogs/DR16/
//       eBOSS_LRG_clustering_data-NGC-vDR16.fits   (北银极 107,500)
//       eBOSS_LRG_clustering_data-SGC-vDR16.fits   (南银极  67,316)
//     每个星系都有**光谱测定的红移**, 由红移换算共动距离 ——
//     于是这是真实的星系三维位置, 不是程序生成的假分布。
//
//  ★ 样本特征 (必须在 UI 说明, 否则会被误读为"全部星系"):
//     红移 0.600 ~ 1.000   共动距离 7,429 ~ 11,093 Mly
//     这不是全天完整样本, 而是 LRG (亮红星系) 的**观测窗口**。
//
//  ★ 与程序生成粒子的关系:
//     SDSS 只覆盖 z=0.6~1.0 这个壳层。近处 (本星系群/室女团) 与
//     更远处 (CMB) 没有数据, 故保留程序生成的示意结构。
//     UI 里明确区分"实测"与"示意" —— 不混淆两者。
//
//  ★ 格式: [uint32 计数][每记录 4 个 float32: ra, dec, z, weight]
//     与 assets/lss/lrg.bin 一致 (由 tools/parse_sdss.py 生成)
// ---------------------------------------------------------------------------

// 加载 SDSS 星表。返回 (方向, 距离 Mly, 权重) 三元组列表。
// 文件缺失时返回空 (不报错) —— 这样没有数据的机器也能正常运行。
struct SdssGalaxy {
    float dirX, dirY, dirZ;
    float distMly;
    float weight;
};

static QVector<SdssGalaxy> loadSdssCatalog(const QString &path)
{
    QVector<SdssGalaxy> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning().noquote()
            << QString("[宇宙] 未找到 SDSS 星表 %1 —— 将只用示意结构").arg(path);
        return out;
    }
    const QByteArray raw = f.readAll();
    f.close();

    // 头 4 字节是计数
    if (raw.size() < 4) {
        qWarning() << "[宇宙] SDSS 星表文件过短";
        return out;
    }
    quint32 n = 0;
    std::memcpy(&n, raw.constData(), 4);
    const qsizetype need = 4 + qsizetype(n) * 4 * 4;
    if (raw.size() < need) {
        qWarning().noquote()
            << QString("[宇宙] SDSS 星表长度不符: 期望 %1, 实际 %2")
                   .arg(need).arg(raw.size());
        return out;
    }

    out.reserve(int(n));
    const float *p = reinterpret_cast<const float *>(raw.constData() + 4);
    for (quint32 i = 0; i < n; ++i) {
        const float ra = p[i * 4 + 0];
        const float dec = p[i * 4 + 1];
        const float z = p[i * 4 + 2];
        const float w = p[i * 4 + 3];
        if (!(z > 0.0f) || !(w > 0.0f)
            || ra < 0.0f || ra > 360.0f || dec < -90.0f || dec > 90.0f)
            continue;

        const double raR = double(ra) * M_PI / 180.0;
        const double decR = double(dec) * M_PI / 180.0;
        const double cd = std::cos(decR);
        SdssGalaxy g;
        g.dirX = float(cd * std::cos(raR));
        g.dirY = float(std::sin(decR));
        g.dirZ = float(cd * std::sin(raR));
        // 距离在 C++ 侧就算好 (它是物理量, 与映射模式无关)
        g.distMly = float(comovingDistanceMly(double(z)));
        g.weight = w;
        out.append(g);
    }
    return out;
}

void Cosmos::build()
{
    QVector<float> data;
    data.reserve(52000 * kFloats);

    std::mt19937 rng(20260916u);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    std::normal_distribution<double> gauss(0.0, 1.0);

    // ---- 1. 节点 (星系团尺度) ----
    struct Node { double x, y, z; };
    QVector<Node> nodes;
    const int kNodeCount = 220;
    for (int i = 0; i < kNodeCount; ++i) {
        // 节点本身也是成团的: 先撒父节点, 再在其附近撒子节点。
        // 单层均匀分布会让纤维网络显得过于规整。
        const double R = kSceneR * std::pow(uni(rng), 0.42);   // 径向分布
        const double th = uni(rng) * 2.0 * M_PI;
        const double ph = std::acos(2.0 * uni(rng) - 1.0);
        nodes.append({ R * std::sin(ph) * std::cos(th),
                       R * std::cos(ph) * 0.72,      // 略压扁: 观测到的结构非完全各向同性
                       R * std::sin(ph) * std::sin(th) });
    }

    // ---- 2. 空洞 ----
    // 在若干位置掏空, 让宇宙网出现明显的"海绵"结构
    struct Void { double x, y, z, r; };
    QVector<Void> voids;
    for (int i = 0; i < 14; ++i) {
        const double R = kSceneR * (0.25 + uni(rng) * 0.65);
        const double th = uni(rng) * 2.0 * M_PI;
        const double ph = std::acos(2.0 * uni(rng) - 1.0);
        voids.append({ R * std::sin(ph) * std::cos(th),
                       R * std::cos(ph) * 0.72,
                       R * std::sin(ph) * std::sin(th),
                       7.0 + uni(rng) * 13.0 });
    }
    auto inVoid = [&](double x, double y, double z) {
        for (const Void &v : voids) {
            const double dx = x - v.x, dy = y - v.y, dz = z - v.z;
            if (dx*dx + dy*dy + dz*dz < v.r * v.r)
                return true;
        }
        return false;
    };

    // ★ push 现在接受**场景坐标**, 内部反推成 (方向, 真实距离 Mly)。
    //
    //   为什么这么做: 顶点格式已改为存物理量 (方向 + 距离), 映射在着色器。
    //   而现有的粒子是用场景坐标撒的 —— 就地反推即可完成迁移:
    //       r = |(x,y,z)|                  场景半径
    //       d = mlyFromSceneRadius(r)      真实距离 (对数映射的反函数)
    //       dir = (x,y,z)/r                方向
    //   由于"反推再正推得到同样的 r", **视觉完全不变**。
    auto push = [&](double x, double y, double z, float bright) {
        const double r = std::sqrt(x * x + y * y + z * z);
        if (r < 1e-9) {
            data << 0.0f << 0.0f << 0.0f << 0.0f << bright;
            return;
        }
        const double d = mlyFromSceneRadius(r);
        data << float(x / r) << float(y / r) << float(z / r)
             << float(d) << bright;
    };

    // ---- 3. 沿纤维撒星系 ----
    //
    //  ★ 跳过 SDSS 覆盖的距离区间 (7,400~11,100 Mly) ——
    //    那里将由真实星系填充。不跳过会出现"双重结构":
    //    同一片空间既有实测点又有假点, 视觉糊成一团。
    // 每个节点连到它的 3~5 个最近邻, 沿连线撒点
    for (int i = 0; i < nodes.size(); ++i) {
        // 找最近邻
        QVector<QPair<double,int>> dists;
        dists.reserve(nodes.size());
        for (int j = 0; j < nodes.size(); ++j) {
            if (i == j) continue;
            const double dx = nodes[j].x - nodes[i].x;
            const double dy = nodes[j].y - nodes[i].y;
            const double dz = nodes[j].z - nodes[i].z;
            dists.append({ dx*dx + dy*dy + dz*dz, j });
        }
        std::sort(dists.begin(), dists.end());

        const int links = 2 + int(uni(rng) * 3);
        for (int k = 0; k < links && k < dists.size(); ++k) {
            const Node &a = nodes[i];
            const Node &b = nodes[dists[k].second];
            const int n = 55 + int(uni(rng) * 90);
            for (int t = 0; t < n; ++t) {
                const double u = uni(rng);
                // 垂直方向抖动: 纤维不是直线, 有自己的粗细
                const double jitter = 0.9;
                const double x = a.x + (b.x - a.x) * u + gauss(rng) * jitter;
                const double y = a.y + (b.y - a.y) * u + gauss(rng) * jitter;
                const double z = a.z + (b.z - a.z) * u + gauss(rng) * jitter;
                if (inVoid(x, y, z))
                    continue;
                // 亮度: 靠近节点更亮 (节点处星系更密)
                const double toNode = qMin(u, 1.0 - u) * 2.0;
                // ★ 跳过 SDSS 覆盖区间: 该处用真实星系, 不再撒假点
                const double rHere =
                    std::sqrt(x * x + y * y + z * z);
                const double dHere = mlyFromSceneRadius(rHere);
                if (dHere > 7400.0 && dHere < 11100.0)
                    continue;

                const float br = float(0.35 + 0.65 * (1.0 - toNode)
                                       * (0.6 + 0.4 * uni(rng)));
                push(x, y, z, br);
                ++m_filamentCount;
            }
        }
    }

    // ---- 4. 空洞边缘 (少量星系, 且偏暗) ----
    for (const Void &v : voids) {
        const int n = 90;
        for (int t = 0; t < n; ++t) {
            const double th = uni(rng) * 2.0 * M_PI;
            const double ph = std::acos(2.0 * uni(rng) - 1.0);
            const double rr = v.r * (0.92 + uni(rng) * 0.28);
            push(v.x + rr * std::sin(ph) * std::cos(th),
                 v.y + rr * std::cos(ph),
                 v.z + rr * std::sin(ph) * std::sin(th),
                 float(0.16 + 0.24 * uni(rng)));
            ++m_voidCount;
        }
    }

    // ---- 5. 背景星系 (稀疏, 填充视野) ----
    for (int i = 0; i < 9000; ++i) {
        const double R = kSceneR * std::pow(uni(rng), 0.35);
        const double th = uni(rng) * 2.0 * M_PI;
        const double ph = std::acos(2.0 * uni(rng) - 1.0);
        const double x = R * std::sin(ph) * std::cos(th);
        const double y = R * std::cos(ph) * 0.72;
        const double z = R * std::sin(ph) * std::sin(th);
        if (inVoid(x, y, z))
            continue;
        // ★ 同上: SDSS 覆盖区间内不撒假点
        {
            const double rHere = std::sqrt(x * x + y * y + z * z);
            const double dHere = mlyFromSceneRadius(rHere);
            if (dHere > 7400.0 && dHere < 11100.0)
                continue;
        }
        push(x, y, z, float(0.10 + 0.22 * uni(rng)));
        ++m_voidCount;
    }

    // ★ 压力测试: SS_COSMOS_MULT=<N> 把粒子数放大 N 倍。
    //   用于测"帧率 vs 粒子数"曲线, 据此判断能承载多大的星表。
    //   放大的方式是**带微小抖动的重复** —— 直接复制会让同一像素
    //   叠加完全相同的数据, 掩盖真实的过度绘制成本。
    {
        const int mult = qEnvironmentVariableIntValue("SS_COSMOS_MULT");
        if (mult > 1) {
            const int baseN = data.size() / kFloats;
            QVector<float> big;
            big.reserve(baseN * mult * kFloats);
            big += data;
            std::mt19937 rng(20260917u);
            std::uniform_real_distribution<float> jit(-0.35f, 0.35f);
            for (int m = 1; m < mult; ++m) {
                for (int i = 0; i < baseN; ++i) {
                    const float *src = &data[i * kFloats];
                    big << src[0] + jit(rng) << src[1] + jit(rng)
                        << src[2] + jit(rng) << src[3];
                }
            }
            data = big;
            qWarning().noquote()
                << QString("[宇宙] 压力测试: 粒子数放大 %1 倍 -> %2")
                       .arg(mult).arg(baseN * mult);
        }
    }

    // ---- 6. SDSS 真实星系 (替换该距离区间内的示意结构) ----
    //
    //  ★ 这是"真实数据优先"的落实: 在 SDSS 有观测的距离区间内,
    //    用**实测的星系位置**而不是程序生成的点。
    //
    //  ★ 顶点格式与新架构一致 (方向 + 真实距离 + 亮度),
    //    所以距离映射仍在着色器里做 —— SDSS 数据自动支持两种映射模式。
    {
        const QVector<SdssGalaxy> cat = loadSdssCatalog(
            QStringLiteral("D:/tmp/solar-system-cpp/assets/lss/lrg.bin"));
        m_sdssCount = cat.size();
        for (const SdssGalaxy &g : cat) {
            // 亮度按权重微调: 权重高的区域观测更完整, 不必额外提亮,
            // 这里只用很小的抖动避免"一片死白"
            const float br = 0.42f + 0.35f * qMin(g.weight, 2.0f) / 2.0f;
            data << g.dirX << g.dirY << g.dirZ << g.distMly << br;
        }
        if (m_sdssCount > 0) {
            qInfo().noquote()
                << QString("[宇宙] SDSS 真实星系已载入 %1 个").arg(m_sdssCount);
        }
    }

    m_count = data.size() / kFloats;
    s_lastBuilt = m_count;
    s_lastBuiltSdss = m_sdssCount;

    // ---- 上传 ----
    if (!m_vao.isCreated())
        m_vao.create();
    if (!m_vbo.isCreated())
        m_vbo.create();

    m_vao.bind();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vbo.allocate(data.constData(), data.size() * int(sizeof(float)));

    // ★ 顶点布局变了: aDir(3) + aDistMly(1) + aBright(1)
    //   亮度从第 4 个 float 挪到第 5 个 —— 漏改这里会导致
    //   所有粒子亮度取到"距离"值, 表现为明暗完全错乱。
    const int stride = kFloats * int(sizeof(float));
    m_f->glEnableVertexAttribArray(0);       // 方向 vec3
    m_f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast<void *>(0));
    m_f->glEnableVertexAttribArray(1);       // 距离 float
    m_f->glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast<void *>(3 * sizeof(float)));
    m_f->glEnableVertexAttribArray(2);       // 亮度 float
    m_f->glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast<void *>(4 * sizeof(float)));
    m_vao.release();
    m_vbo.release();
}

// ---------------------------------------------------------------------------

void Cosmos::render(const QMatrix4x4 &viewProj, float pointScale)
{
    if (!m_ready || !m_f || !m_prog)
        return;

    m_f->glEnable(GL_BLEND);
    m_f->glBlendFunc(GL_SRC_ALPHA, GL_ONE);      // 加法混合: 星系是发光体
    m_f->glDisable(GL_DEPTH_TEST);
    m_f->glEnable(GL_PROGRAM_POINT_SIZE);

    m_prog->bind();
    m_prog->setUniformValue("uViewProj", viewProj);
    m_prog->setUniformValue("uPixelScale", pointScale);
    m_prog->setUniformValue("uSize", 1.15f);
    m_prog->setUniformValue("uColor", QVector3D(0.80f, 0.82f, 0.92f));
    m_prog->setUniformValue("uAlpha", 0.50f);
    // ★ 映射模式: 切换只需改这个 uniform —— 不重建顶点缓冲, 零成本
    m_prog->setUniformValue("uMapMode", int(m_mapMode));

    m_vao.bind();
    // ★ 性能开关: 分段控制可见数量。
    //
    //   顶点缓冲布局: [示意结构粒子] [SDSS 星系]  (SDSS 是最后追加的)
    //
    //   为什么要分段: SDSS 星系成团性强, 单位粒子的填充成本
    //   远高于均匀分布的示意粒子 (过度绘制)。分开控制能独立取舍。
    //
    //   计算提取到了 Cosmos::effectiveDrawn() —— 与 GUI 侧共用,
    //   避免"渲染真画了 N 个、界面显示 M 个"这类公式漂移。
    const int drawCount = effectiveDrawn(m_count, m_sdssCount,
                                         m_sdssVisible, m_visible);
    m_f->glDrawArrays(GL_POINTS, 0, qMax(0, drawCount));
    // 记录本帧真实绘制量, 供 GUI 线程显示 (见 cosmos.h 的 lastDrawn 说明)
    s_lastDrawn = qMax(0, drawCount);
    m_vao.release();

    m_prog->release();
    m_f->glDisable(GL_PROGRAM_POINT_SIZE);
    m_f->glDisable(GL_BLEND);
}
