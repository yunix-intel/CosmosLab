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
    Q_PROPERTY(bool    showBelts   READ showBelts   WRITE setShowBelts   NOTIFY showBeltsChanged)
    // 真实比例模式 —— 教学关键: 关闭艺术压缩, 让尺度关系如实呈现
    Q_PROPERTY(bool    realScale   READ realScale   WRITE setRealScale   NOTIFY realScaleChanged)
    Q_PROPERTY(QString dateText    READ dateText                          NOTIFY dateTextChanged)
    Q_PROPERTY(int     bodyCount   READ bodyCount   CONSTANT)
    // ★ 属性名用 scaleLevel 而非 scale —— QML 的 Item 已有 scale 属性
    //   (item 缩放因子), 同名会遮蔽并导致绑定错乱。
    Q_PROPERTY(int     scaleLevel  READ scale       WRITE setScale        NOTIFY scaleChanged)


    // ---- 宇宙视图性能面板的数据 ----
    //
    // ★★ 关键教训: 这四个值**必须是 Q_PROPERTY**, 不能靠 QML 调用
    //    C++ 函数取值。踩过的三段弯路:
    //      1. `property var perf: scene.cosmosPerf()` —— binding 确实重算
    //         (日志看到值从 0 变 71206), 但 Text 绑定 perf.xxx 不更新:
    //         QML 不追踪 var 属性的**内部字段**变化。
    //      2. QML Timer `running: !perf.ready` —— QVariantMap 缺键时
    //         JQ 得 undefined, 取反仍 false, 定时器永不启动。
    //      3. `running: perf.total <= 0` —— 同样首帧 undefined 比较为
    //         false, 定时器同样不启动。
    //    Q_PROPERTY + NOTIFY 是 Qt 唯一可靠的跨线程→QML 通知机制。
    // 测试用: SS_CARD=<天体id> 启动时自动打开详情卡 (供截图验证)
    // ---- 宇宙视图的距离映射模式 ----
    //
    // ★ 两种模式展示**同一批数据**, 只是距离轴的性质不同:
    //     对数压缩 (0) —— 一屏容纳 0.2 Mly ~ 46.5 Gly 共六个数量级,
    //                     代价是远处间隔被压缩 (看图会以为远处挤在一起)
    //     真实比例 (1) —— 距离成比例。但此时本星系群只有 5e-5 的画面占比,
    //                     屏幕上不可见 —— 这在物理上**正确**。
    //   做成开关是为了让"把宇宙塞进一屏付出了什么代价"这件事可见。
    Q_PROPERTY(int cosmosMapMode READ cosmosMapMode WRITE setCosmosMapMode
               NOTIFY cosmosMapModeChanged)

    Q_PROPERTY(QString testCard READ testCard CONSTANT)
    // 测试用: 模拟点击 3D 标签 (见 sceneitem.cpp 里 SS_MARKER 的说明)。
    // ★ CONSTANT 是合适的 —— 它在构造时定好, 之后不变。
    Q_PROPERTY(QString testMarker READ testMarker CONSTANT)
    // 测试用: SS_HUBBLE=1 时启动即打开哈勃图面板 (供自动化截图验证)
    Q_PROPERTY(bool    testHubble READ testHubble CONSTANT)
    // 测试用: SS_EVO=<scriptId>[:<prog01]> 启动即打开演化播放器并定位
    Q_PROPERTY(QString testEvo READ testEvo CONSTANT)
    // 测试用: SS_STELLAR=1 启动即显示恒星面板
    Q_PROPERTY(bool testStellar READ testStellar CONSTANT)
    // 测试用: SS_POP=1 启动即科普版 (proMode=false)
    Q_PROPERTY(bool testPop READ testPop CONSTANT)

    Q_PROPERTY(int     cosmosTotal    READ cosmosTotal    NOTIFY cosmosTotalChanged)
    Q_PROPERTY(int     cosmosVisible  READ cosmosVisible  WRITE setCosmosVisible
               NOTIFY cosmosVisibleChanged)
    // ---- SDSS 真实星系的显示数量 ----
    //
    // ★ 与"星系数量"档位分开, 因为两者意图不同:
    //     星系数量档位 —— 控制**程序生成的示意结构** (看宇宙网形态)
    //     SDSS 开关    —— 控制**实测星系** (看真实分布)
    //   SDSS 数据位于顶点缓冲末尾, 可独立截断。
    Q_PROPERTY(int     sdssVisible READ sdssVisible WRITE setSdssVisible
               NOTIFY sdssVisibleChanged)
    Q_PROPERTY(int     sdssTotal   READ sdssTotal   NOTIFY sdssVisibleChanged)

    // ★ 实际绘制数 (最近一帧 glDrawArrays 的数量)。
    //
    //   cosmosTotal 是**缓冲总数**, 而"星系数量"档位和"SDSS 开关"
    //   都会二次削减绘制量 —— UI 若显示 total 会高估。这个属性给
    //   UI 一个真实值, 性能预估也基于它计算。
    Q_PROPERTY(int     cosmosDrawn READ cosmosDrawn NOTIFY cosmosPerfChanged)

    Q_PROPERTY(double  cosmosEstMs    READ cosmosEstMs    NOTIFY cosmosPerfChanged)
    Q_PROPERTY(double  cosmosEstFps   READ cosmosEstFps   NOTIFY cosmosPerfChanged)
    Q_PROPERTY(QString scaleName   READ scaleName                         NOTIFY scaleChanged)
    // 太阳在银盘中的位置, 投影到屏幕后的归一化坐标 (0..1)。
    // QML 叠加层用它画"太阳系在这里"的标注 —— 改用 QML 的原因是
    // OpenGL core profile 的线宽上限仅 1px, 细线在星场上完全看不见。
    Q_PROPERTY(double  sunMarkX    READ sunMarkX                          NOTIFY sunMarkChanged)
    Q_PROPERTY(double  sunMarkY    READ sunMarkY                          NOTIFY sunMarkChanged)
    Q_PROPERTY(bool    sunMarkOn   READ sunMarkOn                         NOTIFY sunMarkChanged)

    // ---- 银河系全景标注 (旋臂 / 银心 / 标尺) ----
    // 用 QML 覆盖层绘制: GL core profile 的线宽上限 1px 且无法画文字。
    // 每个标注含屏幕位置 (归一化 0..1) 与名称, 由 C++ 投影得到。
    Q_PROPERTY(QVariantList galaxyLabels READ galaxyLabels               NOTIFY galaxyLabelsChanged)
    // 视野宽度 (光年) —— 用于显示标尺
    Q_PROPERTY(double  galaxyViewWidthLy READ galaxyViewWidthLy          NOTIFY galaxyLabelsChanged)

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
    bool showBelts() const { return m_showBelts; }
    void setShowBelts(bool v);
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
    QVariantList galaxyLabels() const { return m_galaxyLabels; }
    double galaxyViewWidthLy() const { return m_galaxyViewWidthLy; }

    double sunMarkX() const { return m_sunMarkX; }
    double sunMarkY() const { return m_sunMarkY; }
    bool   sunMarkOn() const { return m_sunMarkOn; }

    // ---- 供 QML 调用 ----
    Q_INVOKABLE QVariantList bodyList() const;                 // 天体列表 (列表用)
    Q_INVOKABLE QVariantMap  bodyInfo(const QString &id) const; // 详情面板
    Q_INVOKABLE QVariantMap  galaxyInfo() const;                // 银河系数据面板
    Q_INVOKABLE QVariantList galaxyNotes() const;               // 银河系教学要点
    // ★ 通俗版教学要点 (选项一: 跟随全局 proMode)。
    //   与原版一一对应 (同索引), QML 按开关选择展示哪一套。
    Q_INVOKABLE QVariantList galaxyNotesPop() const;
    Q_INVOKABLE QVariantList cosmosNotesPop() const;
    Q_INVOKABLE QVariantMap  cosmosInfo() const;                 // 宇宙学参数
    // 粒子数与预估 GPU 耗时 (性能开关的反馈)
    Q_INVOKABLE QVariantMap  cosmosPerf() const;
    // 具名天体的详情 (含真实照片路径)。id 为空或找不到时返回空 map。
    //
    // ★ photo 字段是**空字符串**表示"暂无实景图" —— 界面据此
    //   显示提示, 而不是拿一张不相关的图冒充。这符合"只做能确定
    //   真实的"原则。
    Q_INVOKABLE QVariantMap  galaxyDetail(const QString &id) const;

    // 有实景图的天体 id 列表 (供 UI 标记哪些可点开看照片)
    Q_INVOKABLE QStringList  galaxiesWithPhoto() const;
    Q_INVOKABLE QVariantList cosmosNotes() const;                // 宇宙教学要点

    // 哈勃图数据 (Pantheon+ Ia 型超新星)。
    //
    // ★ 为什么由 C++ 读文件而不是 QML 用 XHR:
    //   QML 的 XHR 对 file:// 的支持依赖构建配置, 容易在换环境时突然失效;
    //   而 C++ 直接 QFile 读 + QJsonDocument 解析是最稳的。
    // ★ 解析一次后缓存 —— 这个文件 29 KB, 每次打开面板都重读没必要。
    Q_INVOKABLE QVariantMap  hubbleData() const;
    Q_INVOKABLE QVariantList cosmosStructures() const;           // 大尺度结构清单
    // B.1/B.2 新增代表: 恒星链 / AGN-星系-暂现源。QML 按分组展示。
    Q_INVOKABLE QVariantList stellarList() const;
    Q_INVOKABLE QVariantList agnList() const;
    Q_INVOKABLE QVariantMap  stellarDetail(const QString &id) const;
    Q_INVOKABLE QVariantMap  agnDetail(const QString &id) const;
    // B.5 星际介质/星云/星团。QML 第三个分组展示, 复用同一 galaxyCard。
    Q_INVOKABLE QVariantList ismList() const;
    Q_INVOKABLE QVariantMap  ismDetail(const QString &id) const;
    Q_INVOKABLE void focusOn(const QString &id);

    // ---- 配音播放 (WinMM mciSendString, 无新增依赖) ----
    //
    //  ★ 调用方显式传 usePop (QML 侧取 !proMode), C++ 不持有版本状态,
    //    避免两边状态不同步: true -> <narr>_pop.mp3 (B 女音读 zh_pop),
    //    false -> <narr>_pro.mp3 (A 男音读 zh)。
    //    文件缺失时返回空串并警告, 不崩溃 (配音包可选安装)。
    //  ★ 停止: 传空 id 即停。
    //  ★ 多语种 (v1.1): playNarrationLang(narrId, lang), lang ∈
    //    {zh, pop, yue, ja, en}, 文件 <narr>_<lang>.mp3
    //    (zh 复用 _pro.mp3, pop 复用 _pop.mp3, 其余 _yue/_ja/_en)。
    //    hasNarration 供 QML 置灰按钮 (无音频不崩, 只播不了)。
    Q_INVOKABLE QString playNarration(const QString &narrId, bool usePop);
    Q_INVOKABLE QString playNarrationLang(const QString &narrId, const QString &lang);
    Q_INVOKABLE bool hasNarration(const QString &narrId, const QString &lang) const;

    // ---- 测试/自检专用: 一次设定全套视角参数 ----
    // 供 main.cpp 的批量渲染调用, 避免逐个属性设值时遗漏。
    // 传 -1 / 空 表示"沿用当前值"。立即到位, 不走平滑动画。
    Q_INVOKABLE void testShot(const QString &focusId,
                              double dist, double phiDeg, double thetaDeg,
                              int scaleLevel, double jd);
    int  cosmosVisible() const { return m_cosmosVisible; }
    int  cosmosTotal() const { return m_cosmosTotal; }
    int  cosmosDrawn() const { return m_cosmosDrawn; }
    QString testCard() const;
    QString testMarker() const { return m_testMarker; }
    bool testHubble() const { return m_testHubble; }
    QString testEvo() const { return m_testEvo; }
    bool testStellar() const { return m_testStellar; }
    bool testPop() const { return m_testPop; }

    int  cosmosMapMode() const { return m_cosmosMapMode; }
    void setCosmosMapMode(int m);
    double cosmosEstMs() const { return m_cosmosEstMs; }
    int  sdssVisible() const { return m_sdssVisible; }
    int  sdssTotal() const { return m_sdssTotal; }
    void setSdssVisible(int n);
    double cosmosEstFps() const { return m_cosmosEstFps; }
    void setCosmosVisible(int n);

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
    void showBeltsChanged();
    void realScaleChanged();
    void dateTextChanged();
    void scaleChanged();
    void cosmosVisibleChanged();
    void cosmosTotalChanged();
    void cosmosPerfChanged();
    void sdssVisibleChanged();
    void cosmosMapModeChanged();
    void sunMarkChanged();
    void galaxyLabelsChanged();

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
    int     m_cosmosVisible = 0;   // 宇宙可见粒子数, <=0 全部
    bool    m_cosmosReady = false; // 宇宙粒子总数是否已就绪
    int     m_cosmosTotal = 0;     // 宇宙粒子总数 (就绪后填入)
    int     m_cosmosDrawn = 0;     // 最近一帧实际绘制数 (含 SDSS 开关的削减)
    int     m_cosmosMapMode = 0;   // 0=对数压缩 1=真实比例
    int     m_sdssVisible = 0;     // SDSS 可见数 (0=全部)
    int     m_sdssTotal = 0;       // SDSS 总数
    double  m_cosmosEstMs = 0.0;   // 预估 GPU 耗时 (按实测系数标定)
    double  m_cosmosEstFps = 0.0;  // 预估帧率
    int     m_lastVis = -1;        // 上次算过的可见数 (避免重复发信号)
    QString m_testMarker;          // 测试用: 模拟点击的标签 (SS_MARKER)
    bool    m_testHubble = false;  // 测试用: 启动即开哈勃图 (SS_HUBBLE)
    QString m_testEvo;             // 测试用: SS_EVO=<id>[:<prog>] 开演化播放器
    bool    m_testStellar = false; // 测试用: SS_STELLAR=1 显示恒星面板
    bool    m_testPop = false;     // 测试用: SS_POP=1 启动即科普版
    bool    m_showBelts = true;
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

    QVariantList m_galaxyLabels;        // 银河系标注 (位置 + 名称)
    mutable QVariantMap m_hubbleCache;  // 哈勃图数据 (首次读取后缓存)
    double       m_galaxyViewWidthLy = 0.0;

    // 渲染分辨率缩放。默认按屏幕 DPR 自适应:
    //   dpr >= 2 (高分辨率屏) -> 0.72  (像素太多, 降一点肉眼无感)
    //   dpr <  2              -> 1.0   (已经不小, 不再降)
    double m_renderScale = 1.0;
};
