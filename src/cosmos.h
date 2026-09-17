// ============================================================================
//  cosmos.h —— 宇宙大尺度结构渲染
//
//  ★★ 核心难题: 尺度跨越 6 个数量级
//      小麦哲伦云      0.2  Mly
//      仙女座          2.5  Mly
//      室女座星系团    54   Mly
//      拉尼亚凯亚      250  Mly
//      斯隆巨壁        1000 Mly
//      可观测宇宙边缘  46500 Mly
//
//    线性映射下, 本星系群会缩成一个亚像素点, 整个视图只剩一个大球壳 ——
//    什么结构都看不见。因此必须用**对数径向映射**:
//
//        r_scene = log10(1 + d / d0) / log10(1 + dMax / d0) × R_scene
//
//    d0 = 0.1 Mly 让最近的天体也有合理间距; dMax 取可观测宇宙半径。
//
//  ★ 对数映射的代价与补救:
//    它会**压缩远处的间隔**, 使人误以为"远处的星系挨得更近"。
//    这是必须向学生明说的失真。补救办法:
//      1. 每个标注都直接标出**真实距离**(百万光年), 学生以数字为准
//      2. 右下角的比例尺给出当前视野的真实量级
//      3. 画同心等距圈 (1 / 10 / 100 / 1000 Mly), 让"每圈 ×10"可见
//
//  ★ 宇宙网的生成:
//    真实的大尺度结构是"纤维 + 节点 + 空洞"。用纯随机分布会得到
//    均匀的噪点, 完全不像宇宙网。这里用简化的**纤维网络**模型:
//      1. 随机撒 N 个节点 (代表星系团/超星系团的位置)
//      2. 在邻近节点之间连纤维, 沿纤维撒星系
//      3. 在随机位置挖出球形空洞 (内部几乎不放星系)
//    这三步足以重现宇宙网的视觉特征, 也对应真实的形成机制
//    (暗物质纤维的引力坍缩 → 节点成团 → 空洞被掏空)。
// ============================================================================

#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector>
#include <QVariantList>
#include <QVariantMap>

class QOpenGLFunctions_3_3_Core;

// 一个具名结构在宇宙视图中的位置 (已换算为场景坐标)
struct CosmosMarker
{
    QString nameCn;
    QString nameEn;
    QString detail;        // 真实距离等
    QVector3D pos;
    int     kind;          // 0=星系 1=星系群/团 2=超星系团 3=巨壁 4=空洞
};

class Cosmos
{
public:
    Cosmos();
    ~Cosmos();

    void init(QOpenGLFunctions_3_3_Core *f);
    void destroy();
    bool ready() const { return m_ready; }

    int particleCount() const { return m_count; }

    // ---- 可见粒子数控制 (性能开关) ----
    //
    // ★ 设计要点: 顶点数据**一次上传**后不再变动, 改变显示数量
    //   只需改 glDrawArrays 的 count —— **零成本切换**, 无重传无卡顿。
    //
    // ★ 取前 N 个即可保证"重要结构优先显示": 数据生成时就是按
    //   重要性顺序追加的 (纤维 → 空洞边缘 → 背景填充), 见 cosmos.cpp。
    //   所以截断天然保留了宇宙网的骨架。
    //
    // n <= 0 或 n >= m_count 表示显示全部。
    void setVisibleCount(int n) { m_visible = n; }
    int  visibleCount() const {
        return (m_visible <= 0 || m_visible > m_count) ? m_count : m_visible;
    }
    int filamentCount() const { return m_filamentCount; }
    int voidCount() const { return m_voidCount; }

    // 最近一次 build() 生成的总粒子数。
    // ★ 为什么需要它: 总粒子数只有渲染侧知道 (Cosmos 活在渲染线程),
    //   而 UI 要显示"当前 X / 总数 Y"。用静态缓存是安全的 ——
    //   build() 只在初始化时跑一次, 之后不再变化。
    static int lastBuiltCount() { return s_lastBuilt; }

    void render(const QMatrix4x4 &viewProj, float pointScale);

    // 场景半径 (对数映射后的最大半径)
    static float sceneRadius() { return 100.0f; }

    // 真实距离 (Mly) -> 场景半径。对数映射, 见文件头说明。
    static float distToScene(double mly);

    // 具名结构列表 (位置已换算)
    static QVector<CosmosMarker> markers();

private:
    void build();

    QOpenGLFunctions_3_3_Core *m_f = nullptr;
    QOpenGLShaderProgram *m_prog = nullptr;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};

    int m_count         = 0;
    int m_visible = 0;          // <=0 表示全部
    static int s_lastBuilt;     // 最近一次 build 的总数
    int m_filamentCount = 0;   // 纤维中的星系
    int m_voidCount     = 0;   // 空洞边缘的稀疏星系

    bool m_ready = false;
};
