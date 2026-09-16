// ============================================================================
//  camera.h —— 轨道相机 (支持从行星表面到星系尺度的无缝缩放)
//
//  移植自 Python 版 core/camera.py。两个关键设计:
//
//   * 距离跨度极大 (0.15 ~ 2e6 单位), 用**对数插值**做缩放, 手感才线性。
//   * 远近裁剪面比率巨大时深度精度会崩, 故采用**动态 near/far**:
//     near 随目标距离缩放, far 固定覆盖全场景 (星空不参与深度测试)。
//
//  与 Python 版的唯一区别: 相机状态直接在 C++ 侧持有, 阻尼插值由
//  SceneRenderer 每帧调用 update(dt) 推进 —— Python 版曾因漏调 update()
//  导致焦点切换完全失效, 这里在头文件里就写明契约。
// ============================================================================

#pragma once

#include <QMatrix4x4>
#include <QVector3D>

class OrbitCamera
{
public:
    OrbitCamera();

    // ---- 交互 ----
    void rotate(float dx, float dy);
    void zoom(float delta);
    void pan(float dx, float dy);

    // ---- 每帧推进阻尼插值 ----
    // ★ 必须每帧调用一次, 否则 smooth_* 永远停在初值, 焦点切换会失效。
    void update(double dt);

    // 大跨度跳变时直接吸附 (不做插值)。
    // 阻尼按比例收敛, 从 1150 单位收敛到 3 单位需要数十帧,
    // 用户会看到镜头长时间"飞"向目标, 体验很差。
    void snapToTarget() { m_snap = true; }

    // ---- 目标设定 ----
    // 聚焦某点时保持视距, 只移动目标点。
    void setTarget(const QVector3D &t);
    void moveTarget(const QVector3D &t) { m_target = t; }
    void setDistance(double d);
    void setFov(double f) { m_fov = float(f); }

    // 轨道角度同步 (GUI 线程算好, 渲染线程直接套用)
    void setOrbit(double theta, double phi)
    {
        m_theta = theta;
        m_phi = qBound(0.5 * 3.14159265358979 / 180.0, phi,
                       3.14159265358979 - 0.5 * 3.14159265358979 / 180.0);
    }

    // ---- 查询 ----
    QVector3D eye() const;
    QVector3D target() const { return m_smoothTarget; }
    double    distance() const { return m_smoothDist; }
    QVector3D forward() const;
    QVector3D rightVector() const;

    QMatrix4x4 view() const;
    QMatrix4x4 projection(float aspect) const;
    QMatrix4x4 viewProjection(float aspect) const;

    // ---- 参数 ----
    void setRotationSpeed(float s) { m_rotSpeed = s; }
    void setZoomSpeed(float s) { m_zoomSpeed = s; }
    void setMinDistance(double d) { m_minDist = d; }

private:
    QVector3D m_target{0.0f, 0.0f, 0.0f};
    double m_theta = 35.0 * 3.14159265358979 / 180.0;   // 方位角 (绕 Y 轴)
    double m_phi   = 62.0 * 3.14159265358979 / 180.0;   // 极角 (自 +Y 起算)
    double m_dist  = 300.0;
    float  m_fov   = 52.0f;

    // 交互参数
    double m_minDist  = 0.15;
    double m_maxDist  = 2.0e6;
    float  m_rotSpeed = 0.005f;
    float  m_zoomSpeed = 0.12f;

    // 平滑 (阻尼)
    QVector3D m_smoothTarget{0.0f, 0.0f, 0.0f};
    double m_smoothDist  = 300.0;
    double m_smoothTheta = 35.0 * 3.14159265358979 / 180.0;
    double m_smoothPhi   = 62.0 * 3.14159265358979 / 180.0;
    double m_damping     = 0.34;

    bool m_snap = false;      // 下一帧吸附
    // eye() 是 const 查询, 但需要缓存计算结果 —— 故缓存成员为 mutable
    mutable bool m_hasEyes = false;
    mutable QVector3D m_eyeCache;
};
