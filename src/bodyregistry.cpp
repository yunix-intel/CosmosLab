// ============================================================================
//  bodyregistry.cpp —— 全天体统一注册表实现
// ============================================================================

#include "bodyregistry.h"
#include "smalldata.h"

#include <QVector>
#include <cstring>

namespace registry {

// ---------------------------------------------------------------------------
//  合并表: 基础集 + 扩展集
//
//  构建一次并缓存。注意用**单一静态实例** —— 若把 static 分别写在多个
//  访问器里, 会各自构造一份完整副本, 白白浪费内存, 且两个副本地址不同,
//  调试时容易误判成"指针变了"。
// ---------------------------------------------------------------------------
static const QVector<BodyData> &table()
{
    static const QVector<BodyData> t = [] {
        QVector<BodyData> v;
        v.reserve(ALL_BODIES_COUNT + small::count());
        for (int i = 0; i < ALL_BODIES_COUNT; ++i)
            v.append(ALL_BODIES[i]);
        const small::SmallBody *sb = small::bodies();
        for (int i = 0; i < small::count(); ++i)
            v.append(sb[i].data);
        return v;
    }();
    return t;
}

const BodyData *allBodies()
{
    return table().constData();
}

int count()
{
    return int(table().size());
}

// ---------------------------------------------------------------------------
//  统一查找
// ---------------------------------------------------------------------------

const BodyData *findBody(const char *id)
{
    if (!id)
        return nullptr;

    // 基础集 (八大行星等) —— 数据量小, 线性查找足够
    for (int i = 0; i < ALL_BODIES_COUNT; ++i)
        if (std::strcmp(ALL_BODIES[i].id, id) == 0)
            return &ALL_BODIES[i];

    // 扩展集 (矮行星/卫星/小行星/彗星)
    if (const small::SmallBody *sb = small::find(id))
        return &sb->data;

    return nullptr;
}

const OrbitalElements *findOrbit(const char *id)
{
    if (!id)
        return nullptr;

    for (int i = 0; i < ORBITAL_ELEMENTS_COUNT; ++i)
        if (std::strcmp(ORBITAL_ELEMENTS[i].id, id) == 0)
            return &ORBITAL_ELEMENTS[i];

    const OrbitalElements *ext = small::orbitalElements();
    for (int i = 0; i < small::ORBITAL_ELEMENTS_COUNT; ++i)
        if (std::strcmp(ext[i].id, id) == 0)
            return &ext[i];

    return nullptr;
}

// ---------------------------------------------------------------------------
//  分类标签
//
//  教学上必须严格区分这四类 (IAU 2006 决议):
//    恒星 / 行星 / 矮行星 / 卫星 + 小行星 / 彗星
//  把谷神星写成"行星"或把冥王星写成"行星"都是明确的事实错误。
// ---------------------------------------------------------------------------

const char *categoryCn(const char *id)
{
    if (!id)
        return "";
    if (const small::SmallBody *sb = small::find(id))
        return small::typeNameCn(sb->type);

    // 基础集: 靠 parent 与 kind 判别
    const BodyData *b = findBody(id);
    if (!b)
        return "";
    if (b->kind && std::strcmp(b->kind, "star") == 0)
        return small::typeNameCn(small::BodyType::Star);
    if (b->parent != nullptr)
        return small::typeNameCn(small::BodyType::Moon);
    // 冥王星在基础集中但分类是矮行星 —— 按 id 特判, 避免把它的
    // kind 字段 (值为 "pluto") 误当成行星。
    if (std::strcmp(id, "pluto") == 0)
        return small::typeNameCn(small::BodyType::DwarfPlanet);
    return small::typeNameCn(small::BodyType::Planet);
}

const char *categoryEn(const char *id)
{
    if (!id)
        return "";
    if (const small::SmallBody *sb = small::find(id))
        return small::typeNameEn(sb->type);

    const BodyData *b = findBody(id);
    if (!b)
        return "";
    if (b->kind && std::strcmp(b->kind, "star") == 0)
        return small::typeNameEn(small::BodyType::Star);
    if (b->parent != nullptr)
        return small::typeNameEn(small::BodyType::Moon);
    if (std::strcmp(id, "pluto") == 0)
        return small::typeNameEn(small::BodyType::DwarfPlanet);
    return small::typeNameEn(small::BodyType::Planet);
}

const char *typeNote(const char *id)
{
    if (!id)
        return nullptr;
    if (const small::SmallBody *sb = small::find(id))
        return sb->typeNote;
    return nullptr;
}

} // namespace registry
