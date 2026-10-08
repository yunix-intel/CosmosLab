// ============================================================================
//  viewstate.h —— GUI 线程 -> 渲染线程 的状态快照
//
//  抽成独立头文件的原因: sceneitem.h (GUI 侧) 与 scenerenderer.h (渲染侧)
//  都需要它。若各自定义一份同名结构体, 赋值时会因类型不同而编译失败
//  (no match for 'operator=')。
//
//  约定: 纯数据, 不含任何 GL 对象与指针 —— 因为它是跨线程传递的。
// ============================================================================

#pragma once

#include <QVector3D>

// ---------------------------------------------------------------------------
//  尺度层级
//
//  两个尺度相差约 2e10 倍 (海王星轨道 4.5e-6 光年 vs 银河系 1.06e5 光年),
//  无法用同一套相机参数连续缩放 —— 浮点精度与深度缓冲都撑不住。
//  因此做成**两个独立视图**, 由 UI 的下拉框切换。
// ---------------------------------------------------------------------------
enum class SceneScale
{
    SolarSystem = 0,   // 太阳系: 行星 / 轨道 / 卫星 / 小行星带 / 彗尾
    Galaxy      = 1,   // 银河系: 棒旋星系粒子模型
    Cosmos      = 2,   // 宇宙: 本星系群 → 星系团 → 超星系团 → 大尺度结构
    // ★ 演化 (v1.3): 独立 3D 视图。复用太阳系画布 (星场底), 关闭轨道/
    //   行星/带, 只画 EvoStars 粒子 (112 条目按类型上天球/排布)。
    //   Canvas 示意保留为第二层 (细节曲线), 3D 为第一层 (空间呈现)。
    Evolution   = 3,
};

struct ViewState
{
    bool   valid     = false;   // synchronize() 是否已取到有效状态
    double jd        = 0.0;     // 儒略日
    SceneScale scale = SceneScale::SolarSystem;
    QVector3D camTarget;        // 相机目标点 (场景坐标)
    double camDist   = 300.0;
    double camTheta  = 0.6;     // 方位角 (弧度, 绕 Y 轴)
    double camPhi    = 1.08;    // 极角 (弧度, 自 +Y 起算)
    double fov       = 52.0;
    double orbitExag = 0.6;
    double sizeExag  = 1.0;
    double exposure  = 1.0;
    bool   realScale = false;   // 真实比例 (1:1) 模式
    bool   showOrbits = true;
    bool   showBelts  = true;   // 小行星带 / 柯伊伯带 / 特洛伊群
    bool   showRings  = true;
    bool   showAtmo   = true;
    bool   snap       = false;  // 一次性: 相机直接吸附, 不做阻尼插值

    // 宇宙视图可见粒子数 (性能开关)。<=0 表示全部。
    // ★ 只改 glDrawArrays 的 count, 顶点数据不动 —— 切换零成本。
    int    cosmosVisible = 0;

    // 距离映射模式: 0=对数压缩 1=真实比例
    int    cosmosMapMode = 0;

    // SDSS 真实星系可见数 (0=全部)
    int    sdssVisible = 0;

    // ---- 演化主星 (v1.3): HR 三剧本 (lowmass/midmass/massive) 在演化
    //   视图中央的 3D 发光球。QML 侧每帧经 Q_PROPERTY 推送当前解算值,
    //   render() 内直接取用 —— 跨线程用 double/float 纯数据, 无锁。
    //   script: 0=无 1=lowmass 2=midmass 3=massive
    int    evoScript = 0;
    double evoTeff = 5778.0;   // K
    double evoLogL = 0.0;      // log10(L/Lsun)
    double evoRad = 1.0;       // Rsun
    double evoAge = 0.0;       // 当前物理时间 (剧本单位, 仅读数用)

    // ---- 演化通用模拟量 (v1.3 A/B/C批): 非HR剧本的3D形态驱动 ----
    //
    //  ★ vizCode: 0=无 4=sn 5=merger 6=agn 7=binary
    //             8=planet 9=protostar 10=remnant 11=cluster
    //             12=cosmic 13=ism (与 evoScript 1~3 互斥, 同时只用其一)
    //  ★ p1..p6 含义按 vizCode 分派 (见 sceneitem.h 的 evoSim 注释)。
    //    QML 侧 pushSim() 与 pushStar() 并存: HR剧本调pushStar,
    //    其余调pushSim, C++ 侧各自去重, 互不干扰。
    int    evoViz = 0;
    double evoP1 = 0.0;
    double evoP2 = 0.0;
    double evoP3 = 0.0;
    double evoP4 = 0.0;
    double evoP5 = 0.0;
    double evoP6 = 0.0;
};
