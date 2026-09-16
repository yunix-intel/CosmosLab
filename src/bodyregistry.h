// ============================================================================
//  bodyregistry.h —— 全天体统一注册表
//
//  ★ 为什么需要这一层:
//    src/celestialdata.cpp 是由 tools/gen_data.py **自动生成**的, 手工修改
//    会在下次重新生成时被覆盖。而太阳系小天体(矮行星/小行星/彗星/卫星)是
//    手工维护的数据, 不宜塞进生成流程。
//
//    因此把"基础集 + 扩展集"的合并与统一查找放在这里 —— 这是手写文件,
//    可以安全演进。上层代码只需通过本文件的访问器遍历/查找天体,
//    完全不必关心数据来自哪张表。
// ============================================================================

#pragma once

#include "celestialdata.h"

namespace registry {

// 全部天体 (基础集在前, 扩展集在后)。顺序稳定, 指针在整个进程生命周期有效。
const BodyData *allBodies();
int             count();

// 统一查找: 先查基础集, 再查扩展集。找不到返回 nullptr。
const BodyData *findBody(const char *id);

// 统一轨道根数查找。基础集用 JPL 近似表 (含每世纪变化率),
// 扩展集由 smalldata.cpp 按 JPL 原始根数转换生成。
const OrbitalElements *findOrbit(const char *id);

// 天体分类标签 (恒星/行星/矮行星/卫星/小行星/彗星)。
// 用于详情面板显示 —— 教学上必须明确区分, 不能把矮行星写成行星。
// 返回中文名; 未分类的天体返回空字符串。
const char *categoryCn(const char *id);
const char *categoryEn(const char *id);

// 该天体的教学要点 (扩展天体有, 基础天体返回 nullptr)
const char *typeNote(const char *id);

} // namespace registry
