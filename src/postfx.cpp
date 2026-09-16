// ============================================================================
//  postfx.cpp —— HDR 后期处理实现
// ============================================================================

#include "postfx.h"
#include "mesh.h"
#include "shaders.h"

#include <QDebug>
#include <QVector2D>

namespace {
constexpr int kBloomLevels = 6;   // 与 Python 版一致
}

PostFX::PostFX() = default;

PostFX::~PostFX()
{
    // 与 SceneRenderer 一致: 上下文可能已销毁, 不在析构里主动调 GL 之外的清理。
    // delete 本身由 QOpenGL* 的析构处理, 上下文缺失时它们会安全地跳过。
    delete m_quad;
    delete m_prefilter;
    delete m_down;
    delete m_up;
    delete m_composite;
    delete m_sceneFbo;
    qDeleteAll(m_bloomFbos);
}

// ---------------------------------------------------------------------------
//  初始化
// ---------------------------------------------------------------------------

void PostFX::init(QOpenGLFunctions_3_3_Core *f)
{
    if (m_ready || !f)
        return;

    m_f = f;

    auto make = [](const char *fs, const char *name) -> QOpenGLShaderProgram * {
        auto *p = new QOpenGLShaderProgram;
        const bool ok =
            p->addShaderFromSourceCode(QOpenGLShader::Vertex, shaders::kFullscreenVert)
            && p->addShaderFromSourceCode(QOpenGLShader::Fragment, fs)
            && p->link();
        if (!ok) {
            qWarning() << "[后处理] 着色器" << name << "失败:" << p->log();
            delete p;
            return nullptr;
        }
        return p;
    };

    m_prefilter = make(shaders::kBloomPrefilterFrag, "bloom_prefilter");
    m_down      = make(shaders::kBloomDownFrag,      "bloom_down");
    m_up        = make(shaders::kBloomUpFrag,        "bloom_up");
    m_composite = make(shaders::kCompositeFrag,      "composite");

    m_quad = geom::makeScreenQuad();

    m_ready = m_prefilter && m_down && m_up && m_composite
           && m_quad && m_quad->isValid();

    qInfo() << "[后处理] 初始化" << (m_ready ? "成功" : "失败");
}

// ---------------------------------------------------------------------------
//  尺寸 / FBO 链
// ---------------------------------------------------------------------------

void PostFX::releaseFbos()
{
    delete m_sceneFbo;
    m_sceneFbo = nullptr;
    qDeleteAll(m_bloomFbos);
    m_bloomFbos.clear();
}

void PostFX::resize(int pixelW, int pixelH)
{
    if (!m_f)
        return;

    const int w = qMax(1, pixelW);
    const int h = qMax(1, pixelH);
    if (w == m_w && h == m_h && m_sceneFbo)
        return;

    m_w = w;
    m_h = h;
    releaseFbos();

    // 场景 HDR 缓冲: RGBA16F + 深度
    QOpenGLFramebufferObjectFormat fmt;
    fmt.setInternalTextureFormat(GL_RGBA16F);
    fmt.setAttachment(QOpenGLFramebufferObject::Depth);
    fmt.setMipmap(false);
    m_sceneFbo = new QOpenGLFramebufferObject(m_w, m_h, fmt);

    // Bloom 降采样链: 每级 1/2 分辨率, 递推到小于 4px 为止
    int bw = m_w / 2;
    int bh = m_h / 2;
    for (int i = 0; i < kBloomLevels; ++i) {
        if (bw < 4 || bh < 4)
            break;
        QOpenGLFramebufferObjectFormat bf;
        bf.setInternalTextureFormat(GL_RGBA16F);
        bf.setMipmap(false);
        m_bloomFbos.append(new QOpenGLFramebufferObject(bw, bh, bf));
        bw /= 2;
        bh /= 2;
    }

    qInfo() << "[后处理] FBO 链" << m_w << "x" << m_h
            << " bloom 级数:" << m_bloomFbos.size()
            << (m_sceneFbo->isValid() ? "有效" : "★无效");
}

// ---------------------------------------------------------------------------
//  阶段 1: 绑定 HDR 场景缓冲
// ---------------------------------------------------------------------------

void PostFX::bindScene()
{
    if (!m_ready || !m_sceneFbo)
        return;

    m_sceneFbo->bind();
    m_f->glViewport(0, 0, m_w, m_h);
    m_f->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// ---------------------------------------------------------------------------
//  阶段 2: Bloom 链
// ---------------------------------------------------------------------------

void PostFX::renderBloom()
{
    if (!m_ready || m_bloomFbos.isEmpty() || !m_sceneFbo)
        return;

    // ---- 1. 亮部提取, 写入第 0 级 ----
    QOpenGLFramebufferObject *first = m_bloomFbos[0];
    first->bind();
    m_f->glViewport(0, 0, first->width(), first->height());
    m_f->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_f->glClear(GL_COLOR_BUFFER_BIT);
    m_f->glDisable(GL_DEPTH_TEST);
    m_f->glDisable(GL_BLEND);

    m_prefilter->bind();
    m_f->glActiveTexture(GL_TEXTURE0);
    m_f->glBindTexture(GL_TEXTURE_2D, m_sceneFbo->texture());
    m_prefilter->setUniformValue("uScene", 0);
    m_prefilter->setUniformValue("uThreshold", bloomThreshold);
    m_prefilter->setUniformValue("uSoftKnee", bloomSoftKnee);
    m_prefilter->setUniformValue("uTexel",
        QVector2D(1.0f / float(m_w), 1.0f / float(m_h)));
    m_quad->draw();
    m_prefilter->release();

    // ---- 2. 降采样链 (13-tap) ----
    m_down->bind();
    QOpenGLFramebufferObject *prev = first;
    for (int i = 1; i < m_bloomFbos.size(); ++i) {
        QOpenGLFramebufferObject *dst = m_bloomFbos[i];
        dst->bind();
        m_f->glViewport(0, 0, dst->width(), dst->height());
        m_f->glClear(GL_COLOR_BUFFER_BIT);
        m_f->glActiveTexture(GL_TEXTURE0);
        m_f->glBindTexture(GL_TEXTURE_2D, prev->texture());
        m_down->setUniformValue("uSrc", 0);
        m_down->setUniformValue("uTexel",
            QVector2D(1.0f / float(prev->width()), 1.0f / float(prev->height())));
        m_quad->draw();
        prev = dst;
    }
    m_down->release();

    // ---- 3. 升采样链 (从最小级逐级加法叠加) ----
    m_up->bind();
    m_f->glEnable(GL_BLEND);
    m_f->glBlendFunc(GL_ONE, GL_ONE);
    for (int i = m_bloomFbos.size() - 1; i > 0; --i) {
        QOpenGLFramebufferObject *src = m_bloomFbos[i];
        QOpenGLFramebufferObject *dst = m_bloomFbos[i - 1];
        dst->bind();
        m_f->glViewport(0, 0, dst->width(), dst->height());
        m_f->glActiveTexture(GL_TEXTURE0);
        m_f->glBindTexture(GL_TEXTURE_2D, src->texture());
        m_up->setUniformValue("uSrc", 0);
        m_up->setUniformValue("uTexel",
            QVector2D(1.0f / float(src->width()), 1.0f / float(src->height())));
        m_up->setUniformValue("uRadius", bloomRadius);
        m_quad->draw();
    }
    m_up->release();
    m_f->glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------------
//  阶段 3: 合成到目标 FBO
// ---------------------------------------------------------------------------

void PostFX::composite(GLuint targetFbo, float sunScreenX, float sunScreenY,
                       bool sunVisible, float timeSec)
{
    if (!m_ready || !m_sceneFbo)
        return;

    m_f->glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    m_f->glViewport(0, 0, m_w, m_h);

    // 关键: 关闭裁剪/模板/深度/剔除。
    // QQuickFramebufferObject 在调用 render() 前会启用 scissor 限制作画区域,
    // 我们要画满整个 FBO, 必须关掉它; 否则合成结果会被裁成一小块。
    // (Qt Quick 在后续绘制其它 item 时会自行重设这些状态。)
    m_f->glDisable(GL_SCISSOR_TEST);
    m_f->glDisable(GL_STENCIL_TEST);
    m_f->glDisable(GL_DEPTH_TEST);
    m_f->glDisable(GL_CULL_FACE);
    m_f->glDisable(GL_BLEND);

    m_composite->bind();
    m_f->glActiveTexture(GL_TEXTURE0);
    m_f->glBindTexture(GL_TEXTURE_2D, m_sceneFbo->texture());
    m_composite->setUniformValue("uScene", 0);

    // Bloom 级数为 0 时(极小窗口)退化为不叠加
    const bool hasBloom = !m_bloomFbos.isEmpty();
    m_f->glActiveTexture(GL_TEXTURE1);
    m_f->glBindTexture(GL_TEXTURE_2D, hasBloom ? m_bloomFbos[0]->texture() : m_sceneFbo->texture());
    m_composite->setUniformValue("uBloom", 1);

    m_composite->setUniformValue("uBloomStrength", hasBloom ? bloomStrength : 0.0f);
    m_composite->setUniformValue("uExposure", exposure);
    m_composite->setUniformValue("uVignette", vignette);
    m_composite->setUniformValue("uGrain", grain);
    m_composite->setUniformValue("uTime", timeSec);
    m_composite->setUniformValue("uFlareStrength", flareStrength);
    m_composite->setUniformValue("uResolution", QVector2D(float(m_w), float(m_h)));
    m_composite->setUniformValue("uSunScreenX", sunScreenX);
    m_composite->setUniformValue("uSunScreenY", sunScreenY);
    m_composite->setUniformValue("uSunVisible", sunVisible ? 1.0f : 0.0f);

    m_quad->draw();
    m_composite->release();

    // 还原纹理单元, 避免影响后续 pass
    m_f->glActiveTexture(GL_TEXTURE0);
}

void PostFX::destroy()
{
    releaseFbos();
    delete m_quad;
    m_quad = nullptr;
    delete m_prefilter;
    m_prefilter = nullptr;
    delete m_down;
    m_down = nullptr;
    delete m_up;
    m_up = nullptr;
    delete m_composite;
    m_composite = nullptr;
    m_ready = false;
}
