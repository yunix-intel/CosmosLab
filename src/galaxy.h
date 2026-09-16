// ============================================================================
//  galaxy.h —— 银河系视图 (棒旋星系粒子模型)
//
//  渲染方式: 十万级恒星用**点精灵**一次 draw call 画完。
//  加法混合让密集区域自然累积变亮 —— 银心炽亮、旋臂成条, 这不是靠贴图
//  贴出来的, 而是粒子密度的自然结果, 也因此从任何角度看都成立。
//
//  为什么不用一张银河系贴图:
//    贴图只能贴在某个平面上, 侧视时会露馅成一个薄片。粒子模型是真正的
//    三维分布, 学生可以从侧面看到银盘有多薄、从俯视看到棒与旋臂结构 ——
//    这对教学是必要的。
// ============================================================================

#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>

class QOpenGLFunctions_3_3_Core;

class Galaxy
{
public:
    Galaxy();
    ~Galaxy();

    void init(QOpenGLFunctions_3_3_Core *f);
    void build();                       // CPU 端生成粒子 (仅一次)
    void destroy();

    bool ready() const { return m_ready; }
    int  pointCount() const { return m_count; }

    // 太阳在星系里的位置 (场景坐标)。
    // 设成 static 是因为 GUI 线程也要用它把标记投影到屏幕上 ——
    // 不能只让渲染线程知道"太阳在哪"。
    static QVector3D sunPosition();

    // pointScale 由 CPU 按视口高度与 FOV 算好, 使点的大小随距离正确缩放
    void render(const QMatrix4x4 &viewProj, float pointScale);

private:
    void buildStars(QVector<float> &out);

    QOpenGLFunctions_3_3_Core *m_f = nullptr;

    QOpenGLShaderProgram *m_prog = nullptr;         // 星点

    // 星点
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
    int m_count = 0;

    bool m_ready = false;
};
