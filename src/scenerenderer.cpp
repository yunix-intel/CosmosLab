// ============================================================================
//  scenerenderer.cpp —— OpenGL 场景渲染实现
//
//  绘制顺序 (与 Python 版一致):
//      星空背景 (不写深度) -> 轨道线 -> 行星本体 -> 环系 -> 大气壳 (加法混合)
//
//  深度策略:
//      深度精度取决于 far/near 的比值, 24 位缓冲上限约 1e5~1e6。
//      相机已用动态 near/far 把比值压到安全范围 (见 camera.cpp)。
//      星空背景与尘埃**不参与深度测试**, 故 far 不需要覆盖它们。
// ============================================================================

#include "scenerenderer.h"
#include <QElapsedTimer>
#include "bodyregistry.h"
#include "celestialdata.h"
#include "ephemeris.h"
#include "mesh.h"
#include "shaders.h"

#include <QDateTime>
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLVersionFunctionsFactory>
#include <QtMath>
#include <cmath>

SceneRenderer::SceneRenderer() = default;

SceneRenderer::~SceneRenderer()
{
    delete m_sphere;
    delete m_quad;
    for (Mesh *m : m_ringMeshes)
        delete m;
    for (Mesh *m : m_orbitMeshes)
        delete m;
    // 着色器与纹理由 QOpenGLShaderProgram / TextureCache 自行管理;
    // 上下文已销毁时不应再调用 GL。
}

// ---------------------------------------------------------------------------
//  初始化
// ---------------------------------------------------------------------------

static QOpenGLShaderProgram *makeProgram(const char *vs, const char *fs,
                                         const char *name)
{
    auto *p = new QOpenGLShaderProgram;
    const bool ok = p->addShaderFromSourceCode(QOpenGLShader::Vertex, vs)
                 && p->addShaderFromSourceCode(QOpenGLShader::Fragment, fs)
                 && p->link();
    if (!ok) {
        qWarning() << "[渲染器] 着色器" << name << "失败:" << p->log();
        delete p;
        return nullptr;
    }
    return p;
}

// ---------------------------------------------------------------------------
//  银河系参考底图 —— 半透明叠加层
//
//  ★ 画的是一张朝向相机的四边形, 放在银道面 (y=0) 上。
//    不用固定平面是因为: 侧视时平面退化成一条线; 用 billboard 则
//    任何视角都能看到完整结构。折中办法是按相机俯角淡出 ——
//    接近侧视时本来也看不清盘面, 淡出最自然。
//
//  ★ 透明度用**图像亮度**调制: 暗处全透明。这样不会在银盘外围
//    留下一块方形的暗斑。
//
//  ★ 数据来源: NASA/JPL-Caltech/ESO/R. Hurt 的银河系结构科学插画,
//    不是照片 (我们身处银盘内部, 外部全景物理上无法拍到)。
//    UI 必须标明这一点。
// ---------------------------------------------------------------------------
inline const char *kGalOverlayVert = R"(
#version 330 core
layout(location = 0) in vec2 aPlane;
uniform mat4  uViewProj;
uniform float uHalfSize;
out vec2 vUV;
void main() {
    vUV = aPlane * 0.5 + 0.5;
    // 图像 +y (向下) 对应场景 +z
    // ★★ 必须在**世界空间**缩放, 不能在裁剪空间乘。
    //   踩过的坑: 写成 `uViewProj * vec4(...) * uHalfSize` 时,
    //   因为裁剪空间的 x/y/z/w 被同比例放大, 透视除法 (xyz/w) 后
    //   缩放被**完全抵消** —— 四边形实际只有 1 个世界单位大,
    //   在 200 单位宽的银盘里小到看不见, 表现为"叠加层没出现"。
    vec3 wp = vec3(aPlane.x, 0.0, -aPlane.y) * uHalfSize;
    gl_Position = uViewProj * vec4(wp, 1.0);
}
)";

inline const char *kGalOverlayFrag = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform float uAlpha;
out vec4 FragColor;
void main() {
    vec3 c = texture(uTex, vUV).rgb;
    float lum = max(max(c.r, c.g), c.b);
    float a = uAlpha * smoothstep(0.02, 0.35, lum);
    FragColor = vec4(c, a);
}
)";

// ---------------------------------------------------------------------------
//  GPU 耗时测量 —— 用 glFinish 强制同步后计时
//
//  ★ 为什么不能只看帧率: 场景由 SolarScene::onTick 的 16ms 定时器驱动,
//    帧率上限被锁在 62.5 FPS。GPU 耗时只要低于 16ms, 帧率就恒为 62.5,
//    完全反映不出余量。要评估"能承载多少粒子", 必须测真实耗时。
// ---------------------------------------------------------------------------
namespace {
bool perfEnabled()
{
    static const bool on = qEnvironmentVariableIntValue("SS_PERF") > 0;
    return on;
}

// 累计统计, 每 N 帧输出一次
struct PerfAccum {
    double sumMs = 0.0;
    int    n = 0;
    double minMs = 1e9, maxMs = 0.0;
    void add(double ms) {
        sumMs += ms; ++n;
        if (ms < minMs) minMs = ms;
        if (ms > maxMs) maxMs = ms;
        if (n % 60 == 0) {
            qWarning().noquote()
                << QString("[性能] 最近60帧 GPU 平均 %1 ms (min %2 / max %3)"
                           "  -> 纯渲染理论上限 %4 FPS")
                       .arg(sumMs / n, 0, 'f', 2)
                       .arg(minMs, 0, 'f', 2)
                       .arg(maxMs, 0, 'f', 2)
                       .arg(1000.0 / qMax(sumMs / n, 1e-6), 0, 'f', 1);
            sumMs = 0.0; n = 0; minMs = 1e9; maxMs = 0.0;
        }
    }
};
} // namespace

void SceneRenderer::initialize()
{
    if (m_ready)
        return;

    m_f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(
        QOpenGLContext::currentContext());
    if (!m_f || !m_f->initializeOpenGLFunctions()) {
        qWarning() << "[渲染器] 无法获取 OpenGL 3.3 Core 函数表";
        return;
    }

    buildPrograms();
    buildMeshes();

    m_scene.setJulianDate(eph::jdFromUnixSec(double(QDateTime::currentSecsSinceEpoch())));
    m_scene.update();

    // HDR 后处理链 (Bloom / ACES)。没有它光照必须压在 1.0 以下,
    // 行星会灰蒙蒙、太阳也没有耀光。
    m_postfx.init(m_f);

    // 银河系粒子模型 (只在切到银河系尺度时使用)
    m_galaxy.init(m_f);

    // ---- 银河系参考底图 (NASA/JPL 官方插画, 半透明叠加) ----
    //
    //  ★ 贴图是 tools/build_overlay.py 处理过的: 已抹掉白色文字与
    //    网格线, 保留彩色。文件名 galaxy_overlay.jpg。
    //  ★ 四边形是 -1..1 的 plane, 顶点在着色器里乘 uHalfSize,
    //    故 VBO 只需两个 float。
    {
        m_galOverlayProg = makeProgram(kGalOverlayVert, kGalOverlayFrag,
                                       "galaxyOverlay");
        if (m_galOverlayProg) {
            static const float kPlane[8] = {
                -1.0f, -1.0f,   1.0f, -1.0f,
                -1.0f,  1.0f,   1.0f,  1.0f,
            };
            m_galOverlayVao.create();
            m_galOverlayVao.bind();
            m_galOverlayVbo.create();
            m_galOverlayVbo.bind();
            m_galOverlayVbo.allocate(kPlane, sizeof(kPlane));
            m_galOverlayProg->enableAttributeArray(0);
            m_galOverlayProg->setAttributeBuffer(
                0, GL_FLOAT, 0, 2, 2 * sizeof(float));
            m_galOverlayVao.release();
            m_galOverlayVbo.release();
        }

        // 贴图: 走 TextureCache 的 misc 规则 (无前缀, 文件名即 galaxy_overlay.jpg)
        m_galOverlayTex = m_tex.get(QStringLiteral("misc"),
                                    QStringLiteral("galaxy_overlay"));
        if (!m_galOverlayTex)
            qWarning() << "[渲染器] 银河系底图 galaxy_overlay.jpg 未找到";
    }

    // 小行星带 / 柯伊伯带 / 特洛伊群 (太阳系尺度)
    m_belts.init(m_f, false);

    // 彗尾 (离子尾 + 尘埃尾)
    m_comets.init(m_f);

    // 宇宙大尺度结构
    m_cosmos.init(m_f);

    // 演化视图 112 条目分类星团 (v1.3, CPU 生成一次即上传)
    m_evoStars.init(m_f);
    m_evoStars.build();

    m_ready = m_sky && m_planet && m_ring && m_orbit && m_atmo
           && m_sphere && m_quad;

    qInfo() << "[渲染器] 初始化" << (m_ready ? "成功" : "失败")
            << " 天体数:" << m_scene.bodyCount();
}


void SceneRenderer::buildPrograms()
{
    m_sky    = makeProgram(shaders::kFullscreenVert, shaders::kSkyboxFrag, "skybox");
    m_planet = makeProgram(shaders::kPlanetVert,     shaders::kPlanetFrag, "planet");
    m_ring   = makeProgram(shaders::kRingVert,       shaders::kRingFrag,   "ring");
    m_orbit  = makeProgram(shaders::kOrbitVert,      shaders::kOrbitFrag,  "orbit");
    m_atmo   = makeProgram(shaders::kAtmoVert,       shaders::kAtmoFrag,   "atmo");
}

void SceneRenderer::buildMeshes()
{
    // 单位球: 所有天体共用, 由 model 矩阵按半径缩放
    m_sphere = geom::makeSphere(1.0f, 96, 64);
    m_quad   = geom::makeScreenQuad();

    // 环系: 每个有环行星一张独立网格 (内外径比例各不相同)
    for (int i = 0; i < registry::count(); ++i) {
        const BodyData &b = registry::allBodies()[i];
        if (!b.hasRings)
            continue;
        m_ringMeshes.insert(QString::fromUtf8(b.id),
                            geom::makeRing(float(b.ringInner),
                                           float(b.ringOuter), 256));
    }
}

void SceneRenderer::resize(int pixelW, int pixelH)
{
    m_w = qMax(1, pixelW);
    m_h = qMax(1, pixelH);
    // 参数已是设备像素, 直接建 HDR 缓冲链
    m_postfx.resize(m_w, m_h);
}

// ---------------------------------------------------------------------------
//  主绘制
// ---------------------------------------------------------------------------

// 把太阳世界坐标投影到归一化屏幕坐标 (0..1, 左下原点), 供镜头光斑使用
static void projectSun(const QMatrix4x4 &viewProj, const QVector3D &sunPos,
                       float &u, float &v, bool &visible)
{
    const QVector4D clip = viewProj * QVector4D(sunPos, 1.0f);
    if (clip.w() <= 1e-6f) {          // 太阳在相机背后
        visible = false;
        return;
    }
    const QVector3D ndc = clip.toVector3D() / clip.w();
    u = ndc.x() * 0.5f + 0.5f;
    v = ndc.y() * 0.5f + 0.5f;
    // 留一点余量: 太阳刚出画面时仍有部分光斑残留, 更接近真实镜头
    visible = (u > -0.25f && u < 1.25f && v > -0.25f && v < 1.25f);
}

void SceneRenderer::render(const ViewState &vs)
{
    // ★ 首次调用时 m_f 还是 nullptr —— 必须无条件尝试初始化。
    //   早期写成 if (!m_ready && m_f) 是错的: m_f 为空时条件为假, 会跳过
    //   initialize() 直接往下走, 随后 m_f->glViewport(...) 空指针解引用,
    //   表现为进程在 QML 加载完成后立刻 0xC0000005 崩溃。
    if (!m_ready) {
        initialize();
        if (!m_ready)
            return;
    }

    // ★★ 记下 Qt Quick 当前绑定的 FBO —— 合成阶段必须显式绑回去。
    //    它不是 0: QQuickFramebufferObject 有自己的 FBO, 场景先画到我们的
    //    HDR 缓冲, 最后由 composite() 输出到 Qt 的 FBO。
    //    若直接用 bindDefault()(=0), 画面会跑到屏幕默认缓冲, 随后被 QML
    //    的重绘覆盖 —— 表现为 QML 界面里 3D 区域一片空白。
    GLint qtFbo = 0;
    m_f->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &qtFbo);

    // 时间变化 -> 重算星历
    const bool timeChanged = (vs.jd != m_lastJd);
    const bool scaleChanged = (vs.realScale != m_lastRealScale);
    if (timeChanged || scaleChanged) {
        m_scene.setRealScale(vs.realScale);   // 内部会 update()
        m_scene.setJulianDate(vs.jd);
        m_scene.update();
        m_lastJd = vs.jd;
        m_lastRealScale = vs.realScale;
        m_orbitDirty = true;
        // 比例改变时轨道线几何必须重建 (它按坐标烘焙进了 VBO)
        if (scaleChanged) {
            m_orbitMeshes.clear();
            // 小行星带的半径也是按坐标烘焙的, 同样需要重建 ——
            // 否则切到真实比例后带会留在原来的位置, 与行星轨道错开。
            m_belts.rebuild(vs.realScale);
        }
    }

    // 相机同步。
    // ★★ 必须调用 update(dt) 推进阻尼插值 ★★
    //    只 setTarget/setDistance 而不 update 的话, smooth_* 永远停在初值,
    //    表现为「切换焦点后镜头纹丝不动」。Python 版踩过完全相同的坑
    //    (那里是漏了 self.cam.update(dt), 聚焦地球后相机仍停在 1133 单位外)。
    if (vs.snap)
        m_camera.snapToTarget();
    m_camera.setOrbit(vs.camTheta, vs.camPhi);
    m_camera.setTarget(vs.camTarget);
    m_camera.setDistance(vs.camDist);
    m_camera.setFov(vs.fov);

    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        double dt = double(now - m_lastFrameMs) / 1000.0;
        m_lastFrameMs = now;
        if (dt <= 0.0 || dt > 0.5)
            dt = 1.0 / 60.0;
        m_camera.update(dt);
    }

    const float aspect = float(m_w) / float(qMax(1, m_h));
    const QMatrix4x4 viewProj = m_camera.viewProjection(aspect);

    // ---- 阶段 1: 绑定 HDR 场景缓冲 ----
    if (m_postfx.ready()) {
        m_postfx.bindScene();
    } else {
        // 后处理不可用时的退化路径: 直接画到目标 FBO (会缺 gamma/tonemap)
        m_f->glViewport(0, 0, m_w, m_h);
        m_f->glClearColor(0.004f, 0.006f, 0.014f, 1.0f);
        m_f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    const float timeSec =
        float(QDateTime::currentMSecsSinceEpoch() % 1000000) / 1000.0f;

    // =======================================================================
    //  银河系尺度: 完全独立的绘制流程
    //
    //  不画天空盒 (银盘自己就是星场, 再叠一层地球夜空的星星会显得脏),
    //  不画行星与轨道 —— 在这个尺度上太阳系只是一个点。
    // =======================================================================
    // =======================================================================
    //  宇宙尺度: 本星系群 → 星系团 → 超星系团 → 大尺度结构
    //
    //  与银河系视图一样, 不画天空盒 (宇宙尺度上"星座"毫无意义),
    //  也不画行星 —— 在这个尺度上整个银河系只是一个光点。
    // =======================================================================
    if (vs.scale == SceneScale::Cosmos) {
        const float halfFovC = float(vs.fov) * 0.5f * float(M_PI) / 180.0f;
        const float pScaleC =
            float(m_h) * 0.5f / qMax(std::tan(halfFovC), 1e-4f);

        m_f->glDisable(GL_DEPTH_TEST);
        m_f->glDisable(GL_CULL_FACE);

        // ★ 应用可见粒子数 (性能开关)。零成本 —— 只改 draw count。
        m_cosmos.setVisibleCount(vs.cosmosVisible);

        // ★ 应用距离映射模式。同样零成本 —— 映射在着色器里做,
        //   顶点缓冲存的是"方向 + 真实距离"这样的物理量, 无需重建。
        m_cosmos.setMapMode(vs.cosmosMapMode);
        m_cosmos.setSdssVisible(vs.sdssVisible);

        if (perfEnabled())
            m_f->glFinish();
        QElapsedTimer perfClock;
        if (perfEnabled())
            perfClock.start();
        m_cosmos.render(viewProj, pScaleC);
        if (perfEnabled()) {
            m_f->glFinish();
            static PerfAccum acc;
            acc.add(double(perfClock.nsecsElapsed()) / 1.0e6);
        }

        if (!m_postfx.ready())
            return;
        m_postfx.renderBloom();
        m_postfx.composite(GLuint(qtFbo), 0.5f, 0.5f, false, timeSec);
        return;
    }

    if (vs.scale == SceneScale::Galaxy) {
        // 点精灵的透视缩放系数: 视口高度 / (2·tan(fov/2))
        // 使 gl_PointSize = size · uPixelScale / w 得到正确的世界尺寸投影
        const float halfFov = float(vs.fov) * 0.5f * float(M_PI) / 180.0f;
        const float pointScale =
            float(m_h) * 0.5f / qMax(std::tan(halfFov), 1e-4f);

        m_f->glDisable(GL_DEPTH_TEST);
        m_f->glDisable(GL_CULL_FACE);

        // ★ 先画参考底图 (半透明), 再画粒子 —— 粒子在上层, 保住体积感。
        drawGalaxyOverlay(viewProj, vs);
        if (perfEnabled())
            m_f->glFinish();
        QElapsedTimer perfClock;
        if (perfEnabled())
            perfClock.start();
        m_galaxy.render(viewProj, pointScale);
        if (perfEnabled()) {
            m_f->glFinish();
            static PerfAccum accG;
            accG.add(double(perfClock.nsecsElapsed()) / 1.0e6);
        }

        if (!m_postfx.ready())
            return;
        m_postfx.renderBloom();
        // 银河系视图没有太阳屏幕位置, 光斑传不可见
        m_postfx.composite(GLuint(qtFbo), 0.5f, 0.5f, false, timeSec);
        return;
    }

    // =======================================================================
    //  演化尺度 (v1.3): 星场底 + 112 条目分类星团 (单 draw call 点精灵)
    //  不画轨道/行星/带/彗尾 —— 演化视图只呈现"有哪些天体、在哪类"。
    //  Canvas 示意 (曲线/时间轴) 由 EvoOverlay 保留为第二层。
    // =======================================================================
    if (vs.scale == SceneScale::Evolution) {
        const float halfFovE = float(vs.fov) * 0.5f * float(M_PI) / 180.0f;
        const float pointScaleE =
            float(m_h) * 0.5f / qMax(std::tan(halfFovE), 1e-4f);

        m_f->glDisable(GL_CULL_FACE);

        // ★ 主星球写深度, 粒子做深度测试 —— 主星正确遮挡身后粒子。
        m_f->glEnable(GL_DEPTH_TEST);
        m_f->glDepthFunc(GL_LEQUAL);
        drawEvoStar(vs, viewProj);
        // ★ A批通用模拟体 (sn/merger/agn/binary): 与主星球同深度策略。
        drawEvoSim(vs, viewProj);
        m_evoStars.render(viewProj, pointScaleE);

        if (!m_postfx.ready())
            return;
        m_postfx.renderBloom();
        m_postfx.composite(GLuint(qtFbo), 0.5f, 0.5f, false, timeSec);
        return;
    }

    drawSkybox(vs);

    m_f->glEnable(GL_DEPTH_TEST);
    m_f->glDepthFunc(GL_LEQUAL);

    if (vs.showOrbits)
        drawOrbits(vs, viewProj);

    // 彗尾: 在行星之前画 (加法混合 + 不写深度), 让行星正常遮挡它
    if (vs.showBelts) {
        QVector<CometTail> tails;
        const QVector3D sunP = m_scene.sunPosition();
        for (const SceneItem &it : m_scene.items()) {
            if (!it.isComet || it.tailLength <= 0.0f)
                continue;
            CometTail ct;
            ct.nucleus    = it.center;
            ct.antiSun    = (it.center - sunP);
            if (ct.antiSun.lengthSquared() < 1e-9f)
                continue;                     // 恰好落在太阳上, 跳过
            ct.antiSun.normalize();
            ct.velocity   = it.velocityDir;
            ct.length     = it.tailLength;
            ct.brightness = it.tailBright;
            tails.append(ct);
        }
        if (!tails.isEmpty())
            m_comets.render(viewProj, m_camera.eye(), tails);
    }

    // 小行星带 / 柯伊伯带 / 特洛伊群。
    // 画在行星之前: 它们禁用深度测试且用 alpha 混合, 随后画的行星会
    // 正常遮挡它们 —— 这正是想要的层次关系。
    if (vs.showBelts) {
        const float beltHalfFov = float(vs.fov) * 0.5f * float(M_PI) / 180.0f;
        const float beltPointScale =
            float(m_h) * 0.5f / qMax(std::tan(beltHalfFov), 1e-4f);
        m_belts.render(viewProj, vs.jd - J2000, beltPointScale);
    }

    m_f->glEnable(GL_CULL_FACE);
    m_f->glCullFace(GL_BACK);
    drawBodies(vs, viewProj);
    m_f->glDisable(GL_CULL_FACE);

    if (vs.showRings)
        drawRings(vs, viewProj);

    if (vs.showAtmo)
        drawAtmospheres(vs, viewProj);

    m_f->glDisable(GL_DEPTH_TEST);

    if (!m_postfx.ready())
        return;

    // ---- 阶段 2: Bloom 链 ----
    m_postfx.renderBloom();

    // ---- 阶段 3: 合成 (ACES + 暗角 + 光斑 + 颗粒) ----
    projectSun(viewProj, m_scene.sunPosition(), m_sunScreenX, m_sunScreenY, m_sunVisible);
    m_postfx.composite(GLuint(qtFbo), m_sunScreenX, m_sunScreenY, m_sunVisible, timeSec);
}

// ---------------------------------------------------------------------------
//  星空
// ---------------------------------------------------------------------------




// ---------------------------------------------------------------------------
//  画出银河系参考底图 (半透明)
//
//  ★ 尺寸: 图像 2000x2000 px, 68 ly/px, 故覆盖 ±68,000 ly。
//    场景单位换算 528.5 ly/单位 -> 半宽约 128.7 单位。
//  ★ 透明度按相机俯角调: 俯视时 0.42, 侧视时归零。
//    (侧视时盘面退化成线, 底图没有意义, 留着反而像一块贴纸)
// ---------------------------------------------------------------------------
void SceneRenderer::drawGalaxyOverlay(const QMatrix4x4 &viewProj,
                                      const ViewState &vs)
{
    if (!m_galOverlayProg || !m_galOverlayTex)
        return;

    // ★ 相机俯角由 ViewState::camPhi 直接给出 —— 它本身就是
    //   "自 +Y 起算的极角", 俯视时为 0, 平视时为 π/2。
    //   底图在俯视时最有用 (能看全盘面结构), 平视时盘面退化成线,
    //   此时淡出, 否则会像一块贴在侧面的纸片。
    const double phi = vs.camPhi;                       // 0=俯视, π/2=平视
    const double t = 1.0 - qBound(0.0, (phi - 0.22) / 0.55, 1.0);
    const float alpha = float(0.42 * t);

    // 半宽: 2000px * 68 ly/px / 2 = 68000 ly, 再换场景单位
    const float halfSize = float(68000.0 / 528.5);
    if (alpha < 0.005f)
        return;

    m_f->glEnable(GL_BLEND);
    m_f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_f->glDisable(GL_DEPTH_TEST);

    m_galOverlayProg->bind();
    m_galOverlayProg->setUniformValue("uViewProj", viewProj);
    m_galOverlayProg->setUniformValue("uHalfSize", halfSize);
    m_galOverlayProg->setUniformValue("uAlpha", alpha);

    m_f->glActiveTexture(GL_TEXTURE0);
    m_galOverlayTex->bind();
    m_galOverlayProg->setUniformValue("uTex", 0);

    m_galOverlayVao.bind();
    m_f->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_galOverlayVao.release();
    m_galOverlayProg->release();
}

void SceneRenderer::drawSkybox(const ViewState &vs)
{
    if (!m_sky)
        return;

    m_f->glDisable(GL_DEPTH_TEST);
    m_f->glDepthMask(GL_FALSE);

    const float aspect = float(m_w) / float(qMax(1, m_h));
    const QMatrix4x4 vp = m_camera.projection(aspect) * m_camera.view();

    m_sky->bind();
    m_sky->setUniformValue("uInvViewProj", vp.inverted());
    m_sky->setUniformValue("uCamPos", m_camera.eye());
    // ★ 星空输出的是**线性**辐射值, 曝光统一交给后处理的 composite(),
    //   这里必须是 1.0。若在此处再乘一次曝光, 会与 ACES 前的 uExposure
    //   叠加, 星空会亮到过曝。
    m_sky->setUniformValue("uExposure", 1.0f);

    QOpenGLTexture *mw = m_tex.milkyWay();
    m_sky->setUniformValue("uHasMilkyWay", mw ? 1.0f : 0.0f);
    if (mw) {
        m_f->glActiveTexture(GL_TEXTURE0);
        mw->bind();
        m_sky->setUniformValue("uMilkyWay", 0);
    }

    m_quad->draw();
    m_sky->release();

    m_f->glDepthMask(GL_TRUE);
}

// ---------------------------------------------------------------------------
//  轨道线
// ---------------------------------------------------------------------------

void SceneRenderer::drawOrbits(const ViewState &vs, const QMatrix4x4 &viewProj)
{
    if (!m_orbit)
        return;

    m_f->glEnable(GL_BLEND);
    m_f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_f->glDepthMask(GL_FALSE);

    m_orbit->bind();
    m_orbit->setUniformValue("uViewProj", viewProj);
    m_orbit->setUniformValue("uCamPos", m_camera.eye());
    m_orbit->setUniformValue("uOpacity", 0.42f);
    m_orbit->setUniformValue("uColor", QVector3D(0.42f, 0.56f, 0.78f));

    for (const SceneItem &it : m_scene.items()) {
        if (!it.body || !it.hasOrbit)
            continue;

        const QString id = QString::fromUtf8(it.body->id);
        if (!m_orbitMeshes.contains(id)) {
            // 轨道线用「此刻」的瞬时根数, 每帧不重算 —— 只有时间改变才重建
            QVector<QVector3D> pts;
            eph::sampleOrbit(it.body->id, vs.jd, 512, pts);
            for (QVector3D &p : pts)
                p = sceneconst::orbitToScene(p, vs.realScale);
            m_orbitMeshes.insert(id, geom::makeLineStrip(pts));
        }

        Mesh *line = m_orbitMeshes.value(id);
        if (line)
            line->drawLines();
    }

    m_orbit->release();
    m_f->glDepthMask(GL_TRUE);
    m_f->glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------------
//  天体本体
// ---------------------------------------------------------------------------

void SceneRenderer::drawBodies(const ViewState &vs, const QMatrix4x4 &viewProj)
{
    if (!m_planet)
        return;

    const QVector3D sunPos = m_scene.sunPosition();

    m_planet->bind();
    m_planet->setUniformValue("uViewProj", viewProj);
    m_planet->setUniformValue("uCamPos", m_camera.eye());
    m_planet->setUniformValue("uSunPos", sunPos);
    // ★ 法线强度对「真实照片贴图」要很克制。
    //
    //   原因: 照片本身已经把光照烘焙进了明暗 (照片里环形山的亮面暗面就是
    //   当时的光照结果)。再叠加法线凹凸, 等于把同一份明暗**算了两遍**,
    //   结果是月球像一枚浮雕硬币而不是球体。
    //
    //   旧值 0.65 是针对程序化贴图调的 —— 那种贴图基本是平涂, 没有烘焙
    //   光照, 需要法线来提供立体感。换成真实影像后必须大幅降低。
    //   0.28 只保留一点微起伏, 让不同光照角度下有质感, 而不再喧宾夺主。
    //   SS_NORMAL 环境变量可临时覆盖, 便于标定。
    static const float kNormalScale =
        qEnvironmentVariableIsSet("SS_NORMAL")
            ? qgetenv("SS_NORMAL").toFloat()
            : 0.28f;
    m_planet->setUniformValue("uNormalScale", kNormalScale);

    // 屏幕空间最小尺寸 (仅真实比例模式启用)。
    // 像素半径下限取 3px —— 足够看清是个球, 又不会大到误导尺度感。
    {
        const float halfFov = float(vs.fov) * 0.5f * float(M_PI) / 180.0f;
        const float projScale = float(m_h) / (2.0f * qMax(std::tan(halfFov), 1e-4f));
        m_planet->setUniformValue("uProjScale", projScale);
        m_planet->setUniformValue("uMinPixelR", vs.realScale ? 3.0f : 0.0f);
    }

    for (const SceneItem &it : m_scene.items()) {
        const BodyData *b = it.body;
        if (!b)
            continue;

        const bool isStar = b->kind && std::strcmp(b->kind, "star") == 0;

        // 屏幕空间最小尺寸需要天体的世界中心与**未放大**的真实半径。
        // 不能用 it.radius —— 真实比例下它已被 kMinVisibleUnits 抬过,
        // 拿它算放大倍率会重复放大。直接用原始 km 值换算。
        m_planet->setUniformValue("uBodyCenter", it.center);
        m_planet->setUniformValue("uBodyRadius",
            float(b->radiusKm / sceneconst::kSceneUnitKm));

        // ---- 光照参数 (与 Python 版逐值对齐) ----
        //
        // ★ 这里的光照强度刻意超过 1.0 —— 因为后面有 HDR + ACES 后处理链。
        //   超亮部分由 ACES 平滑滚降, 溢出的能量进入 Bloom 形成耀光,
        //   这才是「行星有体积感、太阳有光芒」的来源。
        //   如果把强度压到 1.0 以下来适应非 HDR 管线, 结果就是灰蒙蒙的
        //   塑料球 —— 早期版本正是如此。
        if (isStar) {
            // 太阳: 不接收其它光源, 用高倍环境项代替光照 (它自己就是光源)。
            //
            // 2.0 的取值同样按 ACES 反解: 太阳纹理中心线性 albedo ≈ 0.89,
            // 目标是让它落在 ACES 滚降段而非饱和段 —— 得到**金白色带层次**
            // 的球体, 而不是一张纯白圆盘。取值 3.4 时中心 23% 像素是 255
            // 的纯白, 边缘与中心的色阶全丢, 太阳看着像剪纸。
            // 太阳的"光芒"由 Bloom(阈值 1.25) 单独负责, 不靠本体亮度硬顶。
            m_planet->setUniformValue("uSunColor", QVector3D(1.0f, 0.93f, 0.76f));
            m_planet->setUniformValue("uSunIntensity", 0.0f);
            m_planet->setUniformValue("uAmbientColor", QVector3D(1.0f, 0.95f, 0.84f));
            m_planet->setUniformValue("uAmbientStrength", 2.0f);
        } else {
            // 行星: 光照随日心距平方反比衰减; 外行星给下限避免全黑
            //
            // ★ 上限的选择有明确依据, 不是随手取的:
            //   ACES 近似在 x > 3 之后基本饱和 (输出 >0.95), 经 gamma 后即
            //   纯白。而最亮的纹理像素(地球云层)线性 albedo 达 0.84,
            //   故 x_max = 0.84 * I * 1.25。要让 x_max 落在 ACES 中段
            //   (约 2.0, 对应屏幕 245 左右 —— 亮而不溢出的"通透"白),
            //   解得 I ≈ 1.45 (1 AU 处的上限)。
            //   还须叠加 Bloom 的回灌: 亮部被提取模糊后又加回本区域,
            //   白云区 HDR 会再抬高约 0.1, 故上限要留出这段余量。
            //   注意: 土星/木星等外行星的强度早被 0.30 下限钳住,
            //   改上限**不影响**它们, 只作用于地球与火星。
            const double au = qMax(it.heliocentricAu, 0.05);
            const float intensity = float(qMin(1.45, 1.45 / std::pow(au, 1.15)));
            m_planet->setUniformValue("uSunColor", QVector3D(1.0f, 0.94f, 0.80f));
            m_planet->setUniformValue("uSunIntensity", qMax(intensity, 0.30f));
            m_planet->setUniformValue("uAmbientColor", QVector3D(0.14f, 0.19f, 0.28f));  // 冷色补光
            m_planet->setUniformValue("uAmbientStrength", 0.55f);
        }

        m_planet->setUniformValue("uModel", it.modelMatrix());

        // ---- 纹理文件名推导 ----
        // ★ stem 必须与 Python 版一致: 那边是**按天体 id 推导**缓存文件名
        //   (core/textures.py: 'albedo_%s' % body.id), 而 data.py 里只有
        //   9 大行星填了 texture 字段 —— 太阳与全部卫星都是 None。
        //   早期这里直接用 b->texture, 于是那些天体一个纹理都拿不到,
        //   全部退化成单色球: 月球表面看不到环形山与月海, 太阳也没有
        //   米粒组织。凡是有对应 PNG 的就该用上, 缺失时 get() 返回
        //   nullptr, 自然回退到 uBaseColor。
        const QString stem = (b->texture && *b->texture)
                                 ? QString::fromUtf8(b->texture)
                                 : QString::fromUtf8(b->id);
        QOpenGLTexture *alb = m_tex.get(QStringLiteral("albedo"), stem);
        QOpenGLTexture *nrm = m_tex.get(QStringLiteral("normal"), stem);

        m_planet->setUniformValue("uHasTexture", alb ? 1.0f : 0.0f);
        m_planet->setUniformValue("uHasNormalMap", nrm ? 1.0f : 0.0f);

        // ---- NoData (未测绘区) 处理 ----
        //
        // ★ 部分天体的贴图是**部分覆盖**的航天器影像, 未拍摄区域在图上
        //   是纯黑。直接采样会在球面上留下一块"被啃掉"的黑斑
        //   (实测 Voyager 2 的天王星卫星黑区占 57~62%)。
        //
        // ★ 不补全的理由: 那是**编造观测数据** —— 那些区域根本没人拍过。
        //   实测各种延拓/插值都会产生明显伪影 (纵向条纹/横向拉丝),
        //   比黑斑更糟。
        //
        // ★ 做法: 在着色器里把近黑像素替换为**按实测反照率着色的底色**,
        //   视觉上承认"此处无数据", 但球体保持完整。
        const float hasNoData =
            m_tex.hasNoData(QStringLiteral("albedo"), stem) ? 1.0f : 0.0f;
        m_planet->setUniformValue("uHasNoData", hasNoData);
        m_planet->setUniformValue("uBaseColor",
                                  QVector3D(b->color[0], b->color[1], b->color[2]));
        // 未测绘区用天体基色 (它由实测反照率标定)
        m_planet->setUniformValue("uNoDataColor",
                                  QVector3D(b->color[0], b->color[1], b->color[2]));

        if (alb) {
            m_f->glActiveTexture(GL_TEXTURE0);
            alb->bind();
            m_planet->setUniformValue("uAlbedo", 0);
        }
        if (nrm) {
            m_f->glActiveTexture(GL_TEXTURE1);
            nrm->bind();
            m_planet->setUniformValue("uNormalMap", 1);
        }

        // 恒星的自发光已由上面的高倍环境项实现, 这里不再叠加第二项
        m_planet->setUniformValue("uEmissive", 0.0f);
        m_planet->setUniformValue("uAtmoRim", !isStar && b->hasAtmo
                                                ? float(b->atmoOpacity * 0.30) : 0.0f);
        m_planet->setUniformValue("uAtmoColor",
            b->hasAtmo ? QVector3D(b->atmoColor[0], b->atmoColor[1], b->atmoColor[2])
                       : QVector3D(0.35f, 0.65f, 1.0f));

        m_sphere->draw();
    }

    m_planet->release();
}

// ---------------------------------------------------------------------------
//  环系
// ---------------------------------------------------------------------------

void SceneRenderer::drawRings(const ViewState &vs, const QMatrix4x4 &viewProj)
{
    if (!m_ring)
        return;

    const QVector3D sunPos = m_scene.sunPosition();

    m_f->glEnable(GL_BLEND);
    m_f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // 环是薄片, 双面都要可见
    m_f->glDisable(GL_CULL_FACE);

    m_ring->bind();
    m_ring->setUniformValue("uViewProj", viewProj);
    m_ring->setUniformValue("uCamPos", m_camera.eye());
    m_ring->setUniformValue("uSunPos", sunPos);
    m_ring->setUniformValue("uSunIntensity", 1.35f);
    m_ring->setUniformValue("uAmbient", 0.16f);

    for (const SceneItem &it : m_scene.items()) {
        const BodyData *b = it.body;
        if (!b || !b->hasRings)
            continue;

        const QString id = QString::fromUtf8(b->id);
        Mesh *ring = m_ringMeshes.value(id);
        if (!ring)
            continue;

        m_ring->setUniformValue("uModel", it.modelMatrix());
        m_ring->setUniformValue("uPlanetPos", it.center);
        m_ring->setUniformValue("uPlanetRadius", it.radius);
        m_ring->setUniformValue("uRingColor",
            QVector4D(b->ringColor[0], b->ringColor[1], b->ringColor[2],
                      float(b->ringOpacity)));

        QOpenGLTexture *rt = nullptr;
        if (b->texture && *b->texture)
            rt = m_tex.get(QStringLiteral("ring"), QString::fromUtf8(b->texture));
        m_ring->setUniformValue("uHasTexture", rt ? 1.0f : 0.0f);
        if (rt) {
            m_f->glActiveTexture(GL_TEXTURE0);
            rt->bind();
            m_ring->setUniformValue("uRingTex", 0);
        }

        ring->draw();
    }

    m_ring->release();
    m_f->glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------------
//  大气壳 (加法混合的后向球)
// ---------------------------------------------------------------------------

void SceneRenderer::drawAtmospheres(const ViewState &vs, const QMatrix4x4 &viewProj)
{
    if (!m_atmo)
        return;

    const QVector3D sunPos = m_scene.sunPosition();

    m_f->glEnable(GL_BLEND);
    m_f->glBlendFunc(GL_SRC_ALPHA, GL_ONE);          // 加法混合, 像辉光
    m_f->glDisable(GL_CULL_FACE);                     // 画背面壳
    m_f->glDepthMask(GL_FALSE);

    m_atmo->bind();
    m_atmo->setUniformValue("uViewProj", viewProj);
    m_atmo->setUniformValue("uCamPos", m_camera.eye());
    m_atmo->setUniformValue("uSunPos", sunPos);

    for (const SceneItem &it : m_scene.items()) {
        const BodyData *b = it.body;
        if (!b || !b->hasAtmo)
            continue;
        if (b->kind && std::strcmp(b->kind, "star") == 0)
            continue;

        // 壳比本体略大
        QMatrix4x4 m = it.modelMatrix();
        m.scale(1.0f + float(b->atmoOpacity) * 0.045f);

        m_atmo->setUniformValue("uModel", m);
        m_atmo->setUniformValue("uAtmoColor",
            QVector3D(b->atmoColor[0], b->atmoColor[1], b->atmoColor[2]));
        m_atmo->setUniformValue("uOpacity", float(b->atmoOpacity) * 0.42f);
        m_atmo->setUniformValue("uPower", float(b->atmoPower));

        m_sphere->draw();
    }

    m_atmo->release();
    m_f->glDepthMask(GL_TRUE);
    m_f->glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------------
//  演化主星 (v1.3): HR 三剧本的 3D 发光球
//
//  复用 m_planet 管线 + m_sphere 网格, 无纹理纯黑体色自发光 ——
//  与太阳同款"高倍环境项代替光照"做法 (见 drawBodies 的 isStar 分支),
//  本体颜色进 ACES + Bloom 后自然带光晕, 无需新着色器。
//  半径映射 rad(Rsun, 0.01~800) -> 场景单位: 1.2·rad^0.35, 钳 0.5~6。
//  黑洞 (rad<=0): 暗球 + 红色临边辉光。
// ---------------------------------------------------------------------------
void SceneRenderer::drawEvoStar(const ViewState &vs, const QMatrix4x4 &viewProj)
{
    if (!m_planet || !m_sphere)
        return;
    if (vs.evoScript < 1 || vs.evoScript > 3)
        return;

    const bool isBH = (vs.evoRad <= 0.0 || vs.evoTeff <= 0.0);
    QVector3D col = EvoStars::starColor(vs.evoTeff);
    if (isBH)
        col = QVector3D(0.02f, 0.02f, 0.03f);

    double dispR = 1.2 * std::pow(std::max(vs.evoRad, 0.01), 0.35);
    dispR = qBound(0.5, dispR, 6.0);
    if (isBH)
        dispR = 0.9;

    const QVector3D hp = EvoStars::heroPos();

    m_planet->bind();
    m_planet->setUniformValue("uViewProj", viewProj);
    m_planet->setUniformValue("uCamPos", m_camera.eye());
    m_planet->setUniformValue("uSunPos", hp);
    m_planet->setUniformValue("uNormalScale", 0.28f);

    const float halfFov = float(vs.fov) * 0.5f * float(M_PI) / 180.0f;
    const float projScale = float(m_h) / (2.0f * qMax(std::tan(halfFov), 1e-4f));
    m_planet->setUniformValue("uProjScale", projScale);
    m_planet->setUniformValue("uMinPixelR", 6.0f);
    m_planet->setUniformValue("uBodyCenter", hp);
    m_planet->setUniformValue("uBodyRadius", float(dispR));

    // 自发光: 不接收光源, 高倍环境项即本体 (太阳同款)
    m_planet->setUniformValue("uSunColor", QVector3D(1.0f, 0.93f, 0.76f));
    m_planet->setUniformValue("uSunIntensity", 0.0f);
    m_planet->setUniformValue("uAmbientColor", col);
    m_planet->setUniformValue("uAmbientStrength", isBH ? 0.6f : 2.2f);

    QMatrix4x4 m;
    m.translate(hp);
    m.scale(float(dispR));
    m_planet->setUniformValue("uModel", m);

    m_planet->setUniformValue("uHasTexture", 0.0f);
    m_planet->setUniformValue("uHasNormalMap", 0.0f);
    m_planet->setUniformValue("uHasNoData", 0.0f);
    m_planet->setUniformValue("uBaseColor", col);
    m_planet->setUniformValue("uNoDataColor", col);
    m_planet->setUniformValue("uEmissive", 0.0f);
    // 临边辉光: 恒星同色弱辉光, 黑洞红色强辉光 (吸积 hint)
    m_planet->setUniformValue("uAtmoRim", isBH ? 1.2f : 0.35f);
    m_planet->setUniformValue("uAtmoColor",
        isBH ? QVector3D(1.0f, 0.35f, 0.30f) : col);

    m_sphere->draw();
    m_planet->release();
}

// ---------------------------------------------------------------------------
//  演化通用模拟体 (v1.3 A/B/C批): 非HR剧本的3D形态
//
//  图元只有两种, 全复用 m_planet + m_sphere, 零新着色器:
//    evoBall  发光球 (高倍环境项自发光, 无纹理纯色, 进 ACES + Bloom 带光晕)
//    evoCone  锥/柱 (m_sphere Z向拉伸, 喷流/外向流/灯塔束)
//
//  ★ 为什么没有"光晕壳": 曾用 m_atmo 背面壳 (加法混合) 实现激波/包层,
//    但实测在演化分支**无任何输出** (同一 m_atmo 在太阳系分支正常,
//    几何/参数均验证无误, 根因未定位)。已全部改用 evoBall 大暗球近似:
//    半径1.6x、低环境倍率 —— 视觉即光晕, 且零风险。
//
//  调用方 (Evolution 分支) 的 GL 状态: CULL_FACE 关 + DEPTH LEQUAL。
//  evoBall/evoCone 均走 m_planet (opaque, 写深度), 兼容。
// ---------------------------------------------------------------------------
namespace {

// 发光球: 中心 hp, 半径 r, 颜色 col(线性), 环境倍率 amb
static void evoBall(QOpenGLShaderProgram *prog, QOpenGLFunctions_3_3_Core *f,
                    Mesh *sphere, const QMatrix4x4 &viewProj,
                    const QVector3D &eye, float projScale,
                    const QVector3D &hp, float r, const QVector3D &col,
                    float amb, float rim, const QVector3D &rimCol)
{
    prog->bind();
    prog->setUniformValue("uViewProj", viewProj);
    prog->setUniformValue("uCamPos", eye);
    prog->setUniformValue("uSunPos", hp);
    prog->setUniformValue("uNormalScale", 0.28f);
    prog->setUniformValue("uProjScale", projScale);
    prog->setUniformValue("uMinPixelR", 4.0f);
    prog->setUniformValue("uBodyCenter", hp);
    prog->setUniformValue("uBodyRadius", r);
    prog->setUniformValue("uSunColor", QVector3D(1.0f, 0.93f, 0.76f));
    prog->setUniformValue("uSunIntensity", 0.0f);
    prog->setUniformValue("uAmbientColor", col);
    prog->setUniformValue("uAmbientStrength", amb);
    QMatrix4x4 m;
    m.translate(hp);
    m.scale(r);
    prog->setUniformValue("uModel", m);
    prog->setUniformValue("uHasTexture", 0.0f);
    prog->setUniformValue("uHasNormalMap", 0.0f);
    prog->setUniformValue("uHasNoData", 0.0f);
    prog->setUniformValue("uBaseColor", col);
    prog->setUniformValue("uNoDataColor", col);
    prog->setUniformValue("uEmissive", 0.0f);
    prog->setUniformValue("uAtmoRim", rim);
    prog->setUniformValue("uAtmoColor", rimCol);
    sphere->draw();
    prog->release();
    Q_UNUSED(f);
}

// 锥/柱: 中心 hp, 朝向 dir(单位), 长度 len, 半径 rad, 颜色 col
// 用 m_sphere Z拉伸近似: 缩放 (rad, rad, len/2), 朝向经旋转对齐 +Z->dir。
static void evoCone(QOpenGLShaderProgram *prog, QOpenGLFunctions_3_3_Core *f,
                    Mesh *sphere, const QMatrix4x4 &viewProj,
                    const QVector3D &eye, float projScale,
                    const QVector3D &hp, const QVector3D &dir,
                    float len, float rad, const QVector3D &col, float amb)
{
    QVector3D d = dir.normalized();
    QMatrix4x4 m;
    m.translate(hp + d * (len * 0.5f));
    // +Z 对齐到 d
    const QVector3D z(0.0f, 0.0f, 1.0f);
    const float cosA = qBound(-1.0f, QVector3D::dotProduct(z, d), 1.0f);
    if (cosA < 0.9999f) {
        if (cosA < -0.9999f) {
            m.rotate(180.0f, 1.0f, 0.0f, 0.0f);
        } else {
            const QVector3D ax = QVector3D::crossProduct(z, d).normalized();
            m.rotate(qRadiansToDegrees(std::acos(cosA)), ax);
        }
    }
    m.scale(rad, rad, len * 0.5f);
    prog->bind();
    prog->setUniformValue("uViewProj", viewProj);
    prog->setUniformValue("uCamPos", eye);
    prog->setUniformValue("uSunPos", hp);
    prog->setUniformValue("uNormalScale", 0.28f);
    prog->setUniformValue("uProjScale", projScale);
    prog->setUniformValue("uMinPixelR", 0.0f);
    prog->setUniformValue("uBodyCenter", hp);
    prog->setUniformValue("uBodyRadius", rad);
    prog->setUniformValue("uSunColor", QVector3D(1.0f, 0.93f, 0.76f));
    prog->setUniformValue("uSunIntensity", 0.0f);
    prog->setUniformValue("uAmbientColor", col);
    prog->setUniformValue("uAmbientStrength", amb);
    prog->setUniformValue("uModel", m);
    prog->setUniformValue("uHasTexture", 0.0f);
    prog->setUniformValue("uHasNormalMap", 0.0f);
    prog->setUniformValue("uHasNoData", 0.0f);
    prog->setUniformValue("uBaseColor", col);
    prog->setUniformValue("uNoDataColor", col);
    prog->setUniformValue("uEmissive", 0.0f);
    prog->setUniformValue("uAtmoRim", 0.0f);
    prog->setUniformValue("uAtmoColor", col);
    sphere->draw();
    prog->release();
    Q_UNUSED(f);
}

} // namespace

void SceneRenderer::drawEvoSim(const ViewState &vs, const QMatrix4x4 &viewProj)
{
    if (!m_planet || !m_sphere)
        return;
    const int viz = vs.evoViz;
    if (viz < 4 || viz > 13)
        return;
    const float n = float(qBound(0.0, vs.evoP1, 1.0));

    const float halfFov = float(vs.fov) * 0.5f * float(M_PI) / 180.0f;
    const float projScale = float(m_h) / (2.0f * qMax(std::tan(halfFov), 1e-4f));
    const QVector3D eye = m_camera.eye();
    const QVector3D hp = EvoStars::heroPos();

    switch (viz) {
    // ---- A批 ----
    case 4: { // sn: 单一光球 —— 半径随膨胀、颜色随降温、亮度随光变
        //
        //  ★ 为什么只有一个球 (2026-10-08 实测): 曾加"外层激波壳"作为
        //    第二球, 但 evoBall 走 m_planet 为 opaque 管线 —— 外层大球
        //    必然遮挡内层亮球, 画面只剩暗红外壳。发光体的**颜色/尺寸/
        //    亮度本身**已足够表达: 早期小且白蓝、峰值大且橙白、
        //    晚期膨胀转暗红 —— 与 Ia 型光变-颜色演化一致。
        const float td = float(qMin(vs.evoP3 / 0.05, 300.0)); // 复原天数
        const float phase = qBound(0.0f, td / 300.0f, 1.0f);
        const float lum = 0.6f + 2.4f * float(std::exp(-n * 2.2)); // 峰值亮
        const float rSn = 0.55f + 1.5f * std::sqrt(phase);          // v·t 膨胀
        // 颜色: 峰值橙白 -> 后期暗红 (光球退入铁核, 颜色转红)
        const float ck = qBound(0.0f, (phase - 0.15f) / 0.85f, 1.0f);
        const QVector3D scol(1.0f, 0.88f - 0.42f * ck, 0.70f - 0.50f * ck);
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp, rSn, scol, lum, 0.5f, scol);
        break;
    }
    case 5: { // merger: 双星系球间距收缩 + 近心点潮汐尾壳
        const float sep = 14.0f * (1.0f - n); // 770kpc->0 映射
        const QVector3D c1 = hp + QVector3D(-sep * 0.5f, 0, 0);
        const QVector3D c2 = hp + QVector3D(sep * 0.5f, 0, 0);
        const QVector3D gcol(0.75f, 0.82f, 0.95f);
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                c1, 2.2f, gcol, 1.4f, 0.2f, gcol);
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                c2, 2.2f, gcol, 1.4f, 0.2f, gcol);
        // 潮汐尾: 近心点 (n~0.5) 最强, 用横向锥近似
        const float tail = float(std::exp(-std::pow((n - 0.5) / 0.16, 2)));
        if (tail > 0.05f) {
            evoCone(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                    c1, QVector3D(-1, 0.4f, 0), 8.0f * tail, 0.7f,
                    QVector3D(0.6f, 0.75f, 1.0f), 0.9f);
            evoCone(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                    c2, QVector3D(1, -0.4f, 0), 8.0f * tail, 0.7f,
                    QVector3D(0.6f, 0.75f, 1.0f), 0.9f);
        }
        break;
    }
    case 6: { // agn: 中央黑球 + 双向喷流锥 + 瓣壳
        const float jetLen = 2.0f + 10.0f * float(std::pow(n, 1.6)); // 0->150kpc
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp, 0.9f, QVector3D(0.02f, 0.02f, 0.03f), 0.6f, 1.2f,
                QVector3D(1.0f, 0.35f, 0.3f));
        const QVector3D jcol(0.55f, 0.85f, 1.0f);
        evoCone(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp, QVector3D(0, 1, 0.15f), jetLen, 0.5f, jcol, 1.6f);
        evoCone(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp, QVector3D(0, -1, -0.15f), jetLen, 0.5f, jcol, 1.6f);
        // ★ 瓣辉光: m_atmo 在演化分支无输出, 改用 m_planet 暗球近似 (同 sn 壳)。
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(0, jetLen * 0.8f, 0),
                2.5f, QVector3D(0.6f, 0.7f, 1.0f), 0.25f, 0.4f,
                QVector3D(0.6f, 0.7f, 1.0f));
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(0, -jetLen * 0.8f, 0),
                2.5f, QVector3D(0.6f, 0.7f, 1.0f), 0.25f, 0.4f,
                QVector3D(0.6f, 0.7f, 1.0f));
        break;
    }
    case 7: { // binary: 双中子星旋进 —— 间距收缩 + 亮度随接近上升
        //
        //  ★ 同样去掉"波纹壳" (opaque 大球会遮住双星本体)。旋进过程用
        //    **间距 + 亮度** 表达: 宽轨道时暗淡, 临近并合时紧贴且炽亮。
        const float t = float(vs.evoP2); // log10(距并合年), 9->-7
        const float f = qBound(0.0f, (9.0f - t) / 16.0f, 1.0f);
        const float sep = 1.2f + 6.0f * float(std::pow(1.0f - f, 1.8));
        const float glow = 1.2f + 2.2f * f;   // 引力波辐射增强 -> 更亮
        const QVector3D ncol(0.82f, 0.88f, 1.0f);
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(-sep * 0.5f, 0, 0), 0.7f, ncol, glow, 0.4f, ncol);
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(sep * 0.5f, 0, 0), 0.7f, ncol, glow, 0.4f, ncol);
        // 并合瞬间 (f->1): 中心爆闪 (短伽马暴/千新星)
        if (f > 0.92f) {
            const float flash = (f - 0.92f) / 0.08f;
            evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                    hp, 1.2f + 1.4f * flash,
                    QVector3D(1.0f, 0.95f, 0.75f), 2.6f * flash, 0.6f,
                    QVector3D(1.0f, 0.9f, 0.6f));
        }
        break;
    }
    // ---- B批 ----
    case 8: { // planet: 中央恒星 + 原行星盘 (扁球) + 3 迁移行星球
        const float t = float(vs.evoP2); // Myr 0..12
        const float gas = float(vs.evoP3); // 气体余量 1->0
        const QVector3D scol(1.0f, 0.93f, 0.75f);
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp, 1.1f, scol, 2.4f, 0.4f, scol);
        // 盘: Z压扁大球 (rxz=9, y=1.2+气体厚度), 随气体耗散变薄变暗
        {
            QMatrix4x4 dm;
            dm.translate(hp);
            dm.scale(9.0f, 0.8f + 1.6f * gas, 9.0f);
            m_planet->bind();
            m_planet->setUniformValue("uViewProj", viewProj);
            m_planet->setUniformValue("uCamPos", eye);
            m_planet->setUniformValue("uSunPos", hp);
            m_planet->setUniformValue("uNormalScale", 0.28f);
            m_planet->setUniformValue("uProjScale", projScale);
            m_planet->setUniformValue("uMinPixelR", 0.0f);
            m_planet->setUniformValue("uBodyCenter", hp);
            m_planet->setUniformValue("uBodyRadius", 9.0f);
            m_planet->setUniformValue("uSunColor", QVector3D(1.0f, 0.93f, 0.76f));
            m_planet->setUniformValue("uSunIntensity", 0.0f);
            const QVector3D dcol(0.55f, 0.62f, 0.78f);
            m_planet->setUniformValue("uAmbientColor", dcol);
            m_planet->setUniformValue("uAmbientStrength", 0.25f + 0.55f * gas);
            m_planet->setUniformValue("uModel", dm);
            m_planet->setUniformValue("uHasTexture", 0.0f);
            m_planet->setUniformValue("uHasNormalMap", 0.0f);
            m_planet->setUniformValue("uHasNoData", 0.0f);
            m_planet->setUniformValue("uBaseColor", dcol);
            m_planet->setUniformValue("uNoDataColor", dcol);
            m_planet->setUniformValue("uEmissive", 0.0f);
            m_planet->setUniformValue("uAtmoRim", 0.0f);
            m_planet->setUniformValue("uAtmoColor", dcol);
            m_sphere->draw();
            m_planet->release();
        }
        // 三行星: 内岩质 (固定) / 气态巨行星 (迁移: 外->内) / 外冰巨星
        //  ★ y 抬高到盘面之上 (1.6~2.0): 盘是不透明扁球, 行星球若与盘
        //    共面会被盘自身遮挡 —— 抬高后"盘上运行的行星"清晰可见。
        const float jx = 6.5f - n * 4.0f; // 迁移 inward
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(-4.5f, 1.8f, 1.5f), 0.45f,
                QVector3D(1.0f, 0.6f, 0.45f), 1.6f, 0.2f,
                QVector3D(1.0f, 0.6f, 0.45f));
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(jx, 1.6f, -1.0f), 0.9f,
                QVector3D(0.9f, 0.72f, 0.42f), 1.8f, 0.25f,
                QVector3D(0.9f, 0.72f, 0.42f));
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(7.5f, 2.0f, 1.0f), 0.6f,
                QVector3D(0.62f, 0.8f, 1.0f), 1.6f, 0.2f,
                QVector3D(0.62f, 0.8f, 1.0f));
        break;
    }
    case 9: { // protostar: 包层主导 —— 暗包层球随耗散收缩 + 双极喷流穿出
        //
        //  ★ 0/I 类原恒星在光学上本就"看不见中心星": 包层主导。因此只画
        //    包层 + 喷流 —— 与观测一致 (IRAS 源在光学/近红外深埋)。
        //  ★ 喷流长度 = 包层半径 + 穿出量, 保证视觉上"穿出"。
        const float env = float(vs.evoP3); // 包层余量 1->0
        const float envR = 3.0f + 8.0f * env;      // 包层 11 -> 3
        const float jetL = envR + 5.0f;            // 始终穿出包层
        const float jetA = 0.4f + 1.4f * env;      // 包层厚时喷流最强
        const QVector3D envCol(0.72f, 0.55f, 0.42f);
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp, envR, envCol, 0.30f + 0.55f * env, 0.35f,
                QVector3D(0.85f, 0.6f, 0.45f));
        const QVector3D jcol(0.55f, 0.78f, 1.0f);
        evoCone(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp, QVector3D(0, 1, 0.1f), jetL, 0.45f, jcol, jetA);
        evoCone(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp, QVector3D(0, -1, -0.1f), jetL, 0.45f, jcol, jetA);
        break;
    }
    case 10: { // remnant: 白矮星小球 + 脉冲星灯塔锥 (旋转)
        const float t = float(vs.evoP2); // logyr 3..10
        const float f = qBound(0.0f, (t - 3.0f) / 7.0f, 1.0f);
        // 白矮星: 随冷却变暗变小
        const float wdB = 2.0f * (1.0f - f * 0.75f);
        const QVector3D wdcol(0.79f, 0.85f, 1.0f);
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(-4.0f, 0, 0), 0.55f, wdcol, wdB, 0.3f, wdcol);
        // 脉冲星: 灯塔锥旋转 (方向随 tickN? 不, 用 p1 进度定相位, 确定性)
        const float ang = n * 6.2831853f * 8.0f; // 转8圈示意自转减慢 (周期随f变长, 此处固定圈数)
        const QVector3D beam(std::cos(ang), 0.25f, std::sin(ang));
        const QVector3D psr(0.85f, 0.9f, 1.0f);
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(4.0f, 0, 0), 0.5f, psr, 2.0f, 0.4f, psr);
        const float beamL = 9.0f * (1.0f - f * 0.6f); // 老年脉冲星熄火变短
        evoCone(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(4.0f, 0, 0), beam, beamL, 0.5f, psr, 1.4f);
        evoCone(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(4.0f, 0, 0),
                QVector3D(-beam.x(), -beam.y(), -beam.z()), beamL, 0.5f, psr, 1.4f);
        break;
    }
    case 11: { // cluster: 束缚星数收缩粒子团 (确定性 Fibonacci 点, 半径随蒸发)
        const float bound = float(vs.evoP3); // 束缚比 1->0.05
        const int N = int(130 * bound);
        // 复用点精灵太重, 用小球阵列示意 (<=130 球, 与行星球同管线, 可接受)
        // 为省 draw call, 只画外层 40 个代表点
        const int M = qMin(N, 40);
        for (int i = 0; i < M; ++i) {
            const float a = float(std::fmod(i * 2.39996, 6.28318530718));
            const float rr = 5.5f * std::sqrt((i + 0.5f) / 40.0f) * (0.4f + 0.6f * bound);
            const QVector3D pos = hp + QVector3D(std::cos(a) * rr, (i % 5 - 2) * 0.5f, std::sin(a) * rr);
            const float b = 0.55f + 0.45f * float((i * 37) % 100) / 100.0f;
            evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                    pos, 0.35f, QVector3D(b * 0.8f, b * 0.85f, b), 1.5f, 0.0f,
                    QVector3D(b, b, b));
        }
        break;
    }
    // ---- C批 ----
    case 12: { // cosmic: 宇宙热历史 —— 中央"可观测宇宙球"随温度变色 + 事件环
        //
        //  ★ 教学要点: 温度史是主线。p2 = log10(t/秒) (QML 传入)。
        //    10^-43s(普朗克) 蓝白极热 -> 1s(核合成) 白热 -> 10^13s(复合)
        //    橙红 -> 10^17s(今天) 深红暗淡。
        //  ★ 颜色用黑体近似: 高温偏蓝白, 冷却转橙红, 今天近暗红。
        const float lg = float(vs.evoP2); // log10(t秒): -43 .. 17.6
        const float u = qBound(0.0f, (lg + 43.0f) / 60.6f, 1.0f); // 归一化
        // 蓝白(0) -> 黄白(0.35) -> 橙(0.7) -> 暗红(1)
        QVector3D ccol;
        if (u < 0.35f) {
            const float k = u / 0.35f;
            ccol = QVector3D(0.75f + 0.25f * k, 0.85f + 0.12f * k, 1.0f - 0.12f * k);
        } else if (u < 0.7f) {
            const float k = (u - 0.35f) / 0.35f;
            ccol = QVector3D(1.0f, 0.97f - 0.22f * k, 0.88f - 0.55f * k);
        } else {
            const float k = (u - 0.7f) / 0.3f;
            ccol = QVector3D(1.0f - 0.45f * k, 0.75f - 0.45f * k, 0.33f - 0.20f * k);
        }
        // 主球: 半径代表可观测宇宙随时间的增长 (早期小, 今天大)
        const float cosR = 1.2f + 5.0f * u;
        const float cosAmb = 2.4f * (1.0f - 0.55f * u); // 早期极亮, 今天暗淡
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp, cosR, ccol, cosAmb, 0.4f, ccol);
        // 事件环: 5 个关键节点 (普朗克/暴胀/核合成/复合/再电离) 沿 X 轴排开,
        // 已过的节点点亮, 未到的暗。让"我们在时间轴的哪里"一目了然。
        {
            static const float kNode[5] = {0.0f, 0.18f, 0.72f, 0.86f, 0.95f};
            for (int i = 0; i < 5; ++i) {
                const float reached = (u >= kNode[i]) ? 1.0f : 0.18f;
                const QVector3D pos = hp + QVector3D((i - 2) * 3.2f, -9.0f, 0.0f);
                evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                        pos, 0.6f, ccol, 1.8f * reached, 0.25f * reached, ccol);
            }
        }
        break;
    }
    case 13: { // ism: 星云认知链 —— 三色代表球 (发射红/反射蓝/暗黑) 轮流点亮
        //
        //  ★ p1 传当前站号 k (0..10) 的归一化值 k/10, 据此决定哪个球亮。
        //  ★ 三种光的教学核心: 发射(红, 氢α) / 反射(蓝, 尘埃散射) /
        //    暗(近黑, 遮光)。球的位置固定不跳, 只切换亮度, 便于对照。
        const float k = n * 10.0f;
        // 发射星云 (左): 红
        const float emOn = (k < 3.5f || k > 7.5f) ? 1.0f : 0.22f;
        // 反射星云 (中): 蓝
        const float reOn = (k >= 3.5f && k < 6.0f) ? 1.0f : 0.22f;
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(-7.5f, 0.5f, 0.0f), 2.2f,
                QVector3D(1.0f, 0.35f, 0.32f), 2.0f * emOn, 0.3f * emOn,
                QVector3D(1.0f, 0.35f, 0.32f));
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(0.0f, 0.2f, 0.0f), 2.0f,
                QVector3D(0.42f, 0.58f, 1.0f), 2.0f * reOn, 0.3f * reOn,
                QVector3D(0.42f, 0.58f, 1.0f));
        // 暗星云 (右): 近黑但**轮廓必须可见** —— 太黑等于没画, 教学上
        //   "暗星云靠遮挡背景星光被认出"这一点需要观众先看见它。
        const float dkOn = (k >= 6.0f && k <= 7.5f) ? 1.0f : 0.4f;
        evoBall(m_planet, m_f, m_sphere, viewProj, eye, projScale,
                hp + QVector3D(7.5f, -0.4f, 0.0f), 2.4f,
                QVector3D(0.20f, 0.21f, 0.26f), 0.9f + 0.7f * dkOn, 0.5f * dkOn,
                QVector3D(0.42f, 0.46f, 0.55f));
        break;
    }
    default:
        break; // 全部 13 剧本已覆盖 (viz 4..13)
    }
}
