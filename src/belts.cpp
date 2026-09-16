// ============================================================================
//  belts.cpp —— 小行星带 / 柯伊伯带 / 特洛伊群实现
//
//  三组结构, 全部按真实分布生成:
//
//  1. 主小行星带 (Main belt)
//     范围 2.06 – 3.28 AU。这两条边界**不是随意取的**:
//       内边界 2.06 AU = 与木星的 4:1 共振
//       外边界 3.28 AU = 与木星的 2:1 共振
//     带内还有若干 Kirkwood 空隙, 由与木星的共振把该半径上的天体清空:
//       2.50 AU  (3:1)   2.82 AU (5:2)   2.96 AU (7:3)   3.28 AU (2:1)
//     Kirkwood 空隙是"轨道共振"最直观的展示, 也是本带最重要的教学点。
//
//  2. 柯伊伯带 (Kuiper belt)
//     30 – 50 AU。海王星的共振在带内刻出清晰结构:
//       冥王星所在的 3:2 共振 (39.4 AU) 与 2:1 共振 (47.8 AU) 是密度峰,
//       中间的 40–47 AU 是经典带 (动态最稳定, 天体最密集)。
//
//  3. 木星特洛伊群 (Jupiter Trojans)
//     锁定在木星的 L4 / L5 拉格朗日点 (黄经差 ±60°)。
//     它们与木星保持 1:1 共振, 是"共轨天体"的实例。
//
//  ★ 数量说明: 粒子数是**代表性采样**, 不对应真实天体数。UI 上显示的是
//    真实估算数量 (主带 >130 万颗), 二者不可混为一谈。
// ============================================================================

#include "belts.h"
#include "celestialdata.h"
#include "scene.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QtMath>
#include <cmath>
#include <random>

namespace {

// 每个粒子的属性布局: R, P(3), Q(3), phase, speed  = 9 floats
constexpr int kFloatsPerParticle = 9;

const char *kVert = R"(
#version 330 core
layout(location = 0) in float aRadius;   // 压缩后的轨道半径 (场景单位)
layout(location = 1) in vec3  aP;        // 轨道平面基向量 P
layout(location = 2) in vec3  aQ;        // 轨道平面基向量 Q
layout(location = 3) in float aPhase;    // 初始相位 (弧度)
layout(location = 4) in float aSpeed;    // 角速度 (弧度/天)

uniform mat4  uViewProj;
uniform float uDays;        // 自 J2000 起的天数
uniform float uPixelScale;  // 视口高度 / (2·tan(fov/2))
uniform float uSize;

out float vFade;

void main() {
    // ★ 相位推进放在着色器里 —— 每帧只需更新 uDays 一个 uniform。
    //   若改成 CPU 算 7 万个位置再上传, 光传输就 ~840 KB/帧, 纯浪费。
    //
    //   用 fract 把相位限制在 [0,1) 再乘 2π: 直接累加 theta 会让数值
    //   随时间无限增大, 几天后 float 精度就不够分辨相邻粒子了。
    float turns = fract(aPhase + aSpeed * uDays / 6.28318530718);
    float th = turns * 6.28318530718;

    vec3 local = aRadius * (cos(th) * aP + sin(th) * aQ);

    vec4 clip = uViewProj * vec4(local, 1.0);
    gl_Position = clip;
    float sz = uSize * uPixelScale / max(clip.w, 0.001);
    gl_PointSize = clamp(sz, 0.8, 6.0);

    // 远处粒子淡出, 避免外带变成一片噪点
    vFade = clamp(1.0 - clip.w / 2600.0, 0.18, 1.0);
}
)";

const char *kFrag = R"(
#version 330 core
in float vFade;
uniform vec3  uColor;
uniform float uAlpha;
out vec4 FragColor;

void main() {
    // 圆形软边: 方形点在小尺寸下会看出马赛克
    vec2 d = gl_PointCoord - vec2(0.5);
    float r2 = dot(d, d) * 4.0;
    if (r2 > 1.0) discard;
    float a = (1.0 - r2) * uAlpha * vFade;
    FragColor = vec4(uColor * a, a);
}
)";

// 轨道根数 (J2000 历元, JPL SBDB)
// 主带内/外边界与 Kirkwood 空隙
constexpr double kMainInner = 2.06;
constexpr double kMainOuter = 3.28;
// Kirkwood 空隙: 半宽 (AU) 与共振比
struct Kirkwood { double a; double halfWidth; const char *ratio; };
const Kirkwood KIRKWOOD[] = {
    { 2.502, 0.016, "3:1" },
    { 2.825, 0.014, "5:2" },
    { 2.958, 0.011, "7:3" },
    { 3.279, 0.020, "2:1" },
};
constexpr int KIRKWOOD_COUNT = int(sizeof(KIRKWOOD) / sizeof(KIRKWOOD[0]));

// 柯伊伯带
constexpr double kKuiperInner = 30.0;
constexpr double kKuiperOuter = 50.0;
// 海王星共振处是密度峰
constexpr double kResonance32 = 39.4;    // 3:2 (冥王星)
constexpr double kResonance21 = 47.8;    // 2:1

// 木星轨道 (特洛伊群用)
constexpr double kJupiterAu = 5.2026;

inline bool isBadWord(const char *) { return false; }

// 主带密度函数: 1.0 = 无空隙, 0.0 = 完全清空
double mainBeltDensity(double a)
{
    double d = 1.0;
    for (int i = 0; i < KIRKWOOD_COUNT; ++i) {
        const double dx = (a - KIRKWOOD[i].a) / KIRKWOOD[i].halfWidth;
        d -= 0.93 * std::exp(-dx * dx);   // 高斯凹陷
    }
    // 带的两端也平滑收边
    const double edgeIn  = (a - kMainInner) / 0.10;
    const double edgeOut = (kMainOuter - a) / 0.08;
    d *= (1.0 / (1.0 + std::exp(-edgeIn))) * (1.0 / (1.0 + std::exp(-edgeOut)));
    return qMax(0.0, qMin(1.0, d));
}

// 柯伊伯带密度: 共振峰 + 稀疏的散射带
double kuiperDensity(double a)
{
    double d = 0.25;                     // 散射盘基线
    // 经典带 (动态稳定区, 天体最密)
    if (a > 42.0 && a < 47.0)
        d += 0.75;
    // 两个共振峰
    const double d32 = (a - kResonance32) / 0.9;
    const double d21 = (a - kResonance21) / 0.9;
    d += 0.9 * std::exp(-d32 * d32);
    d += 0.6 * std::exp(-d21 * d21);
    // 30–39 AU 之间是共振交错区, 密度低
    if (a < 39.0)
        d *= 0.45;
    return qMin(1.0, d);
}

} // namespace

// ---------------------------------------------------------------------------

Belts::Belts() = default;
Belts::~Belts() = default;

void Belts::destroy()
{
    if (m_vbo.isCreated())
        m_vbo.destroy();
    if (m_vao.isCreated())
        m_vao.destroy();
    delete m_prog;
    m_prog = nullptr;
    m_ready = false;
}

void Belts::init(QOpenGLFunctions_3_3_Core *f, bool realScale)
{
    if (m_ready || !f)
        return;
    m_f = f;

    auto *p = new QOpenGLShaderProgram;
    const bool ok = p->addShaderFromSourceCode(QOpenGLShader::Vertex, kVert)
                 && p->addShaderFromSourceCode(QOpenGLShader::Fragment, kFrag)
                 && p->link();
    if (!ok) {
        qWarning() << "[小行星带] 着色器失败:" << p->log();
        delete p;
        return;
    }
    m_prog = p;

    rebuild(realScale);
    m_ready = m_prog && m_vbo.isCreated() && m_count > 0;

    qInfo() << "[小行星带] 初始化" << (m_ready ? "成功" : "失败")
            << " 主带:" << m_mainCount
            << " 柯伊伯带:" << m_kuiperCount
            << " 特洛伊:" << m_trojanCount
            << " 合计:" << m_count;
}

// ---------------------------------------------------------------------------
//  生成
// ---------------------------------------------------------------------------

void Belts::rebuild(bool realScale)
{
    QVector<float> data;
    // 主带 42000 + 柯伊伯 26000 + 特洛伊 5000
    data.reserve(73000 * kFloatsPerParticle);

    std::mt19937 rng(20260916u);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    std::normal_distribution<double> gauss(0.0, 1.0);

    // ★ 半径变换必须与行星轨道完全一致 (sceneconst::orbitToScene),
    //   否则小行星带会和火星/木星轨道错位。
    //   对圆轨道而言该变换是径向幂次的, 压缩后仍是同心圆, 故只需
    //   把半径单独换算一次。
    auto radiusToScene = [realScale](double aAu) {
        const double km = aAu * AU_KM;
        QVector3D v = sceneconst::orbitToScene(
            QVector3D(float(km), 0.0f, 0.0f), realScale);
        return double(v.x());
    };

    // 生成一个粒子的 9 个 float
    auto pushParticle = [&](double aAu, double incDeg, double nodeDeg,
                    double phase, float r, float g, float b) {
        (void)r; (void)g; (void)b;
        const double R = radiusToScene(aAu);

        // 轨道平面基向量: 由倾角 i 与升交点 Ω 确定
        //   标准做法 —— 先绕 x 轴倾斜, 再绕 y 轴旋进
        const double i = incDeg * M_PI / 180.0;
        const double om = nodeDeg * M_PI / 180.0;
        const double ci = std::cos(i), si = std::sin(i);
        const double co = std::cos(om), so = std::sin(om);

        // P = (Ω 方向的单位向量), Q = 平面内与之垂直的方向
        const double Px =  co, Py = 0.0, Pz = -so;
        const double Qx =  so * ci, Qy = si, Qz = co * ci;

        // 角速度由开普勒第三定律给出: n = 360°/P, P = 365.25·a^1.5 天
        // (这里用弧度/天)
        const double periodDays = 365.25 * std::pow(aAu, 1.5);
        const double speed = 2.0 * M_PI / qMax(periodDays, 1e-6);

        data << float(R)
             << float(Px) << float(Py) << float(Pz)
             << float(Qx) << float(Qy) << float(Qz)
             << float(phase) << float(speed);
    };

    // ==== 1. 主小行星带 ====
    // 目标 42000 粒子。用拒绝采样按真实密度分布 (含 Kirkwood 空隙)。
    {
        const int target = 42000;
        int made = 0, tries = 0;
        const int maxTries = target * 60;
        // 主带倾角分布: 多数在 10° 以内, 长尾到 ~20° (实测分布)
        while (made < target && tries < maxTries) {
            ++tries;
            const double a = kMainInner + uni(rng) * (kMainOuter - kMainInner);
            if (uni(rng) > mainBeltDensity(a))
                continue;

            // 倾角: 指数型衰减
            const double inc = qMin(22.0, std::fabs(gauss(rng)) * 7.5);
            const double node = uni(rng) * 360.0;
            const double phase = uni(rng) * 2.0 * M_PI;

            pushParticle(a, inc, node, phase, 0.62f, 0.60f, 0.56f);
            ++made;
        }
        m_mainCount = made;
    }

    // ==== 2. 柯伊伯带 ====
    {
        const int target = 26000;
        int made = 0, tries = 0;
        while (made < target && tries < target * 60) {
            ++tries;
            const double a = kKuiperInner + uni(rng) * (kKuiperOuter - kKuiperInner);
            if (uni(rng) > kuiperDensity(a))
                continue;

            // 柯伊伯带天体倾角普遍较大 (10–30°), 这是它与主带的显著差别
            const double inc = qMin(35.0, std::fabs(gauss(rng)) * 11.0);
            const double node = uni(rng) * 360.0;
            const double phase = uni(rng) * 2.0 * M_PI;

            // 柯伊伯带天体表面多为富有机质的红色, 比主带偏红
            pushParticle(a, inc, node, phase, 0.66f, 0.52f, 0.44f);
            ++made;
        }
        m_kuiperCount = made;
    }

    // ==== 3. 木星特洛伊群 ====
    //
    // 锁定在木星的 L4 (前导 +60°) 与 L5 (后随 −60°)。
    // 拉格朗日点并非一个点 —— 实际天体在 L4/L5 附近 ±30° 的黄经范围
    // 内摆动 (蝌蚪形轨道), 故用较宽的角分布。
    {
        const int target = 5000;
        const double R = radiusToScene(kJupiterAu);
        const double periodDays = 365.25 * std::pow(kJupiterAu, 1.5);
        const double speed = 2.0 * M_PI / periodDays;

        for (int k = 0; k < target; ++k) {
            // 木星此刻的黄经作为基准; 用相位表示
            const bool leading = (k % 2 == 0);
            const double offset = (leading ? 1.0 : -1.0) * (60.0 * M_PI / 180.0);
            // 围绕 L4/L5 的散布 (±25°)
            const double spread = gauss(rng) * (13.0 * M_PI / 180.0);

            // 木星在 J2000 的平黄经 (JPL: L = 34.39644051)
            const double jupL0 = 34.39644051 * M_PI / 180.0;
            const double phase = jupL0 + offset + spread;

            const double inc = std::fabs(gauss(rng)) * 9.0;
            const double node = uni(rng) * 360.0;

            pushParticle(kJupiterAu + gauss(rng) * 0.28, inc, node, phase,
                 0.58f, 0.50f, 0.44f);
        }
        m_trojanCount = target;
    }

    m_count = data.size() / kFloatsPerParticle;

    // ---- 上传 ----
    if (!m_vao.isCreated())
        m_vao.create();
    if (!m_vbo.isCreated())
        m_vbo.create();

    m_vao.bind();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vbo.allocate(data.constData(), data.size() * int(sizeof(float)));

    const int stride = kFloatsPerParticle * int(sizeof(float));
    m_f->glEnableVertexAttribArray(0);
    m_f->glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast<void *>(0));
    m_f->glEnableVertexAttribArray(1);
    m_f->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast<void *>(1 * sizeof(float)));
    m_f->glEnableVertexAttribArray(2);
    m_f->glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast<void *>(4 * sizeof(float)));
    m_f->glEnableVertexAttribArray(3);
    m_f->glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast<void *>(7 * sizeof(float)));
    m_f->glEnableVertexAttribArray(4);
    m_f->glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast<void *>(8 * sizeof(float)));

    m_vao.release();
    m_vbo.release();
}

// ---------------------------------------------------------------------------
//  渲染
// ---------------------------------------------------------------------------

void Belts::render(const QMatrix4x4 &viewProj, double days, float pointScale)
{
    if (!m_ready || !m_f || !m_prog)
        return;

    m_f->glEnable(GL_BLEND);
    // 普通 alpha 混合 —— 小行星是暗弱天体, 用加法混合会糊成一片亮雾
    m_f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_f->glDisable(GL_DEPTH_TEST);
    m_f->glEnable(GL_PROGRAM_POINT_SIZE);

    m_prog->bind();
    m_prog->setUniformValue("uViewProj", viewProj);
    m_prog->setUniformValue("uDays", float(days));
    m_prog->setUniformValue("uPixelScale", pointScale);
    // ★ 粒子尺寸必须很小。
    //   初版用 1.5px, 而主带 42000 个粒子挤在 2.06–3.28 AU 的环里,
    //   屏幕上叠成一片实心白盘 —— 完全看不出"这是一颗颗小行星",
    //   反而像行星环。真实的小行星带极其空旷 (成员平均间距上百万公里),
    //   视觉上就该是疏朗的点。
    //   降到 0.9px 并降低不透明度后, 才呈现出应有的颗粒感。
    m_prog->setUniformValue("uSize", 0.9f);
    m_prog->setUniformValue("uColor", QVector3D(0.78f, 0.74f, 0.68f));
    m_prog->setUniformValue("uAlpha", 0.42f);

    m_vao.bind();
    m_f->glDrawArrays(GL_POINTS, 0, m_count);
    m_vao.release();

    m_prog->release();
    m_f->glDisable(GL_PROGRAM_POINT_SIZE);
    m_f->glDisable(GL_BLEND);
}
