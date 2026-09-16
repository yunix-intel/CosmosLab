"""给 main.cpp 加上批量渲染支持，避免反复启动进程造成的窗口闪现。

★ 背景:
  每次启动进程渲染一张图，都要重新初始化 GL/纹理/粒子 (约 3s)，
  而且**每次都会创建一个窗口**。批量验证场景时要出 6-10 张图，
  连续启动会让窗口反复闪现，干扰用户。

★ 方案:
  读一个 SS_SHOTS 文件，每行描述一张图 (输出路径 + 相机参数)，
  在**同一个进程内**依次切换场景并抓帧。

★ 注意 (踩过的坑):
  QML 里写 `visible: true`，窗口在 engine.load() 期间就已显示。
  若等到加载完才 setOpacity(0)，用户会看到一次可见的窗口。
  必须在加载 QML **之前**注入 ssHeadless 上下文属性，
  让 QML 构造窗口时就把 opacity 设为 0。
  但也不能用 visible: false —— 窗口不可见时 Qt 会跳过场景图渲染，
  grabWindow() 只能抓到空白。
"""
import re

P = r'D:\tmp\solar-system-cpp\src\main.cpp'

OLD = '''    QTimer::singleShot(3200, &app, [&] {
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
    });'''

NEW = r'''    // ---- 批量渲染: 一次进程出多张图 ----
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

    auto applyShot = [root](const Shot &sh) {
        if (!sh.focus.isEmpty())
            QMetaObject::invokeMethod(root, "focusOn", Q_ARG(QVariant, sh.focus));
        auto setD = [&](const char *name, double v) {
            if (v >= 0.0)
                root->setProperty(name, v);
        };
        setD("testDist",  sh.dist);
        setD("testPhi",   sh.phi);
        setD("testTheta", sh.theta);
        if (sh.scale >= 0)
            root->setProperty("scaleLevel", sh.scale);
        setD("testJd",    sh.jd);
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

    QTimer::singleShot(0, &app, [step]() { (*step)(); });'''


def main():
    s = open(P, encoding='utf-8').read()
    if 'SS_SHOTS' in s:
        print('已经加过了，跳过')
        return
    assert OLD in s, '未找到原抓帧代码'
    s = s.replace(OLD, NEW, 1)

    # 补齐头文件
    for h in ['<QFile>', '<QFileInfo>', '<QTextStream>', '<QVector>',
              '<QStringList>', '<functional>', '<memory>']:
        if h not in s:
            s = s.replace('#include <QQmlApplicationEngine>',
                          '#include ' + h + '\n#include <QQmlApplicationEngine>', 1)

    open(P, 'w', encoding='utf-8').write(s)
    print('main.cpp 已加入批量渲染')


if __name__ == '__main__':
    main()
