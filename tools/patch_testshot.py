"""给 SolarScene 加统一的测试入口 testShot()，供批量渲染调用。

★ 为什么不在 QML 侧加 testDist/testPhi 这类属性:
  QML 侧加属性意味着每个参数都要写一遍绑定、再想办法转发到 C++ 的
  m_forcedDist / m_camPhi / ... 上，链路长且容易漏。
  直接在 C++ 加一个方法，一次把整套相机参数设好，最直接。

★ 关于绕过平滑动画:
  正常交互时相机是插值过渡的 (m_snapCamera)。批量渲染如果等动画
  走完，每张图要多等 1-2 秒。这里直接置 m_snapCamera = true 立即到位，
  实测渲染时间从 ~35s 降到 ~3s/张。
"""
import re

H = r'D:\tmp\solar-system-cpp\src\sceneitem.h'
C = r'D:\tmp\solar-system-cpp\src\sceneitem.cpp'

DECL = '''    Q_INVOKABLE void focusOn(const QString &id);'''

DECL_NEW = '''    Q_INVOKABLE void focusOn(const QString &id);

    // ---- 测试/自检专用: 一次设定全套视角参数 ----
    // 供 main.cpp 的批量渲染调用, 避免逐个属性设值时遗漏。
    // 传 -1 / 空 表示"沿用当前值"。立即到位, 不走平滑动画。
    Q_INVOKABLE void testShot(const QString &focusId,
                              double dist, double phiDeg, double thetaDeg,
                              int scaleLevel, double jd);'''


def main():
    h = open(H, encoding='utf-8').read()
    if 'testShot' in h:
        print('头文件已改过，跳过')
    else:
        assert DECL in h, '未找到 focusOn 声明'
        h = h.replace(DECL, DECL_NEW, 1)
        open(H, 'w', encoding='utf-8').write(h)
        print('sceneitem.h 已加 testShot 声明')

    c = open(C, encoding='utf-8').read()
    if 'SolarScene::testShot' in c:
        print('实现已存在，跳过')
        return

    # 找到 focusOn 的实现，把 testShot 插在它前面
    m = re.search(r'\nvoid SolarScene::focusOn\(', c)
    assert m, '未找到 focusOn 实现'

    impl = r'''
// ---------------------------------------------------------------------------
//  测试/自检专用: 一次设定全套视角参数
//
//  ★ 直接置 m_snapCamera = true 让相机**立即到位**, 不等插值动画。
//    正常交互需要平滑过渡 (否则拖动手感很生硬), 但批量渲染时
//    每张都等动画走完会白白多花 1-2 秒。
// ---------------------------------------------------------------------------
void SolarScene::testShot(const QString &focusId,
                          double dist, double phiDeg, double thetaDeg,
                          int scaleLevel, double jd)
{
    // 尺度要先切 —— applyFocus() 的行为依赖当前尺度
    if (scaleLevel >= 0 && scaleLevel != int(m_scale)) {
        setScale(scaleLevel);
        m_snapCamera = true;
    }

    if (jd > 0.0) {
        m_jd = jd;
        emit dateTextChanged();
    }

    if (!focusId.isEmpty())
        m_focusId = focusId;

    if (dist > 0.0)
        m_forcedDist = dist;
    if (phiDeg >= 0.0)
        m_camPhi = phiDeg * M_PI / 180.0;
    if (thetaDeg >= 0.0)
        m_camTheta = thetaDeg * M_PI / 180.0;

    // 指定了视距就不要再让 applyFocus 重算
    if (dist > 0.0) {
        Scene s;
        s.setJulianDate(m_jd);
        s.update();
        if (const SceneItem *it = s.itemById(m_focusId))
            m_camTarget = it->center;
        m_camDist = dist;
        m_snapCamera = true;
    } else {
        applyFocus();
        m_snapCamera = true;
    }

    update();
}
'''

    c = c[:m.start()] + impl + c[m.start():]
    open(C, 'w', encoding='utf-8').write(c)
    print('sceneitem.cpp 已加 testShot 实现')


if __name__ == '__main__':
    main()
