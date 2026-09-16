// ============================================================================
//  belts.h —— 小行星带 / 柯伊伯带 / 特洛伊群
//
//  ★ 为什么必须做粒子带而不是只列几颗具名小行星:
//    主小行星带已知成员超过 **130 万颗** (直径 > 1 km 的估计有 100–200 万颗),
//    柯伊伯带已知天体也有数千颗。只放 Vesta / Pallas 那几颗, 会让学生
//    以为"太阳系里小行星屈指可数"—— 这恰恰是最常见的误解之一。
//    带结构本身也是教学重点: 主带内外边界、Kirkwood 空隙、特洛伊群锁定在
//    木星的 L4/L5 点, 这些都是"共振"这一概念最直观的展示。
//
//  ★ 实现: 粒子全部常驻显存, **轨道相位由顶点着色器按时间推进**。
//    若改成每帧 CPU 更新 7 万个位置再上传, 光传输量就有 ~840 KB/帧,
//    纯属浪费。着色器方案下每帧只更新一个 uniform。
//
//  ★ 比例处理: 带的位置必须与行星轨道用**同一套压缩变换**, 否则会和
//    行星轨道对不上。由于该变换是径向幂次的, 圆轨道压缩后仍是圆,
//    因此可以在生成时把压缩后的半径直接算好, 存进属性里。
// ============================================================================

#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>

class QOpenGLFunctions_3_3_Core;

class Belts
{
public:
    Belts();
    ~Belts();

    // realScale 决定用哪套坐标变换 (与行星轨道保持一致)
    void init(QOpenGLFunctions_3_3_Core *f, bool realScale);
    void rebuild(bool realScale);        // 比例模式切换时重建
    void destroy();

    bool ready() const { return m_ready; }
    int  particleCount() const { return m_count; }
    int  mainBeltCount() const { return m_mainCount; }
    int  kuiperCount() const { return m_kuiperCount; }
    int  trojanCount() const { return m_trojanCount; }

    // days 为自 J2000 起的天数; pointScale 用于透视缩放点的大小
    void render(const QMatrix4x4 &viewProj, double days, float pointScale);

private:
    void build(bool realScale);

    QOpenGLFunctions_3_3_Core *m_f = nullptr;
    QOpenGLShaderProgram *m_prog = nullptr;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};

    int m_count      = 0;
    int m_mainCount  = 0;
    int m_kuiperCount = 0;
    int m_trojanCount = 0;

    bool m_ready = false;
};
