// ============================================================================
//  evostars.cpp —— 演化视图 3D 粒子层实现 (112 条目分类星团)
//
//  v1.4 视觉特征化改造 (2026-10-09):
//    ★ 形状点精灵: 6 类形状 —— 圆光点/星芒/环/双瓣/喷流/团簇, 按天体类型分配,
//      让"这是什么"一眼可辨 (不再是清一色小圆点)。
//    ★ 立体感: 深度雾 (远暗近亮) + 呼吸闪烁 (缓慢脉动) + Z 加厚 + 透视放大。
//    ★ 深度遮罩: 粒子不写深度 (glDepthMask false) —— 修复"部分点被隐藏"。
//    ★ 加法混合自管: 不依赖外部 GL 状态 (之前依赖 drawEvoSim 残留状态)。
//
//  布局 (教学核心, 非真实天球 —— 三表无坐标字段, 见头文件说明):
//    恒星 53: 赫罗图立体星团 (X=logTeff, Y=logL, Z=薄抖动), 中心 (-55,0,0)
//    AGN 34:  距离对数壳层 (半径 8..28 按 log(dist)), 中心 (55,0,0)
//    ISM 25:  类型分组圆盘 (发射/反射/暗/遗迹/星团), 中心 (0,-45,20)
//  颜色: 恒星按 teff 黑体分段; AGN 按类型 (Seyfert/类星体/耀变体/射电/团);
//        ISM 按发射红/反射蓝/暗黑/遗迹橙/星团青。
//
//  形状分配 (shape):
//    0 圆光点 —— 主序星 / Seyfert / 发射星云 / 白矮星(暗)
//    1 星芒   —— 超巨星/巨星 / 类星体
//    2 环     —— 行星状 / 遗迹 / 反射星云 / 暗星云(剪影环)
//    3 双瓣   —— 射电星系
//    4 喷流   —— 耀变体
//    5 团簇   —— 星团(疏散/球状/星协) / 星系团
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
layout(location = 4) in float aShape;
uniform mat4  uViewProj;
uniform float uPixelScale;
uniform vec3  uEye;
uniform float uTime;
uniform float uEvoT;     // ★ v1.7 演化演示时间轴 0..1
out vec3  vColor;
out float vBright;
out float vShape;
void main() {
    // ★ v1.7 演示: 星点从"大爆炸奇点"(场景中心) 扩散到各自分类位置,
    //   前 60% 时间轴完成; uEvoT=1 时与静态星图完全一致。
    float spread = smoothstep(0.03, 0.60, uEvoT);
    vec3 center = vec3(0.0, -8.0, 8.0);
    vec3 pos = mix(center, aPos, spread);
    vec4 clip = uViewProj * vec4(pos, 1.0);
    gl_Position = clip;
    float sz = aSize * uPixelScale / max(clip.w, 0.001);
    // ★ 上限放 72: 近处点大 (透视层次); 下限 2: 远处点不消失。
    gl_PointSize = clamp(sz, 2.0, 72.0);
    // ★ 深度雾: 远的暗、近的亮 —— 立体层次的主要来源。
    float dist = length(pos - uEye);
    float fog = clamp(220.0 / max(dist, 1.0), 0.40, 1.25);
    // ★ 呼吸闪烁: 每点缓慢脉动 (相位按位置哈希散开, 非同步)。
    float hsh = fract(sin(dot(aPos, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
    float tw = 0.80 + 0.20 * sin(uTime * 1.6 + hsh * 6.2832);
    // ★ v1.7 依次点亮: 星系/黑洞先形成 (0.44 起), 恒星次之 (0.55 起),
    //   星云最后 (0.68 起) —— 讲"结构逐级出现"的宇宙史。
    float birth;
    if (aPos.x > 20.0)       birth = 0.44 + 0.18 * hsh;
    else if (aPos.x < -20.0) birth = 0.55 + 0.23 * hsh;
    else                     birth = 0.68 + 0.22 * hsh;
    float lit = smoothstep(birth, birth + 0.06, uEvoT);
    // ★ v1.7 未来 (t>0.92): 整体缓慢变暗 (加速膨胀时代的意象)。
    float fut = 1.0 - 0.55 * smoothstep(0.92, 1.0, uEvoT);
    vColor  = aColor;
    vBright = aBright * fog * tw * lit * fut;
    vShape  = aShape;
    // 未点亮的点移出裁剪空间 (不可见, 不浪费片元)。
    if (lit < 0.01)
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
}
)";

const char *kEvoFrag = R"(
#version 330 core
in vec3  vColor;
in float vBright;
in float vShape;
out vec4 FragColor;
void main() {
    vec2 d = gl_PointCoord - vec2(0.5);
    float r = length(d) * 2.0;
    float a = 0.0;
    int sh = int(vShape + 0.5);
    if (sh == 0) {
        // 圆光点 (柔光斑)
        a = exp(-r * r * 3.2) - 0.03;
    } else if (sh == 1) {
        // 星芒: 亮核 + 十字光芒 (亮星/类星体)
        float core = exp(-r * r * 5.5);
        float ray = (exp(-abs(d.x) * 22.0) + exp(-abs(d.y) * 22.0)) * exp(-r * 2.4);
        a = core + ray * 0.85;
    } else if (sh == 2) {
        // 环: 行星状/遗迹/反射星云/暗星云剪影
        float ring = exp(-pow((r - 0.60) * 5.0, 2.0));
        float core = exp(-r * r * 9.0) * 0.42;
        a = ring + core;
    } else if (sh == 3) {
        // 双瓣: 射电星系 (左右两瓣 + 弱核)
        float l  = exp(-pow(length(d - vec2(-0.30, 0.0)) * 3.4, 2.0));
        float rr = exp(-pow(length(d - vec2( 0.30, 0.0)) * 3.4, 2.0));
        float core = exp(-r * r * 10.0) * 0.5;
        a = (l + rr) * 0.9 + core;
    } else if (sh == 4) {
        // 喷流: 竖直双条 (耀变体)
        float jets = exp(-abs(d.x) * 28.0) * exp(-pow(d.y * 1.9, 2.0));
        float core = exp(-r * r * 8.0);
        a = jets * 0.95 + core;
    } else if (sh == 5) {
        // 团簇: 中心 + 四卫星点 (星团/星系团)
        float c  = exp(-r * r * 9.0);
        float s1 = exp(-pow(length(d - vec2( 0.31, 0.18)) * 7.0, 2.0));
        float s2 = exp(-pow(length(d - vec2(-0.28, 0.26)) * 7.0, 2.0));
        float s3 = exp(-pow(length(d - vec2(-0.22,-0.29)) * 7.0, 2.0));
        float s4 = exp(-pow(length(d - vec2( 0.26,-0.22)) * 7.0, 2.0));
        a = c + (s1 + s2 + s3 + s4) * 0.75;
    } else {
        a = exp(-r * r * 3.2) - 0.03;
    }
    if (a < 0.02) discard;
    vec3 col = vColor * vBright * a;
    FragColor = vec4(col, 1.0);
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
        const float a = fibA(index), rr = fibR(index, STELLAR_COUNT) * 6.0f;
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
        // ★ v1.4: 盘加厚 —— 用 fib 球面角给 Z 分量, 不再是 ±1.2 的薄片。
        const float thick = float(std::cos(fibA(index * 5 + 2)) * fibR(index, ISM_COUNT) * 4.5);
        return QVector3D(c.x() + std::cos(a) * rr,
                         c.y() + thick,
                         c.z() + std::sin(a) * rr * 0.8f);
    }
    return QVector3D();
}

void EvoStars::build()
{
    if (m_ready || !m_f)
        return;

    struct Pt { float x, y, z, r, g, b, size, bright, shape; };
    QVector<Pt> pts;
    pts.reserve(120);

    // ---- 恒星 53: 赫罗图立体星团 (形状按光度级别) ----
    {
        const QVector3D c = stellarCenter();
        for (int i = 0; i < STELLAR_COUNT; ++i) {
            const StellarEntry &e = STELLAR_ENTRIES[i];
            const double lte = e.teff > 0 ? std::log10(e.teff) : 3.5;
            double logL = 0.0;
            if (e.massSol > 0)
                logL = 3.5 * std::log10(e.massSol);
            if (e.massSol <= 0 && e.teff > 20000)
                logL = -2.5;
            const float x = c.x() + float((4.7 - lte) / (4.7 - 3.3) * 40.0 - 20.0);
            const float y = c.y() + float((logL + 4.5) / 11.0 * 30.0 - 15.0);
            const float a = fibA(i), rr = fibR(i, STELLAR_COUNT) * 6.0f;
            const QVector3D col = starColor(e.teff);
            // ★ v1.4 形状/大小/亮度: 超巨星=大星芒, 巨星=星芒, 白矮星=小亮点, 主序=光点
            float size = 3.4f, bright = 1.05f, shape = 0.0f;
            if (logL > 3.0)      { size = 7.0f;  bright = 1.40f; shape = 1.0f; }
            else if (logL > 1.0) { size = 5.5f;  bright = 1.25f; shape = 1.0f; }
            else if (logL < -1.0){ size = 1.9f;  bright = 0.85f; shape = 0.0f; }
            pts.append({x, y, c.z() + std::cos(a) * rr,
                        col.x(), col.y(), col.z(), size, bright, shape});
        }
    }

    // ---- AGN 34: 距离对数壳层 (形状按类型特征) ----
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
            QVector3D col(0.7f, 0.8f, 1.0f);
            float size = 3.8f, bright = 1.10f, shape = 0.0f;
            const QString cat = QString::fromUtf8(e.catCn);
            if (cat.contains(QStringLiteral("类星体"))) {
                col = QVector3D(1.0f, 0.9f, 0.6f);
                size = 6.5f; bright = 1.40f; shape = 1.0f;   // 星芒
            } else if (cat.contains(QStringLiteral("耀变体"))) {
                col = QVector3D(0.6f, 0.9f, 1.0f);
                size = 6.0f; bright = 1.30f; shape = 4.0f;   // 喷流
            } else if (cat.contains(QStringLiteral("射电"))) {
                col = QVector3D(1.0f, 0.6f, 0.4f);
                size = 7.0f; bright = 1.20f; shape = 3.0f;   // 双瓣
            } else if (cat.contains(QStringLiteral("团")) || cat.contains(QStringLiteral("超团"))) {
                col = QVector3D(0.8f, 0.6f, 1.0f);
                size = 7.5f; bright = 1.25f; shape = 5.0f;   // 团簇
            }
            pts.append({x, y, z, col.x(), col.y(), col.z(), size, bright, shape});
        }
    }

    // ---- ISM 25: 类型分组圆盘 (形状按类型特征) ----
    {
        const QVector3D c = ismCenter();
        for (int i = 0; i < ISM_COUNT; ++i) {
            const IsmEntry &e = ISM_ENTRIES[i];
            const QString cat = QString::fromUtf8(e.catCn);
            int grp = 2;
            QVector3D col(0.3f, 0.3f, 0.35f);
            // 形状: 0光点 2环 5团簇; 暗星云=暗色环(剪影)
            float size = 6.0f, bright = 1.15f, shape = 0.0f;
            if (cat.contains(QStringLiteral("发射")) || cat.contains(QStringLiteral("超新星"))
                || cat.contains(QStringLiteral("遗迹"))) {
                grp = 0; col = QVector3D(1.0f, 0.45f, 0.45f);
                shape = 2.0f; size = 6.5f; bright = 1.30f;     // 遗迹: 环
                if (cat.contains(QStringLiteral("超新星")))
                    size = 7.2f;
            } else if (cat.contains(QStringLiteral("反射")) || cat.contains(QStringLiteral("疏散"))
                       || cat.contains(QStringLiteral("球状")) || cat.contains(QStringLiteral("星协"))) {
                grp = 1; col = QVector3D(0.55f, 0.7f, 1.0f);
                if (cat.contains(QStringLiteral("疏散")) || cat.contains(QStringLiteral("球状"))
                    || cat.contains(QStringLiteral("星协"))) {
                    shape = 5.0f; size = 5.5f; bright = 1.15f; // 星团: 团簇
                } else {
                    shape = 2.0f; size = 5.5f; bright = 1.10f; // 反射: 环
                }
            } else if (cat.contains(QStringLiteral("行星状"))) {
                grp = 0; col = QVector3D(0.4f, 1.0f, 0.7f);
                shape = 2.0f; size = 5.2f; bright = 1.25f;     // 行星状: 环
            } else if (cat.contains(QStringLiteral("暗"))) {
                // ★ 暗星云: 深灰紫色剪影环 —— 在深空背景上以微光边缘示意"剪影"。
                grp = 2; col = QVector3D(0.34f, 0.30f, 0.40f);
                shape = 2.0f; size = 6.0f; bright = 0.75f;
            }
            const float a = fibA(i * 3 + grp);
            const float rr = 6.0f + grp * 9.0f + fibR(i, ISM_COUNT) * 5.0f;
            const float x = c.x() + std::cos(a) * rr;
            const float z = c.z() + std::sin(a) * rr * 0.8f;
            const float thick = float(std::cos(fibA(i * 5 + 2)) * fibR(i, ISM_COUNT) * 4.5);
            const float y = c.y() + thick;
            if (e.sizeLy > 20.0) size += 1.0f;
            pts.append({x, y, z, col.x(), col.y(), col.z(), size, bright, shape});
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
    m_f->glEnableVertexAttribArray(4);
    m_f->glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Pt),
                               reinterpret_cast<void *>(8 * sizeof(float)));
    m_vbo.release();
    m_vao.release();

    m_ready = true;
    qInfo() << "[演化星团] 构建完成, 点数:" << m_count
            << "(恒星" << STELLAR_COUNT << "/AGN" << AGN_COUNT
            << "/ISM" << ISM_COUNT << ") v1.4 形状特征化";
}

void EvoStars::render(const QMatrix4x4 &viewProj, float pointScale,
                      const QVector3D &eye, float timeSec, float evoT)
{
    if (!m_ready || !m_prog)
        return;
    m_prog->bind();
    m_prog->setUniformValue("uViewProj", viewProj);
    m_prog->setUniformValue("uPixelScale", pointScale);
    m_prog->setUniformValue("uEye", eye);
    m_prog->setUniformValue("uTime", timeSec);
    m_prog->setUniformValue("uEvoT", evoT);

    // ★ v1.4: 自管混合状态 —— 加法发光, 粒子不写深度 (修复点被遮挡问题)。
    //   深度测试仍开 (主星/模拟体正确遮挡身后粒子), 但粒子之间不相互遮挡。
    m_f->glEnable(GL_BLEND);
    m_f->glBlendFunc(GL_ONE, GL_ONE);
    m_f->glDepthMask(GL_FALSE);
    m_vao.bind();
    m_f->glDrawArrays(GL_POINTS, 0, m_count);
    m_vao.release();
    m_f->glDepthMask(GL_TRUE);
    m_f->glDisable(GL_BLEND);
    m_prog->release();
}
