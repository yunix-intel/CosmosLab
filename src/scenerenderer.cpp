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

    m_ready = m_sky && m_planet && m_ring && m_orbit && m_atmo
           && m_sphere && m_quad;

    qInfo() << "[渲染器] 初始化" << (m_ready ? "成功" : "失败")
            << " 天体数:" << m_scene.bodyCount();
}

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
    for (int i = 0; i < ALL_BODIES_COUNT; ++i) {
        const BodyData &b = ALL_BODIES[i];
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
        if (scaleChanged)
            m_orbitMeshes.clear();
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
    if (vs.scale == SceneScale::Galaxy) {
        // 点精灵的透视缩放系数: 视口高度 / (2·tan(fov/2))
        // 使 gl_PointSize = size · uPixelScale / w 得到正确的世界尺寸投影
        const float halfFov = float(vs.fov) * 0.5f * float(M_PI) / 180.0f;
        const float pointScale =
            float(m_h) * 0.5f / qMax(std::tan(halfFov), 1e-4f);

        m_f->glDisable(GL_DEPTH_TEST);
        m_f->glDisable(GL_CULL_FACE);
        m_galaxy.render(viewProj, pointScale);

        if (!m_postfx.ready())
            return;
        m_postfx.renderBloom();
        // 银河系视图没有太阳屏幕位置, 光斑传不可见
        m_postfx.composite(GLuint(qtFbo), 0.5f, 0.5f, false, timeSec);
        return;
    }

    drawSkybox(vs);

    m_f->glEnable(GL_DEPTH_TEST);
    m_f->glDepthFunc(GL_LEQUAL);

    if (vs.showOrbits)
        drawOrbits(vs, viewProj);

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
        m_planet->setUniformValue("uBaseColor",
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
