"""让旋臂标签锚点跟随 Reid 2019 的逐臂参数。

★ 旧实现按"统一螺距角 12° + 固定起止半径"算标签角度。
  现在旋臂改用逐臂螺距角与折点, 标签必须用**同一套公式**, 否则
  标签会飘到臂外 —— 那正是之前注释里警惕过的问题。

★ 新做法: 取该臂 β 区间的中点, 用与粒子生成完全相同的
  R(β) 公式算出锚点, 保证标签必然落在臂的实体上。
"""

C = r'D:\tmp\solar-system-cpp\src\galaxyarms.cpp'

NEW = r'''// ============================================================================
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
    const double dbeta = beta - A.betaKinkDeg;
    const double tk = (dbeta >= 0.0)
                          ? std::tan(A.pitchPostDeg * M_PI / 180.0)
                          : std::tan(A.pitchPreDeg * M_PI / 180.0);
    return A.rKinkLy * std::exp(-dbeta * tk);
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
'''


def main():
    open(C, 'w', encoding='utf-8').write(NEW)
    print('galaxyarms.cpp 已重写')

    # 头文件补一个半径查询 (sceneitem 可能要用)
    H = r'D:\tmp\solar-system-cpp\src\galaxyarms.h'
    h = open(H, encoding='utf-8').read()
    if 'gx_armLabelRadiusLy' not in h:
        h = h.replace('double gx_armLabelAngle(int i);',
                      'double gx_armLabelAngle(int i);\n\n'
                      '// 第 i 条臂的标注锚点半径 (ly), 与粒子生成同一公式\n'
                      'double gx_armLabelRadiusLy(int i);')
        h = h.replace('// 旋臂数量 (与 galaxydata.h 的 kArms 一致)',
                      '// 结构段数量 (含 4 条主臂 + 外臂 + 猎户支), 与 kArmSpiral 一致')
        open(H, 'w', encoding='utf-8').write(h)
        print('galaxyarms.h 已补声明')

    # galaxy.cpp 删除已不用的旧变量
    G = r'D:\tmp\solar-system-cpp\src\galaxy.cpp'
    g = open(G, encoding='utf-8').read()
    g = g.replace('''    const float rArm0   = float(gx::kArmStartLy     * ly2u);       // 47.3
    const float rArm1   = float(gx::kArmEndLy       * ly2u);       // 94.6
''', '')
    g = g.replace('''    const float pitch   = float(gx::kArmPitchDeg * M_PI / 180.0);
    const float bCoef   = std::tan(pitch);
''', '')
    open(G, 'w', encoding='utf-8').write(g)
    print('galaxy.cpp 已清理旧变量')


if __name__ == '__main__':
    main()
