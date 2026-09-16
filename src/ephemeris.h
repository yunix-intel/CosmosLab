// ============================================================================
//  ephemeris.h —— 星历引擎 (开普勒轨道力学)
//
//  求解流程:
//      儒略日 T
//        -> 由平均根数 + 变化率算出瞬时轨道根数 (a, e, i, L, peri, node)
//        -> 平近点角 M = L - peri
//        -> 解开普勒方程 E - e·sinE = M  (牛顿-拉弗森迭代)
//        -> 真近点角 nu 与半径 r
//        -> 在轨道平面内定位, 再按 i/peri/node 旋转到黄道坐标系
//
//  精度: 1800-2050 年约 1 角分 (Standish 近似根数的固有精度)。
//  移植自 Python 版 core/ephemeris.py。
// ============================================================================

#pragma once

#include <QVector3D>

namespace eph {

// 角度制常量
constexpr double DEG = 3.14159265358979323846 / 180.0;
constexpr double TAU = 3.14159265358979323846 * 2.0;

// ---- 时间 ----
double julianDate(int year, int month, double day);
double jdFromUnixSec(double unixSeconds);
double centuriesSinceJ2000(double jd);

// ---- 开普勒方程 ----
// 解 E - e·sinE = M, 返回偏近点角 E (弧度)。
// 大偏心率下 (冥王星 e=0.249) 用 M + e·sinM 起步可显著改善收敛。
double solveKepler(double meanAnomaly, double ecc);

// ---- 位置 ----
// 行星的日心黄道坐标 (km)。对卫星或太阳本身返回 (0,0,0)。
QVector3D heliocentricPosition(const char *bodyId, double jd);

double orbitalRadiusKm(const char *bodyId, double jd);

// 活力公式: v = sqrt(GM_sun * (2/r - 1/a)), 单位 km/s
double orbitalSpeedKms(const char *bodyId, double jd);

// 开普勒第三定律: P = 2*pi*sqrt(a^3 / mu), 单位天
double orbitalPeriodDays(double aKm, double muKm);

// ---- 轨道曲线采样 (画轨道线用) ----
// 对偏近点角 E 均匀采样 (而非平近点角 M), 这样椭圆上的点分布均匀,
// 视觉上不会有疏密。返回 segments+1 个点, 坐标单位 km。
void sampleOrbit(const char *bodyId, double jd, int segments,
                 QVector<QVector3D> &out);

// ---- 自转轴 ----
// 由 IAU 极轴赤经/赤纬求自转轴在**黄道坐标系**中的方向 (单位向量, y = 黄道北极)。
QVector3D poleDirection(double poleRaDeg, double poleDecDeg);

// 自转相位 (度)。以恒星日周期推进。
double rotationAngleDeg(double rotHours, double jd);

} // namespace eph
