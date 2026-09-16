// ============================================================================
//  sceneitem.cpp —— C++ 渲染器与 QML 的桥接实现
// ============================================================================

#include "sceneitem.h"
#include "celestialdata.h"
#include "ephemeris.h"
#include "galaxydata.h"
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
    vs.showRings   = m_snapshot.showRings;
    vs.showAtmo    = m_snapshot.showAtmo;
    vs.snap  = m_snapshot.snap;

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

    // 测试用: SS_FOCUS=<天体id> 指定初始聚焦对象;
    //         SS_FOCUS=overview 表示拉远看整个太阳系。
    const QByteArray focusEnv = qgetenv("SS_FOCUS");
    if (!focusEnv.isEmpty())
        m_focusId = QString::fromUtf8(focusEnv.constData());
    const QByteArray distEnv = qgetenv("SS_DIST");
    if (!distEnv.isEmpty())
        m_forcedDist = distEnv.toDouble();

    // 测试用: SS_SCALE=1 直接进入银河系视图 (跳过 UI 交互)
    const QByteArray scaleEnv = qgetenv("SS_SCALE");
    if (!scaleEnv.isEmpty() && scaleEnv.toInt() != 0) {
        m_scale     = SceneScale::Galaxy;
        m_camTarget = QVector3D(0.0f, 0.0f, 0.0f);
        m_camDist   = 340.0;
        m_camTheta  = 40.0 * M_PI / 180.0;
        m_camPhi    = 62.0 * M_PI / 180.0;
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
//  这个取舍是为了避免跨线程同步 —— 标注是"我们在哪"的指示, 轻微滞后
//  在教学上无影响, 而把渲染线程的平滑状态搬过来需要加锁, 得不偿失。
// ---------------------------------------------------------------------------
void SolarScene::updateSunMark()
{
    if (m_scale != SceneScale::Galaxy) {
        if (m_sunMarkOn) {
            m_sunMarkOn = false;
            emit sunMarkChanged();
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

    // 近远面取任意合理值 —— 只关心 NDC 的 x/y, 与裁剪无关
    QMatrix4x4 proj;
    const double aspect = qMax(1.0, double(width())) / qMax(1.0, double(height()));
    proj.perspective(float(m_fov), float(aspect), 1.0f, 10000.0f);

    const QVector4D clip = (proj * view)
                         * QVector4D(Galaxy::sunPosition(), 1.0f);

    double nx = m_sunMarkX, ny = m_sunMarkY;
    bool on = false;
    if (clip.w() > 1e-6f) {
        const QVector3D ndc = clip.toVector3D() / clip.w();
        nx = ndc.x() * 0.5 + 0.5;
        ny = 0.5 - ndc.y() * 0.5;        // QML 的 y 轴向下
        // 留 8% 余量: 标注刚出画时仍有部分可见, 不会突兀地消失
        on = (nx > -0.08 && nx < 1.08 && ny > -0.08 && ny < 1.08);
    }

    // 只在变化明显时发信号, 避免每帧都触发 QML 重绑
    const bool moved = std::fabs(nx - m_sunMarkX) > 1e-4
                    || std::fabs(ny - m_sunMarkY) > 1e-4;
    if (moved || on != m_sunMarkOn) {
        m_sunMarkX = nx;
        m_sunMarkY = ny;
        m_sunMarkOn = on;
        emit sunMarkChanged();
    }
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

void SolarScene::setRealScale(bool v)
{
    if (m_realScale == v)
        return;
    m_realScale = v;
    // 切换比例后天体的"合理视距"完全不同 —— 艺术压缩下地球半径 0.9 单位,
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
    return ALL_BODIES_COUNT;
}

// ---------------------------------------------------------------------------
//  相机与焦点
// ---------------------------------------------------------------------------

void SolarScene::applyFocus()
{
    // 银河系尺度下, 太阳系的"聚焦某天体"逻辑完全不适用
    // (相机距离量级差 2e10 倍), 直接跳过。
    if (m_scale == SceneScale::Galaxy)
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
        //   放大到 ×260 让天体填满合理画面, 同时保留"它确实远小于轨道"
        //   这个正确的尺度认知。
        const double r = qMax(double(it->radius), 1e-4);
        const double baseMul = m_realScale ? 260.0 : 5.5;
        double want = r * baseMul;
        if (it->body && it->body->hasRings)
            want = r * (m_realScale ? 420.0 : 9.0);   // 有环的要退远些

        // 卫星要收紧视距。
        // 判据: 天体与母体的距离小于自身半径的 3 倍 —— 即真正的"贴身"卫星。
        // 对行星 dParent 是 1 AU 量级 (地球 150 单位), 条件自然不成立;
        // 对月球 dParent ≈ 1.5 单位, 成立。
        // 不加这条时聚焦月球会把地球整个框进画面 (实测地球占了 60% 高度,
        // 月球反倒成了配角), 因为月球离地球太近了。
        const float dParent = (it->center - it->parentCenter).length();
        if (dParent > 0.0f && dParent < float(r * 3.0))
            want = r * (m_realScale ? 140.0 : 3.0);

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

void SolarScene::focusOn(const QString &id)
{
    setFocusId(id);
}

void SolarScene::resetView()
{
    // 复位到**当前尺度**的默认视角 —— 银河系视图下复位到俯视全景,
    // 而不是跳回太阳系那套参数。
    if (m_scale == SceneScale::Galaxy) {
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
    return m_scale == SceneScale::Galaxy ? QStringLiteral("银河系")
                                         : QStringLiteral("太阳系");
}

void SolarScene::setScale(int s)
{
    const auto ns = (s == 1) ? SceneScale::Galaxy : SceneScale::SolarSystem;
    if (m_scale == ns)
        return;
    m_scale = ns;

    // 切换尺度时相机的可达范围与默认位姿都要整套换掉。
    // 太阳系的"距地球 5 倍半径"在银河系尺度下毫无意义, 反之亦然。
    m_keepUserAngle = false;
    if (m_scale == SceneScale::Galaxy) {
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
    m["sunDistance"]   = QStringLiteral("26,000 光年");
    m["sunSpeed"]      = QStringLiteral("230 km/s");
    m["galacticYear"]  = QStringLiteral("2.25 亿年");
    m["starCount"]     = QStringLiteral("1000–4000 亿");
    m["mass"]          = QStringLiteral("1.5 万亿太阳质量");
    m["blackHole"]     = QStringLiteral("430 万太阳质量");
    m["arms"]          = QStringLiteral("4 条主旋臂 (俯仰角 12°)");
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
    s.showRings  = m_showRings;
    s.showAtmo   = m_showAtmo;

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
    for (int i = 0; i < ALL_BODIES_COUNT; ++i) {
        const BodyData &b = ALL_BODIES[i];

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
    const BodyData *b = findBody(id.toUtf8().constData());
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
        const BodyData *p = findBody(b->parent);
        m["distLabel"] = QStringLiteral("距母星");
        m["speedLabel"] = QStringLiteral("轨道速度");
        m["parentName"] = p ? QString::fromUtf8(p->name) : QString();
    } else {
        m["distLabel"] = QStringLiteral("日心距");
        m["speedLabel"] = QStringLiteral("轨道速度");
    }

    return m;
}
