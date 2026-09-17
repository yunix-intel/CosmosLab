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
};
