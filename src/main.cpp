// ============================================================================
//  太阳系模拟器 · C++ + QML + OpenGL
//
//  main.cpp 只负责启动 —— 渲染在 scenerenderer.cpp, 界面在 qml/Main.qml,
//  星历在 ephemeris.cpp, 天体数据在 celestialdata.cpp (由脚本生成)。
//
//  用法:
//      太阳系模拟器.exe                          正常启动
//      SS_SELFTEST=<png路径> 太阳系模拟器.exe     无头自检抓帧后退出
// ============================================================================

#include <QGuiApplication>
#include <QQmlApplicationEngine>
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
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("太阳系模拟器"));
    app.setApplicationVersion(QStringLiteral("2.0.0"));
    app.setOrganizationName(QStringLiteral("SolarSystem"));

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
    qInfo() << "[启动] 4 加载 QML";
    engine.load(QUrl(QStringLiteral("qrc:/SolarSystem/qml/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML 加载失败 —— 检查 qrc:/SolarSystem/qml/Main.qml";
        return 1;
    }

    QObject *root = engine.rootObjects().first();
    qInfo() << "[启动] 5 QML 加载完成";
    qInfo().noquote() << "太阳系模拟器启动 (C++ / QML / OpenGL)"
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

    QTimer::singleShot(3200, &app, [&] {
        auto *win = qobject_cast<QQuickWindow *>(root);
        if (!win) {
            qCritical() << "根对象不是 QQuickWindow";
            app.exit(1);
            return;
        }

        if (auto *ri = win->rendererInterface()) {
            const auto api = ri->graphicsApi();
            qInfo().noquote() << "场景图后端:"
                              << (api == QSGRendererInterface::OpenGL ? "OpenGL"
                                                                     : "非 OpenGL(!)");
        }

        const QImage img = win->grabWindow();
        const bool ok = img.save(outPath);

        int nz = 0, tot = 0;
        for (int y = 0; y < img.height(); y += 11) {
            for (int x = 0; x < img.width(); x += 11) {
                const QColor c = img.pixelColor(x, y);
                ++tot;
                if (c.red() + c.green() + c.blue() > 30)
                    ++nz;
            }
        }

        qInfo().noquote() << QString("图像 %1x%2  保存%3  非黑像素 %4%")
                                 .arg(img.width()).arg(img.height())
                                 .arg(ok ? QStringLiteral("成功") : QStringLiteral("失败"))
                                 .arg(nz * 100.0 / qMax(tot, 1), 0, 'f', 1);

        app.exit(ok ? 0 : 1);
    });

    return app.exec();
}
