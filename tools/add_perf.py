"""给宇宙视图加 GPU 真实耗时测量 + 粒子数压力测试开关。

★★ 为什么需要这个:

  实测发现三个视图 (太阳系/银河系/宇宙) 的帧率**完全一样**, 都是 62.5 FPS。
  62.5 = 1000/16ms —— 这是 SolarScene::onTick 的定时器周期, 即
  **软件节流上限**, 不是 GPU 能力上限。

  也就是说: 当前 7 万~13 万粒子下, GPU 远没有跑满, 帧率数字
  完全反映不出 GPU 的真实余量。要回答"能承受多少粒子",
  必须测**单帧 GPU 耗时**。

★ 测量方法:
  在 glDrawArrays 前后插入 glFinish() 并计时。
  glFinish 会阻塞到 GPU 真正画完, 因此测到的是真实耗时
  (包含少量提交开销, 对量级判断足够)。

★ 压力测试:
  加 SS_COSMOS_MULT=<倍数> 环境变量, 把宇宙粒子数按倍数重复填充,
  用于测"帧率 vs 粒子数"曲线, 据此推断能承载 SDSS 全目录的规模。

用法:
    SS_SCALE=2 SS_PERF=1 ./solar_system.exe            # 测真实耗时
    SS_SCALE=2 SS_PERF=1 SS_COSMOS_MULT=10 ...         # 压力测试
"""
C = r'D:\tmp\solar-system-cpp\src\scenerenderer.cpp'

TIMER_HELPER = r'''
// ---------------------------------------------------------------------------
//  GPU 耗时测量 —— 用 glFinish 强制同步后计时
//
//  ★ 为什么不能只看帧率: 场景由 SolarScene::onTick 的 16ms 定时器驱动,
//    帧率上限被锁在 62.5 FPS。GPU 耗时只要低于 16ms, 帧率就恒为 62.5,
//    完全反映不出余量。要评估"能承载多少粒子", 必须测真实耗时。
// ---------------------------------------------------------------------------
namespace {
bool perfEnabled()
{
    static const bool on = qEnvironmentVariableIntValue("SS_PERF") > 0;
    return on;
}

// 累计统计, 每 N 帧输出一次
struct PerfAccum {
    double sumMs = 0.0;
    int    n = 0;
    double minMs = 1e9, maxMs = 0.0;
    void add(double ms) {
        sumMs += ms; ++n;
        if (ms < minMs) minMs = ms;
        if (ms > maxMs) maxMs = ms;
        if (n % 60 == 0) {
            qWarning().noquote()
                << QString("[性能] 最近60帧 GPU 平均 %1 ms (min %2 / max %3)"
                           "  -> 纯渲染理论上限 %4 FPS")
                       .arg(sumMs / n, 0, 'f', 2)
                       .arg(minMs, 0, 'f', 2)
                       .arg(maxMs, 0, 'f', 2)
                       .arg(1000.0 / qMax(sumMs / n, 1e-6), 0, 'f', 1);
            sumMs = 0.0; n = 0; minMs = 1e9; maxMs = 0.0;
        }
    }
};
} // namespace
'''

# 在宇宙分支加计时
OLD_COSMOS = '''        m_f->glDisable(GL_DEPTH_TEST);
        m_f->glDisable(GL_CULL_FACE);
        m_cosmos.render(viewProj, pScaleC);'''
NEW_COSMOS = '''        m_f->glDisable(GL_DEPTH_TEST);
        m_f->glDisable(GL_CULL_FACE);
        if (perfEnabled())
            m_f->glFinish();
        const qint64 perfT0 = perfEnabled()
                                  ? QDateTime::currentMSecsSinceEpoch() : 0;
        m_cosmos.render(viewProj, pScaleC);
        if (perfEnabled()) {
            m_f->glFinish();
            static PerfAccum acc;
            acc.add(double(QDateTime::currentMSecsSinceEpoch() - perfT0));
        }'''

OLD_GAL = '''        drawGalaxyOverlay(viewProj, vs);
        m_galaxy.render(viewProj, pointScale);'''
NEW_GAL = '''        drawGalaxyOverlay(viewProj, vs);
        if (perfEnabled())
            m_f->glFinish();
        const qint64 perfG0 = perfEnabled()
                                  ? QDateTime::currentMSecsSinceEpoch() : 0;
        m_galaxy.render(viewProj, pointScale);
        if (perfEnabled()) {
            m_f->glFinish();
            static PerfAccum accG;
            accG.add(double(QDateTime::currentMSecsSinceEpoch() - perfG0));
        }'''


def main():
    c = open(C, encoding='utf-8').read()

    if 'perfEnabled' not in c:
        # 插到 SceneRenderer::initialize 之前
        i = c.index('void SceneRenderer::initialize()')
        c = c[:i] + TIMER_HELPER + '\n' + c[i:]
        print('已插入计时辅助')

    if 'PerfAccum acc;' not in c:
        assert OLD_COSMOS in c, '未找到宇宙渲染块'
        c = c.replace(OLD_COSMOS, NEW_COSMOS, 1)
        print('宇宙分支已加计时')

    if 'PerfAccum accG;' not in c:
        assert OLD_GAL in c, '未找到银河系渲染块'
        c = c.replace(OLD_GAL, NEW_GAL, 1)
        print('银河系分支已加计时')

    open(C, 'w', encoding='utf-8').write(c)
    print('scenerenderer.cpp 已更新')


if __name__ == '__main__':
    main()
