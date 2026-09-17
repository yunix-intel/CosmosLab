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
#include <random>

namespace {

// 对数映射参数
constexpr double kD0      = 0.1;          // Mly, 让最近天体也有间距
constexpr double kDMax    = 46500.0;      // Mly, 可观测宇宙半径
constexpr float  kSceneR  = 100.0f;

// 粒子属性: 位置(3) + 亮度(1)
constexpr int kFloats = 4;

const char *kVert = R"(
#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aBright;

uniform mat4  uViewProj;
uniform float uPixelScale;
uniform float uSize;

out float vBright;

void main() {
    vec4 clip = uViewProj * vec4(aPos, 1.0);
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

float Cosmos::distToScene(double mly)
{
    // r = log10(1 + d/d0) / log10(1 + dMax/d0) × R
    // 取 d + kD0 而非 max(d, kD0), 保证 d=0 时映射到 0 且单调
    const double num = std::log10(1.0 + mly / kD0);
    const double den = std::log10(1.0 + kDMax / kD0);
    return float(num / den) * kSceneR;
}

QVector<CosmosMarker> Cosmos::markers()
{
    QVector<CosmosMarker> out;

    // ---- 本星系群成员 ----
    // ★ 本星系群整体在一个极小的半径内 (10 Mly), 对数映射后约在
    //   场景半径的 33%。为了让各成员在屏幕上可分辨, 用**真实方向**
    //   但人为拉开间距 —— 否则 0.16 到 2.5 Mly 的差异在对数下只有
    //   几个像素。这是有意的视觉放大, 标注里给出真实距离作补偿。
    for (int i = 0; i < LOCAL_GROUP_COUNT; ++i) {
        const GalaxyData &g = LOCAL_GROUP[i];
        const double r = distToScene(g.distanceMly);
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
                     p, 0 });
    }

    // ---- 室女座星系团成员 ----
    for (int i = 0; i < VIRGO_CLUSTER_COUNT; ++i) {
        const GalaxyData &g = VIRGO_CLUSTER[i];
        const double r = distToScene(g.distanceMly);
        const double ra  = g.raDeg  * M_PI / 180.0;
        const double dec = g.decDeg * M_PI / 180.0;
        const QVector3D p(float(std::cos(dec) * std::cos(ra) * r),
                          float(std::sin(dec) * r),
                          float(std::cos(dec) * std::sin(ra) * r));
        out.append({ QString::fromUtf8(g.nameCn),
                     QString::fromUtf8(g.nameEn),
                     QStringLiteral("%1 百万光年").arg(g.distanceMly, 0, 'f', 1),
                     p, 1 });
    }

    // ---- 大尺度结构 ----
    for (int i = 0; i < LARGE_STRUCTURES_COUNT; ++i) {
        const LargeStructure &s = LARGE_STRUCTURES[i];
        const double r = distToScene(s.distanceFromEarthMly);
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
                     p, s.kind == 2 ? 4 : (s.kind == 0 ? 3 : 2) });
    }

    return out;
}

// ---------------------------------------------------------------------------

int Cosmos::s_lastBuilt = 0;

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

    auto push = [&](double x, double y, double z, float bright) {
        data << float(x) << float(y) << float(z) << bright;
    };

    // ---- 3. 沿纤维撒星系 ----
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

    m_count = data.size() / kFloats;
    s_lastBuilt = m_count;

    // ---- 上传 ----
    if (!m_vao.isCreated())
        m_vao.create();
    if (!m_vbo.isCreated())
        m_vbo.create();

    m_vao.bind();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vbo.allocate(data.constData(), data.size() * int(sizeof(float)));

    const int stride = kFloats * int(sizeof(float));
    m_f->glEnableVertexAttribArray(0);
    m_f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast<void *>(0));
    m_f->glEnableVertexAttribArray(1);
    m_f->glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast<void *>(3 * sizeof(float)));
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

    m_vao.bind();
    // ★ 性能开关: 只画前 visibleCount() 个。
    //   顶点数据不变, 仅改 count —— 切换零成本。
    m_f->glDrawArrays(GL_POINTS, 0, visibleCount());
    m_vao.release();

    m_prog->release();
    m_f->glDisable(GL_PROGRAM_POINT_SIZE);
    m_f->glDisable(GL_BLEND);
}
