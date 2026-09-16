// ============================================================================
//  scene.h —— 场景装配: 把星历坐标变成可绘制的场景项
//
//  移植自 Python 版 core/renderer.py 的场景布局部分。
//
//  两处关键的「艺术化压缩」—— 没有它们, 真实比例下画面既看不到内行星、
//  也看不到行星本体:
//
//   1. 轨道半径幂次压缩 (orbit_exag)
//      真实比例下海王星在 4498 单位外 (1 单位 = 1e6 km), 内行星全挤在中心的
//      几个像素里。用 r' = r^exag 压缩后内外行星才能同框。
//
//   2. 天体半径幂次压缩 (DISPLAY_*)
//      木星半径是水星的 29 倍, 真实比例下水星只是个点。用 r^0.36 压缩后
//      小天体仍然可见 (地球恰好约 0.9 单位)。
// ============================================================================

#pragma once

#include <QMatrix4x4>
#include <QString>
#include <QVector>
#include <QVector3D>

struct BodyData;

// ---------------------------------------------------------------------------
//  场景常量
// ---------------------------------------------------------------------------
namespace sceneconst {
inline constexpr double kSceneUnitKm    = 1.0e6;   // 1 场景单位 = 1e6 km
inline constexpr double kOrbitExag      = 0.6;     // 轨道半径压缩指数 (默认)
inline constexpr double kSizeExag       = 0.36;    // 天体半径压缩指数 (默认)
inline constexpr double kMoonOrbitExag  = 3.2;     // 卫星轨道放大 (真实半长轴紧贴行星不可见)
inline constexpr double kDisplayK       = 0.9;     // 与 kSizeExag 配合: Earth -> 0.9 单位
inline constexpr double kAuUnits        = 1.495978707e8 / kSceneUnitKm;  // 1 AU = 149.6 单位

// 真实比例模式下天体远小于一个像素 (地球 0.0064 单位, 而它在 150 单位
// 外的角直径只有约 0.0025°)。完全不放大就什么都看不到, 教学上毫无意义。
//
// 折中方案: 给一个**最小可见尺寸**, 使天体至少占几像素。这当然不是真实
// 大小, 所以 UI 上必须明确标注"天体尺寸已放大以便观察, 轨道与间距为真实
// 比例" —— 让学生知道**间距是真实的**, 只有球体被适当放大以免看不见。
// 这是所有教学天文软件 (Stellarium、Celestia) 的通行做法。
inline constexpr double kMinVisibleUnits = 0.06;

// 按模式计算半径与轨道坐标
double displayRadius(double radiusKm, bool realScale);
QVector3D orbitToScene(const QVector3D &km, bool realScale);
QString formatAu(double au);
} // namespace sceneconst

// ---------------------------------------------------------------------------
//  一个可绘制天体
// ---------------------------------------------------------------------------
struct SceneItem
{
    const BodyData *body = nullptr;
    QVector3D center;              // 场景坐标
    float     radius = 1.0f;       // 场景单位
    QVector3D rotationAxis{0.0f, 1.0f, 0.0f};
    float     rotationDeg = 0.0f;  // 自转相位
    QVector3D parentCenter;        // 母星/太阳位置 (供 UI 算日心距)
    double    heliocentricAu = 0.0;
    double    speedKms = 0.0;
    bool      hasOrbit = false;    // 是否画轨道线

    // ---- 彗星专用 (仅彗星有彗尾) ----
    bool      isComet = false;
    float     tailLength = 0.0f;   // 彗尾长度 (场景单位)
    float     tailBright = 0.0f;   // 0..1
    QVector3D velocityDir;         // 轨道运动方向 (单位向量)

    QMatrix4x4 modelMatrix() const;
};

// ---------------------------------------------------------------------------
//  场景
// ---------------------------------------------------------------------------
class Scene
{
public:
    Scene();

    // 设定时间 (儒略日)。默认 = 当前真实时刻。
    void setJulianDate(double jd) { m_jd = jd; }
    double julianDate() const { return m_jd; }

    // 按当前 jd 重新计算全部天体位置
    void update();

    const QVector<SceneItem> &items() const { return m_items; }
    const SceneItem *itemById(const QString &id) const;

    // 太阳位置 (场景坐标), 光源就放在这里
    QVector3D sunPosition() const { return m_sunPos; }

    // 时间流速: 秒(真实) -> 秒(模拟)。1.0 表示实时。
    void setTimeScale(double s) { m_timeScale = s; }
    double timeScale() const { return m_timeScale; }

    // 真实比例模式。
    // false (默认) = 艺术压缩, 内外行星同框, 适合观赏与讲解轨道形态;
    // true         = 1:1 真实比例, 用于纠正尺度认知 —— 学生能亲眼看到
    //                "地球在 1 AU 处到底有多小"。
    void setRealScale(bool r);
    bool realScale() const { return m_realScale; }

    // 用真实时间推进 (dt 为真实秒)
    void advance(double realDtSeconds);

    const BodyData *bodyAt(int index) const;
    int bodyCount() const;

private:
    double m_jd = 0.0;
    double m_timeScale = 1.0;
    bool   m_realScale = false;
    QVector<SceneItem> m_items;
    QVector3D m_sunPos{0.0f, 0.0f, 0.0f};

    void rebuildBodies();
    int  m_indexOfSun = -1;
    int  m_indexOfEarth = -1;
};
