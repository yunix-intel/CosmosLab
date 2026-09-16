// ============================================================================
//  galaxy.cpp —— 银河系粒子模型实现
//
//  粒子分布按天体物理结构分成五类:
//    核球 (bulge)  —— 球状分布的年老恒星, 偏黄红
//    棒   (bar)    —— 中央棒旋结构的"棒", 沿一条轴拉长的椭球
//    旋臂 (arms)   —— 4 条对数螺旋, 年轻恒星偏蓝, 夹杂粉红 HII 区
//    盘   (disk)   —— 旋臂之间的弥散星场, 指数盘剖面
//    晕   (halo)   —— 球状分布的稀薄年老恒星与球状星团
//
//  ★ 全部按真实比例: 银盘半径 100 场景单位对应 52850 ly, 薄盘厚仅 1.9 单位。
//    "银盘极薄"是银河系最反直觉的特征, 教学上如实呈现比好看重要。
// ============================================================================

#include "galaxy.h"
#include "galaxydata.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QtMath>
#include <cmath>
#include <random>

// ---------------------------------------------------------------------------
//  GLSL
// ---------------------------------------------------------------------------
namespace {

const char *kStarVert = R"(
#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in vec3  aColor;
layout(location = 2) in float aSize;
layout(location = 3) in float aBright;

uniform mat4  uViewProj;
uniform float uPixelScale;      // 视口高度 / (2·tan(fov/2)), 使点大小随距离正确缩放

out vec3  vColor;
out float vBright;

void main() {
    vec4 clip = uViewProj * vec4(aPos, 1.0);
    gl_Position = clip;
    // 透视缩放: 距离越远点越小, 但设下限避免远处完全消失
    float sz = aSize * uPixelScale / max(clip.w, 0.001);
    gl_PointSize = clamp(sz, 0.6, 14.0);
    vColor  = aColor;
    vBright = aBright;
}
)";

const char *kStarFrag = R"(
#version 330 core
in vec3  vColor;
in float vBright;
out vec4 FragColor;

void main() {
    // 圆形软边点精灵: 用 gl_PointCoord 算径向衰减。
    // 方形点在小尺寸下会看出马赛克, 高斯衰减则天然融合成星云的质感。
    vec2 d = gl_PointCoord - vec2(0.5);
    float r2 = dot(d, d) * 4.0;              // 0 = 中心, 1 = 边缘
    if (r2 > 1.0) discard;
    float fall = exp(-r2 * 3.0) - 0.05;      // 高斯, 边缘略减去避免硬边
    if (fall <= 0.0) discard;
    FragColor = vec4(vColor * vBright * fall, 1.0);
}
)";

// 太阳位置的标记改由 QML 叠加层绘制 (见 Main.qml 的 galaxySunMarker)。
// 原因: OpenGL core profile 里 glLineWidth 上限通常是 1px, 细线画在
//       12 万粒子的星场上完全看不见 (实测黄圈被亮臂彻底淹没)。
//       QML 那边可以画粗描边 + 文字标签, 教学效果也好得多。
//       这里只需把太阳的世界坐标暴露出去供投影。

} // namespace

// ---------------------------------------------------------------------------
//  构造 / 销毁
// ---------------------------------------------------------------------------

Galaxy::Galaxy() = default;

Galaxy::~Galaxy() = default;

void Galaxy::destroy()
{
    if (m_vbo.isCreated())
        m_vbo.destroy();
    if (m_vao.isCreated())
        m_vao.destroy();
    delete m_prog;
    m_prog = nullptr;
    m_ready = false;
}

// ---------------------------------------------------------------------------
//  初始化
// ---------------------------------------------------------------------------

void Galaxy::init(QOpenGLFunctions_3_3_Core *f)
{
    if (m_ready || !f)
        return;
    m_f = f;

    auto make = [](const char *vs, const char *fs, const char *name) {
        auto *p = new QOpenGLShaderProgram;
        const bool ok =
            p->addShaderFromSourceCode(QOpenGLShader::Vertex, vs)
            && p->addShaderFromSourceCode(QOpenGLShader::Fragment, fs)
            && p->link();
        if (!ok) {
            qWarning() << "[银河系] 着色器" << name << "失败:" << p->log();
            delete p;
            return static_cast<QOpenGLShaderProgram *>(nullptr);
        }
        return p;
    };

    m_prog = make(kStarVert, kStarFrag, "galaxy_star");

    build();

    m_ready = m_prog && m_vbo.isCreated() && m_count > 0;
    qInfo() << "[银河系] 初始化" << (m_ready ? "成功" : "失败")
            << " 粒子数:" << m_count;
}

// ---------------------------------------------------------------------------
//  粒子生成
//
//  一次生成十万级粒子。开销主要在 std::normal_distribution 的调用次数上,
//  实测整体在 100 ms 量级 —— 只在启动时发生一次, 可接受。
// ---------------------------------------------------------------------------

void Galaxy::buildStars(QVector<float> &out)
{
    std::mt19937 rng(20240915u);
    std::normal_distribution<float> gauss(0.0f, 1.0f);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);

    // 尺度换算: 光年 -> 场景单位
    const float ly2u = float(1.0 / gx::kLyPerUnit);

    const float rDisk   = float(gx::kSceneRadius);                 // 100
    const float rBulge  = float(gx::kBulgeRadiusLy  * ly2u);       // 9.46
    const float barHalf = float(gx::kBarHalfLenLy   * ly2u);       // 25.5
    const float diskTh  = float(gx::kThinDiskLy     * ly2u);       // 1.89
    const float rArm0   = float(gx::kArmStartLy     * ly2u);       // 47.3
    const float rArm1   = float(gx::kArmEndLy       * ly2u);       // 94.6

    const float pitch   = float(gx::kArmPitchDeg * M_PI / 180.0);
    const float bCoef   = std::tan(pitch);

    auto put = [&out](float x, float y, float z,
                      float r, float g, float bl, float size, float bright) {
        out << x << y << z << r << g << bl << size << bright;
    };

    // 亮度分布: 幂律 —— 绝大多数恒星暗, 极少数很亮。
    // 均匀亮度会让星系看起来像一片均匀的雾, 失去颗粒感。
    //
    // ★ 整体量级压得很低是刻意的: 加法混合下数十个粒子会在同一像素上
    //   累积, 若单粒子亮度接近 1.0, 旋臂立刻饱和成实心白带 (实测就是
    //   这样, 旋臂糊成一片白, 看不出是恒星集合)。压到 0.6 以下后,
    //   只有真正密集的银心与旋臂内缘才自然亮起来, 其余保持颗粒感。
    auto brightness = [&](float lo, float hi) {
        const float u = uni(rng);
        return lo + (hi - lo) * std::pow(u, 3.0f);
    };

    // ==== 1. 核球 (球状, 沿 Y 压扁) ====
    {
        const int n = 22000;
        for (int i = 0; i < n; ++i) {
            // 半径用 pow(u,1.7) 使中心高度密集 (实际核球是 r^(1/4) 律)
            const float rr = rBulge * std::pow(uni(rng), 1.7f);
            const float u  = uni(rng) * 2.0f - 1.0f;
            const float ph = uni(rng) * 2.0f * float(M_PI);
            const float s  = std::sqrt(qMax(0.0f, 1.0f - u * u));

            const float x = rr * s * std::cos(ph);
            const float z = rr * s * std::sin(ph);
            const float y = rr * u * 0.62f;        // 压扁成扁球

            // 年老恒星: 黄橙, 略偏红
            const float t = uni(rng);
            put(x, y, z,
                1.00f, 0.80f + 0.10f * t, 0.55f + 0.14f * t,
                0.7f + uni(rng) * 1.1f,
                brightness(0.16f, 0.62f));
        }
    }

    // ==== 2. 中央棒 ====
    // 棒旋星系的"棒"是拉长的椭球, 恒星沿一条轴排布。
    // 银河系棒长约 27000 ly (半长 13500 ly)。
    {
        const int n = 13000;
        for (int i = 0; i < n; ++i) {
            // |x| 用 U 形分布 —— 棒的两端比中段更密 (实际观测如此)
            const float u  = uni(rng);
            const float ax = barHalf * (0.15f + 0.85f * std::pow(u, 0.6f));
            const float x  = (uni(rng) < 0.5f ? -ax : ax);

            const float y = gauss(rng) * 1.6f;
            const float z = gauss(rng) * 3.4f;

            put(x, y, z,
                1.00f, 0.86f, 0.62f,
                0.7f + uni(rng) * 1.0f,
                brightness(0.15f, 0.55f));
        }
    }

    // ==== 3. 旋臂 (4 条对数螺旋) ====
    //
    // r = r0 · e^(b·θ), 反解得 θ = ln(r/r0)/b。
    // 沿 r 用对数分布采样 —— 保证每条臂在视觉上的粒子密度均匀
    // (若按 r 均匀采样, 内侧会挤成一团)。
    {
        const int perArm = 15000;
        for (int arm = 0; arm < gx::kArmCount; ++arm) {
            const float baseAng = float(arm) * (2.0f * float(M_PI) / gx::kArmCount);

            for (int i = 0; i < perArm; ++i) {
                // 对数插值采样半径 (前密后疏更贴近实际)
                const float t  = std::pow(uni(rng), 0.75f);
                const float rr = rArm0 * std::pow(rArm1 / rArm0, t);

                const float theta = std::log(rr / rArm0) / bCoef;
                const float ang   = baseAng + theta;

                // 旋臂有宽度, 且越靠外越松散 (真实旋臂的形态)。
                // ★ 实测宽度给到 7 单位时旋臂会连成实心白带 —— 真实旋臂
                //   是"恒星密集带"而非实心块, 必须留出臂间的暗区。
                const float width = 1.1f + 2.9f * (rr / rDisk);
                const float dRad  = gauss(rng) * width * 0.35f;
                const float dAng  = gauss(rng) * width / qMax(rr, 4.0f);

                const float r2 = qMax(0.5f, rr + dRad);
                const float a2 = ang + dAng;

                const float x = r2 * std::cos(a2);
                const float z = r2 * std::sin(a2);
                // 垂直方向: 薄盘, 向外略增厚但整体很薄
                const float y = gauss(rng) * diskTh * (0.55f + 0.5f * (rr / rDisk));

                // 旋臂以年轻蓝白星为主; 少数 HII 区呈粉红
                float cr = 0.70f, cg = 0.82f, cb = 1.00f;
                if (uni(rng) < 0.045f) {           // 约 4.5% 的 HII 区
                    cr = 1.00f; cg = 0.62f; cb = 0.72f;
                } else {
                    const float v = uni(rng) * 0.22f;
                    cr += v; cg += v * 0.8f; cb -= v * 0.35f;
                }

                put(x, y, z, cr, cg, cb,
                    0.6f + uni(rng) * 1.1f,
                    brightness(0.14f, 0.72f));
            }
        }
    }

    // ==== 4. 盘弥散星场 (旋臂之间) ====
    {
        const int n = 26000;
        // 指数标长用真实值 (1e4 ly ≈ 19 场景单位), 而不是随手取的经验值。
        // 标长过大 -> 盘中心反而显得空旷, 因为采样拒绝率在那里很低。
        const float hScale = float(gx::kDiskScaleLengthLy * ly2u);
        for (int i = 0; i < n; ++i) {
            // 指数盘: P(r) ∝ r·e^(-r/h)
            float rr;
            do {
                rr = uni(rng) * rDisk;
            } while (uni(rng) > (rr / hScale) * std::exp(1.0f - rr / hScale));
            if (rr < 1.0f)
                continue;

            const float a = uni(rng) * 2.0f * float(M_PI);
            const float x = rr * std::cos(a);
            const float z = rr * std::sin(a);
            const float y = gauss(rng) * diskTh * 0.9f;

            put(x, y, z,
                0.96f, 0.94f, 0.88f,
                0.55f + uni(rng) * 0.8f,
                brightness(0.07f, 0.34f));
        }
    }

    // ==== 5. 银晕 + 球状星团 ====
    //
    // 球状星团是银河系最有教学价值的特征之一: 它们在银晕里呈球状分布,
    // 且都是年老恒星 (偏红), 与银盘的年轻蓝星形成鲜明对比。
    {
        const int n = 4500;
        for (int i = 0; i < n; ++i) {
            // 晕半径远大于盘 —— 用 r^2 分布让外围不至于空荡
            const float rr = rDisk * (1.0f + 1.6f * std::pow(uni(rng), 2.0f));
            const float u  = uni(rng) * 2.0f - 1.0f;
            const float ph = uni(rng) * 2.0f * float(M_PI);
            const float s  = std::sqrt(qMax(0.0f, 1.0f - u * u));

            put(rr * s * std::cos(ph), rr * u, rr * s * std::sin(ph),
                1.00f, 0.68f, 0.48f,
                0.6f + uni(rng) * 0.9f,
                brightness(0.08f, 0.40f));
        }

        // 球状星团: 约 150 个小而密的团块
        const int nClusters = 130;
        for (int c = 0; c < nClusters; ++c) {
            const float rc = rDisk * (0.25f + 2.1f * std::pow(uni(rng), 1.5f));
            const float u  = uni(rng) * 2.0f - 1.0f;
            const float ph = uni(rng) * 2.0f * float(M_PI);
            const float s  = std::sqrt(qMax(0.0f, 1.0f - u * u));
            const float cx = rc * s * std::cos(ph);
            const float cy = rc * u;
            const float cz = rc * s * std::sin(ph);

            const float spread = 0.35f + uni(rng) * 0.5f;
            const int   m = 18 + int(uni(rng) * 22.0f);
            for (int k = 0; k < m; ++k) {
                put(cx + gauss(rng) * spread,
                    cy + gauss(rng) * spread,
                    cz + gauss(rng) * spread,
                    1.00f, 0.74f, 0.52f,
                    0.7f, brightness(0.3f, 0.9f));
            }
        }
    }

    m_count = out.size() / 8;
}

void Galaxy::build()
{
    QVector<float> data;
    data.reserve(150000 * 8);

    buildStars(data);

    if (!m_vao.isCreated())
        m_vao.create();
    if (!m_vbo.isCreated())
        m_vbo.create();

    m_vao.bind();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vbo.allocate(data.constData(), data.size() * int(sizeof(float)));

    m_f->glEnableVertexAttribArray(0);
    m_f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                               reinterpret_cast<void *>(0));
    m_f->glEnableVertexAttribArray(1);
    m_f->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                               reinterpret_cast<void *>(3 * sizeof(float)));
    m_f->glEnableVertexAttribArray(2);
    m_f->glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                               reinterpret_cast<void *>(6 * sizeof(float)));
    m_f->glEnableVertexAttribArray(3);
    m_f->glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                               reinterpret_cast<void *>(7 * sizeof(float)));

    m_vao.release();
    m_vbo.release();
}

// ---------------------------------------------------------------------------
//  渲染
// ---------------------------------------------------------------------------

QVector3D Galaxy::sunPosition()
{
    // 太阳距银心 26000 ly, 位于银道面附近
    const float d = float(gx::kSunDistFromCenterLy / gx::kLyPerUnit);
    const float h = float(gx::kSunHeightFromDiskLy / gx::kLyPerUnit);
    // 方位角取一条旋臂附近, 视觉上"落在臂上"更有说服力
    const float ang = 1.15f;
    return QVector3D(d * std::cos(ang), h, d * std::sin(ang));
}

void Galaxy::render(const QMatrix4x4 &viewProj, float pointScale)
{
    if (!m_ready || !m_f || !m_prog)
        return;

    // 加法混合: 密集处自然累积变亮 —— 银心的炽亮与旋臂的成条
    // 都是粒子密度的自然结果, 无需额外高光贴图。
    m_f->glEnable(GL_BLEND);
    m_f->glBlendFunc(GL_ONE, GL_ONE);
    m_f->glDisable(GL_DEPTH_TEST);
    // gl_PointSize 在 core profile 里必须显式启用
    m_f->glEnable(GL_PROGRAM_POINT_SIZE);

    m_prog->bind();
    m_prog->setUniformValue("uViewProj", viewProj);
    m_prog->setUniformValue("uPixelScale", pointScale);

    m_vao.bind();
    m_f->glDrawArrays(GL_POINTS, 0, m_count);
    m_vao.release();
    m_prog->release();

    // 太阳位置标注由 QML 叠加层绘制 —— 见 Main.qml 的 galaxySunMarker
    m_f->glDisable(GL_PROGRAM_POINT_SIZE);
    m_f->glDisable(GL_BLEND);
}
