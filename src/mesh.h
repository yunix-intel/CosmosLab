// ============================================================================
//  mesh.h —— 几何体与 GL 缓冲封装
//
//  移植自 Python 版 core/geometry.py + core/gl/gl_core.py。
//
//  与 Python 版的一个重要差异: 那边因为 PySide6 的 glDrawElements 参数校验
//  异常, 被迫把索引展开成非索引顶点数组 (球体 6k -> 37k 顶点)。C++ 里
//  glDrawElements 完全正常, 故这里恢复使用索引绘制。
// ============================================================================

#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QVector>
#include <QVector2D>
#include <QVector3D>

// 顶点: pos(3) normal(3) uv(2) tangent(3) = 11 float = 44 字节
struct Vertex
{
    QVector3D pos;
    QVector3D normal;
    QVector2D uv;
    QVector3D tangent;
};

// ---------------------------------------------------------------------------
//  GL 缓冲封装
//
//  典型用法: 构造 geometry -> upload() 一次 -> 每帧 draw()
//  ★ 不要频繁 destroy/重建 VAO —— 在 Python 版实测单次约 6 ms,
//    9 条轨道就是 160 ms。顶点数据变化时只更新 VBO 即可。
// ---------------------------------------------------------------------------
class Mesh
{
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;

    // 顶点布局: 位置 0 = pos, 1 = normal, 2 = uv, 3 = tangent
    void upload(const QVector<Vertex> &vertices, const QVector<quint32> &indices);
    void uploadRaw(const QVector<float> &interleaved, int floatsPerVertex,
                   const QVector<quint32> &indices);

    void draw();          // GL_TRIANGLES (按索引)
    void drawLines();     // GL_LINE_STRIP (按索引)
    void destroy();

    bool isValid() const { return m_vao.isCreated() && m_count > 0; }
    int  indexCount() const { return m_count; }

private:
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_ibo{QOpenGLBuffer::IndexBuffer};
    int m_count = 0;
    int m_stride = 0;
    bool m_created = false;
};

// ---------------------------------------------------------------------------
//  几何体生成
// ---------------------------------------------------------------------------

namespace geom {

// UV 球体。切线沿经度方向 (+U), 这是等距柱状贴图的标准做法。
Mesh *makeSphere(float radius = 1.0f, int segments = 96, int rings = 64);

// 环系圆盘 (三角带)。
//   UV.x = 径向归一化 (0 = 内缘, 1 = 外缘)
//   UV.y = 角度归一化 (0..1 绕一圈)
// 早期版本每个扇形段都映射 UV.x 0->1 且 UV.y 固定 0.5, 导致纹理在每个
// 扇形内重复形成放射状条纹; 改用「径向 + 角度」二维 UV 后自然环绕无接缝。
// 法线朝 +Y, 环面位于 XZ 平面。
Mesh *makeRing(float inner, float outer, int segments = 256);

// 全屏四边形 (后期处理用)。顶点仅含 pos.xy, 走独立的 2 属性布局。
Mesh *makeScreenQuad();

// 轨道线 (GL_LINE_STRIP)。颜色由调用方通过 uniform 设置。
Mesh *makeLineStrip(const QVector<QVector3D> &points);

} // namespace geom
