// ============================================================================
//  agndata.h —— 活动星系核 / 类星体 / 射电星系具名代表数据 (B.2)
//
//  对标: Carroll & Ostlie 第 27-28 章; Karttunen 第 19 章;
//  EHT 2019 (M87*) / 2022 (Sgr A*) 一手证认。
//  M87 / Sgr A* 已在 cosmosdata, 本文件只收新增项, 不重复。
//
//  双文本约定与 stellardata 一致: desc 教科书精准, pop 通俗科普。
// ============================================================================

#pragma once

struct AgnEntry
{
    const char *id;
    const char *nameCn;
    const char *nameEn;
    const char *catCn;      // Seyfert / 类星体 / 耀变体 / 射电星系 / 超大黑洞
    double distMly;         // 距地球 (百万光年); 高红移用光行距离常用值
    double redshift;        // 红移 z; <0 表示不适用
    double massLog10;       // 中心黑洞质量 log10 Msun; <0 表示未知
    const char *desc;
    const char *pop;
};

extern const AgnEntry AGN_ENTRIES[];
extern const int      AGN_COUNT;

const AgnEntry *findAgn(const char *id);
