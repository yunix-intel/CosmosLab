// ============================================================================
//  ismdata.h —— 星际介质 / 星云 / 星团具名代表数据 (B.5)
//
//  对标: Carroll & Ostlie 第 5/12/13 章; Karttunen 第Ⅳ部分。
//  第二编 (ISM: 发射/反射/暗星云、分子云、超泡、本地泡、银心分子环)
//  + 第三编 (星团: 疏散/球状/星协) 按需补齐。超新星遗迹只收
//  恒星链未覆盖者 (M1/第谷已在 stellardata, 此处不再重复)。
//
//  双文本约定与 stellardata 一致: desc 教科书精准, pop 通俗科普。
// ============================================================================

#pragma once

struct IsmEntry
{
    const char *id;
    const char *nameCn;
    const char *nameEn;
    const char *catCn;      // 发射星云 / 反射星云 / 暗星云 / 行星状星云 /
                            // 超新星遗迹 / 分子云 / 星际泡 / 银心结构 /
                            // 疏散星团 / 球状星团 / 星协
    double distLy;          // 距地球 (光年); 0 = 太阳系位于其内部 (本地泡)
    double sizeLy;          // 典型尺度/直径 (光年); <0 表示不适用
    const char *desc;
    const char *pop;
};

extern const IsmEntry ISM_ENTRIES[];
extern const int      ISM_COUNT;

const IsmEntry *findIsm(const char *id);
