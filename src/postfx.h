// ============================================================================
//  postfx.h —— HDR 后期处理管线
//
//  移植自 Python 版 core/postfx.py。
//
//  流程
//  ----
//      场景 -> RGBA16F HDR 缓冲
//           -> 亮部提取 (软膝阈值 1.05)
//           -> 降采样链 (6 级, 每级 1/2 分辨率)
//           -> 升采样链 (3x3 tent, 逐级加法叠加)
//           -> 合成: 场景 + Bloom + 镜头光斑 + 暗角 + ACES + 颗粒
//           -> 目标 FBO
//
//  ★ 关于目标 FBO
//    QQuickFramebufferObject 的 render() 运行在 Qt 自己绑定的 FBO 上,
//    这个 FBO 的 id **不是 0**。所以合成时不能简单 bindDefault(), 必须
//    先用 glGetIntegerv(GL_FRAMEBUFFER_BINDING) 记下 Qt 的 FBO id,
//    合成阶段再显式绑回去。否则画面会画到屏幕默认缓冲上, QML 里
//    表现为整个 3D 区域空白 (被 QML 的重绘覆盖)。
// ============================================================================

#pragma once

#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QVector>

class Mesh;

class PostFX
{
public:
    PostFX();
    ~PostFX();

    // GL 上下文就绪后调用一次 (SceneRenderer::initialize 内)
    void init(QOpenGLFunctions_3_3_Core *f);

    // 尺寸变化时重建 FBO 链。参数为**设备像素**。
    void resize(int pixelW, int pixelH);

    void destroy();

    bool ready() const { return m_ready; }

    // ---- 三阶段绘制 ----

    // 1. 绑定 HDR 场景缓冲并清屏, 之后即可绘制场景
    void bindScene();

    // 2. 场景绘制完成后跑 Bloom 链
    void renderBloom();

    // 3. 合成到 targetFbo
    //    sunScreenX/Y: 太阳的归一化屏幕坐标 (0..1, 左下原点)
    //    sunVisible   : 太阳是否在视口内 (决定镜头光斑是否出现)
    void composite(GLuint targetFbo, float sunScreenX, float sunScreenY,
                   bool sunVisible, float timeSec);

    // ---- 可调参数 ----
    //
    // ★ 阈值与强度是调出来的, 不是照抄:
    //   阈值 1.05 时行星的明亮表面(地球云层)也会被提取进 Bloom, 再经
    //   升采样回灌到原区域, 把 HDR 值又抬高约 0.1 —— 正好越过 ACES 的
    //   饱和点, 云层整片变纯白(实测中心纯白 15%)。
    //   提到 1.25 后只有太阳(3.4)和真正的镜面高光能触发, 行星表面不再
    //   自我加亮, 而太阳的耀光反而更突出。
    float bloomThreshold = 1.25f;
    float bloomSoftKnee  = 0.6f;
    float bloomStrength  = 0.45f;
    float bloomRadius    = 1.0f;
    float exposure       = 1.25f;
    float vignette       = 0.42f;
    float grain          = 0.016f;
    float flareStrength  = 0.30f;

    int width()  const { return m_w; }
    int height() const { return m_h; }

private:
    void releaseFbos();
    void drawQuad();

    QOpenGLFunctions_3_3_Core *m_f = nullptr;

    QOpenGLShaderProgram *m_prefilter = nullptr;
    QOpenGLShaderProgram *m_down      = nullptr;
    QOpenGLShaderProgram *m_up        = nullptr;
    QOpenGLShaderProgram *m_composite = nullptr;

    Mesh *m_quad = nullptr;

    QOpenGLFramebufferObject *m_sceneFbo = nullptr;
    QVector<QOpenGLFramebufferObject *> m_bloomFbos;

    int  m_w = 0;
    int  m_h = 0;
    bool m_ready = false;
};
