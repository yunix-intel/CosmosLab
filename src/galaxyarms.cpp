// ============================================================================
//  galaxyarms.cpp —— 旋臂名称与标注几何
// ============================================================================

#include "galaxyarms.h"
#include "galaxydata.h"

#include <cmath>

int gx_armInfoCount()
{
    return gx::kArmInfoCount;
}

QString gx_armName(int i)
{
    if (i < 0 || i >= gx::kArmInfoCount)
        return QString();
    return QString::fromUtf8(gx::kArms[i].nameCn);
}

QString gx_armNameEn(int i)
{
    if (i < 0 || i >= gx::kArmInfoCount)
        return QString();
    return QString::fromUtf8(gx::kArms[i].nameEn);
}

bool gx_armIsMajor(int i)
{
    if (i < 0 || i >= gx::kArmInfoCount)
        return false;
    return gx::kArms[i].isMajor;
}

double gx_armLabelAngle(int i)
{
    if (i < 0 || i >= gx::kArmInfoCount)
        return 0.0;

    // 标注锚点半径取旋臂中段 (与 sceneitem.cpp 中的取值保持一致)
    const double r = gx::kArmStartLy
                   + (gx::kArmEndLy - gx::kArmStartLy) * 0.55;

    // ★ 对数螺旋的实际方位角:
    //     r(θ) = r0 · e^(b·θ),  b = tan(pitch)
    //   => θ(r) = ln(r / r0) / b
    //
    //   这正是旋臂粒子生成时用的关系, 因此标注能精确落在旋臂实体上。
    //   若偷懒直接用 startAngleDeg, 标签会偏到旋臂起点 (靠近棒端),
    //   那里粒子最密, 文字会被淹没 —— 而且视觉上明显"没贴在臂上"。
    const double b = std::tan(gx::kArmPitchDeg * M_PI / 180.0);
    const double dTheta = std::log(r / gx::kArmStartLy) / b;

    const double startRad = gx::kArms[i].startAngleDeg * M_PI / 180.0;
    return startRad + dTheta;
}
