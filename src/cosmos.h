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

    // ★ 真实距离 (百万光年)。
    //   标签位置必须随映射模式变化, 所以这里保留**物理量**,
    //   而不是只存映射后的坐标 —— 否则切换模式时标签会与粒子错位。
    double  distMly = 0.0;
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
    // ---- SDSS 真实星系的独立显示控制 ----
    //
    // ★ 为什么不与"示意结构"共用一个开关:
    //   实测 SDSS 的 171,398 个星系让总粒子达 242,604, 性能明显下降。
    //   根因是**星系成团** —— 标定系数 0.067 us/粒子 是在均匀分布下测的,
    //   而真实星系在星系团/纤维里极度聚集, 大量点落在同一像素上,
    //   **过度绘制**把填充率成本拉高。
    //
    //   分别控制的好处: 用户可以先关掉 SDSS 看宇宙网的整体形态,
    //   再打开看真实星系分布 —— 两者对应不同的观察意图。
    //
    // n <= 0 表示显示全部 SDSS 数据。
    void setSdssVisible(int n) { m_sdssVisible = n; }
    int  sdssVisible() const {
        if (m_sdssCount <= 0) return 0;
        return (m_sdssVisible <= 0 || m_sdssVisible > m_sdssCount)
                   ? m_sdssCount : m_sdssVisible;
    }

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
    // 最近一次的 SDSS 星系数 (供 GUI 线程查询)
    static int lastBuiltSdss() { return s_lastBuiltSdss; }

    // ★ 最近一帧**实际绘制**的粒子数 (供 GUI 线程查询)。
    //
    //   为什么不能直接用 lastBuiltCount() 做 UI 显示:
    //   "星系数量"档位和"SDSS 实测星系"开关都会**二次削减**实际绘制量 ——
    //   SDSS 有独立的可见性控制 (关闭/25%/50%/全部), 它是在
    //   示意结构之外**另算**的。于是 total 是缓冲总数,
    //   真实绘制数可能远小于它 (例如 SDSS 关到 25% 时只剩 45%)。
    //   UI 若打印 total 会**高估**, 性能预估也会失真。
    static int lastDrawn() { return s_lastDrawn; }

    void render(const QMatrix4x4 &viewProj, float pointScale);

    // ★ 实际绘制量的**唯一实现** —— 渲染线程和 GUI 线程共用。
    //
    //   为什么必须是唯一实现: 这个公式有两个坑, 两处各写一遍必然漂移。
    //     ① 顶点缓冲布局是 [示意结构][SDSS], SDSS 在末尾可独立截断;
    //     ② "星系数量"档位和"SDSS 开关"是**两个独立维度**,
    //        要按分段逻辑取 qMin 而不是简单相乘 —— 例如档位落在
    //        示意结构段内时, SDSS 应当完全不画。
    //
    //   参数: total     缓冲总粒子数
    //         sdssCount 其中 SDSS 星系数 (位于末尾)
    //         sdssVis   SDSS 开关 (-1=关闭, 0=全部, >0=指定数量)
    //         vis       "星系数量"档位 (<=0 或 > total 表示全部)
    static int effectiveDrawn(int total, int sdssCount, int sdssVis, int vis)
    {
        const int sdssWant = (sdssVis < 0) ? 0
                           : ((sdssVis == 0 || sdssVis > sdssCount)
                                  ? sdssCount : sdssVis);
        const int schCount = total - sdssCount;
        if (vis <= 0 || vis > total)
            return schCount + sdssWant;
        if (vis <= schCount)
            return vis;
        return schCount + qMin(vis - schCount, sdssWant);
    }

    // 场景半径 (映射后的最大半径)
    static float sceneRadius() { return 100.0f; }

    // ---- 距离映射模式 ----
    //
    // ★ 两种模式展示**同一批数据**, 只是轴的性质不同:
    //     对数压缩 (Log)  —— 一屏容纳 0.2 Mly ~ 46.5 Gly 共六个数量级,
    //                        代价是远处间隔被压缩 (看起来像挤在一起)
    //     真实比例 (Linear)—— 距离成比例, 但本星系群会缩到亚像素
    //                        (这在物理上**正确**: 它在宇宙尺度上确实那么小)
    //   做成开关, 是为了让"把宇宙塞进一屏付出了什么"这件事**可见** ——
    //   这本身就是教学要点。
    enum MapMode { MapLog = 0, MapLinear = 1 };

    void setMapMode(int m) { m_mapMode = (m == MapLinear) ? MapLinear : MapLog; }
    int  mapMode() const { return int(m_mapMode); }

    // 距离 (Mly) -> 场景半径。静态, 供 UI 侧算标签位置时复用。
    static float sceneRadiusFromMly(double mly, int mode);

    // 反函数: 场景半径 -> 真实距离 (Mly)。把旧的场景坐标迁移成物理量用。
    static double mlyFromSceneRadius(double r);

    // 真实距离 (Mly) -> 场景半径。对数映射, 见文件头说明。
    static float distToScene(double mly);

    // 具名结构列表 (位置已换算)
    // mode: 决定返回的场景坐标用哪种映射 —— 标签必须与粒子一致
    static QVector<CosmosMarker> markers(int mode = MapLog);

private:
    void build();

    // 距离映射模式 (对数压缩 / 真实比例)
    MapMode m_mapMode = MapLog;

    QOpenGLFunctions_3_3_Core *m_f = nullptr;
    QOpenGLShaderProgram *m_prog = nullptr;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};

    int m_count         = 0;
    int m_sdssCount = 0;      // SDSS 真实星系数
    int m_sdssVisible = 0;    // SDSS 可见数, <=0 全部
    int m_visible = 0;          // <=0 表示全部
    static int s_lastBuilt;
    static int s_lastBuiltSdss;     // 最近一次 build 的总数
    static int s_lastDrawn;         // 最近一帧实际 glDrawArrays 的数量
    int m_filamentCount = 0;   // 纤维中的星系
    int m_voidCount     = 0;   // 空洞边缘的稀疏星系

    bool m_ready = false;
};
