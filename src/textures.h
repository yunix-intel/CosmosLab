// ============================================================================
//  textures.h —— 纹理加载与缓存
//
//  纹理来源: Python 版已经烘焙好的 PNG (35 张, 20 MB), 直接从磁盘读取。
//  这样 C++ 版不必先重写 953 行的程序化纹理生成 (numpy -> C++ 工作量大),
//  可以先把渲染管线跑通, 后续再把生成器移植过来。
//
//  查找顺序 (便携优先):
//      <exe目录>/tex  ->  <exe目录>/cache/tex  ->  <工作目录>/cache/tex
//      ->  D:/tmp/solar-system-qt/cache/tex     (开发期回退)
// ============================================================================

#pragma once

#include <QHash>
#include <QOpenGLTexture>
#include <QString>

class TextureCache
{
public:
    ~TextureCache();

    // 取纹理 (首次调用时加载)。name 不含前缀与扩展名, 如 "earth"。
    // kind: "albedo" / "normal" / "ring"
    QOpenGLTexture *get(const QString &kind, const QString &name);

    // 银河背景 (单张, kind 固定)
    QOpenGLTexture *milkyWay() { return get(QStringLiteral("misc"),
                                            QStringLiteral("milkyway")); }

    void clear();

    // 纹理根目录 (可在启动时覆盖)
    static void setRootDir(const QString &dir);

private:
    QOpenGLTexture *load(const QString &path);

    QHash<QString, QOpenGLTexture *> m_cache;
};
