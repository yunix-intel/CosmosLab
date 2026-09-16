// ============================================================================
//  camera.cpp —— 轨道相机实现 (移植自 Python 版 core/camera.py)
// ============================================================================

#include "camera.h"

#include <QtMath>
#include <cmath>

static constexpr double kPi = 3.14159265358979323846;

OrbitCamera::OrbitCamera() = default;

// ---------------------------------------------------------------------------
//  交互
// ---------------------------------------------------------------------------

void OrbitCamera::rotate(float dx, float dy)
{
    m_theta -= dx * m_rotSpeed;
    m_phi   -= dy * m_rotSpeed;

    // 限制极角, 避免在极点处视图矩阵退化
    const double lim = 0.5 * kPi / 180.0;
    m_phi = qBound(lim, m_phi, kPi - lim);
}

void OrbitCamera::zoom(float delta)
{
    // 按对数缩放, 使各尺度下的手感一致
    const double factor = std::exp(-double(delta) * m_zoomSpeed);
    m_dist = qBound(m_minDist, m_dist * factor, m_maxDist);
}

void OrbitCamera::pan(float dx, float dy)
{
    // 沿相机右/上方向平移目标点, 平移量与距离成正比
    const double scale = m_dist * 0.0016;

    QVector3D fwd = m_target - eye();
    if (fwd.length() < 1e-9f)
        fwd = QVector3D(0.0f, 0.0f, -1.0f);
    fwd.normalize();

    QVector3D right = QVector3D::crossProduct(fwd, QVector3D(0.0f, 1.0f, 0.0f));
    if (right.length() < 1e-6f)
        right = QVector3D(1.0f, 0.0f, 0.0f);
    right.normalize();

    const QVector3D up = QVector3D::crossProduct(right, fwd).normalized();
    m_target += right * float(-dx * scale) + up * float(dy * scale);
}

// ---------------------------------------------------------------------------
//  目标设定
// ---------------------------------------------------------------------------

void OrbitCamera::setTarget(const QVector3D &t)
{
    m_target = t;
}

void OrbitCamera::setDistance(double d)
{
    m_dist = qBound(m_minDist, d, m_maxDist);
}

// ---------------------------------------------------------------------------
//  阻尼插值
// ---------------------------------------------------------------------------

void OrbitCamera::update(double dt)
{
    const double dTarget = std::fabs(m_dist - m_smoothDist);
    const bool large = m_snap
            || dTarget > qMax(m_smoothDist, m_dist) * 0.55
            || dTarget > 40.0;

    if (large) {
        m_smoothDist  = m_dist;
        m_smoothTheta = m_theta;
        m_smoothPhi   = m_phi;
        m_smoothTarget = m_target;
        m_snap = false;
        m_hasEyes = false;
        return;
    }

    const double k = 1.0 - std::pow(1.0 - m_damping, dt * 60.0);
    m_smoothDist   += (m_dist - m_smoothDist) * k;
    m_smoothTheta  += (m_theta - m_smoothTheta) * k;
    m_smoothPhi    += (m_phi - m_smoothPhi) * k;
    m_smoothTarget += (m_target - m_smoothTarget) * float(k);
    m_hasEyes = false;
}

// ---------------------------------------------------------------------------
//  查询
// ---------------------------------------------------------------------------

QVector3D OrbitCamera::eye() const
{
    if (m_hasEyes)
        return m_eyeCache;

    const double sp = std::sin(m_smoothPhi);
    QVector3D e(
        m_smoothTarget.x() + float(m_smoothDist * sp * std::sin(m_smoothTheta)),
        m_smoothTarget.y() + float(m_smoothDist * std::cos(m_smoothPhi)),
        m_smoothTarget.z() + float(m_smoothDist * sp * std::cos(m_smoothTheta)));

    m_eyeCache = e;
    m_hasEyes = true;
    return e;
}

QVector3D OrbitCamera::forward() const
{
    QVector3D f = m_smoothTarget - eye();
    if (f.length() < 1e-9f)
        return QVector3D(0.0f, 0.0f, -1.0f);
    f.normalize();
    return f;
}

QVector3D OrbitCamera::rightVector() const
{
    const QVector3D f = forward();
    QVector3D r = QVector3D::crossProduct(f, QVector3D(0.0f, 1.0f, 0.0f));
    if (r.length() < 1e-6f)
        return QVector3D(1.0f, 0.0f, 0.0f);
    return r.normalized();
}

QMatrix4x4 OrbitCamera::view() const
{
    QMatrix4x4 v;
    QVector3D up(0.0f, 1.0f, 0.0f);
    // 接近极点时换一个 up 向量, 避免视图矩阵退化
    if (std::fabs(m_smoothPhi) < 1e-3 || std::fabs(m_smoothPhi - kPi) < 1e-3)
        up = QVector3D(0.0f, 0.0f, 1.0f);
    v.lookAt(eye(), m_smoothTarget, up);
    return v;
}

QMatrix4x4 OrbitCamera::projection(float aspect) const
{
    // 动态近远裁剪面 —— 深度精度取决于 far/near 的**比值**。
    // 24 位深度缓冲可靠处理的比值上限约 1e5~1e6。
    //
    // 早期版本 far = max(d*4000, 1e7) 使比值高达 7.7e8, 表现为土星环被
    // 行星"切掉"一块矩形缺口 —— 实为深度退化导致的错误遮挡。
    //
    //   near = d * 0.001   (跟随距离, 保证近处细节)
    //   far  = 8000        (固定, 覆盖太阳系 + 轨道线余量)
    const double d = qMax(m_smoothDist, 1e-4);
    const double nearPlane = qMax(d * 0.001, 1e-4);
    // far 跟随距离增长 —— 银河系视图要能拉到几千单位外 (看到整个银盘
    // 乃至邻近星系), 固定 8000 会把远处直接裁掉。
    // 下限仍是 8000, 保证太阳系视图 (轨道线一直画到远处) 不被裁。
    const double farPlane = qMax(8000.0, d * 40.0);

    QMatrix4x4 p;
    p.perspective(m_fov, aspect, float(nearPlane), float(farPlane));
    return p;
}

QMatrix4x4 OrbitCamera::viewProjection(float aspect) const
{
    return projection(aspect) * view();
}
