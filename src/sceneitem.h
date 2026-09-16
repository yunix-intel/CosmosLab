// ============================================================================
//  sceneitem.h —— C++ 渲染器与 QML 之间的桥
//
//  分工 (与 Python 版的能力对应):
//    * 本类持有 GUI 线程侧的状态 (时间、选中天体、显示开关、相机参数),
//      并把 QML 界面需要的天体数据以 QVariantMap 暴露出去。
//    * GL 资源与真正的绘制在 SceneRenderer 中, 运行在渲染线程。
//    * 两者在 synchronize() 处交接 —— 那是 Qt 保证 GUI 线程与渲染线程
//      同步的唯一安全时机。
//
//  ★ 本类必须在独立头文件中 —— 与 main() 挤在同一个 .cpp 会强制
//    #include "main.moc", 进而与 AUTOMOC / qmltyperegistrations 冲突
//    (multiple definition of staticMetaObject)。
// ============================================================================

#pragma once

#include <QQuickFramebufferObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "viewstate.h"

class SceneRenderer;
class SolarScene;

// ---------------------------------------------------------------------------
//  渲染器桥接: 运行在渲染线程
// ---------------------------------------------------------------------------
class SolarSceneRenderer : public QQuickFramebufferObject::Renderer
{
public:
    SolarSceneRenderer();
    ~SolarSceneRenderer() override;

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;
    void synchronize(QQuickFramebufferObject *item) override;
    void render() override;

private:
    SceneRenderer *m_renderer = nullptr;
    QSize  m_size;

    // 分辨率缩放 (0.35 ~ 1.0)。1.0 = 按设备像素全分辨率渲染。
    // 后处理链有约 10 个全屏 pass, 在 2880x1800 下是纯填充率瓶颈,
    // 降低它可显著提升帧率, 代价是画面略软。
    // 由 synchronize() 从 GUI 线程取来。
    double m_renderScale = 1.0;

    // synchronize() 期间从 GUI 线程取来的状态快照
    ViewState m_snapshot;
};

// ---------------------------------------------------------------------------
//  QML 可见类型
// ---------------------------------------------------------------------------
class SolarScene : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SolarScene)

    Q_PROPERTY(double  timeScale   READ timeScale   WRITE setTimeScale   NOTIFY timeScaleChanged)
    Q_PROPERTY(bool    paused      READ paused      WRITE setPaused      NOTIFY pausedChanged)
    Q_PROPERTY(QString focusId     READ focusId     WRITE setFocusId     NOTIFY focusIdChanged)
    Q_PROPERTY(bool    showOrbits  READ showOrbits  WRITE setShowOrbits  NOTIFY showOrbitsChanged)
    Q_PROPERTY(bool    showRings   READ showRings   WRITE setShowRings   NOTIFY showRingsChanged)
    Q_PROPERTY(bool    showAtmo    READ showAtmo    WRITE setShowAtmo    NOTIFY showAtmoChanged)
    // 真实比例模式 —— 教学关键: 关闭艺术压缩, 让尺度关系如实呈现
    Q_PROPERTY(bool    realScale   READ realScale   WRITE setRealScale   NOTIFY realScaleChanged)
    Q_PROPERTY(QString dateText    READ dateText                          NOTIFY dateTextChanged)
    Q_PROPERTY(int     bodyCount   READ bodyCount   CONSTANT)
    // ★ 属性名用 scaleLevel 而非 scale —— QML 的 Item 已有 scale 属性
    //   (item 缩放因子), 同名会遮蔽并导致绑定错乱。
    Q_PROPERTY(int     scaleLevel  READ scale       WRITE setScale        NOTIFY scaleChanged)
    Q_PROPERTY(QString scaleName   READ scaleName                         NOTIFY scaleChanged)
    // 太阳在银盘中的位置, 投影到屏幕后的归一化坐标 (0..1)。
    // QML 叠加层用它画"太阳系在这里"的标注 —— 改用 QML 的原因是
    // OpenGL core profile 的线宽上限仅 1px, 细线在星场上完全看不见。
    Q_PROPERTY(double  sunMarkX    READ sunMarkX                          NOTIFY sunMarkChanged)
    Q_PROPERTY(double  sunMarkY    READ sunMarkY                          NOTIFY sunMarkChanged)
    Q_PROPERTY(bool    sunMarkOn   READ sunMarkOn                         NOTIFY sunMarkChanged)

public:
    explicit SolarScene(QQuickItem *parent = nullptr);

    Renderer *createRenderer() const override;

    // ---- 属性 ----
    double timeScale() const { return m_timeScale; }
    void   setTimeScale(double s);

    bool paused() const { return m_paused; }
    void setPaused(bool p);

    QString focusId() const { return m_focusId; }
    void    setFocusId(const QString &id);

    bool showOrbits() const { return m_showOrbits; }
    void setShowOrbits(bool v);

    bool showRings() const { return m_showRings; }
    void setShowRings(bool v);

    bool showAtmo() const { return m_showAtmo; }
    void setShowAtmo(bool v);

    bool realScale() const { return m_realScale; }
    void setRealScale(bool v);

    QString dateText() const;
    int bodyCount() const;

    // ---- 尺度层级 ----
    // 0 = 太阳系, 1 = 银河系。两者相差约 2e10 倍, 无法连续缩放,
    // 故做成独立视图由 UI 切换。
    int  scale() const;
    void setScale(int s);
    QString scaleName() const;

    // 太阳标注的屏幕位置 (归一化 0..1, 原点左上)
    double sunMarkX() const { return m_sunMarkX; }
    double sunMarkY() const { return m_sunMarkY; }
    bool   sunMarkOn() const { return m_sunMarkOn; }

    // ---- 供 QML 调用 ----
    Q_INVOKABLE QVariantList bodyList() const;                 // 天体列表 (列表用)
    Q_INVOKABLE QVariantMap  bodyInfo(const QString &id) const; // 详情面板
    Q_INVOKABLE QVariantMap  galaxyInfo() const;                // 银河系数据面板
    Q_INVOKABLE QVariantList galaxyNotes() const;               // 银河系教学要点
    Q_INVOKABLE void focusOn(const QString &id);
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void setTimeToNow();

    // ---- 交互 (由 QML 的 MouseArea 转发) ----
    Q_INVOKABLE void rotateCamera(double dx, double dy);
    Q_INVOKABLE void zoomCamera(double delta);
    Q_INVOKABLE void panCamera(double dx, double dy);

    // ---- 状态检查 (updatePaintNode 里由 SolarSceneRenderer 读取) ----
    double renderScale() const { return m_renderScale; }

    // 渲染线程在 synchronize() 时取走的状态快照
    ViewState takeSnapshot() const;

signals:
    void timeScaleChanged();
    void pausedChanged();
    void focusIdChanged();
    void showOrbitsChanged();
    void showRingsChanged();
    void showAtmoChanged();
    void realScaleChanged();
    void dateTextChanged();
    void scaleChanged();
    void sunMarkChanged();

private slots:
    void onTick();

private:
    void applyFocus();
    // 把太阳的银盘坐标投影到屏幕, 更新 sunMark* 属性
    void updateSunMark();

    double  m_timeScale = 1.0;          // 1.0 = 实时
    bool    m_paused = false;
    QString m_focusId = QStringLiteral("earth");
    bool    m_showOrbits = true;
    bool    m_showRings = true;
    bool    m_showAtmo = true;
    bool    m_realScale = false;
    bool    m_snapCamera = true;

    double    m_jd = 0.0;               // GUI 侧的时间基准
    qint64    m_lastTickMs = 0;

    // 相机参数 (GUI 侧持有, 每帧传给渲染线程)
    QVector3D m_camTarget{0.0f, 0.0f, 0.0f};
    double    m_camDist = 300.0;
    double    m_camTheta = 35.0 * M_PI / 180.0;
    double    m_camPhi   = 62.0 * M_PI / 180.0;
    double    m_fov = 52.0;
    double    m_forcedDist = 0.0;   // SS_DIST 覆盖值 (>0 时生效), 用于测试
    bool      m_keepUserAngle = false;  // 用户手动转过角度后, 不再自动摆位
    SceneScale m_scale = SceneScale::SolarSystem;
    bool      m_scaleInit = false;      // 首次设置尺度时不要做平滑过渡

    double m_sunMarkX = 0.5;            // 太阳标注的屏幕位置 (归一化)
    double m_sunMarkY = 0.5;
    bool   m_sunMarkOn = false;

    // 渲染分辨率缩放。默认按屏幕 DPR 自适应:
    //   dpr >= 2 (高分辨率屏) -> 0.72  (像素太多, 降一点肉眼无感)
    //   dpr <  2              -> 1.0   (已经不小, 不再降)
    double m_renderScale = 1.0;
};
