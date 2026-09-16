"""把 main.cpp 的 applyShot 改为调用 scene 的 testShot()。

★ 为什么不逐个设 QML 属性:
  初版在 main.cpp 里写 root->setProperty("testDist", ...) 之类，
  但那要求 QML 侧额外定义 testDist/testPhi/... 一堆属性，每条都要
  再转发到 C++ 的 m_forcedDist / m_camPhi / ...，链路长且易漏。
  改为直接调用 C++ 方法，一次设完。
"""
P = r'D:\tmp\solar-system-cpp\src\main.cpp'

OLD = '''    auto applyShot = [root](const Shot &sh) {
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
    };'''

NEW = '''    // ★ 直接调用 C++ 的 testShot(), 而不是逐个设 QML 属性。
    //
    //   初版写成 root->setProperty("testDist", ...) —— 那要求 QML 侧
    //   额外定义一堆 testXxx 属性并逐个转发回 C++, 链路长且容易漏参数。
    //   直接从 C++ 一次设完更可靠。
    //
    //   注意 root 是 QML 的 ApplicationWindow, scene 属性才是 SolarScene。
    auto applyShot = [root](const Shot &sh) {
        const QVariant sv = root->property("scene");
        QObject *scene = sv.value<QObject *>();
        if (!scene) {
            qWarning() << "找不到 QML 的 scene 对象, 无法应用镜头参数";
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
    };'''


def main():
    s = open(P, encoding='utf-8').read()
    if 'invokeMethod(\n            scene, "testShot"' in s or '"testShot"' in s:
        print('已经改过，跳过')
        return
    assert OLD in s, '未找到 applyShot'
    s = s.replace(OLD, NEW, 1)
    open(P, 'w', encoding='utf-8').write(s)
    print('main.cpp 的 applyShot 已改为调用 testShot')


if __name__ == '__main__':
    main()
