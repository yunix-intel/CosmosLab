// ============================================================================
//  mesh.cpp —— 几何体生成与 GL 缓冲封装
// ============================================================================

#include "mesh.h"

#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLContext>
#include <QOpenGLVersionFunctionsFactory>
#include <QtMath>
#include <cmath>

// ---------------------------------------------------------------------------
//  取当前上下文的 3.3 Core 函数表
// ---------------------------------------------------------------------------
static QOpenGLFunctions_3_3_Core *gl33()
{
    static QOpenGLFunctions_3_3_Core *f = nullptr;
    if (!f) {
        f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(
            QOpenGLContext::currentContext());
        if (f)
            f->initializeOpenGLFunctions();
    }
    return f;
}

// ---------------------------------------------------------------------------
//  Mesh
// ---------------------------------------------------------------------------

Mesh::~Mesh()
{
    // 析构发生在 GL 上下文可能已销毁之后, 这里不主动释放;
    // 上下文存活时应由调用方显式 destroy()。
}

void Mesh::upload(const QVector<Vertex> &vertices, const QVector<quint32> &indices)
{
    QVector<float> raw;
    raw.reserve(vertices.size() * 11);
    for (const Vertex &v : vertices) {
        raw << v.pos.x() << v.pos.y() << v.pos.z()
            << v.normal.x() << v.normal.y() << v.normal.z()
            << v.uv.x() << v.uv.y()
            << v.tangent.x() << v.tangent.y() << v.tangent.z();
    }
    uploadRaw(raw, 11, indices);
}

void Mesh::uploadRaw(const QVector<float> &interleaved, int floatsPerVertex,
                     const QVector<quint32> &indices)
{
    QOpenGLFunctions_3_3_Core *f = gl33();
    if (!f)
        return;

    m_stride = floatsPerVertex * int(sizeof(float));
    m_count  = indices.size();

    if (!m_created) {
        m_vao.create();
        m_vbo.create();
        m_ibo.create();
        m_created = true;
    }

    m_vao.bind();

    m_vbo.bind();
    m_vbo.allocate(interleaved.constData(),
                   int(interleaved.size() * sizeof(float)));

    m_ibo.bind();
    m_ibo.allocate(indices.constData(),
                   int(indices.size() * sizeof(quint32)));

    // 顶点属性布局
    //   0 pos(3) 1 normal(3) 2 uv(2) 3 tangent(3)   —— 11 float
    //   0 pos(2)                                    —— 2 float (全屏四边形)
    if (floatsPerVertex == 11) {
        const int offsets[4] = {0, 3, 6, 9};
        const int sizes[4]   = {3, 3, 2, 3};
        for (int i = 0; i < 4; ++i) {
            f->glEnableVertexAttribArray(GLuint(i));
            f->glVertexAttribPointer(GLuint(i), sizes[i], GL_FLOAT, GL_FALSE,
                                     m_stride,
                                     reinterpret_cast<void *>(quintptr(offsets[i] * sizeof(float))));
        }
    } else {
        f->glEnableVertexAttribArray(0);
        f->glVertexAttribPointer(0, floatsPerVertex, GL_FLOAT, GL_FALSE,
                                 m_stride, nullptr);
    }

    m_vao.release();
}

void Mesh::draw()
{
    if (!isValid())
        return;
    QOpenGLFunctions_3_3_Core *f = gl33();
    if (!f)
        return;
    m_vao.bind();
    f->glDrawElements(GL_TRIANGLES, m_count, GL_UNSIGNED_INT, nullptr);
    m_vao.release();
}

void Mesh::drawLines()
{
    if (!isValid())
        return;
    QOpenGLFunctions_3_3_Core *f = gl33();
    if (!f)
        return;
    m_vao.bind();
    f->glLineWidth(1.0f);
    f->glDrawElements(GL_LINE_STRIP, m_count, GL_UNSIGNED_INT, nullptr);
    m_vao.release();
}

void Mesh::destroy()
{
    if (m_created) {
        m_vao.destroy();
        m_vbo.destroy();
        m_ibo.destroy();
        m_created = false;
        m_count = 0;
    }
}

// ---------------------------------------------------------------------------
//  几何体生成
// ---------------------------------------------------------------------------

namespace geom {

Mesh *makeSphere(float radius, int segments, int rings)
{
    const int seg = qMax(3, segments);
    const int rng = qMax(2, rings);

    QVector<Vertex> verts;
    verts.reserve((rng + 1) * (seg + 1));

    for (int i = 0; i <= rng; ++i) {
        const double lat = -M_PI / 2.0 + M_PI * double(i) / double(rng);
        const double cosLat = std::cos(lat);
        const double sinLat = std::sin(lat);

        for (int j = 0; j <= seg; ++j) {
            const double lon = -M_PI + 2.0 * M_PI * double(j) / double(seg);
            const double cosLon = std::cos(lon);
            const double sinLon = std::sin(lon);

            Vertex v;
            const QVector3D n(float(cosLat * cosLon), float(sinLat),
                              float(cosLat * sinLon));
            v.normal  = n;
            v.pos     = n * radius;
            v.uv      = QVector2D(float((lon + M_PI) / (2.0 * M_PI)),
                                  float((lat + M_PI / 2.0) / M_PI));
            // 切线沿 +U (经度增大) 方向
            v.tangent = QVector3D(float(-sinLon), 0.0f, float(cosLon));
            verts.append(v);
        }
    }

    QVector<quint32> idx;
    idx.reserve(rng * seg * 6);
    for (int i = 0; i < rng; ++i) {
        for (int j = 0; j < seg; ++j) {
            const quint32 a = quint32(i * (seg + 1) + j);
            const quint32 b = a + 1;
            const quint32 c = a + quint32(seg + 1);
            const quint32 d = c + 1;
            // 逆时针 = 正面朝外
            idx << a << c << b;
            idx << b << c << d;
        }
    }

    auto *m = new Mesh;
    m->upload(verts, idx);
    return m;
}

Mesh *makeRing(float inner, float outer, int segments)
{
    const int seg = qMax(3, segments);

    QVector<Vertex> verts;
    verts.reserve((seg + 1) * 2);

    for (int k = 0; k <= seg; ++k) {
        const double ang = 2.0 * M_PI * double(k) / double(seg);
        const float ca = float(std::cos(ang));
        const float sa = float(std::sin(ang));

        Vertex vi;
        vi.pos     = QVector3D(ca * inner, 0.0f, sa * inner);
        vi.normal  = QVector3D(0.0f, 1.0f, 0.0f);
        vi.uv      = QVector2D(0.0f, float(k) / float(seg));   // 内缘
        vi.tangent = QVector3D(-sa, 0.0f, ca);
        verts.append(vi);

        Vertex vo;
        vo.pos     = QVector3D(ca * outer, 0.0f, sa * outer);
        vo.normal  = QVector3D(0.0f, 1.0f, 0.0f);
        vo.uv      = QVector2D(1.0f, float(k) / float(seg));   // 外缘
        vo.tangent = QVector3D(-sa, 0.0f, ca);
        verts.append(vo);
    }

    QVector<quint32> idx;
    idx.reserve(seg * 6);
    for (int k = 0; k < seg; ++k) {
        const quint32 i0 = quint32(k * 2);
        idx << i0 << (i0 + 1) << (i0 + 2);
        idx << (i0 + 1) << (i0 + 3) << (i0 + 2);
    }

    auto *m = new Mesh;
    m->upload(verts, idx);
    return m;
}

Mesh *makeScreenQuad()
{
    const float q[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f,
    };
    QVector<float> verts(q, q + 12);
    QVector<quint32> idx;
    idx << 0 << 1 << 2 << 3 << 4 << 5;

    auto *m = new Mesh;
    m->uploadRaw(verts, 2, idx);
    return m;
}

Mesh *makeLineStrip(const QVector<QVector3D> &points)
{
    if (points.size() < 2)
        return nullptr;

    QVector<Vertex> verts;
    verts.reserve(points.size());
    for (const QVector3D &p : points) {
        Vertex v;
        v.pos     = p;
        v.normal  = QVector3D(0.0f, 1.0f, 0.0f);
        v.uv      = QVector2D(0.0f, 0.0f);
        v.tangent = QVector3D(1.0f, 0.0f, 0.0f);
        verts.append(v);
    }

    QVector<quint32> idx;
    idx.reserve(points.size());
    for (int i = 0; i < points.size(); ++i)
        idx << quint32(i);

    auto *m = new Mesh;
    m->upload(verts, idx);
    return m;
}

} // namespace geom
