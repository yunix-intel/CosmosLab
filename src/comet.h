// ============================================================================
//  comet.h —— 彗尾渲染
//
//  ★ 为什么彗尾必须单独做:
//    彗星的教学价值几乎全在尾巴上。一个几公里的小冰核在屏幕上只是个点,
//    而彗尾可以长达上亿公里 —— **比太阳还大**。这恰恰是讲解"彗发与彗尾
//    如何形成"的切入点。
//
//  ★ 两条尾巴的物理机制完全不同 (这是最常见的错误认知:
//    以为"尾巴是被风吹的, 所以往后退"):
//
//    1. 离子尾 (Ion tail) —— 蓝色, 笔直, **严格背离太阳**
//       太阳风把彗发中的气体电离, 带电粒子被太阳风磁场**直接吹走**。
//       因为太阳风速度 (~400 km/s) 远大于彗星的轨道速度,
//       所以离子尾总是指向背离太阳的方向 —— 与彗星运动方向无关。
//       **彗星离开太阳时, 离子尾在前方"推着"它走** —— 这是最反直觉、
//       也最值得讲的一点。
//
//    2. 尘埃尾 (Dust tail) —— 黄白色, 弯曲, 沿轨道方向滞后
//       尘埃颗粒不受磁场影响, 只受太阳光压和引力。它们离开彗核后
//       基本保持原有的轨道速度, 因此会**滞后于彗核**, 形成弯曲的尾迹。
//
//  ★ 长度随日心距变化:
//    彗发活动强度 ∝ 1/r² 附近 (太阳辐射加热), 因此越接近近日点,
//    尾巴越长越亮。取 L ∝ r^-1.5 作为经验关系, 并在 r < 0.5 AU 时饱和
//    (过近日点时不会无限增长)。
// ============================================================================

#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector>

class QOpenGLFunctions_3_3_Core;

// 单颗彗星的尾巴参数 (由场景每帧算出并传入)
struct CometTail
{
    QVector3D nucleus;        // 彗核位置 (场景坐标)
    QVector3D antiSun;        // 背离太阳的单位向量
    QVector3D velocity;       // 轨道速度方向 (单位向量)
    float     length;         // 尾巴长度 (场景单位)
    float     brightness;     // 0..1, 由日心距决定
};

class CometRenderer
{
public:
    CometRenderer();
    ~CometRenderer();

    void init(QOpenGLFunctions_3_3_Core *f);
    void destroy();
    bool ready() const { return m_ready; }

    // 每帧调用。tails 里的长度/亮度由 Scene 算好。
    void render(const QMatrix4x4 &viewProj, const QVector3D &camPos,
                const QVector<CometTail> &tails);

private:
    // 每条尾巴用一个扇形网格 (沿轴逐渐变宽变淡)
    void buildMesh();

    QOpenGLFunctions_3_3_Core *m_f = nullptr;
    QOpenGLShaderProgram *m_prog = nullptr;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
    int m_vertexCount = 0;

    bool m_ready = false;
};
