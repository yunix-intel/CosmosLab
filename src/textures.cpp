// ============================================================================
//  textures.cpp —— 纹理加载实现
// ============================================================================

#include "textures.h"
#include <QFile>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QDebug>

static QString g_rootOverride;

void TextureCache::setRootDir(const QString &dir)
{
    g_rootOverride = dir;
}

TextureCache::~TextureCache()
{
    clear();
}

void TextureCache::clear()
{
    for (QOpenGLTexture *t : m_cache) {
        if (t)
            t->destroy();
        delete t;
    }
    m_cache.clear();
}

// ---------------------------------------------------------------------------
//  搜索纹理根目录
// ---------------------------------------------------------------------------
static QStringList candidateDirs()
{
    QStringList dirs;
    if (!g_rootOverride.isEmpty())
        dirs << g_rootOverride;

    const QString appDir = QCoreApplication::applicationDirPath();

    // 项目内置的真实贴图 —— **最高优先级**。
    // 开发期在 build/ 下运行, ../assets/tex 就是项目的资产目录;
    // 打包分发时贴图会拷到 exe 旁的 tex/。
    // 这条必须在 Python 版缓存之前, 否则会用到那批程序化生成的旧纹理
    // (月球会显示成"奶酪洞"而不是真实环形山照片)。
    dirs << appDir + QStringLiteral("/tex")
         << appDir + QStringLiteral("/assets/tex")
         << appDir + QStringLiteral("/../assets/tex");

    // 便携式回退: exe 旁的自定义缓存
    dirs << appDir + QStringLiteral("/cache/tex")
         << appDir + QStringLiteral("/../cache/tex");

    // 最后的回退: Python 版烘焙的旧纹理 (法线贴图仍只有这里有)
    dirs << QStringLiteral("D:/tmp/solar-system-qt/cache/tex");

    return dirs;
}

QOpenGLTexture *TextureCache::load(const QString &path)
{
    QImage img;
    if (!img.load(path)) {
        qWarning() << "[纹理] 加载失败:" << path;
        return nullptr;
    }

    // 统一转成 RGBA8 并翻转 Y —— GL 的纹理原点在左下,
    // 而 PNG 原点在左上, 不翻转会导致南北半球镜像。
    if (img.format() != QImage::Format_RGBA8888)
        img = img.convertToFormat(QImage::Format_RGBA8888);
    img = img.mirrored(false, true);

    auto *tex = new QOpenGLTexture(QOpenGLTexture::Target2D);
    tex->setSize(img.width(), img.height());
    tex->setFormat(QOpenGLTexture::RGBA8_UNorm);
    tex->allocateStorage();
    // 用最简重载: 像素格式 + 类型 + 数据。加 MipMapGeneration 参数的版本
    // 在 Qt 6 里签名不同, 会编译不过。
    tex->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, img.constBits());

    // 等距柱状贴图在 U 方向环绕, V 方向夹紧 (避免极点处出现反色带)
    tex->setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::Repeat);
    tex->setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
    tex->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
    tex->setMagnificationFilter(QOpenGLTexture::Linear);
    tex->generateMipMaps();

    return tex;
}

// ---------------------------------------------------------------------------
//  NoData 检测
//
//  ★ 为什么不在"生成贴图资产"阶段做, 而在运行时做:
//    贴图来源分散 (Commons / NASA / USGS / PDS), 每换一张就要重跑
//    离线分析。放在运行时 + 缓存, 换图后自动生效。
//
//  ★ 判据 (实测标定):
//      纯黑像素占比 > 3%  且  < 92%
//    下界 3%: 放过只有零星黑边的正常贴图 (如 Callisto 边缘 1-2 像素)。
//    上界 92%: 排除几乎整张全黑的废图 (那是该被替换的, 不是该被替换色的)。
// ---------------------------------------------------------------------------
bool TextureCache::hasNoData(const QString &kind, const QString &name) const
{
    // ★★ hasNoData 的文件名规则必须与 get() **完全一致**。
    //
    //   初版我图省事只试了 "albedo_<name>.jpg" —— 结果**一个文件都找不到**,
    //   所有天体都报"未找到文件", NoData 替换完全没生效,
    //   球面上的黑斑依旧 (实测 Ariel 仍然是"被啃掉"的样子)。
    //
    //   两处硬性差异:
    //     1. 前缀由 kind 决定: albedo_ / normal_ / ring_
    //        (kind == "misc" 时**无前缀**, 如 milkyway.jpg)
    //     2. 扩展名要试 .jpg / .jpeg / .png 三种
    const QString key = kind + QLatin1Char('/') + name;
    if (m_noData.contains(key))
        return m_noData.value(key);

    const QStringList exts{QStringLiteral(".jpg"), QStringLiteral(".jpeg"),
                           QStringLiteral(".png")};
    QStringList names;
    for (const QString &e : exts) {
        if (kind == QLatin1String("misc"))
            names << name + e;
        else
            names << kind + QLatin1Char('_') + name + e;
    }

    bool result = false;
    for (const QString &dir : candidateDirs()) {
        for (const QString &fn : names) {
            const QString path = dir + QLatin1Char('/') + fn;
            if (!QFileInfo::exists(path))
                continue;
            QImage img(path);
            if (img.isNull())
                continue;

            // 缩到 256x128 分析即可 —— 只需要"黑区占比"这个统计量
            img = img.convertToFormat(QImage::Format_RGB32)
                     .scaled(256, 128, Qt::IgnoreAspectRatio,
                             Qt::FastTransformation);
            int black = 0, total = 0;
            for (int y = 0; y < img.height(); ++y) {
                const QRgb *line =
                    reinterpret_cast<const QRgb *>(img.constScanLine(y));
                for (int x = 0; x < img.width(); ++x) {
                    const QRgb c = line[x];
                    ++total;
                    if (qRed(c) < 20 && qGreen(c) < 20 && qBlue(c) < 20)
                        ++black;
                }
            }
            if (total > 0) {
                const double frac = double(black) / double(total);
                result = (frac > 0.03 && frac < 0.92);
            }
            break;
        }
        if (result)
            break;
    }

    m_noData.insert(key, result);
    return result;
}

QOpenGLTexture *TextureCache::get(const QString &kind, const QString &name)
{
    if (name.isEmpty())
        return nullptr;

    const QString key = kind + QLatin1Char('/') + name;
    if (m_cache.contains(key))
        return m_cache.value(key);

    // 文件名规则: albedo_earth.png/jpg / normal_earth.jpg / ring_saturn.png
    //             milkyway 是单张, 无前缀
    //
    // ★ 扩展名要同时试多种: 真实贴图来自 solarsystemscope (JPEG) 与
    //   Commons (JPEG/PNG), 而旧缓存是 PNG。只认 .png 会完全找不到新资产。
    //   顺序上 .jpg 优先 —— 新资产都是 JPEG, 且体积小得多。
    const QStringList exts{QStringLiteral(".jpg"), QStringLiteral(".jpeg"),
                           QStringLiteral(".png")};
    QStringList names;
    for (const QString &e : exts) {
        if (kind == QLatin1String("misc"))
            names << name + e;
        else
            names << kind + QLatin1Char('_') + name + e;
    }

    QOpenGLTexture *tex = nullptr;
    for (const QString &dir : candidateDirs()) {
        for (const QString &fn : names) {
            const QString path = dir + QLatin1Char('/') + fn;
            if (QFileInfo::exists(path)) {
                tex = load(path);
                if (tex)
                    break;
            }
        }
        if (tex)
            break;
    }

    m_cache.insert(key, tex);   // 失败也缓存 nullptr, 避免每帧重试
    return tex;
}
