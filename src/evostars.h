// ============================================================================
//  evostars.h —— 演化视图 3D 粒子层 (112 条目分类星团)
//
//  为什么是"分类星团"而不是"真实天球位置":
//    三表 (stellar/agn/ism) 均无赤经赤纬/银经银纬字段, 编造坐标等于
//    编造观测数据。替代方案按教学核心排布:
//      恒星 53 -> 赫罗图立体星团 (X=色温log, Y=光度log, Z=薄层抖动)
//      AGN 34  -> 深场 (按距离对数排壳层, 颜色按类型)
//      ISM 25  -> 星云展区 (按类型分组排布, 颜色按发射/反射/暗)
//    三团以内 Fibonacci 球面/圆盘分布, 团间拉开距离, 任何角度可辨。
//
//  用法: init(f) 一次; build() 从三表生成 CPU 数据并上传 (仅一次);
//        render(viewProj, pointScale) 单 draw call 点精灵。
//  恒星颜色按 teff 黑体分段 (与 QML starCol() 同款); 大小按光度对数。
// ============================================================================

#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>

class QOpenGLFunctions_3_3_Core;

class EvoStars
{
public:
    EvoStars();
    ~EvoStars();

    void init(QOpenGLFunctions_3_3_Core *f);
    void build();                       // CPU 端生成 112 点并上传 (仅一次)
    void destroy();

    bool ready() const { return m_ready; }
    int  pointCount() const { return m_count; }

    // 三团中心 (场景坐标, 供相机/标签用)
    static QVector3D stellarCenter() { return QVector3D(-55.0f, 0.0f, 0.0f); }
    static QVector3D agnCenter()     { return QVector3D( 55.0f, 0.0f, 0.0f); }
    static QVector3D ismCenter()     { return QVector3D(0.0f, -45.0f, 20.0f); }

    // ★ 代表条目在团内的世界坐标 —— build() 与标签投影共用同一套公式,
    //   保证标签永远贴合粒子 (银河系旋臂标签同款做法, 见 sceneitem)。
    //   table: 0=恒星 1=AGN 2=ISM; index: 该表内序号。
    static QVector3D starPos(int table, int index);

    void render(const QMatrix4x4 &viewProj, float pointScale);

private:
    // teff -> RGB (与 QML starCol() 同款分段, 线性空间近似)
    static QVector3D starColor(double teff);

    QOpenGLFunctions_3_3_Core *m_f = nullptr;
    QOpenGLShaderProgram *m_prog = nullptr;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
    int m_count = 0;

    bool m_ready = false;
};
