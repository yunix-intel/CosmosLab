// ============================================================================
//  galaxyarms.cpp —— 旋臂名称与标注几何
//
//  ★ 标注锚点必须与**粒子生成用同一套公式**, 否则标签会飘到臂外。
//    粒子侧 (galaxy.cpp) 的公式是:
//        R(β) = Rk · exp(-(β - βk) · tan(ψ))
//        φ    = φ_sun + β
//    这里照搬同一式子, 锚点落在臂的实体上。
//
//  ★ 锚点取该臂 β 区间的**中点**。取起点会让标签挤在靠近银心的一端
//    (粒子最密处, 文字被淹没); 取终点则可能落在臂的稀疏末端。
//    中点兼顾"在臂上"与"看得清"。
// ============================================================================

#include "galaxyarms.h"
#include "galaxydata.h"

#include <cmath>

int gx_armInfoCount()
{
    return gx::kArmSpiralCount;
}

QString gx_armName(int i)
{
    if (i < 0 || i >= gx::kArmSpiralCount)
        return QString();
    return QString::fromUtf8(gx::kArmSpiral[i].nameCn);
}

QString gx_armNameEn(int i)
{
    if (i < 0 || i >= gx::kArmSpiralCount)
        return QString();
    return QString::fromUtf8(gx::kArmSpiral[i].nameEn);
}

bool gx_armIsMajor(int i)
{
    if (i < 0 || i >= gx::kArmSpiralCount)
        return false;
    return gx::kArmSpiral[i].isMajor;
}

double gx_armLabelRadiusLy(int i)
{
    if (i < 0 || i >= gx::kArmSpiralCount)
        return 0.0;
    const gx::ArmSpiral &A = gx::kArmSpiral[i];
    const double beta = 0.5 * (A.betaBeginDeg + A.betaEndDeg);
    // ★ 同 galaxy.cpp: Δβ 必须用弧度 (用度会让半径指数爆炸)
    const double dbetaRad = (beta - A.betaKinkDeg) * M_PI / 180.0;
    const double tk = (dbetaRad >= 0.0)
                          ? std::tan(A.pitchPostDeg * M_PI / 180.0)
                          : std::tan(A.pitchPreDeg * M_PI / 180.0);
    return A.rKinkLy * std::exp(-dbetaRad * tk);
}

double gx_armLabelAngle(int i)
{
    if (i < 0 || i >= gx::kArmSpiralCount)
        return 0.0;
    const gx::ArmSpiral &A = gx::kArmSpiral[i];
    const double beta = 0.5 * (A.betaBeginDeg + A.betaEndDeg);
    const double phiSun = gx::kOrionSpurAngleDeg * M_PI / 180.0;
    return phiSun + beta * M_PI / 180.0;
}
