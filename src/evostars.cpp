// ============================================================================
//  evostars.cpp —— 演化视图 3D 粒子层实现 (112 条目分类星团)
//
//  布局 (教学核心, 非真实天球 —— 三表无坐标字段, 见头文件说明):
//    恒星 53: 赫罗图立体星团 (X=logTeff, Y=logL, Z=薄抖动), 中心 (-55,0,0)
//    AGN 34:  距离对数壳层 (半径 8..28 按 log(dist)), 中心 (55,0,0)
//    ISM 25:  类型分组圆盘 (发射/反射/暗/遗迹/星团), 中心 (0,-45,20)
//  颜色: 恒星按 teff 黑体分段; AGN 按类型 (Seyfert/类星体/耀变体/射电/团);
//        ISM 按发射红/反射蓝/暗黑/遗迹橙/星团青。
// ============================================================================

#include "evostars.h"
#include "stellardata.h"
#include "agndata.h"
#include "ismdata.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QtMath>
#include <cstring>
#include <cmath>

namespace {

const char *kEvoVert = R"(
#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in vec3  aColor;
layout(location = 2) in float aSize;
layout(location = 3) in float aBright;
uniform mat4  uViewProj;
uniform float uPixelScale;
out vec3  vColor;
out float vBright;
void main() {
    vec4 clip = uViewProj * vec4(aPos, 1.0);
    gl_Position = clip;
    float sz = aSize * uPixelScale / max(clip.w, 0.001);
    gl_PointSize = clamp(sz, 1.0, 22.0);
    vColor  = aColor;
    vBright = aBright;
}
)";

const char *kEvoFrag = R"(
#version 330 core
in vec3  vColor;
in float vBright;
out vec4 FragColor;
void main() {
    vec2 d = gl_PointCoord - vec2(0.5);
    float r2 = dot(d, d) * 4.0;
    if (r2 > 1.0) discard;
    float fall = exp(-r2 * 3.0) - 0.05;
    if (fall <= 0.0) discard;
    FragColor = vec4(vColor * vBright * fall, 1.0);
}
)";

// 黄金角螺旋点 (确定性, 无随机 —— 构建可复现)
static float fibA(int i) { return float(std::fmod(i * 2.39996, 6.28318530718)); }
static float fibR(int i, int n) { return std::sqrt((i + 0.5f) / n); }

} // namespace

EvoStars::EvoStars() = default;
EvoStars::~EvoStars() = default;

void EvoStars::destroy()
{
    if (m_vbo.isCreated())
        m_vbo.destroy();
    if (m_vao.isCreated())
        m_vao.destroy();
    delete m_prog;
    m_prog = nullptr;
    m_ready = false;
}

void EvoStars::init(QOpenGLFunctions_3_3_Core *f)
{
    m_f = f;
}

QVector3D EvoStars::starColor(double teff)
{
    // 与 QML starCol() 同款分段 (线性空间近似, sRGB^2.2)
    auto srgb = [](float r, float g, float b) {
        return QVector3D(std::pow(r, 2.2f), std::pow(g, 2.2f), std::pow(b, 2.2f));
    };
    if (teff <= 0)    return QVector3D(0.2f, 0.2f, 0.2f);
    if (teff < 3500)  return srgb(1.00f, 0.60f, 0.36f);
    if (teff < 5000)  return srgb(1.00f, 0.81f, 0.53f);
    if (teff < 6000)  return srgb(1.00f, 0.93f, 0.75f);
    if (teff < 7500)  return srgb(0.98f, 0.97f, 0.91f);
    if (teff < 10000) return srgb(0.79f, 0.85f, 1.00f);
    if (teff < 30000) return srgb(0.68f, 0.78f, 1.00f);
    return srgb(0.62f, 0.71f, 1.00f);
}

QVector3D EvoStars::starPos(int table, int index)
{
    if (table == 0 && index >= 0 && index < STELLAR_COUNT) {
        const StellarEntry &e = STELLAR_ENTRIES[index];
        const QVector3D c = stellarCenter();
        const double lte = e.teff > 0 ? std::log10(e.teff) : 3.5;
        double logL = 0.0;
        if (e.massSol > 0)
            logL = 3.5 * std::log10(e.massSol);
        if (e.massSol <= 0 && e.teff > 20000)
            logL = -2.5;
        const float x = c.x() + float((4.7 - lte) / (4.7 - 3.3) * 40.0 - 20.0);
        const float y = c.y() + float((logL + 4.5) / 11.0 * 30.0 - 15.0);
        const float a = fibA(index), rr = fibR(index, STELLAR_COUNT) * 3.0f;
        return QVector3D(x, y, c.z() + std::cos(a) * rr);
    }
    if (table == 1 && index >= 0 && index < AGN_COUNT) {
        const AgnEntry &e = AGN_ENTRIES[index];
        const QVector3D c = agnCenter();
        const double d = e.distMly > 0 ? e.distMly : 100.0;
        const float rad = 8.0f + float(std::log10(d + 1.0) / std::log10(30001.0) * 20.0);
        const float a = fibA(index * 2 + 1);
        const float b = float(std::acos(1.0 - 2.0 * fibR(index, AGN_COUNT)));
        return QVector3D(c.x() + rad * std::sin(b) * std::cos(a),
                         c.y() + rad * std::cos(b) * 0.7f,
                         c.z() + rad * std::sin(b) * std::sin(a));
    }
    if (table == 2 && index >= 0 && index < ISM_COUNT) {
        const IsmEntry &e = ISM_ENTRIES[index];
        const QVector3D c = ismCenter();
        const QString cat = QString::fromUtf8(e.catCn);
        int grp = 2;
        if (cat.contains(QStringLiteral("发射")) || cat.contains(QStringLiteral("超新星"))
            || cat.contains(QStringLiteral("遗迹")) || cat.contains(QStringLiteral("行星状")))
            grp = 0;
        else if (cat.contains(QStringLiteral("反射")) || cat.contains(QStringLiteral("疏散"))
                 || cat.contains(QStringLiteral("球状")) || cat.contains(QStringLiteral("星协")))
            grp = 1;
        const float a = fibA(index * 3 + grp);
        const float rr = 6.0f + grp * 9.0f + fibR(index, ISM_COUNT) * 5.0f;
        return QVector3D(c.x() + std::cos(a) * rr,
                         c.y() + float((index % 5) - 2) * 1.2f,
                         c.z() + std::sin(a) * rr * 0.8f);
    }
    return QVector3D();
}

void EvoStars::build()
{
    if (m_ready || !m_f)
        return;

    struct Pt { float x, y, z, r, g, b, size, bright; };
    QVector<Pt> pts;
    pts.reserve(120);

    // ---- 恒星 53: 赫罗图立体星团 ----
    {
        const QVector3D c = stellarCenter();
        for (int i = 0; i < STELLAR_COUNT; ++i) {
            const StellarEntry &e = STELLAR_ENTRIES[i];
            // X = logTeff (热左冷右, 跨度 40), Y = logL 需从 mass 估
            // (无 logL 字段: 主序 L~M^3.5, 非主序按 teff 档给示意值)
            const double lte = e.teff > 0 ? std::log10(e.teff) : 3.5;
            double logL = 0.0;
            if (e.massSol > 0)
                logL = 3.5 * std::log10(e.massSol);
            // 白矮星/脉冲星 (massSol<=0 或 teff 极高低光度) 压到下方
            if (e.massSol <= 0 && e.teff > 20000)
                logL = -2.5;
            const float x = c.x() + float((4.7 - lte) / (4.7 - 3.3) * 40.0 - 20.0);
            const float y = c.y() + float((logL + 4.5) / 11.0 * 30.0 - 15.0);
            const float a = fibA(i), rr = fibR(i, STELLAR_COUNT) * 3.0f;
            const QVector3D col = starColor(e.teff);
            // 大小按光度: 巨星/超巨星大, 白矮星小
            float size = 2.2f;
            if (logL > 3.0) size = 4.5f;
            else if (logL > 1.0) size = 3.2f;
            else if (logL < -1.0) size = 1.4f;
            pts.append({x, y, c.z() + std::cos(a) * rr,
                        col.x(), col.y(), col.z(), size, 1.2f});
            Q_UNUSED(rr);
        }
    }

    // ---- AGN 34: 距离对数壳层 ----
    {
        const QVector3D c = agnCenter();
        for (int i = 0; i < AGN_COUNT; ++i) {
            const AgnEntry &e = AGN_ENTRIES[i];
            const double d = e.distMly > 0 ? e.distMly : 100.0;
            const float rad = 8.0f + float(std::log10(d + 1.0) / std::log10(30001.0) * 20.0);
            const float a = fibA(i * 2 + 1);
            const float b = float(std::acos(1.0 - 2.0 * fibR(i, AGN_COUNT)));
            const float x = c.x() + rad * std::sin(b) * std::cos(a);
            const float y = c.y() + rad * std::cos(b) * 0.7f;
            const float z = c.z() + rad * std::sin(b) * std::sin(a);
            // 颜色按类型
            QVector3D col(0.7f, 0.8f, 1.0f);
            const QString cat = QString::fromUtf8(e.catCn);
            if (cat.contains(QStringLiteral("类星体")))
                col = QVector3D(1.0f, 0.9f, 0.6f);
            else if (cat.contains(QStringLiteral("耀变体")))
                col = QVector3D(0.6f, 0.9f, 1.0f);
            else if (cat.contains(QStringLiteral("射电")))
                col = QVector3D(1.0f, 0.6f, 0.4f);
            else if (cat.contains(QStringLiteral("团")) || cat.contains(QStringLiteral("超团")))
                col = QVector3D(0.8f, 0.6f, 1.0f);
            float size = 2.4f;
            if (e.massLog10 >= 9.0) size = 3.6f;
            pts.append({x, y, z, col.x(), col.y(), col.z(), size, 1.1f});
        }
    }

    // ---- ISM 25: 类型分组圆盘 ----
    {
        const QVector3D c = ismCenter();
        for (int i = 0; i < ISM_COUNT; ++i) {
            const IsmEntry &e = ISM_ENTRIES[i];
            const QString cat = QString::fromUtf8(e.catCn);
            int grp = 2; // 暗/其他
            QVector3D col(0.3f, 0.3f, 0.35f);
            if (cat.contains(QStringLiteral("发射")) || cat.contains(QStringLiteral("超新星"))
                || cat.contains(QStringLiteral("遗迹"))) {
                grp = 0; col = QVector3D(1.0f, 0.45f, 0.45f);
            } else if (cat.contains(QStringLiteral("反射")) || cat.contains(QStringLiteral("疏散"))
                       || cat.contains(QStringLiteral("球状")) || cat.contains(QStringLiteral("星协"))) {
                grp = 1; col = QVector3D(0.55f, 0.7f, 1.0f);
            } else if (cat.contains(QStringLiteral("行星状"))) {
                grp = 0; col = QVector3D(0.4f, 1.0f, 0.7f);
            }
            const float a = fibA(i * 3 + grp);
            const float rr = 6.0f + grp * 9.0f + fibR(i, ISM_COUNT) * 5.0f;
            const float x = c.x() + std::cos(a) * rr;
            const float z = c.z() + std::sin(a) * rr * 0.8f;
            const float y = c.y() + float((i % 5) - 2) * 1.2f;
            float size = 3.0f;
            if (e.sizeLy > 20.0) size = 4.2f;
            pts.append({x, y, z, col.x(), col.y(), col.z(), size, 1.0f});
        }
    }

    m_count = pts.size();

    m_prog = new QOpenGLShaderProgram();
    m_prog->addShaderFromSourceCode(QOpenGLShader::Vertex, kEvoVert);
    m_prog->addShaderFromSourceCode(QOpenGLShader::Fragment, kEvoFrag);
    if (!m_prog->link()) {
        qWarning() << "[演化星团] 着色器链接失败:" << m_prog->log();
        delete m_prog;
        m_prog = nullptr;
        return;
    }

    m_vao.create();
    m_vao.bind();
    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(pts.constData(), int(pts.size() * sizeof(Pt)));
    m_f->glEnableVertexAttribArray(0);
    m_f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Pt), nullptr);
    m_f->glEnableVertexAttribArray(1);
    m_f->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Pt),
                               reinterpret_cast<void *>(3 * sizeof(float)));
    m_f->glEnableVertexAttribArray(2);
    m_f->glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Pt),
                               reinterpret_cast<void *>(6 * sizeof(float)));
    m_f->glEnableVertexAttribArray(3);
    m_f->glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Pt),
                               reinterpret_cast<void *>(7 * sizeof(float)));
    m_vbo.release();
    m_vao.release();

    m_ready = true;
    qInfo() << "[演化星团] 构建完成, 点数:" << m_count
            << "(恒星" << STELLAR_COUNT << "/AGN" << AGN_COUNT
            << "/ISM" << ISM_COUNT << ")";
}

void EvoStars::render(const QMatrix4x4 &viewProj, float pointScale)
{
    if (!m_ready || !m_prog)
        return;
    m_prog->bind();
    m_prog->setUniformValue("uViewProj", viewProj);
    m_prog->setUniformValue("uPixelScale", pointScale);
    m_vao.bind();
    m_f->glDrawArrays(GL_POINTS, 0, m_count);
    m_vao.release();
    m_prog->release();
}
