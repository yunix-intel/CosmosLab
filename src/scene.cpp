// ============================================================================
//  scene.cpp —— 场景装配实现
// ============================================================================

#include "scene.h"
#include "bodyregistry.h"
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

    for (int i = 0; i < registry::count(); ++i) {
        const BodyData &b = registry::allBodies()[i];
        SceneItem it;
        it.body = &b;
        it.rotationAxis = eph::poleDirection(b.poleRa, b.poleDec);
        // 彗星: kind == "comet" (扩展天体表里设定)
        it.isComet = b.kind && std::strcmp(b.kind, "comet") == 0;
        m_items.append(it);

        if (b.kind && std::strcmp(b.kind, "star") == 0)
            m_indexOfSun = m_items.size() - 1;
        if (b.id && std::strcmp(b.id, "earth") == 0)
            m_indexOfEarth = m_items.size() - 1;
    }
}

const BodyData *Scene::bodyAt(int index) const
{
    if (index < 0 || index >= registry::count())
        return nullptr;
    return &registry::allBodies()[index];
}

int Scene::bodyCount() const
{
    return registry::count();
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

            // ---- 彗尾参数 ----
            //
            // ★★ 尾巴长度必须走**与轨道相同的压缩变换** (orbitToScene),
            //    不能用一个拍脑袋的场景单位常数。
            //
            //    初版写成 tailLength = 9.0 × activity, 那个 9.0 是凭感觉
            //    取的场景单位数, 而场景里的距离是压缩过的 —— 结果是哈雷
            //    的尾巴从彗核一直延伸到木星轨道外 (跨越 5 AU), 完全失真。
            //
            //    正确做法: 用真实物理长度, 再经同一变换换算。
            //    实测依据: 哈雷在近日点附近彗尾长约 1 亿公里 (~0.67 AU);
            //    一般彗星在 1 AU 处约 0.1–0.3 AU。取
            //        L_real(r) = 0.55 AU × (r / 1 AU)^-1.5,  上限 1.0 AU
            //    (彗发活动强度随太阳辐射加热上升, 大致 ~ r^-1.5 到 r^-2;
            //     上限避免过近日点时尾巴夸张到盖住整个内太阳系)
            if (it.isComet) {
                const double rAu = qMax(it.heliocentricAu, 0.05);

                // 活动强度: 相对 1 AU 的倍数
                const double activity = std::pow(rAu, -1.5);
                // 亮度归一到 0..1
                it.tailBright = float(qBound(0.03, activity * 0.40, 1.0));

                // 真实尾长 (AU) -> 场景单位 (经同一压缩变换)
                const double tailAu  = qBound(0.05, 0.55 * activity, 1.00);
                const double rScene  = sceneconst::orbitToScene(
                    QVector3D(float(rAu * AU_KM), 0.0f, 0.0f), m_realScale).x();
                const double rTailScene = sceneconst::orbitToScene(
                    QVector3D(float((rAu + tailAu) * AU_KM), 0.0f, 0.0f),
                    m_realScale).x();
                it.tailLength = float(qMax(0.05, rTailScene - rScene));

                // 运动方向: 用有限差分求瞬时速度方向 (开普勒解已是位置函数,
                // 差分比再推一套速度公式更简单且不易出错)
                const QVector3D p1 = sceneconst::orbitToScene(
                    eph::heliocentricPosition(b->id, m_jd), m_realScale);
                const QVector3D p2 = sceneconst::orbitToScene(
                    eph::heliocentricPosition(b->id, m_jd + 0.5), m_realScale);
                const QVector3D vel = p2 - p1;
                it.velocityDir = vel.lengthSquared() > 1e-12f
                               ? vel.normalized()
                               : QVector3D(0.0f, 0.0f, 1.0f);
            }
        } else {
            // ---- 卫星: 以**母星显示半径为基准**定位 ----
            //
            // ★★ 这里有个曾经犯过的严重错误, 记录以免重犯:
            //    初版把卫星轨道写成  smaKm / kSceneUnitKm × 3.2,
            //    而天体半径走的是**幂次压缩** displayRadius(r) = k·r^0.36。
            //    两套不匹配的变换叠加后比例彻底失真:
            //      月球显示轨道 / 地球显示半径 = 1.37
            //      而真实值是 60.3 —— 差 44 倍
            //    结果是月球几乎贴着地球表面, "月球距地球 60 个地球半径"
            //    这个最基本的教学事实被画成了错的。
            //
            //    正确做法: 保持"多少个母星半径"这个**真实比例**,
            //    即  aScene = 母星显示半径 × (真实半长轴 / 母星真实半径)。
            //
            // ★ 这个公式在两种比例模式下都自动正确, 无需分支:
            //     艺术压缩: 0.9 × 60.3 = 54.3 单位 (比例关系保真)
            //     真实 1:1: 0.006371 × 60.3 = 0.3842 单位 = 384200 km ✓
            const QVector3D p = planetPos.value(QString::fromUtf8(b->parent));
            const BodyData *parentBody = registry::findBody(b->parent);
            if (!parentBody)
                continue;

            const double parentVisR =
                sceneconst::displayRadius(parentBody->radiusKm, m_realScale);
            const double radRatio = b->semiMajorKm
                                  / qMax(parentBody->radiusKm, 1.0);
            const double aScene = parentVisR * radRatio;

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
