// ============================================================================
//  宇宙实验室 CosmosLab · C++ + QML + OpenGL
//
//  main.cpp 只负责启动 —— 渲染在 scenerenderer.cpp, 界面在 qml/Main.qml,
//  星历在 ephemeris.cpp, 天体数据在 celestialdata.cpp (由脚本生成)。
//
//  用法:
//      cosmoslab.exe                          正常启动
//      SS_SELFTEST=<png路径> cosmoslab.exe     无头自检抓帧后退出
// ============================================================================

#include <QGuiApplication>
#include <QCoreApplication>
#include <QFont>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QVector>
#include <QStringList>
#include <functional>
#include <memory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QTimer>
#include <QDateTime>
#include <QImage>
#include <QDir>
#include <QDebug>

int main(int argc, char **argv)
{
    // ------------------------------------------------------------------
    //  必须最先做: 指定场景图后端为 OpenGL。
    //
    //  Qt 6 在 Windows 上默认用 Direct3D 11 作为 RHI 后端。而
    //  QQuickFramebufferObject::Renderer 里我们调用的是原生 OpenGL,
    //  与 D3D11 后端不兼容 —— 表现为该节点被整个跳过, createRenderer()
    //  一次都不进, 界面上是一块黑且**没有任何报错**。
    //
    //  这一行必须在任何 QQuickWindow 实例创建之前执行。
    // ------------------------------------------------------------------
    qInfo() << "[启动] 1 指定 OpenGL 后端";
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(4);
    fmt.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(fmt);

    qInfo() << "[启动] 2 构造 QGuiApplication";
    // ★ 中文小字号清晰度: 强制全 hinting + 整数度量。
    //
    //   默认策略下, 10-11px 的中文会出现笔画粘连/发虚 —— 因为
    //   Qt 用的是系统的字体渲染提示, 在 96 DPI 下对小字号 CJK 不够。
    //   PreferFullHinting 会把字形吸附到像素网格, 小字明显更锐利。
    //
    //   (开发机有双显示器且缩放不同, 窗口落在哪块屏 DPR 就不同,
    //    自检图分辨率也会变; 这属于环境差异, 无法从代码统一,
    //    但 hinting 能保证**任何** DPR 下中文都不糊。)
    QFont::insertSubstitution(QStringLiteral("Microsoft YaHei"),
                              QStringLiteral("Microsoft YaHei"));
    QGuiApplication app(argc, argv);
    {
        QFont f = app.font();
        f.setHintingPreference(QFont::PreferFullHinting);
        app.setFont(f);
    }
    app.setApplicationName(QStringLiteral("宇宙实验室"));
    app.setApplicationVersion(QStringLiteral("2.0.0"));
    app.setOrganizationName(QStringLiteral("CosmosLab"));

    // 控件样式: FluentWinUI3 是 Qt 6.11 新增的 Windows 11 原生风格
    const QByteArray style = qEnvironmentVariableIsSet("SS_STYLE")
                                 ? qgetenv("SS_STYLE")
                                 : QByteArrayLiteral("FluentWinUI3");
    qputenv("QT_QUICK_CONTROLS_STYLE", style);

    // 允许通过环境变量指定纹理目录 (开发期指向 Python 版烘焙结果)
    const QByteArray texRoot = qgetenv("SS_TEX_DIR");
    if (!texRoot.isEmpty())
        qputenv("SS_TEX_DIR", texRoot);

    qInfo() << "[启动] 3 构造 QML 引擎";
    QQmlApplicationEngine engine;

    // ★ 绿色包: exe 旁的 qml/ 即 QML import 路径。
    //   主 QML 已编译进 qrc, 但 QtQuick.Layouts/Controls 等是外部插件 DLL,
    //   不加这句, 脱离开发环境 (无 QT_QML_IMPORT_PATH) 即报
    //   module "QtQuick.Layouts" is not installed。
    engine.addImportPath(
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));

    // ---- ★★ 自检模式必须"从第一帧就透明" ----
    //
    // 踩过的坑: 初版在 QML 加载**之后**才 setOpacity(0) —— 但 QML 里
    // 写的是 visible: true, 窗口在 engine.load() 期间就已经显示出来了。
    // 于是每次自检渲染都会有一次肉眼可见的窗口闪现 (实测批量渲染时
    // 连续闪 8 次, 非常干扰)。
    //
    // 正解: 在加载 QML **之前**注入一个上下文属性, 让 QML 在构造窗口时
    // 就把 opacity 设为 0 —— 窗口创建的第一帧即不可见, 没有任何闪现。
    //
    // 注意不能改成 visible: false: 窗口不可见时 Qt 会跳过场景图渲染,
    // grabWindow() 抓到的会是空白 (实测过 offscreen 平台也是同样问题)。
    // opacity 0 的窗口仍然正常渲染, 只是不显示。
    // ★ 基准测试 (SS_BENCH) 同样让窗口透明 —— 它要跑好几秒,
    //   不该在用户屏幕上一直显示一个窗口。
    engine.rootContext()->setContextProperty(
        QStringLiteral("ssHeadless"),
        !qEnvironmentVariable("SS_SELFTEST").isEmpty()
            || qEnvironmentVariableIntValue("SS_BENCH") > 0);


    qInfo() << "[启动] 4 加载 QML";
    engine.load(QUrl(QStringLiteral("qrc:/SolarSystem/qml/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML 加载失败 —— 检查 qrc:/SolarSystem/qml/Main.qml";
        return 1;
    }

    QObject *root = engine.rootObjects().first();
    qInfo() << "[启动] 5 QML 加载完成";

    qInfo().noquote() << "宇宙实验室 CosmosLab 启动 (C++ / QML / OpenGL)"
                      << "根对象:" << root->metaObject()->className();

    // ---- 自检模式: 让窗口对用户不可见, 但保持正常渲染 ----
    //
    // ★ 为什么不是"真无头":
    //   试过 QT_QPA_PLATFORM=offscreen —— 退出码 0 且出图, 但尺寸变成
    //   1440x900 (dpr 丢失), 内容近乎全黑: offscreen 插件下
    //   QQuickFramebufferObject 的原生 GL 渲染路径根本没执行。
    //   自检需要**真实的窗口**才能走完整 GL 场景图。
    //
    // ★ 也不用 setPosition 移出屏幕:
    //   虽然渲染正确, 但窗口脱离所有显示器后 DWM 不再合成它,
    //   实测单张耗时 33s~2min (对照: 留在屏幕内约 35s) —— 收益不明确,
    //   且万一被窗口管理器 clamp 回屏幕内, 用户还是会被闪到。
    //
    // ★ 更不要动 window flags:
    //   setFlag(Qt::Tool) / setFlag(WindowTransparentForInput) 会触发窗口
    //   重建, 连几何尺寸都跟着变 —— 自检图从 2880x1800 变成 2882x1847,
    //   构图整体偏移, 前后不可比。
    //
    // 结论: 单设 opacity 0 最稳 —— 窗口属性一律不变, 用户看不见内容。
    const QString outPath = qEnvironmentVariable("SS_SELFTEST");
    if (!outPath.isEmpty()) {
        if (auto *w = qobject_cast<QQuickWindow *>(root))
            w->setOpacity(0.0);
    }

    // ---- 轨道求解自检 ----
    //
    // 用 SS_ORBTEST=1 打开: 打印各天体的日心距与轨道速度, 用于与
    // JPL Horizons 的实测值对照。这是验证开普勒求解器正确性的关键手段 ——
    // 尤其是极端偏心率的目标 (哈雷 e=0.967, NEOWISE e=0.99918),
    // 求解器若在近日点附近不收敛, 日心距会明显偏离。
    if (qEnvironmentVariableIsSet("SS_ORBTEST")) {
        extern void runOrbitSelfTest();
        runOrbitSelfTest();
        return 0;
    }

    // ---- 帧率基准 ----
    //
    // ★ 这段必须放在下面 `if (outPath.isEmpty()) return app.exec();` **之前**。
    //   初版放在后面, 结果是: 只给 SS_BENCH 而不给 SS_SELFTEST 时, 程序在
    //   那行就 return 进了正常 GUI 循环, 基准代码根本没注册 —— 进程永远
    //   不退出 (实测跑了 15 分钟还没结束)。纯基准模式必须能自行终止。
    //
    // 场景是连续重绘的 (onTick 每 16ms 调用 update()), 所以统计窗口重绘
    // 次数即可得到真实帧率。
    //
    // 统计方式: 用 frameSwapped 信号 —— 它是"这一帧真的呈现到屏幕"的回调,
    // 比在 render() 里计数更接近用户实际感受 (含合成开销)。
    const QByteArray benchMs = qgetenv("SS_BENCH");
    const bool benchMode = !benchMs.isEmpty() && benchMs.toInt() > 0;
    if (benchMode) {
        auto *w = qobject_cast<QQuickWindow *>(root);
        if (w) {
            auto *counter = new int(0);
            auto *t0 = new qint64(0);
            const int dur = qMax(500, benchMs.toInt());
            QObject::connect(w, &QQuickWindow::frameSwapped, w,
                             [counter] { ++(*counter); });
            // 先等场景稳定 (纹理加载、首帧编译) 再开始计时
            QTimer::singleShot(3200, &app, [&app, w, counter, t0, dur] {
                *counter = 0;
                *t0 = QDateTime::currentMSecsSinceEpoch();
                QTimer::singleShot(dur, &app, [&app, w, counter, t0, dur] {
                    const qint64 el = QDateTime::currentMSecsSinceEpoch() - *t0;
                    const double fps = el > 0 ? (*counter * 1000.0 / el) : 0.0;
                    qInfo().noquote()
                        << QString("[基准] %1 帧 / %2 ms  =  %3 FPS   (%4x%5)")
                               .arg(*counter).arg(el)
                               .arg(fps, 0, 'f', 1)
                               .arg(qRound(w->width() * w->devicePixelRatio()))
                               .arg(qRound(w->height() * w->devicePixelRatio()));
                    delete counter;
                    delete t0;
                    app.exit(0);
                });
            });
        }
    }

    // ---- 无头自检 ----
    // 只给 SS_BENCH 时也走正常事件循环 (基准需要窗口真的在跑),
    // 退出由上面的基准定时器负责。
    if (outPath.isEmpty())
        return app.exec();

    // ---- 批量渲染: 一次进程出多张图 ----
    //
    // 用法: SS_SELFTEST=<首张路径>  SS_SHOTS=<清单文件>
    //   清单每行: <输出路径>|<focus>|<dist>|<phi>|<theta>|<scale>|<jd>
    //   空字段表示沿用默认值。以 # 开头的行为注释。
    //
    // 只有 SS_SELFTEST 时退化为"一张的批量", 行为与原来一致。
    struct Shot {
        QString out, focus;
        double dist = -1, phi = -1, theta = -1;
        int    scale = -1;
        double jd = -1;
    };
    QVector<Shot> shots;
    {
        Shot first;
        first.out = outPath;
        shots.append(first);

        const QString listPath = qEnvironmentVariable("SS_SHOTS");
        if (!listPath.isEmpty()) {
            shots.clear();
            QFile f(listPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&f);
                while (!in.atEnd()) {
                    const QString line = in.readLine().trimmed();
                    if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                        continue;
                    const QStringList p = line.split(QLatin1Char('|'));
                    if (p.isEmpty() || p[0].trimmed().isEmpty())
                        continue;
                    Shot sh;
                    sh.out = p[0].trimmed();
                    auto numAt = [&](int i) -> double {
                        return (p.size() > i && !p[i].trimmed().isEmpty())
                                   ? p[i].trimmed().toDouble() : -1.0;
                    };
                    if (p.size() > 1) sh.focus = p[1].trimmed();
                    sh.dist  = numAt(2);
                    sh.phi   = numAt(3);
                    sh.theta = numAt(4);
                    sh.scale = (p.size() > 5 && !p[5].trimmed().isEmpty())
                                   ? p[5].trimmed().toInt() : -1;
                    sh.jd    = numAt(6);
                    shots.append(sh);
                }
            } else {
                qWarning() << "无法打开 SS_SHOTS:" << listPath;
            }
        }
        qInfo().noquote() << QString("自检模式: 共 %1 张").arg(shots.size());
    }

    // ★ 直接调用 C++ 的 testShot(), 而不是逐个设 QML 属性。
    //
    //   初版写成 root->setProperty("testDist", ...) —— 那要求 QML 侧
    //   额外定义一堆 testXxx 属性并逐个转发回 C++, 链路长且容易漏参数。
    //   直接从 C++ 一次设完更可靠。
    //
    //   注意 root 是 QML 的 ApplicationWindow, scene 属性才是 SolarScene。
    auto applyShot = [root](const Shot &sh) {
        // ★ 注意: QML 里写的是 id: scene, 而 **id 不是属性** ——
        //   C++ 的 root->property("scene") 取不到它 (会返回无效 QVariant)。
        //   正确做法是在 QML 侧加 objectName: "scene", 再用 findChild 定位。
        QObject *scene = root->findChild<QObject *>(QStringLiteral("scene"));
        if (!scene) {
            qWarning() << "找不到 objectName=scene 的对象, 无法应用镜头参数";
            return;
        }
        QMetaObject::invokeMethod(
            scene, "testShot",
            Q_ARG(QString, sh.focus),
            Q_ARG(double, sh.dist),
            Q_ARG(double, sh.phi),
            Q_ARG(double, sh.theta),
            Q_ARG(int, sh.scale),
            Q_ARG(double, sh.jd));
    };

    auto idx   = std::make_shared<int>(0);
    auto fails = std::make_shared<int>(0);
    auto step  = std::make_shared<std::function<void()>>();

    *step = [&app, root, shots, idx, fails, step, applyShot]() {
        if (*idx >= shots.size()) {
            qInfo().noquote() << QString("=== 自检完成: %1 张, %2 张失败 ===")
                                     .arg(shots.size()).arg(*fails);
            app.exit(*fails == 0 ? 0 : 1);
            return;
        }
        const Shot &sh = shots[*idx];
        applyShot(sh);

        // 首张要多等: 纹理与粒子尚在初始化
        const int waitMs = (*idx == 0) ? 3200 : 1500;
        QTimer::singleShot(waitMs, &app,
                           [&app, root, shots, idx, fails, step, sh]() {
            auto *win = qobject_cast<QQuickWindow *>(root);
            if (!win) {
                qCritical() << "根对象不是 QQuickWindow";
                app.exit(1);
                return;
            }
            if (*idx == 0) {
                if (auto *ri = win->rendererInterface()) {
                    const auto api = ri->graphicsApi();
                    qInfo().noquote() << "场景图后端:"
                                      << (api == QSGRendererInterface::OpenGL
                                              ? "OpenGL" : "非 OpenGL(!)");
                }
            }

            const QImage img = win->grabWindow();
            const bool ok = img.save(sh.out);

            int nz = 0, tot = 0;
            for (int y = 0; y < img.height(); y += 11) {
                for (int x = 0; x < img.width(); x += 11) {
                    const QColor c = img.pixelColor(x, y);
                    ++tot;
                    if (c.red() + c.green() + c.blue() > 30)
                        ++nz;
                }
            }
            const double pct = nz * 100.0 / qMax(tot, 1);
            if (!ok || pct < 1.0)
                ++(*fails);

            qInfo().noquote() << QString("[%1/%2] %3  %4x%5  非黑 %6%  %7")
                                     .arg(*idx + 1).arg(shots.size())
                                     .arg(QFileInfo(sh.out).fileName())
                                     .arg(img.width()).arg(img.height())
                                     .arg(pct, 0, 'f', 1)
                                     .arg(ok ? QStringLiteral("保存成功")
                                             : QStringLiteral("保存失败"));

            ++(*idx);
            QTimer::singleShot(0, &app, [step]() { (*step)(); });
        });
    };

    QTimer::singleShot(0, &app, [step]() { (*step)(); });

    return app.exec();
}
