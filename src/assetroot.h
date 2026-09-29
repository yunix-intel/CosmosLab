// ============================================================================
//  assetroot.h —— 资源路径解析 (便携优先)
//
//  查找顺序 (Release):
//      <exe目录>/assets/<rel>  ->  <exe目录>/../assets/<rel> (开发期 build/ 下运行)
//  找不到时返回第一项 (exe 相对路径), 便于报错时看出期望位置。
//
//  ★ P1-1 修复 (2026-09-28, DeepSeek v1.2 测试报告):
//    旧版第三项是硬编码开发者本机绝对路径
//    "D:/tmp/solar-system-cpp/assets/" —— 在开发机上, 包内缺件会被该
//    回退静默兜住, README 承诺的"缺件降级"分支永远不可达, 降级测试
//    全是假通过; 且发布包会读取包外资源, 破坏"绿色包自包含"承诺。
//    现仅在 Debug 构建保留该回退 (方便开发期单步调试), Release 移除。
// ============================================================================

#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>

inline QString assetPath(const QString &rel)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList cands = {
        appDir + QStringLiteral("/assets/") + rel,
        appDir + QStringLiteral("/../assets/") + rel,
#ifdef QT_DEBUG
        QStringLiteral("D:/tmp/solar-system-cpp/assets/") + rel,
#endif
    };
    for (const QString &p : cands) {
        if (QFile::exists(p))
            return p;
    }
    return cands.first();
}

inline QString assetDir(const QString &relDir)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList cands = {
        appDir + QStringLiteral("/assets/") + relDir,
        appDir + QStringLiteral("/../assets/") + relDir,
#ifdef QT_DEBUG
        QStringLiteral("D:/tmp/solar-system-cpp/assets/") + relDir,
#endif
    };
    for (const QString &d : cands) {
        if (QDir(d).exists())
            return d.endsWith(QLatin1Char('/')) ? d : d + QLatin1Char('/');
    }
    QString f = cands.first();
    return f.endsWith(QLatin1Char('/')) ? f : f + QLatin1Char('/');
}
