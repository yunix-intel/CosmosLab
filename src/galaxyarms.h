// ============================================================================
//  galaxyarms.h —— 旋臂名称与标注几何辅助
//
//  单独成文件的原因: galaxydata.h 是纯常量表 (可被任意翻译单元包含),
//  而这里需要返回 QString, 涉及 Qt 类型, 不适合放进纯数据头。
// ============================================================================

#pragma once

#include <QString>

// 旋臂数量 (与 galaxydata.h 的 kArms 一致)
int gx_armInfoCount();

// 第 i 条旋臂的中文名 (越界返回空串)
QString gx_armName(int i);

// 第 i 条旋臂的英文名
QString gx_armNameEn(int i);

// 第 i 条旋臂在极坐标中的**标注锚点角度** (弧度)。
//
// ★ 为什么不是直接用 ArmInfo::startAngleDeg:
//   对数螺旋的极角沿半径是连续变化的。旋臂在 r 处的实际方位角为
//       θ(r) = θ0 + ln(r / r_start) / tan(pitch)
//   若直接用起始角做标注锚点, 标签会落在旋臂**起点**附近 (靠近棒端),
//   而那里粒子最密, 标签会被淹没。
//   本函数按锚点半径算出实际方位角, 让标签落在旋臂中段的实体上。
double gx_armLabelAngle(int i);

// 第 i 条旋臂是否为"主旋臂" (猎户支等次级结构返回 false)
bool gx_armIsMajor(int i);
