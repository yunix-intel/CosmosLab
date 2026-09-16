// ============================================================================
//  comet.cpp —— 彗尾渲染实现
//
//  ★ 几何方案: 每条尾巴画成一个**沿轴向的三角带 (ribbon)**,
//    面向相机 (billboard)。相比用粒子系统, ribbon 的开销极小
//    (每条尾巴几十个三角形), 而彗尾本来就是连续的发光气体流。
//
//    亮度沿轴向衰减用 shader 内的 smoothstep 实现, 无需额外纹理。
//
//  ★ 两条尾巴的几何差别:
//      离子尾 —— 直接沿 antiSun 方向延伸 (笔直)
//      尘埃尾 —— 沿 (antiSun 与 -velocity 的混合) 方向, 且沿程弯曲
//                混合系数 0.45 让尾部明显滞后于彗核 —— 这正是
//                "尘埃尾弯曲"的成因。
// ============================================================================

#include "comet.h"
#include "shaders.h"

#include <QDebug>
#include <QOpenGLFunctions_3_3_Core>
#include <QtMath>
#include <cmath>

namespace {

// 尾巴长度方向上的分段数。24 段足够平滑, 且让弯曲可见。
constexpr int kSegments = 24;

const char *kVert = R"(
#version 330 core
layout(location = 0) in vec2 aParam;   // x: 沿轴 0..1, y: 横向 -1..1
uniform mat4  uViewProj;
uniform vec3  uOrigin;      // 彗核位置
uniform vec3  uAxis;        // 尾巴主轴 (单位向量)
uniform vec3  uCamPos;      // 相机位置 —— 用于真正的 billboard
uniform float uLength;
uniform float uWidth;       // 根部半宽
uniform float uSpread;      // 末端相对根部的展宽倍数
uniform float uAlpha;
uniform vec3  uColor;

out float vAlpha;
out vec3  vColor;
out float vAcross;     // 横向位置 -1..1, 用于边缘柔化

void main() {
    float t = aParam.x;
    vec3 axisPos = uOrigin + uAxis * (t * uLength);

    // ★★ 真正的 billboard —— 必须用**相机位置**算侧向量, 不能用一个
    //    固定的世界向量。初版取 side = normalize(cross(uAxis, worldY)),
    //    那是世界空间里固定的一个方向, 于是当视线恰好落在该平面内时,
    //    整条尾巴退化成一条细线 (实测截图里看到的"扁平带子")。
    //    正确做法: side = normalize(cross(uAxis, viewDir)),
    //    viewDir = axisPos - camPos, 这样侧向量始终垂直于视线,
    //    尾巴平面永远正对相机。
    vec3 viewDir = normalize(axisPos - uCamPos);
    vec3 side = cross(uAxis, viewDir);
    float sl = length(side);
    // 视线与轴近平行时叉积趋近 0 —— 此时用任意垂直向量兜底,
    // 避免出现 NaN 让整个三角形消失
    side = (sl > 1e-4) ? (side / sl)
                       : normalize(cross(uAxis, vec3(0.0, 1.0, 0.0) + vec3(0.001, 0.0, 0.0)));

    // 沿轴弯曲: 二次侧偏, 让尘埃尾呈现自然的弧形
    vec3 bent = axisPos + side * (t * t * uLength * 0.10);

    // 半宽沿轴增长: 根部窄、末端宽 (彗尾是发散的锥形)
    float halfW = uWidth * (1.0 + (uSpread - 1.0) * t);
    vec3 pos = bent + side * (aParam.y * halfW);

    gl_Position = uViewProj * vec4(pos, 1.0);

    // 亮度: 根部最亮, 末端淡出。用 smoothstep 让衰减更自然,
    // 而不是线性 (线性会让尾巴看起来像一条均匀的带子)。
    vAlpha = uAlpha * smoothstep(1.0, 0.05, t);
    vColor = uColor;
    vAcross = aParam.y;
}
)";

const char *kFrag = R"(
#version 330 core
in float vAlpha;
in vec3  vColor;
in float vAcross;
out vec4 FragColor;

void main() {
    // ★★ 横向柔化 —— 这是"弥散羽流"与"硬边带子"的分水岭。
    //   初版直接用 aParam.y 做几何宽度, 片元里不做任何横向衰减,
    //   于是尾巴呈现为一条边缘锐利的四边形 —— 一眼就假。
    //   真实彗尾是稀薄气体/尘埃, 中心浓、边缘渐渐淡出。
    //   用高斯型衰减 exp(-2.2·x²) 模拟: 中心 1.0, 边缘趋 0。
    float x = vAcross;
    float edge = exp(-2.2 * x * x);
    // 边缘再叠一层平滑截断, 避免正方形角落留下残留
    edge *= smoothstep(1.0, 0.86, abs(x));

    float a = vAlpha * edge;
    if (a < 0.002) discard;
    FragColor = vec4(vColor * a, a);
}
)";

} // namespace

CometRenderer::CometRenderer() = default;
CometRenderer::~CometRenderer() = default;

void CometRenderer::destroy()
{
    if (m_vbo.isCreated())
        m_vbo.destroy();
    if (m_vao.isCreated())
        m_vao.destroy();
    delete m_prog;
    m_prog = nullptr;
    m_ready = false;
}

void CometRenderer::init(QOpenGLFunctions_3_3_Core *f)
{
    if (m_ready || !f)
        return;
    m_f = f;

    auto *p = new QOpenGLShaderProgram;
    const bool ok = p->addShaderFromSourceCode(QOpenGLShader::Vertex, kVert)
                 && p->addShaderFromSourceCode(QOpenGLShader::Fragment, kFrag)
                 && p->link();
    if (!ok) {
        qWarning() << "[彗尾] 着色器失败:" << p->log();
        delete p;
        return;
    }
    m_prog = p;

    buildMesh();
    m_ready = m_prog && m_vbo.isCreated() && m_vertexCount > 0;
    qInfo() << "[彗尾] 初始化" << (m_ready ? "成功" : "失败")
            << " 顶点数:" << m_vertexCount;
}

void CometRenderer::buildMesh()
{
    // 生成一个 (kSegments+1) × 2 的三角带网格, 参数化为 (t, side)
    QVector<float> verts;
    verts.reserve((kSegments + 1) * 2 * 2);
    for (int i = 0; i <= kSegments; ++i) {
        const float t = float(i) / float(kSegments);
        verts << t << -1.0f;      // 左
        verts << t <<  1.0f;      // 右
    }
    m_vertexCount = verts.size() / 2;

    if (!m_vao.isCreated())
        m_vao.create();
    if (!m_vbo.isCreated())
        m_vbo.create();

    m_vao.bind();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vbo.allocate(verts.constData(), verts.size() * int(sizeof(float)));
    m_f->glEnableVertexAttribArray(0);
    m_f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                               reinterpret_cast<void *>(0));
    m_vao.release();
    m_vbo.release();
}

void CometRenderer::render(const QMatrix4x4 &viewProj,
                           const QVector3D &camPos,
                           const QVector<CometTail> &tails)
{
    if (!m_ready || tails.isEmpty())
        return;

    m_f->glEnable(GL_BLEND);
    // ★ 加法混合 —— 彗尾是发光的稀薄气体, 加法混合才能表现
    //   "重叠处更亮"的物理特征。用普通 alpha 混合会显得像一块塑料片。
    m_f->glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    m_f->glDepthMask(GL_FALSE);        // 不写深度, 避免尾巴互相遮挡

    m_prog->bind();
    m_prog->setUniformValue("uViewProj", viewProj);
    m_prog->setUniformValue("uCamPos", camPos);

    m_vao.bind();

    for (const CometTail &ct : tails) {
        if (ct.length <= 0.0f || ct.brightness <= 0.01f)
            continue;

        m_prog->setUniformValue("uOrigin", ct.nucleus);

        // ---- 离子尾: 严格背离太阳, 笔直, 蓝色 ----
        m_prog->setUniformValue("uAxis", ct.antiSun);
        m_prog->setUniformValue("uLength", ct.length);
        m_prog->setUniformValue("uWidth", ct.length * 0.011f);
        m_prog->setUniformValue("uSpread", 4.2f);
        m_prog->setUniformValue("uAlpha", ct.brightness * 0.32f);   // 离子尾
        m_prog->setUniformValue("uColor", QVector3D(0.42f, 0.72f, 1.0f));
        m_f->glDrawArrays(GL_TRIANGLE_STRIP, 0, m_vertexCount);

        // ---- 尘埃尾: 沿轨道方向滞后, 弯曲, 黄白色 ----
        //   把 antiSun 与 **反运动方向** 混合 —— 尘埃保持原有轨道速度,
        //   因此相对彗核向后偏。系数 0.45 让弯曲明显但不至于完全拖尾。
        QVector3D dustAxis = ct.antiSun * 0.55f - ct.velocity * 0.45f;
        if (dustAxis.lengthSquared() < 1e-6f)
            dustAxis = ct.antiSun;
        dustAxis.normalize();

        m_prog->setUniformValue("uAxis", dustAxis);
        m_prog->setUniformValue("uLength", ct.length * 0.72f);
        m_prog->setUniformValue("uWidth", ct.length * 0.016f);
        m_prog->setUniformValue("uSpread", 5.0f);
        m_prog->setUniformValue("uAlpha", ct.brightness * 0.20f);   // 尘埃尾
        m_prog->setUniformValue("uColor", QVector3D(1.0f, 0.93f, 0.74f));
        m_f->glDrawArrays(GL_TRIANGLE_STRIP, 0, m_vertexCount);
    }

    m_vao.release();
    m_prog->release();
    m_f->glDepthMask(GL_TRUE);
    m_f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_f->glDisable(GL_BLEND);
}
