// ============================================================================
//  sceneitem.cpp —— C++ 渲染器与 QML 的桥接实现
// ============================================================================

#include "sceneitem.h"
#include "bodyregistry.h"
#include "celestialdata.h"
#include "ephemeris.h"
#include "galaxydata.h"
#include "cosmos.h"
#include "cosmosdata.h"
#include "galaxyarms.h"
#include "scenerenderer.h"
#include "scene.h"

#include <QDateTime>
#include <QDebug>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>
#include <QTimer>
#include <cmath>

// ---------------------------------------------------------------------------
//  SolarSceneRenderer —— 渲染线程
// ---------------------------------------------------------------------------

SolarSceneRenderer::SolarSceneRenderer()
    : m_renderer(new SceneRenderer)
{
}

SolarSceneRenderer::~SolarSceneRenderer()
{
    delete m_renderer;
}

QOpenGLFramebufferObject *SolarSceneRenderer::createFramebufferObject(const QSize &size)
{    // ★ size 已经是**设备像素** (item 逻辑尺寸 x dpr)。
    //   后面 viewport 直接用这个值, 不能再乘一次 dpr ——
    //   乘了会把 viewport 撑成 FBO 的 4 倍, 只有四分之一画面可见
    //   (现象: 天体被巨幅放大并偏出屏幕)。
    //
    // 分辨率缩放 (性能开关)。
    //   在 2880x1800 (dpr 2) 下, 每个全屏 pass 要写 520 万像素; 后处理链
    //   有亮部提取 + 5 级降采样 + 5 级升采样 + 合成 ≈ 10 个全屏 pass。
    //   对入门独显 (Radeon 550X) 而言这是纯填充率瓶颈。
    //   降采样到 0.7 边长 = 0.49 倍像素量, 帧率几乎翻倍, 而画面在
    //   2880x1800 的屏幕上肉眼差别很小。
    //
    // ★ 必须在 synchronize() 时读走 —— createFramebufferObject 是在
    //   Qt 认为尺寸变化时调用的, 那个时机不一定能安全访问 GUI 线程对象。
    const double s = qBound(0.35, m_renderScale, 1.0);
    QSize fboSize(qMax(2, int(size.width() * s)),
                  qMax(2, int(size.height() * s)));
    m_size = fboSize;

    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::Depth);
    // ★ 不要开 MSAA。
    //   我们的渲染路径是: 场景 -> 自己的 RGBA16F 缓冲 -> 全屏四边形合成到
    //   这个 FBO。也就是说**往这个 FBO 上画的只有一个全屏四边形**, 它的
    //   边缘就是屏幕边缘 —— MSAA 对全屏四边形毫无作用, 却要每帧付一次
    //   4x 多重采样的解析开销 (2880x1800 下不便宜)。
    //   初版这里设了 setSamples(4), 属于纯浪费。
    fmt.setSamples(0);
    return new QOpenGLFramebufferObject(fboSize, fmt);
}

void SolarSceneRenderer::synchronize(QQuickFramebufferObject *item)
{
    // 此刻渲染线程被阻塞, 可以安全读取 GUI 线程侧的状态
    auto *scene = static_cast<SolarScene *>(item);
    m_snapshot = scene->takeSnapshot();
    m_snapshot.valid = true;
    m_renderScale = scene->renderScale();
}

void SolarSceneRenderer::render()
{
    if (!m_snapshot.valid)
        return;

    m_renderer->resize(m_size.width(), m_size.height());

    ViewState vs;
    vs.jd          = m_snapshot.jd;
    vs.camTarget   = m_snapshot.camTarget;
    vs.camDist     = m_snapshot.camDist;
    vs.camTheta    = m_snapshot.camTheta;
    vs.camPhi      = m_snapshot.camPhi;
    vs.fov         = m_snapshot.fov;
    // ★ 逐字段拷贝快照 —— 新增字段必须在这里补上, 否则渲染侧永远拿到
    //   结构的默认值。此前漏了 scale, 结果是银河系视图下太阳系的轨道线
    //   与行星照旧绘制 (画面上叠了一圈白色弧线与几个球)。
    vs.scale       = m_snapshot.scale;
    vs.realScale   = m_snapshot.realScale;
    vs.showOrbits  = m_snapshot.showOrbits;
    vs.showBelts   = m_snapshot.showBelts;
    vs.showRings   = m_snapshot.showRings;
    vs.showAtmo    = m_snapshot.showAtmo;
    vs.snap  = m_snapshot.snap;
    vs.cosmosVisible = m_snapshot.cosmosVisible;

    m_renderer->render(vs);
}

// ---------------------------------------------------------------------------
//  SolarScene
// ---------------------------------------------------------------------------

SolarScene::SolarScene(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);

    m_jd = eph::jdFromUnixSec(double(QDateTime::currentSecsSinceEpoch()));

    // 测试用: SS_JD=<儒略日> 直接指定时刻。
    // 必要性: 彗尾只在近日点附近才明显 (哈雷 2026 年在 35 AU 外的远日点,
    // 尾巴几乎不可见 —— 那是对的物理, 但没法用来验证渲染)。
    // 要检查彗尾必须能把时间拨到某个已知的过近日点时刻。
    if (qEnvironmentVariableIsSet("SS_JD"))
        m_jd = qgetenv("SS_JD").toDouble();

    m_lastTickMs = QDateTime::currentMSecsSinceEpoch();

    // 渲染分辨率自适应。
    //
    // 后处理链每帧要过约 10 个全屏 pass (亮部提取 + 5 级降采样 +
    // 5 级升采样 + 合成), 填充率是主要瓶颈。在 dpr=2 的 2880x1800 屏上
    // 单帧要写 5000 万像素, 入门独显会掉到 20 FPS 左右。
    //
    // 高 DPI 屏降一档到 0.72 边长 (= 0.52 倍像素量) 后帧率明显改善,
    // 而因为物理像素密度本就高, 观感差别很小 —— 这也是游戏里常见的
    // "渲染分辨率 vs 显示分辨率" 解耦做法。
    //
    // SS_RES 可覆盖, 便于对比画质与性能。
    if (qEnvironmentVariableIsSet("SS_RES"))
        m_renderScale = qgetenv("SS_RES").toDouble();
    else if (window() && window()->devicePixelRatio() >= 1.9)
        m_renderScale = 0.72;

    // 测试用: SS_COSMOS_N=<数量> 指定宇宙视图可见粒子数 (性能测试用)
    if (qEnvironmentVariableIsSet("SS_COSMOS_N"))
        m_cosmosVisible = qEnvironmentVariableIntValue("SS_COSMOS_N");

    // 测试用: SS_FOCUS=<天体id> 指定初始聚焦对象;
    //         SS_FOCUS=overview 表示拉远看整个太阳系。
    const QByteArray focusEnv = qgetenv("SS_FOCUS");
    if (!focusEnv.isEmpty())
        m_focusId = QString::fromUtf8(focusEnv.constData());
    const QByteArray distEnv = qgetenv("SS_DIST");
    if (!distEnv.isEmpty())
        m_forcedDist = distEnv.toDouble();

    // 测试用: SS_SCALE=<0|1|2> 直接进入指定尺度 (跳过 UI 交互)
    //
    // ★ 必须尊重实际数值。初版写成\"非零即银河系", 于是 SS_SCALE=2
    //   被静默当成 1 —— 想验证宇宙视图却渲染出了银河系, 而且因为
    //   UI 标题也跟着 scaleLevel 走, 画面上看不出任何异常, 很容易
    //   误判为\"宇宙视图渲染失败"。
    const QByteArray scaleEnv = qgetenv("SS_SCALE");
    if (!scaleEnv.isEmpty() && scaleEnv.toInt() != 0) {
        const int sv = scaleEnv.toInt();
        if (sv >= 2) {
            m_scale     = SceneScale::Cosmos;
            m_camTarget = QVector3D(0.0f, 0.0f, 0.0f);
            m_camDist   = 300.0;
            m_camTheta  = 35.0 * M_PI / 180.0;
            m_camPhi    = 68.0 * M_PI / 180.0;
        } else {
            m_scale     = SceneScale::Galaxy;
            m_camTarget = QVector3D(0.0f, 0.0f, 0.0f);
            m_camDist   = 340.0;
            m_camTheta  = 40.0 * M_PI / 180.0;
            m_camPhi    = 62.0 * M_PI / 180.0;
        }
        m_snapCamera = true;
    }

    // 测试用: 覆盖相机角度 (度), 便于从特定视角检查渲染
    const QByteArray thEnv = qgetenv("SS_THETA");
    if (!thEnv.isEmpty())
        m_camTheta = thEnv.toDouble() * M_PI / 180.0;
    const QByteArray phEnv = qgetenv("SS_PHI");
    if (!phEnv.isEmpty())
        m_camPhi = phEnv.toDouble() * M_PI / 180.0;

    // 测试用: SS_REAL=1 开启真实比例
    if (qgetenv("SS_REAL").toInt() != 0) {
        m_realScale = true;
        applyFocus();
    }

    // 时间推进: 60 Hz 定时器驱动。
    // Python 版的经验: 阻尼/插值类状态一定要确认更新函数真的被调用 ——
    // 那边曾因漏掉 cam.update(dt) 导致焦点切换完全失效。
    auto *timer = new QTimer(this);
    timer->setInterval(16);
    connect(timer, &QTimer::timeout, this, &SolarScene::onTick);
    timer->start();

    applyFocus();
}

QQuickFramebufferObject::Renderer *SolarScene::createRenderer() const
{
    return new SolarSceneRenderer;
}

void SolarScene::onTick()
{
    // ★ 宇宙粒子总数在渲染线程 build() 之后才可知, 而 QML 面板在窗口
    //   构造时就会被求值 (那时还是 0)。这里每帧检查一次, 一旦总数
    //   就绪就发信号让 QML 重新求值 —— 比在 QML 里挂 Timer 可靠,
    //   因为不依赖 QML 绑定重算的时机。
    // ★ 在这里统一算好四项数值并存进成员, 由 Q_PROPERTY 通知 QML。
    //   每帧只做几个算术运算, 开销可忽略。
    {
        const int tot = Cosmos::lastBuiltCount();
        if (tot > 0) {
            if (!m_cosmosReady) {
                m_cosmosReady = true;
                m_cosmosTotal = tot;
                emit cosmosTotalChanged();
            }
            const int vis = (m_cosmosVisible <= 0 || m_cosmosVisible > tot)
                                ? tot : m_cosmosVisible;
            // ★ 系数标定 (1440x900, 点精灵, 实测):
            //      7,120  个 -> 1.46 ms
            //     17,802 个 -> 1.86 ms
            //     35,603 个 -> 3.69 ms
            //     71,206 个 -> 5.63 ms
            //   最小二乘: 0.067 us/粒子 + 0.94 ms 固定开销。
            //
            //   ★ 注意: 早先用「重复粒子放大」做压力测试得到的是
            //     18.8 us/千粒子 —— 比这个高 280 倍, 因为重复点全部
            //     落在同一位置, 导致**极端过度绘制**, 完全不能代表
            //     真实的空间分布。**必须用真实截断来标定**。
            //   ★ 这也意味着: 接入 SDSS 真实星表后, 由于星系的空间
            //     聚集性 (成团而非均匀), 实际填充率会高于这里的均匀
            //     假设, 系数需重新实测。
            const double ms = 0.94 + 0.000067 * vis;
            const double fps = ms > 0.01 ? 1000.0 / ms : 999.0;
            if (qAbs(ms - m_cosmosEstMs) > 0.01 || qAbs(vis - m_lastVis) != 0) {
                m_cosmosEstMs = ms;
                m_cosmosEstFps = fps;
                m_lastVis = vis;
                emit cosmosPerfChanged();
            }
        }
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const double dt = double(now - m_lastTickMs) / 1000.0;
    m_lastTickMs = now;

    if (!m_paused && dt > 0.0 && dt < 0.5)
        m_jd += dt * m_timeScale / 86400.0;

    updateSunMark();

    // 连续重绘 (动画)
    update();
    emit dateTextChanged();
}

// ---------------------------------------------------------------------------
//  太阳标注的屏幕投影
//
//  用与渲染侧相同的相机约定重算一遍投影。相机参数取**目标值**而非渲染
//  侧的阻尼平滑值, 所以拖动过程中标注会略有滞后 (停止后立即吻合)。
//  这个取舍是为了避免跨线程同步 —— 标注是\"我们在哪\"的指示, 轻微滞后
//  在教学上无影响, 而把渲染线程的平滑状态搬过来需要加锁, 得不偿失。
// ---------------------------------------------------------------------------
void SolarScene::updateSunMark()
{
    // ---- 宇宙尺度: 投影具名结构 ----
    //
    // ★ 与银河系视图的关键差别: 这里的距离跨了 6 个数量级, 场景坐标是
    //   **对数映射**的结果。因此标注位置必须用同一个 Cosmos::distToScene
    //   换算, 否则标注会漂到完全错误的地方。
    if (m_scale == SceneScale::Cosmos) {
        const double spC = std::sin(m_camPhi);
        const QVector3D eyeC(
            float(m_camDist * spC * std::sin(m_camTheta)),
            float(m_camDist * std::cos(m_camPhi)),
            float(m_camDist * spC * std::cos(m_camTheta)));
        QVector3D upC(0.0f, 1.0f, 0.0f);
        if (std::fabs(m_camPhi) < 1e-3 || std::fabs(m_camPhi - M_PI) < 1e-3)
            upC = QVector3D(0.0f, 0.0f, 1.0f);
        QMatrix4x4 vC, pC;
        vC.lookAt(eyeC, m_camTarget, upC);
        const double aspectC = qMax(1.0, double(width()))
                             / qMax(1.0, double(height()));
        pC.perspective(float(m_fov), float(aspectC), 1.0f, 10000.0f);
        const QMatrix4x4 vpC = pC * vC;

        QVariantList labels;
        const QVector<CosmosMarker> marks = Cosmos::markers();
        for (const CosmosMarker &mk : marks) {
            const QVector4D clip = vpC * QVector4D(mk.pos, 1.0f);
            if (clip.w() <= 1e-6f)
                continue;
            const QVector3D ndc = clip.toVector3D() / clip.w();
            const double nx = ndc.x() * 0.5 + 0.5;
            const double ny = 0.5 - ndc.y() * 0.5;
            if (nx < 0.02 || nx > 0.98 || ny < 0.02 || ny > 0.98)
                continue;
            // kind: 0=星系 1=星系群/团 2=超星系团 3=巨壁 4=空洞
            const char *kn = mk.kind == 4 ? "void"
                           : mk.kind == 3 ? "wall"
                           : mk.kind == 2 ? "supercluster"
                           : mk.kind == 1 ? "cluster" : "galaxy";
            labels.append(QVariantMap{
                { "x", nx }, { "y", ny },
                { "text", mk.nameCn },
                { "sub",  mk.detail },
                { "kind", QString::fromUtf8(kn) },
            });
        }

        // 视野尺度 (百万光年) —— 反推: 场景单位 -> Mly
        //   r = log10(1+d/d0)/log10(1+dMax/d0)·R  =>  d = d0·(K^(r/R) - 1)
        double viewLy = 0.0;
        {
            const double halfFov = double(m_fov) * 0.5 * M_PI / 180.0;
            const double hUnits = 2.0 * m_camDist * std::tan(halfFov);
            const double den = std::log10(1.0 + 46500.0 / 0.1);
            const double rEdge = qMin(double(Cosmos::sceneRadius()), hUnits * 0.5);
            const double dEdge = 0.1 * (std::pow(10.0, rEdge / Cosmos::sceneRadius() * den) - 1.0);
            viewLy = dEdge * 2.0 * aspectC;
        }

        bool dirty = !m_sunMarkOn
                  || std::fabs(viewLy - m_galaxyViewWidthLy) > 1.0;
        m_sunMarkOn = true;
        m_galaxyViewWidthLy = viewLy;
        m_galaxyLabels = labels;
        emit galaxyLabelsChanged();
        if (dirty)
            emit sunMarkChanged();
        return;
    }

    if (m_scale != SceneScale::Galaxy) {
        bool dirty = false;
        if (m_sunMarkOn) {
            m_sunMarkOn = false;
            dirty = true;
        }
        if (!m_galaxyLabels.isEmpty()) {
            m_galaxyLabels.clear();
            m_galaxyViewWidthLy = 0.0;
            dirty = true;
        }
        if (dirty) {
            emit sunMarkChanged();
            emit galaxyLabelsChanged();
        }
        return;
    }

    // 与 camera.cpp 的 eye() 同一套球坐标约定
    const double sp = std::sin(m_camPhi);
    const QVector3D eye(
        float(m_camDist * sp * std::sin(m_camTheta)),
        float(m_camDist * std::cos(m_camPhi)),
        float(m_camDist * sp * std::cos(m_camTheta)));

    QVector3D up(0.0f, 1.0f, 0.0f);
    if (std::fabs(m_camPhi) < 1e-3 || std::fabs(m_camPhi - M_PI) < 1e-3)
        up = QVector3D(0.0f, 0.0f, 1.0f);   // 极点附近换 up, 避免退化

    QMatrix4x4 view;
    view.lookAt(eye, m_camTarget, up);

    QMatrix4x4 proj;
    const double aspect = qMax(1.0, double(width())) / qMax(1.0, double(height()));
    proj.perspective(float(m_fov), float(aspect), 1.0f, 10000.0f);
    const QMatrix4x4 vp = proj * view;

    // 把世界坐标投影到归一化屏幕坐标 (0..1, 原点左上)。
    // 返回 false 表示该点落在相机背后 (w <= 0), 此时不能投影。
    auto project = [&](const QVector3D &world, double &nx, double &ny) {
        const QVector4D clip = vp * QVector4D(world, 1.0f);
        if (clip.w() <= 1e-6f)
            return false;
        const QVector3D ndc = clip.toVector3D() / clip.w();
        nx = ndc.x() * 0.5 + 0.5;
        ny = 0.5 - ndc.y() * 0.5;          // QML 的 y 轴向下
        return true;
    };

    // ---- 太阳标记 ----
    double nx = m_sunMarkX, ny = m_sunMarkY;
    bool on = false;
    if (project(Galaxy::sunPosition(), nx, ny)) {
        // 留 8% 余量: 标注刚出画时仍有部分可见, 不会突兀地消失
        on = (nx > -0.08 && nx < 1.08 && ny > -0.08 && ny < 1.08);
    }

    const bool moved = std::fabs(nx - m_sunMarkX) > 1e-4
                    || std::fabs(ny - m_sunMarkY) > 1e-4;
    if (moved || on != m_sunMarkOn) {
        m_sunMarkX = nx;
        m_sunMarkY = ny;
        m_sunMarkOn = on;
        emit sunMarkChanged();
    }

    // ---- 旋臂 / 银心 / 猎户支 标注 ----
    //
    // ★ 这些标注是银河系视图的教学价值所在:
    //   否则学生看到的只是\"一团有旋臂的粒子", 不知道哪条是英仙臂、
    //   太阳在哪条臂上、银心在哪个方向。
    //
    //   投影在 CPU 侧完成, 与太阳标记共用同一套相机参数 ——
    //   保证标注永远贴合几何。
    QVariantList labels;

    // 银心 (标在核球中心稍偏, 避免被最亮的粒子完全淹没)
    {
        double gx, gy;
        if (project(QVector3D(0.0f, 0.0f, 0.0f), gx, gy)
            && gx > -0.05 && gx < 1.05 && gy > -0.05 && gy < 1.05) {
            labels.append(QVariantMap{
                { "x", gx }, { "y", gy },
                { "text", QStringLiteral("银心") },
                { "sub",  QStringLiteral("人马座 A*") },
                { "kind", QStringLiteral("core") },
            });
        }
    }

    // 旋臂标注: 锚点取该臂 β 区间中点, 用与粒子生成**同一套公式**算得的
    // 半径与方位角 —— 这样标签必然贴在臂的实体上。
    // (旧实现按"统一螺距角 + 固定起止半径"估算, 旋臂改参数后标签会飘出臂外)
    for (int a = 0; a < gx_armInfoCount(); ++a) {
        const QString nm = gx_armName(a);
        const double rLy = gx_armLabelRadiusLy(a);
        const double ang = gx_armLabelAngle(a);
        const QVector3D wp(
            float(rLy * std::cos(ang) / gx::kLyPerUnit),
            0.0f,
            float(rLy * std::sin(ang) / gx::kLyPerUnit));
        double lx, ly;
        if (project(wp, lx, ly) && lx > 0.02 && lx < 0.98 && ly > 0.02 && ly < 0.98) {
            labels.append(QVariantMap{
                { "x", lx }, { "y", ly },
                { "text", nm },
                { "sub",  QString() },
                { "kind", QStringLiteral("arm") },
            });
        }
    }

    // 猎户支 (太阳所在) —— 教学上必须与主旋臂区分
    {
        const double rLy = gx::kOrionSpurDistLy;
        const double ang = gx::kOrionSpurAngleDeg * M_PI / 180.0;
        const QVector3D wp(
            float(rLy * std::cos(ang) / gx::kLyPerUnit),
            0.0f,
            float(rLy * std::sin(ang) / gx::kLyPerUnit));
        double lx, ly;
        if (project(wp, lx, ly) && lx > 0.02 && lx < 0.98 && ly > 0.02 && ly < 0.98) {
            labels.append(QVariantMap{
                { "x", lx }, { "y", ly },
                { "text", QStringLiteral("猎户支") },
                { "sub",  QStringLiteral("太阳所在") },
                { "kind", QStringLiteral("spur") },
            });
        }
    }

    // 视野宽度 (光年) —— 用于显示比例尺。
    // 由相机距离与 FOV 反推: 在目标平面处, 可见高度 = 2·d·tan(fov/2)
    {
        const double halfFov = double(m_fov) * 0.5 * M_PI / 180.0;
        const double hUnits = 2.0 * m_camDist * std::tan(halfFov);
        m_galaxyViewWidthLy = hUnits * gx::kLyPerUnit * aspect;
    }

    m_galaxyLabels = labels;
    emit galaxyLabelsChanged();
}

// ---------------------------------------------------------------------------
//  属性
// ---------------------------------------------------------------------------

void SolarScene::setTimeScale(double s)
{
    if (qFuzzyCompare(m_timeScale, s))
        return;
    m_timeScale = s;
    m_paused = false;
    emit timeScaleChanged();
    emit pausedChanged();
}

void SolarScene::setPaused(bool p)
{
    if (m_paused == p)
        return;
    m_paused = p;
    emit pausedChanged();
}

void SolarScene::setFocusId(const QString &id)
{
    if (m_focusId == id)
        return;
    m_focusId = id;
    applyFocus();
    emit focusIdChanged();
}

void SolarScene::setShowOrbits(bool v)
{
    if (m_showOrbits == v)
        return;
    m_showOrbits = v;
    emit showOrbitsChanged();
    update();
}

void SolarScene::setShowRings(bool v)
{
    if (m_showRings == v)
        return;
    m_showRings = v;
    emit showRingsChanged();
    update();
}

void SolarScene::setShowAtmo(bool v)
{
    if (m_showAtmo == v)
        return;
    m_showAtmo = v;
    emit showAtmoChanged();
    update();
}

void SolarScene::setShowBelts(bool v)
{
    if (m_showBelts == v)
        return;
    m_showBelts = v;
    emit showBeltsChanged();
    update();
}

void SolarScene::setRealScale(bool v)
{
    if (m_realScale == v)
        return;
    m_realScale = v;
    // 切换比例后天体的\"合理视距\"完全不同 —— 艺术压缩下地球半径 0.9 单位,
    // 真实比例下只有 0.0064 单位, 按同一公式算出的距离会差 100 倍以上,
    // 不重新聚焦就会看到一片空。
    applyFocus();
    emit realScaleChanged();
    emit focusIdChanged();
    update();
}

QString SolarScene::dateText() const
{
    // 儒略日 -> 公历 (用 QDateTime 反推: 由 Unix 时间戳换算)
    const double unixSec = (m_jd - 2440587.5) * 86400.0;
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(qint64(unixSec));
    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

int SolarScene::bodyCount() const
{
    return registry::count();
}

// ---------------------------------------------------------------------------
//  相机与焦点
// ---------------------------------------------------------------------------

void SolarScene::applyFocus()
{
    // 银河系尺度下, 太阳系的\"聚焦某天体\"逻辑完全不适用
    // (相机距离量级差 2e10 倍), 直接跳过。
    // 银河系与宇宙尺度下, "聚焦某个天体\"的逻辑都不适用
    if (m_scale == SceneScale::Galaxy || m_scale == SceneScale::Cosmos)
        return;

    // overview 模式: 拉远看整个太阳系 (测试与「全景」按钮用)
    if (m_forcedDist > 0.0) {
        Scene s;
        s.setJulianDate(m_jd);
        s.update();
        if (const SceneItem *it = s.itemById(m_focusId))
            m_camTarget = it->center;
        m_camDist = m_forcedDist;
        m_snapCamera = true;
        return;
    }
    if (m_focusId == QLatin1String("overview")) {
        m_camTarget = QVector3D(0.0f, 0.0f, 0.0f);
        m_camDist = 320.0;
        m_snapCamera = true;
        return;
    }

    // 在 GUI 侧同样需要天体位置 (为了算相机目标点)。
    // 用一份临时 Scene 计算 —— 18 个天体的开普勒求解开销极小。
    Scene s;
    s.setJulianDate(m_jd);
    s.update();

    if (const SceneItem *it = s.itemById(m_focusId)) {
        m_camTarget = it->center;

        // 视距: 按天体半径取合适倍数, 太小会穿模、太大看不到细节
        //
        // ★ 真实比例模式下必须用**大得多的倍数**。
        //   艺术压缩时地球半径 0.9 单位, ×5.5 ≈ 5 单位视距即可看清;
        //   真实比例下地球半径只有 0.0064 单位, 同样 ×5.5 得 0.035 单位,
        //   而太阳在 150 单位外 —— 取景框里空无一物, 看上去就像故障。
        //   放大到 ×260 让天体填满合理画面, 同时保留\"它确实远小于轨道"
        //   这个正确的尺度认知。
        const double r = qMax(double(it->radius), 1e-4);
        const double baseMul = m_realScale ? 260.0 : 5.5;
        double want = r * baseMul;
        if (it->body && it->body->hasRings)
            want = r * (m_realScale ? 420.0 : 9.0);   // 有环的要退远些

        // ---- 卫星: 与其它天体用同一套"按自身半径取景" + 母星避让 ----
        //
        // ★★ 这里试过两种错误做法, 都记录以免重犯:
        //
        //   错误一: want = dParent × 0.75, 意图"把轨道框进来"。
        //     木卫四的轨道是 57 场景单位, 相机于是退到 43 单位外,
        //     卫星的角直径只剩 1.7° —— 屏幕上就是个点。
        //     根因是几何上的硬约束: 卫星轨道远大于卫星自身
        //     (木卫四轨道 / 半径 ≈ 780 倍), **"看见轨道"与"看清卫星"
        //     不可兼得**。教学上"看清这颗卫星长什么样"优先级更高,
        //     轨道关系可以切到全景视图看。
        //
        //   错误二: 判据写成 dParent > 0。
        //     但行星/矮行星/小行星的 parentCenter 存的是**太阳位置**,
        //     于是所有日心天体都被拉到 15 场景单位外 ——
        //     谷神星本该是 1.9 单位的特写, 却成了整个内太阳系的远景。
        //     正确判据是结构性的 body->parent != nullptr。
        //
        // ★ 正确做法: 沿用上面的 want = r × baseMul (标准取景),
        //   只额外加一条约束 —— **相机不许进入母星内部**。
        //   最坏情况是相机恰好落在"卫星 → 母星中心"的连线上,
        //   此时相机到母星中心的距离 = dParent − want, 要求它
        //   大于 1.35 倍母星显示半径。
        //
        //   实测: 火卫一轨道 1.98 单位、火星显示半径 0.717,
        //   若不加约束, 0.75×1.98 = 1.49 会把相机推到距火星星心
        //   0.50 处 —— **钻进火星内部**, 整幅画面连同星空一起变黑。
        if (it->body && it->body->parent != nullptr) {
            const float dParent = (it->center - it->parentCenter).length();
            const BodyData *pb = registry::findBody(it->body->parent);
            if (dParent > 0.0f && pb) {
                const double parentVisR =
                    sceneconst::displayRadius(pb->radiusKm, m_realScale);
                const double cap = double(dParent) - parentVisR * 1.35;
                if (cap > 0.0)
                    want = qMin(want, cap);
                // 相机也不能比卫星自身还近 —— 否则会穿模
                want = qMax(want, double(r) * 1.6);
            }
        }

        m_camDist = qBound(0.02, want, 2.0e6);

        // ---- 自动光照视角 ----
        // 把相机放到**偏向阳侧的三分之四角度**, 而不是正对阳光。
        //
        // ★ 角度选择很关键, 这是「好看」与「死板」的分水岭:
        //   * 正对阳光 (toSun 分量 ~0.8): 整个球都在白天 -> 看不出体积,
        //     且亮面大面积过曝成白盘 (实测就是这样)。
        //   * 完全侧向 (toSun ~0.0): 一半全黑, 外行星的暗面细节全丢。
        //   * 取 ~0.34 的向阳分量 (夹角约 70°): 亮面占 ~2/3, 晨昏线落在
        //     可见范围内 —— 既有充足细节, 又有明确的明暗交界与立体感,
        //     与 Python 版参考截图一致。
        if (!m_keepUserAngle) {
            QVector3D toSun = s.sunPosition() - it->center;
            if (toSun.length() > 1e-6f) {
                toSun.normalize();
                const QVector3D worldUp(0.0f, 1.0f, 0.0f);
                QVector3D side = QVector3D::crossProduct(toSun, worldUp);
                if (side.length() < 1e-4f)
                    side = QVector3D(1.0f, 0.0f, 0.0f);
                side.normalize();

                const QVector3D eyeDir =
                    (toSun * 0.34f + side * 0.86f + worldUp * 0.32f).normalized();
                m_camPhi   = std::acos(qBound(-1.0f, eyeDir.y(), 1.0f));
                m_camTheta = std::atan2(double(eyeDir.x()), double(eyeDir.z()));
            }
        }

        m_snapCamera = true;
    }
}

// ---------------------------------------------------------------------------
//  测试/自检专用: 一次设定全套视角参数
//
//  ★ 直接置 m_snapCamera = true 让相机**立即到位**, 不等插值动画。
//    正常交互需要平滑过渡 (否则拖动手感很生硬), 但批量渲染时
//    每张都等动画走完会白白多花 1-2 秒。
// ---------------------------------------------------------------------------
void SolarScene::testShot(const QString &focusId,
                          double dist, double phiDeg, double thetaDeg,
                          int scaleLevel, double jd)
{
    // 尺度要先切 —— applyFocus() 的行为依赖当前尺度
    if (scaleLevel >= 0 && scaleLevel != int(m_scale)) {
        setScale(scaleLevel);
        m_snapCamera = true;
    }

    if (jd > 0.0) {
        m_jd = jd;
        emit dateTextChanged();
    }

    if (!focusId.isEmpty())
        m_focusId = focusId;

    if (dist > 0.0)
        m_forcedDist = dist;
    if (phiDeg >= 0.0)
        m_camPhi = phiDeg * M_PI / 180.0;
    if (thetaDeg >= 0.0)
        m_camTheta = thetaDeg * M_PI / 180.0;

    // 指定了视距就不要再让 applyFocus 重算
    if (dist > 0.0) {
        Scene s;
        s.setJulianDate(m_jd);
        s.update();
        if (const SceneItem *it = s.itemById(m_focusId))
            m_camTarget = it->center;
        m_camDist = dist;
        m_snapCamera = true;
    } else {
        applyFocus();
        m_snapCamera = true;
    }

    update();
}

void SolarScene::focusOn(const QString &id)
{
    setFocusId(id);
}

// ---------------------------------------------------------------------------
//  宇宙视图可见粒子数 —— 性能开关
//
//  ★ 这是**零成本**的: 顶点数据在 build() 时一次性上传, 此处只改
//    glDrawArrays 的 count。既不重传缓冲, 也不重建 VAO。
//
//  ★ 由于数据按重要性顺序生成 (纤维 → 空洞边缘 → 背景填充),
//    截断天然保留宇宙网的骨架 —— 低档位不是"随机丢掉一半",
//    而是"只画最必要的结构"。
// ---------------------------------------------------------------------------
void SolarScene::setCosmosVisible(int n)
{
    if (n == m_cosmosVisible)
        return;
    m_cosmosVisible = n;
    emit cosmosVisibleChanged();
    // 预估耗时会随可见数变化, 但实际重算在 onTick 里统一做
    // (那里能读到渲染线程写入的总数)。这里只需触发一次重绘。
    update();
}

void SolarScene::resetView()
{
    // 复位到**当前尺度**的默认视角 —— 银河系视图下复位到俯视全景,
    // 而不是跳回太阳系那套参数。
    if (m_scale == SceneScale::Cosmos) {
        m_camTarget = QVector3D(0.0f, 0.0f, 0.0f);
        m_camDist   = 300.0;
        m_camTheta  = 35.0 * M_PI / 180.0;
        m_camPhi    = 68.0 * M_PI / 180.0;
        m_keepUserAngle = false;
    } else if (m_scale == SceneScale::Galaxy) {
        m_camTarget = QVector3D(0.0f, 0.0f, 0.0f);
        m_camDist   = 340.0;
        m_camTheta  = 40.0 * M_PI / 180.0;
        m_camPhi    = 62.0 * M_PI / 180.0;
        m_keepUserAngle = false;
    } else {
        m_camTheta = 35.0 * M_PI / 180.0;
        m_camPhi   = 62.0 * M_PI / 180.0;
        applyFocus();
    }
    m_snapCamera = true;
    update();
}

void SolarScene::setTimeToNow()
{
    m_jd = eph::jdFromUnixSec(double(QDateTime::currentSecsSinceEpoch()));
    applyFocus();
    update();
    emit dateTextChanged();
}

int SolarScene::scale() const
{
    return int(m_scale);
}

QString SolarScene::scaleName() const
{
    switch (m_scale) {
    case SceneScale::Cosmos: return QStringLiteral("宇宙");
    case SceneScale::Galaxy: return QStringLiteral("银河系");
    default:                 return QStringLiteral("太阳系");
    }
}

void SolarScene::setScale(int s)
{
    // 三档映射: 0=太阳系 1=银河系 2=宇宙
    const SceneScale ns = (s == 2) ? SceneScale::Cosmos
                        : (s == 1) ? SceneScale::Galaxy
                                   : SceneScale::SolarSystem;
    if (m_scale == ns)
        return;
    m_scale = ns;

    // 切换尺度时相机的可达范围与默认位姿都要整套换掉。
    // 太阳系的\"距地球 5 倍半径\"在银河系尺度下毫无意义, 反之亦然。
    m_keepUserAngle = false;
    if (m_scale == SceneScale::Cosmos) {
        // 宇宙尺度: 视距覆盖整个对数映射后的场景 (半径 100 单位)
        m_camTarget = QVector3D(0.0f, 0.0f, 0.0f);
        m_camDist   = 300.0;
        m_camTheta  = 35.0 * M_PI / 180.0;
        m_camPhi    = 68.0 * M_PI / 180.0;
    } else if (m_scale == SceneScale::Galaxy) {
        // 俯视 62° —— 稍微倾斜的俯视最能同时展现棒的走向与旋臂的展开
        m_camTarget = QVector3D(0.0f, 0.0f, 0.0f);
        m_camDist   = 340.0;
        m_camTheta  = 40.0 * M_PI / 180.0;
        m_camPhi    = 62.0 * M_PI / 180.0;
    } else {
        m_camTheta = 35.0 * M_PI / 180.0;
        m_camPhi   = 62.0 * M_PI / 180.0;
        applyFocus();
    }
    m_snapCamera = true;
    emit scaleChanged();
    update();
}

// ---------------------------------------------------------------------------
//  宇宙尺度信息
//
//  ★ 数值全部取自 Planck 2018 —— 当代宇宙学的定量基础, 教学上不能给
//    模糊值或旧值。暗能量占 68.5%、暗物质+重子 31.5% 这个比例是
//    "标准宇宙学模型 (ΛCDM)" 最核心的一组数字。
// ---------------------------------------------------------------------------
QVariantMap SolarScene::cosmosInfo() const
{
    QVariantMap m;

    // 标题
    m["title"]       = QStringLiteral("\u5b87\u5b99\u5927\u5c3a\u5ea6\u7ed3\u6784");
    m["titleEn"]     = QStringLiteral("Large-Scale Structure");

    // 宇宙学参数 (Planck 2018) —— 当代宇宙学的定量基础
    m["age"]         = QStringLiteral("%1 \u4ebf\u5e74")
                           .arg(cosmo::kAgeGyr * 10.0, 0, 'f', 1);
    m["h0"]          = QStringLiteral("%1 km/s/Mpc").arg(cosmo::kH0, 0, 'f', 1);
    m["omegaM"]      = QStringLiteral("%1 %").arg(cosmo::kOmegaM * 100.0, 0, 'f', 1);
    m["omegaLambda"] = QStringLiteral("%1 %").arg(cosmo::kOmegaLambda * 100.0, 0, 'f', 1);
    m["omegaB"]      = QStringLiteral("%1 %").arg(cosmo::kOmegaB * 100.0, 0, 'f', 1);
    m["cmb"]         = QStringLiteral("%1 K").arg(cosmo::kCMBTempK, 0, 'f', 4);
    m["cmbZ"]        = QStringLiteral("z = %1").arg(cosmo::kCMBRedshift, 0, 'f', 0);
    m["obsRadius"]   = QStringLiteral("465 \u4ebf\u5149\u5e74");
    m["obsDia"]      = QStringLiteral("930 \u4ebf\u5149\u5e74");
    m["recombT"]     = QStringLiteral("\u5927\u7206\u70b8\u540e 38 \u4e07\u5e74");
    m["galaxies"]    = QStringLiteral("\u7ea6 2 \u4e07\u4ebf\u4e2a");

    // 结构层级 —— 逐级放大, 教学上最直观的切入方式
    m["hierarchy"] = QVariantList{
        QVariantMap{{"lvl", QStringLiteral("\u884c\u661f\u7cfb")},
                    {"size", QStringLiteral("~10^-4 \u5149\u5e74")}},
        QVariantMap{{"lvl", QStringLiteral("\u6052\u661f\u7cfb")},
                    {"size", QStringLiteral("~1 \u5149\u5e74")}},
        QVariantMap{{"lvl", QStringLiteral("\u661f\u7cfb")},
                    {"size", QStringLiteral("10 \u4e07\u5149\u5e74")}},
        QVariantMap{{"lvl", QStringLiteral("\u661f\u7cfb\u7fa4/\u56e2")},
                    {"size", QStringLiteral("1000 \u4e07\u5149\u5e74")}},
        QVariantMap{{"lvl", QStringLiteral("\u8d85\u661f\u7cfb\u56e2")},
                    {"size", QStringLiteral("5 \u4ebf\u5149\u5e74")}},
        QVariantMap{{"lvl", QStringLiteral("\u5b87\u5b99\u7f51")},
                    {"size", QStringLiteral("> 100 \u4ebf\u5149\u5e74")}},
    };
    return m;
}

// ---------------------------------------------------------------------------
//  宇宙视图的粒子数与预估 GPU 耗时
//
//  ★ 标定依据 (本机实测, 1440x900, 点精灵):
//       10,000 可见 -> 0.73 ms      25,000 -> 1.01 ms
//       50,000 可见 -> 1.48 ms      71,206 -> 2.05 ms
//    差分得**边际成本约 18.8 us / 千粒子** (≈18.8 ms / 百万),
//    另加约 0.5 ms 固定开销 (后处理链)。
//
//  ★ 这是**填充率**主导的: gl_PointSize 下限 0.7 px 意味着再远的
//    星系也至少占 1 像素, 于是粒子数直接换算成像素覆盖量。
//    1440x900 = 130 万像素; 260 万粒子 = 260 万像素覆盖, 超屏幕两倍。
//
//  ★ 换数据集 (如接 SDSS 星表) 后此系数需重新标定。
// ---------------------------------------------------------------------------
QVariantMap SolarScene::cosmosPerf() const
{
    QVariantMap m;
    // ★ 时序问题: Cosmos::build() 跑在渲染线程的 init() 里, 而 QML 在
    //   窗口构造时就会调用本函数 —— 那时 GPU 尚未初始化, 总数还是 0,
    //   界面会显示"显示 0 / 0"。这里返回 ready 标志, 由 QML 定时重试,
    //   等真正 build 完再显示真实数字。
    const int total = m_cosmosTotal > 0 ? m_cosmosTotal
                                       : Cosmos::lastBuiltCount();
    m["ready"] = total > 0;
    const int vis = (m_cosmosVisible <= 0 || m_cosmosVisible > total)
                        ? total : m_cosmosVisible;
    const double estMs = 0.5 + 0.0188 * (vis / 1000.0);
    m["total"]   = total;
    m["visible"] = vis;
    m["estMs"]   = estMs;
    m["estFps"]  = estMs > 0.01 ? 1000.0 / estMs : 999.0;
    return m;
}

QVariantList SolarScene::cosmosNotes() const
{
    QVariantList out;
    const char *notes[] = {
        "\u2605 \u8ddd\u79bb\u4e3a\u5bf9\u6570\u6620\u5c04: \u8fdc\u5904\u7684\u95f4\u9694\u88ab\u538b\u7f29\u4e86, \u6807\u6ce8\u4e2d\u7684\u6570\u5b57\u624d\u662f\u771f\u5b9e\u8ddd\u79bb\u3002\u8fd9\u662f\u4e3a\u4e86\u628a 6 \u4e2a\u6570\u91cf\u7ea7\u7684\u5c3a\u5ea6\u653e\u8fdb\u540c\u4e00\u753b\u9762\u5fc5\u987b\u4ed8\u51fa\u7684\u4ee3\u4ef7\u3002",
        "\u5b87\u5b99\u5728\u5927\u4e8e\u7ea6 3 \u4ebf\u5149\u5e74\u7684\u5c3a\u5ea6\u4e0a\u624d\u8868\u73b0\u51fa\u5747\u5300\u6027, \u8fd9\u5c31\u662f\u5b87\u5b99\u5b66\u539f\u7406\u3002\u66f4\u5c0f\u7684\u5c3a\u5ea6\u4e0a, \u661f\u7cfb\u5448\u7ea4\u7ef4\u72b6\u6210\u56e2\u5206\u5e03, \u4e2d\u95f4\u662f\u5de8\u5927\u7684\u7a7a\u6d1e\u3002",
        "\u2605 \u6697\u80fd\u91cf\u5360 68.5%, \u6697\u7269\u8d28\u4e0e\u666e\u901a\u7269\u8d28\u5408\u8ba1\u4ec5 31.5%\u3002\u6211\u4eec\u719f\u6089\u7684\u7269\u8d28\u53ea\u5360\u5b87\u5b99\u7684\u4e0d\u5230 5% \u2014\u2014 \u8fd9\u662f\u5f53\u4ee3\u5b87\u5b99\u5b66\u6700\u53cd\u76f4\u89c9\u7684\u7ed3\u8bba\u3002",
        "\u65af\u9686\u5de8\u58c1\u957f\u7ea6 13.8 \u4ebf\u5149\u5e74, \u5149\u7a7f\u8d8a\u5b83\u9700\u8981 13.8 \u4ebf\u5e74 \u2014\u2014 \u7ea6\u4e3a\u5b87\u5b99\u5e74\u9f84 (137.97 \u4ebf\u5e74) \u7684\u5341\u5206\u4e4b\u4e00\u3002",
        "\u5b87\u5b99\u5fae\u6ce2\u80cc\u666f (CMB) \u662f\u5927\u7206\u70b8\u540e 38 \u4e07\u5e74\u7684\u5149, \u6e29\u5ea6 2.7255 K, \u7ea2\u79fb z \u7ea6 1090\u3002\u5b83\u662f\u6211\u4eec\u80fd\u770b\u5230\u7684\u5b87\u5b99\u6700\u53e4\u8001\u7684\u7167\u7247\u3002",
        "\u672c\u661f\u7cfb\u7fa4\u6b63\u4ee5\u7ea6 185 km/s \u671d\u5ba4\u5973\u5ea7\u661f\u7cfb\u56e2\u5760\u843d; \u800c\u66f4\u5927\u7684\u5c3a\u5ea6\u4e0a, \u6574\u4e2a\u62c9\u5c3c\u4e9a\u51ef\u4e9a\u8d85\u661f\u7cfb\u56e2\u90fd\u671d\u5de8\u5f15\u6e90\u6d41\u52a8 \u2014\u2014 \u8bf4\u660e\u8fd0\u52a8\u662f\u5206\u5c42\u7684\u3002",
    };
    for (const char *n : notes)
        out.append(QString::fromUtf8(n));
    return out;
}

QVariantList SolarScene::cosmosStructures() const
{
    QVariantList out;
    for (int i = 0; i < LARGE_STRUCTURES_COUNT; ++i) {
        const LargeStructure &s = LARGE_STRUCTURES[i];
        QVariantMap m;
        m["name"] = QString::fromUtf8(s.nameCn);
        m["en"]   = QString::fromUtf8(s.nameEn);
        m["dist"] = s.distanceFromEarthMly < 1.0
                    ? QStringLiteral("\u672c\u661f\u7cfb\u7fa4")
                    : (s.distanceFromEarthMly >= 1000.0
                       ? QStringLiteral("%1 \u4ebf\u5149\u5e74")
                             .arg(s.distanceFromEarthMly / 100.0, 0, 'f', 0)
                       : QStringLiteral("%1 \u767e\u4e07\u5149\u5e74")
                             .arg(s.distanceFromEarthMly, 0, 'f', 0));
        m["size"] = s.sizeMly >= 1000.0
                    ? QStringLiteral("%1 \u4ebf\u5149\u5e74")
                          .arg(s.sizeMly / 100.0, 0, 'f', 0)
                    : QStringLiteral("%1 \u767e\u4e07\u5149\u5e74")
                          .arg(s.sizeMly, 0, 'f', 0);
        m["kind"] = s.kind;
        m["desc"] = QString::fromUtf8(s.desc);
        out.append(m);
    }
    return out;
}
QVariantMap SolarScene::galaxyInfo() const
{
    QVariantMap m;
    m["name"]   = QString::fromUtf8(gx::kName);
    m["en"]     = QString::fromUtf8(gx::kNameEn);
    m["type"]   = QString::fromUtf8(gx::kType);
    m["diameter"] = QStringLiteral("105,700 光年");
    m["diskThickness"] = QStringLiteral("1,000 光年");
    m["bulgeRadius"]   = QStringLiteral("5,000 光年");
    m["barLength"]     = QStringLiteral("27,000 光年");
    // ★ 数值统一采用 Reid et al. 2019 (ApJ 885, 131) 的实测值。
    //   该论文基于 BeSSeL 巡天 (VLBA) + VERA 约 200 个大质量恒星形成区
    //   脉泽的**三角视差**直接测距, 是当前银臂结构最可靠的约束。
    m["sunDistance"]   = QStringLiteral("26,582 光年 (8.15 kpc)");
    m["sunSpeed"]      = QStringLiteral("236 km/s");
    m["galacticYear"]  = QStringLiteral("2.25 亿年");
    m["starCount"]     = QStringLiteral("1000–4000 亿");
    m["mass"]          = QStringLiteral("1.5 万亿太阳质量");
    m["blackHole"]     = QStringLiteral("430 万太阳质量");
    // ★ 不再写"俯仰角 12°" —— 实测每条臂的螺距角各不相同 (8.7°~19.5°),
    //   且多数臂带折点。用统一值会传递错误印象。
    m["arms"]          = QStringLiteral("4 条主臂 + 外臂 + 猎户支 (螺距角 8.7°–19.5°)");
    m["armRef"]        = QStringLiteral("Reid et al. 2019, ApJ 885, 131 (BeSSeL/VLBA 脉泽视差)");
    m["sunOrbits"]     = QStringLiteral("约 20 圈");
    return m;
}

QVariantList SolarScene::galaxyNotes() const
{
    QVariantList out;
    for (const char **p = gx::kNotes; *p; ++p)
        out << QString::fromUtf8(*p);
    return out;
}

void SolarScene::rotateCamera(double dx, double dy)
{
    m_camTheta -= dx * 0.005;
    m_camPhi   -= dy * 0.005;
    const double lim = 0.5 * M_PI / 180.0;
    m_camPhi = qBound(lim, m_camPhi, M_PI - lim);
    m_keepUserAngle = true;      // 用户自己转过之后就别再自动摆位
    update();
}

void SolarScene::zoomCamera(double delta)
{
    // 对数缩放: 距离跨度极大, 只有对数插值手感才线性。
    // 银河系视图的尺度范围与太阳系完全不同, 不能共用上下限 ——
    // 否则滚轮在银河系视图里会瞬间冲到上限而失去控制。
    const double factor = std::exp(-delta * 0.12);
    if (m_scale == SceneScale::Galaxy) {
        // 银盘半径 100 单位: 从贴近盘面 (5) 到整个星系连晕 (900)
        // 上限放宽到 3000, 便于拉远观察银河系与邻近星系的相对位置
        m_camDist = qBound(5.0, m_camDist * factor, 3000.0);
    } else {
        m_camDist = qBound(0.02, m_camDist * factor, 2.0e6);
    }
    update();
}

void SolarScene::panCamera(double dx, double dy)
{
    const double scale = m_camDist * 0.0016;

    // 相机基向量 (与渲染侧同一套球坐标约定)
    const QVector3D eye(
        float(m_camDist * std::sin(m_camPhi) * std::sin(m_camTheta)),
        float(m_camDist * std::cos(m_camPhi)),
        float(m_camDist * std::sin(m_camPhi) * std::cos(m_camTheta)));
    QVector3D fwd = -eye;
    if (fwd.length() < 1e-9f)
        fwd = QVector3D(0.0f, 0.0f, -1.0f);
    fwd.normalize();

    QVector3D right = QVector3D::crossProduct(fwd, QVector3D(0.0f, 1.0f, 0.0f));
    if (right.length() < 1e-6f)
        right = QVector3D(1.0f, 0.0f, 0.0f);
    right.normalize();

    const QVector3D up = QVector3D::crossProduct(right, fwd).normalized();
    m_camTarget += right * float(-dx * scale) + up * float(dy * scale);
    update();
}

ViewState SolarScene::takeSnapshot() const
{
    ViewState s;
    s.jd         = m_jd;
    s.scale      = m_scale;
    s.realScale  = m_realScale;
    s.camTarget  = m_camTarget;
    s.camDist    = m_camDist;
    s.camTheta   = m_camTheta;
    s.camPhi     = m_camPhi;
    s.fov        = m_fov;
    s.showOrbits = m_showOrbits;
    s.showBelts  = m_showBelts;
    s.showRings  = m_showRings;
    s.showAtmo   = m_showAtmo;
    s.cosmosVisible = m_cosmosVisible;

    // snap 是一次性标志: 取走即清除。
    // takeSnapshot 是 const 的, 故用 mutable 成员。
    s.snap = m_snapCamera;
    const_cast<SolarScene *>(this)->m_snapCamera = false;
    return s;
}

// ---------------------------------------------------------------------------
//  给 QML 的数据
// ---------------------------------------------------------------------------

QVariantList SolarScene::bodyList() const
{
    QVariantList list;
    for (int i = 0; i < registry::count(); ++i) {
        const BodyData &b = registry::allBodies()[i];

        // 只列出太阳、行星与月球 —— 木卫/土卫太多会让列表过长
        const bool isMoon = (b.parent != nullptr);
        if (isMoon && std::strcmp(b.id, "moon") != 0)
            continue;

        QVariantMap m;
        m["id"]    = QString::fromUtf8(b.id);
        m["name"]  = QString::fromUtf8(b.name);
        m["en"]    = QString::fromUtf8(b.en);
        m["kind"]  = QString::fromUtf8(b.kind ? b.kind : "rock");
        m["color"] = QColor::fromRgbF(qBound(0.0, double(b.color[0]), 1.0),
                                      qBound(0.0, double(b.color[1]), 1.0),
                                      qBound(0.0, double(b.color[2]), 1.0));
        list.append(m);
    }
    return list;
}

QVariantMap SolarScene::bodyInfo(const QString &id) const
{
    QVariantMap m;
    const BodyData *b = registry::findBody(id.toUtf8().constData());
    if (!b)
        return m;

    Scene s;
    s.setJulianDate(m_jd);
    s.update();
    const SceneItem *it = s.itemById(id);

    m["id"]   = id;
    m["name"] = QString::fromUtf8(b->name);
    m["en"]   = QString::fromUtf8(b->en);
    m["kind"] = QString::fromUtf8(b->kind ? b->kind : "rock");
    m["desc"] = QString::fromUtf8(b->desc ? b->desc : "");
    m["color"] = QColor::fromRgbF(qBound(0.0, double(b->color[0]), 1.0),
                                  qBound(0.0, double(b->color[1]), 1.0),
                                  qBound(0.0, double(b->color[2]), 1.0));

    auto km = [](double v) { return QString::number(v, 'f', 0); };

    m["radius"]   = km(b->radiusKm) + QStringLiteral(" km");
    m["mass"]     = QString::number(b->massKg, 'e', 3) + QStringLiteral(" kg");
    m["rotation"] = QString::number(b->rotHours, 'f', 2)
                  + QStringLiteral(" 小时") + (b->rotHours < 0 ? QStringLiteral(" (逆行)") : QString());
    m["albedo"]   = QString::number(b->albedo, 'f', 3);
    m["gravity"]  = QString::number(b->gravity, 'f', 2) + QStringLiteral(" m/s²");
    m["escape"]   = QString::number(b->escapeKms, 'f', 2) + QStringLiteral(" km/s");
    m["density"]  = QString::number(b->density, 'f', 3) + QStringLiteral(" g/cm³");
    m["temp"]     = QString::number(b->tempC, 'f', 0) + QStringLiteral(" °C");

    if (it) {
        m["distAu"] = QString::number(it->heliocentricAu, 'f', 4)
                    + QStringLiteral(" AU");
        m["speed"]  = QString::number(it->speedKms, 'f', 2)
                    + QStringLiteral(" km/s");
    } else {
        m["distAu"] = QStringLiteral("—");
        m["speed"]  = QStringLiteral("—");
    }

    if (b->kind && std::strcmp(b->kind, "star") == 0) {
        m["distLabel"] = QStringLiteral("距银心");
        m["speedLabel"] = QStringLiteral("公转速度");
    } else if (b->parent) {
        const BodyData *p = registry::findBody(b->parent);
        m["distLabel"] = QStringLiteral("距母星");
        m["speedLabel"] = QStringLiteral("轨道速度");
        m["parentName"] = p ? QString::fromUtf8(p->name) : QString();
    } else {
        m["distLabel"] = QStringLiteral("日心距");
        m["speedLabel"] = QStringLiteral("轨道速度");
    }

    return m;
}
