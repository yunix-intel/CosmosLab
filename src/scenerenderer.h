// ============================================================================
//  scenerenderer.h —— OpenGL 场景渲染器
//
//  职责: 持有全部 GL 资源 (着色器 / 网格 / 纹理), 按给定的相机与时间绘制
//        一帧场景。它运行在 Qt Quick 的**渲染线程**上。
//
//  与 QML 的桥接在 sceneitem.cpp —— 那里负责把 GUI 线程的状态
//  (相机、时间、选中天体) 在 synchronize() 时安全地传进来。
// ============================================================================

#pragma once

#include "belts.h"
#include "comet.h"
#include "cosmos.h"
#include "camera.h"
#include "evostars.h"
#include "galaxy.h"
#include "postfx.h"
#include "scene.h"
#include "textures.h"
#include "viewstate.h"

#include <QHash>
#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>

class Mesh;

class SceneRenderer
{
public:
    SceneRenderer();
    ~SceneRenderer();

    void initialize();                      // GL 上下文就绪后调用一次
    void resize(int pixelW, int pixelH);
    void render(const ViewState &vs);       // 绘制一帧

    // 供 sceneitem 查询 (UI 显示用)
    const Scene &lastScene() const { return m_scene; }

private:
    void buildPrograms();
    void buildMeshes();
    void drawSkybox(const ViewState &vs);
    void drawGalaxyOverlay(const QMatrix4x4 &viewProj, const ViewState &vs);
    void drawOrbits(const ViewState &vs, const QMatrix4x4 &viewProj);
    void drawBodies(const ViewState &vs, const QMatrix4x4 &viewProj);
    void drawRings(const ViewState &vs, const QMatrix4x4 &viewProj);
    void drawAtmospheres(const ViewState &vs, const QMatrix4x4 &viewProj);

    // ★ 演化主星 (v1.3): HR 三剧本的 3D 发光球, 由 vs.evo* 驱动。
    //   复用 m_planet 管线 + m_sphere 网格, 无纹理纯黑体色自发光。
    void drawEvoStar(const ViewState &vs, const QMatrix4x4 &viewProj);

    // ★ 演化通用模拟体 (v1.3 A/B/C批): 非HR剧本的3D形态, 由 vs.evoViz/p1..p6 驱动。
    //   图元只有三种 (全复用现有管线, 零新着色器):
    //     evoBall  发光球 (m_planet + m_sphere, 太阳同款自发光)
    //     evoCone  锥/柱 (m_sphere Z拉伸, 喷流/外向流/灯塔/潮汐尾)
    //     evoShell 光晕壳 (m_atmo 背面壳加法混合, 激波/引力波纹/包层)
    void drawEvoSim(const ViewState &vs, const QMatrix4x4 &viewProj);

    // 银河系尺度视图 (与太阳系完全独立的一套绘制流程)
    void renderGalaxy(const ViewState &vs, const QMatrix4x4 &viewProj, GLint targetFbo);

    QOpenGLFunctions_3_3_Core *m_f = nullptr;

    QOpenGLShaderProgram *m_sky = nullptr;
    QOpenGLShaderProgram *m_planet = nullptr;
    QOpenGLShaderProgram *m_ring = nullptr;
    QOpenGLShaderProgram *m_orbit = nullptr;
    QOpenGLShaderProgram *m_atmo = nullptr;

    Mesh *m_sphere = nullptr;      // 单位球 (半径 1), 由 model 矩阵缩放
    Mesh *m_quad = nullptr;        // 全屏四边形
    QHash<QString, Mesh *> m_ringMeshes;
    QHash<QString, Mesh *> m_orbitMeshes;

    TextureCache m_tex;

    Scene m_scene;
    OrbitCamera m_camera;

    Galaxy m_galaxy;               // 银河系粒子模型 (只在银河系尺度使用)

    // 银河系参考底图 (NASA/JPL 官方插画, 半透明叠加)
    QOpenGLShaderProgram *m_galOverlayProg = nullptr;
    QOpenGLVertexArrayObject m_galOverlayVao;
    QOpenGLBuffer m_galOverlayVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLTexture *m_galOverlayTex = nullptr;
    Belts  m_belts;                // 小行星带 / 柯伊伯带 / 特洛伊群
    CometRenderer m_comets;        // 彗尾 (离子尾 + 尘埃尾)
    Cosmos        m_cosmos;        // 宇宙大尺度结构
    EvoStars      m_evoStars;      // 演化视图 112 条目分类星团 (v1.3)

    PostFX m_postfx;               // HDR + Bloom + ACES 后处理链

    int m_w = 1;
    int m_h = 1;
    bool m_ready = false;

    // 上一帧的相机状态, 用于检测变化
    double m_lastJd = -1.0;
    bool   m_lastRealScale = false;
    qint64 m_lastFrameMs = 0;
    bool   m_orbitDirty = true;

    // 镜头光斑所需的太阳屏幕位置
    float m_sunScreenX = 0.5f;
    float m_sunScreenY = 0.5f;
    bool  m_sunVisible = false;
};
