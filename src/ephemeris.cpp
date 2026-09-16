// ============================================================================
//  ephemeris.cpp —— 星历引擎实现 (移植自 Python 版 core/ephemeris.py)
// ============================================================================

#include "ephemeris.h"
#include "bodyregistry.h"
#include "celestialdata.h"

#include <QVector>
#include <cmath>

namespace eph {

static const double KEPLER_TOL      = 1e-13;
static const int    KEPLER_MAX_ITER = 60;

// ---------------------------------------------------------------------------
//  时间
// ---------------------------------------------------------------------------

double julianDate(int year, int month, double day)
{
    int y = year, m = month;
    if (m <= 2) {
        y -= 1;
        m += 12;
    }
    const int a = y / 100;
    const int b = 2 - a + a / 4;
    return std::floor(365.25 * (y + 4716)) + std::floor(30.6001 * (m + 1))
           + day + b - 1524.5;
}

double jdFromUnixSec(double unixSeconds)
{
    return 2440587.5 + unixSeconds / 86400.0;
}

double centuriesSinceJ2000(double jd)
{
    return (jd - J2000) / 36525.0;
}

// ---------------------------------------------------------------------------
//  开普勒方程
// ---------------------------------------------------------------------------

double solveKepler(double meanAnomaly, double ecc)
{
    // 归一到 [-pi, pi], 提升收敛性
    double M = std::fmod(meanAnomaly + M_PI, TAU) - M_PI;

    // ★ 初值选择决定高偏心率下能否收敛。
    //   低偏心率用经典牛顿起步 (E = M + e·sin M) 即可。
    //   但 e → 1 时该起步点极差: 例如 e = 0.99918 (NEOWISE 彗星),
    //   在近日点附近 M≈0 而 E 的实际解接近 0, 牛顿法却会因导数
    //   f' = 1 - e·cos E ≈ 0 而剧烈震荡, 60 次迭代都可能不收敛。
    //   改用 Danby 起步 (0.85·e·sign(sin M) 的偏置), 它把初值推向
    //   解所在的一侧, 对 e 直到 0.9999 都稳定收敛。
    double E;
    if (ecc < 0.8) {
        E = M + ecc * std::sin(M);
    } else {
        // sign(0) 取 +1, 避免 sin(M)=0 时初值停在 M 上
        const double s = (std::sin(M) >= 0.0) ? 1.0 : -1.0;
        E = M + 0.85 * ecc * s;
    }

    for (int i = 0; i < KEPLER_MAX_ITER; ++i) {
        const double f  = E - ecc * std::sin(E) - M;
        const double fp = 1.0 - ecc * std::cos(E);
        if (std::fabs(fp) < 1e-14)
            break;
        const double dE = f / fp;
        E -= dE;
        if (std::fabs(dE) < KEPLER_TOL)
            break;
    }
    return E;
}

// ---------------------------------------------------------------------------
//  位置
// ---------------------------------------------------------------------------

QVector3D heliocentricPosition(const char *bodyId, double jd)
{
    const OrbitalElements *el = registry::findOrbit(bodyId);
    if (!el)
        return QVector3D(0.0f, 0.0f, 0.0f);

    const double T = centuriesSinceJ2000(jd);

    // 瞬时轨道根数
    const double a    = el->a    + el->ra    * T;
    const double e    = qBound(0.0, el->e + el->re * T, 0.99999);
    const double inc  = el->inc  + el->ri    * T;
    const double L    = el->L    + el->rL    * T;
    const double peri = el->peri + el->rperi * T;
    const double node = el->node + el->rnode * T;

    // 平近点角 -> 偏近点角
    const double E = solveKepler((L - peri) * DEG, e);

    // 轨道平面内坐标
    const double xOrb = a * (std::cos(E) - e);
    const double yOrb = a * std::sqrt(qMax(0.0, 1.0 - e * e)) * std::sin(E);

    // 由 (peri, node, i) 旋转到黄道坐标系: R_z(-om) · R_x(-i) · R_z(-w)
    const double w  = (peri - node) * DEG;   // 近日点幅角
    const double om = node * DEG;            // 升交点黄经
    const double i  = inc * DEG;

    const double cw = std::cos(w),  sw = std::sin(w);
    const double co = std::cos(om), so = std::sin(om);
    const double ci = std::cos(i),  si = std::sin(i);

    const double x = (cw * co - sw * so * ci) * xOrb
                   + (-sw * co - cw * so * ci) * yOrb;
    const double y = (cw * so + sw * co * ci) * xOrb
                   + (-sw * so + cw * co * ci) * yOrb;
    const double z = (sw * si) * xOrb + (cw * si) * yOrb;

    return QVector3D(float(x * AU_KM), float(y * AU_KM), float(z * AU_KM));
}

double orbitalRadiusKm(const char *bodyId, double jd)
{
    const QVector3D p = heliocentricPosition(bodyId, jd);
    return std::sqrt(double(p.x()) * p.x()
                   + double(p.y()) * p.y()
                   + double(p.z()) * p.z());
}

double orbitalSpeedKms(const char *bodyId, double jd)
{
    const OrbitalElements *el = registry::findOrbit(bodyId);
    if (!el)
        return 0.0;

    const double T = centuriesSinceJ2000(jd);
    const double a = (el->a + el->ra * T) * AU_KM;
    const double r = orbitalRadiusKm(bodyId, jd);
    if (r <= 0.0 || a <= 0.0)
        return 0.0;
    return std::sqrt(qMax(0.0, GM_SUN * (2.0 / r - 1.0 / a)));
}

double orbitalPeriodDays(double aKm, double muKm)
{
    if (aKm <= 0.0 || muKm <= 0.0)
        return 0.0;
    return TAU * std::sqrt(aKm * aKm * aKm / muKm) / 86400.0;
}

// ---------------------------------------------------------------------------
//  轨道曲线采样
// ---------------------------------------------------------------------------

void sampleOrbit(const char *bodyId, double jd, int segments,
                 QVector<QVector3D> &out)
{
    out.clear();

    const OrbitalElements *el = registry::findOrbit(bodyId);
    if (!el)
        return;

    const double T = centuriesSinceJ2000(jd);
    const double a    = el->a    + el->ra    * T;
    const double e    = qBound(0.0, el->e + el->re * T, 0.99999);
    const double inc  = el->inc  + el->ri    * T;
    const double peri = el->peri + el->rperi * T;
    const double node = el->node + el->rnode * T;

    const double w  = (peri - node) * DEG;
    const double om = node * DEG;
    const double i  = inc * DEG;

    const double cw = std::cos(w),  sw = std::sin(w);
    const double co = std::cos(om), so = std::sin(om);
    const double ci = std::cos(i),  si = std::sin(i);

    const double b = a * std::sqrt(qMax(0.0, 1.0 - e * e));

    out.reserve(segments + 1);
    for (int k = 0; k <= segments; ++k) {
        const double E = TAU * double(k) / double(segments);
        const double xo = a * (std::cos(E) - e);
        const double yo = b * std::sin(E);

        const double x = (cw * co - sw * so * ci) * xo
                       + (-sw * co - cw * so * ci) * yo;
        const double y = (cw * so + sw * co * ci) * xo
                       + (-sw * so + cw * co * ci) * yo;
        const double z = (sw * si) * xo + (cw * si) * yo;

        out.append(QVector3D(float(x * AU_KM), float(y * AU_KM), float(z * AU_KM)));
    }
}

// ---------------------------------------------------------------------------
//  自转轴与自转相位
// ---------------------------------------------------------------------------

QVector3D poleDirection(double poleRaDeg, double poleDecDeg)
{
    const double ra  = poleRaDeg * DEG;
    const double dec = poleDecDeg * DEG;

    // 赤道坐标 -> 黄道坐标 (黄赤交角 23.43928°)
    const double eps = 23.43928 * DEG;

    const double xEq = std::cos(dec) * std::cos(ra);
    const double yEq = std::cos(dec) * std::sin(ra);
    const double zEq = std::sin(dec);

    // 绕 x 轴旋转 -eps
    const double yEc =  yEq * std::cos(eps) + zEq * std::sin(eps);
    const double zEc = -yEq * std::sin(eps) + zEq * std::cos(eps);

    return QVector3D(float(xEq), float(yEc), float(zEc));
}

double rotationAngleDeg(double rotHours, double jd)
{
    if (rotHours == 0.0)
        return 0.0;
    const double hoursSinceJ2000 = (jd - J2000) * 24.0;
    const double turns = hoursSinceJ2000 / rotHours;
    return (turns - std::floor(turns)) * 360.0;
}

} // namespace eph
