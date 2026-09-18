// ============================================================================
//  stellardata.h —— 恒星 / 残骸 / 变星 / 系外行星具名代表数据 (B.1)
//
//  对标: Carroll & Ostlie 第 8/10-13/15-16 章; Karttunen 第 10-11 章。
//  数值以 SIMBAD / NED / 原始论文常用值为准, 入库前已按"常用值+出处方向"
//  核对; 不确定处 (如参宿四距离) 在 desc 中明确注区间。
//
//  双文本约定 (用户确认新增):
//    desc —— 教科书精准说明 (术语完整, 数字带条件)
//    pop  —— 通俗科普说明 (无公式, 一句话类比, 可独立阅读)
//  QML 详情卡以切换选项同时提供两者, 默认显示 desc。
// ============================================================================

#pragma once

struct StellarEntry
{
    const char *id;
    const char *nameCn;
    const char *nameEn;
    const char *catCn;      // 分组: 光谱型 / 演化阶段 / 残骸 / 变星超新星 / 系外行星
    double distLy;          // 距地球 (光年); <0 表示不适用/河外 (见 desc)
    const char *spec;       // 光谱型或类型标签, 如 "A0V"
    double teff;            // 有效温度 (K); <0 表示不适用
    double massSol;         // 质量 (太阳质量); <0 表示不适用/未知
    const char *desc;       // 教科书精准说明
    const char *pop;        // 通俗科普说明
};

extern const StellarEntry STELLAR_ENTRIES[];
extern const int          STELLAR_COUNT;

const StellarEntry *findStellar(const char *id);
