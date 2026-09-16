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

    // ★ 贴图是否含 NoData (未测绘区, 即大片纯黑)
    //
    //   部分天体的贴图是部分覆盖的航天器影像, 未拍摄区域是纯黑。
    //   着色器据此把近黑像素替换成按反照率着色的底色,
    //   避免球面上出现"被啃掉"的黑斑。
    //
    //   判据: 纯黑 (RGB 全 < 20) 像素占比 > 3% 且 < 92%
    //     * 上界 92% 是为了排除"整张图几乎全黑"的废图
    //     * 下界 3%  是为了放过只有零星黑边的正常贴图
    bool hasNoData(const QString &kind, const QString &name) const;

    // 银河背景 (单张, kind 固定)
    QOpenGLTexture *milkyWay() { return get(QStringLiteral("misc"),
                                            QStringLiteral("milkyway")); }

    void clear();

    // 纹理根目录 (可在启动时覆盖)
    static void setRootDir(const QString &dir);

private:
    mutable QHash<QString, bool> m_noData;   // 贴图名 -> 是否含 NoData
    QOpenGLTexture *load(const QString &path);

    QHash<QString, QOpenGLTexture *> m_cache;
};
