// ============================================================================
//  scene.cpp —— 场景装配实现
// ============================================================================

#include "scene.h"
#include "celestialdata.h"
#include "ephemeris.h"

#include <QDateTime>
#include <QtMath>
#include <cmath>

// ---------------------------------------------------------------------------
//  场景常量
// ---------------------------------------------------------------------------

namespace sceneconst {

double displayRadius(double radiusKm, bool realScale)
{
    if (realScale) {
        // 真实比例: 直接换算。6371 km 的地球 = 0.0064 场景单位。
        // 加可见下限only是为了"还能看见一个点"。
        return qMax(radiusKm / kSceneUnitKm, kMinVisibleUnits);
    }

    // 幂次压缩: 真实比例下水星 (2440 km) 相对木星 (69911 km) 只是个点。
    // 指数 0.36 时地球 (6371 km) 恰好约 0.9 场景单位。
    constexpr double ref = 6371.0;                       // 地球半径
    constexpr double k = 0.9 / std::pow(ref, kSizeExag);
    return k * std::pow(qMax(radiusKm, 1.0), kSizeExag);
}

QVector3D orbitToScene(const QVector3D &km, bool realScale)
{
    QVector3D a = km / float(kSceneUnitKm);
    // 真实比例下不做幂次压缩 —— 只做单位换算。
    if (!realScale && std::fabs(kOrbitExag - 1.0) > 1e-9) {
        const double r = a.length();
        if (r > 1e-9) {
            const double factor = std::pow(r, kOrbitExag) / r;
            a *= float(factor);
        }
    }
    return a;
}

QString formatAu(double au)
{
    return QString::number(au, 'f', 4);
}

} // namespace sceneconst

// ---------------------------------------------------------------------------
//  SceneItem
// ---------------------------------------------------------------------------

QMatrix4x4 SceneItem::modelMatrix() const
{
    QMatrix4x4 m;
    m.translate(center);
    m.scale(radius);

    // 自转轴朝向: 把默认 +Y 轴旋到 rotationAxis
    const QVector3D axis = rotationAxis.normalized();
    const QVector3D up(0.0f, 1.0f, 0.0f);
    const float d = QVector3D::dotProduct(up, axis);
    if (d < 0.9999f) {
        if (d < -0.9999f) {
            m.rotate(180.0f, 1.0f, 0.0f, 0.0f);
        } else {
            const QVector3D rot = QVector3D::crossProduct(up, axis);
            m.rotate(qRadiansToDegrees(std::acos(qBound(-1.0f, d, 1.0f))),
                     rot.normalized());
        }
    }
    m.rotate(rotationDeg, 0.0f, 1.0f, 0.0f);
    return m;
}

// ---------------------------------------------------------------------------
//  Scene
// ---------------------------------------------------------------------------

Scene::Scene()
{
    // 默认 = 当前真实时刻。
    // 用户确认过: 打开软件应停在「此刻真实时刻」, 而不是以 1 天/秒推进。
    m_jd = eph::jdFromUnixSec(double(QDateTime::currentSecsSinceEpoch()));
    rebuildBodies();
}

void Scene::rebuildBodies()
{
    m_items.clear();
    m_indexOfSun = -1;
    m_indexOfEarth = -1;

    for (int i = 0; i < ALL_BODIES_COUNT; ++i) {
        const BodyData &b = ALL_BODIES[i];
        SceneItem it;
        it.body = &b;
        it.rotationAxis = eph::poleDirection(b.poleRa, b.poleDec);
        m_items.append(it);

        if (b.kind && std::strcmp(b.kind, "star") == 0)
            m_indexOfSun = m_items.size() - 1;
        if (b.id && std::strcmp(b.id, "earth") == 0)
            m_indexOfEarth = m_items.size() - 1;
    }
}

const BodyData *Scene::bodyAt(int index) const
{
    if (index < 0 || index >= ALL_BODIES_COUNT)
        return nullptr;
    return &ALL_BODIES[index];
}

int Scene::bodyCount() const
{
    return ALL_BODIES_COUNT;
}

void Scene::advance(double realDtSeconds)
{
    m_jd += realDtSeconds * m_timeScale / 86400.0;
}

void Scene::setRealScale(bool r)
{
    if (m_realScale == r)
        return;
    m_realScale = r;
    update();     // 半径与轨道坐标都要按新模式重算
}

void Scene::update()
{
    // ---- 太阳 ----
    const QVector3D sunScene(0.0f, 0.0f, 0.0f);
    m_sunPos = sunScene;

    // ---- 行星: 真实星历 ----
    QHash<QString, QVector3D> planetPos;
    for (SceneItem &it : m_items) {
        const BodyData *b = it.body;
        if (!b || b->parent != nullptr)
            continue;
        if (b->kind && std::strcmp(b->kind, "star") == 0)
            continue;

        const QVector3D km = eph::heliocentricPosition(b->id, m_jd);
        const QVector3D sc = sceneconst::orbitToScene(km, m_realScale);
        planetPos.insert(QString::fromUtf8(b->id), sc);
    }

    // ---- 写入各项 ----
    for (SceneItem &it : m_items) {
        const BodyData *b = it.body;
        if (!b)
            continue;

        it.radius = float(sceneconst::displayRadius(b->radiusKm, m_realScale));
        it.rotationDeg = float(eph::rotationAngleDeg(b->rotHours, m_jd)
                               + b->rotationOffset);

        if (b->kind && std::strcmp(b->kind, "star") == 0) {
            it.center = sunScene;
            it.parentCenter = sunScene;
            it.heliocentricAu = 0.0;
            it.speedKms = 0.0;
            it.hasOrbit = false;
        } else if (b->parent == nullptr) {
            const QVector3D sc = planetPos.value(QString::fromUtf8(b->id));
            it.center = sc;
            it.parentCenter = sunScene;
            it.hasOrbit = true;

            const double r = eph::orbitalRadiusKm(b->id, m_jd);
            it.heliocentricAu = r / AU_KM;
            it.speedKms = eph::orbitalSpeedKms(b->id, m_jd);
        } else {
            // 卫星: 简化为绕母星的圆轨道, 按真实半长轴与周期。
            // 放大系数只在艺术压缩模式下生效 —— 真实比例模式下必须保持
            // 1:1, 否则"月球离地球多远"这个基本事实就被扭曲了。
            const QVector3D p = planetPos.value(QString::fromUtf8(b->parent));
            const double moonExag = m_realScale ? 1.0 : sceneconst::kMoonOrbitExag;
            const double aScene = b->semiMajorKm / sceneconst::kSceneUnitKm
                                * moonExag;
            const double period = qMax(std::fabs(b->periodDays), 1e-6);
            const double phase = (m_jd / period) * 2.0 * M_PI;
            const double inc = b->inc * eph::DEG;

            const double x = std::cos(phase) * aScene;
            const double z = std::sin(phase) * aScene;
            const double y = std::sin(phase * 0.5) * aScene * std::sin(inc);

            it.center = p + QVector3D(float(x), float(y), float(z));
            it.parentCenter = p;
            it.hasOrbit = false;

            const QVector3D rel = it.center - p;
            it.heliocentricAu = double(rel.length()) * sceneconst::kSceneUnitKm / AU_KM;
            it.speedKms = period > 0.0
                    ? 2.0 * M_PI * b->semiMajorKm / (period * 86400.0)
                    : 0.0;
        }
    }
}

const SceneItem *Scene::itemById(const QString &id) const
{
    const QByteArray utf8 = id.toUtf8();
    for (const SceneItem &it : m_items) {
        if (it.body && std::strcmp(it.body->id, utf8.constData()) == 0)
            return &it;
    }
    return nullptr;
}
