// ============================================================================
//  assetroot.h —— 资源路径解析 (便携优先)
//
//  查找顺序:
//      <exe目录>/assets/<rel>  ->  <exe目录>/../assets/<rel> (开发期 build/ 下运行)
//      ->  D:/tmp/solar-system-cpp/assets/<rel>  (开发期回退)
//  找不到时返回第一项 (exe 相对路径), 便于报错时看出期望位置。
// ============================================================================

#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>

inline QString assetPath(const QString &rel)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList cands = {
        appDir + QStringLiteral("/assets/") + rel,
        appDir + QStringLiteral("/../assets/") + rel,
        QStringLiteral("D:/tmp/solar-system-cpp/assets/") + rel,
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
    const QStringList cands = {
        appDir + QStringLiteral("/assets/") + relDir,
        appDir + QStringLiteral("/../assets/") + relDir,
        QStringLiteral("D:/tmp/solar-system-cpp/assets/") + relDir,
    };
    for (const QString &d : cands) {
        if (QDir(d).exists())
            return d.endsWith(QLatin1Char('/')) ? d : d + QLatin1Char('/');
    }
    QString f = cands.first();
    return f.endsWith(QLatin1Char('/')) ? f : f + QLatin1Char('/');
}
