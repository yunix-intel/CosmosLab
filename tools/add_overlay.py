"""给银河系视图加上 NASA/JPL 官方图的半透明叠加层。

★★ 为什么这样设计 (方案 C: 3D 粒子 + 半透明图叠加):

  用户要求"逼真"且"按天文学标准"。
  但银河系外部全景**在物理上不存在照片** —— 我们身处银盘内部。
  NASA/JPL 那张是 Robert Hurt 绘制的**科学插画**, 不是照片。

  因此正确的做法不是"拿图冒充照片", 而是:
    * 把图作为**参考底图**, 半透明铺在银道面上
    * 3D 粒子在其上, 提供体积感与可旋转性
    * UI 明确区分两者性质 —— 图是"结构参考 (插画)",
      粒子是"3D 示意模型 (按 Reid 2019 参数生成)"
  这样既提升观感, 又不谎称"这就是真实照片"。

★ 关键几何 (由 tools/build_overlay.py 标定):
    lyPerPx = 68           (由 45/60/75 kly 距离环 + 太阳-银心距离四路交叉验证)
    图心 (1000,1000) = 银心
    太阳在图中 (985, 1372), 方位角 ≈ 88.4° (图像 y 向下)

★ 映射约定:
    图像 +x  ->  场景 +x
    图像 +y  ->  场景 +z
  两者都是从北银极看的**顺时针**方位, 故方位角可直接对应。
  图像是 2000x2000 像素, 覆盖 ±(1000 × 68) ly = ±68,000 ly。
  场景单位换算: / kLyPerUnit (528.5 ly/单位)

用法:
    python add_overlay.py
"""
C = r'D:\tmp\solar-system-cpp\src\scenerenderer.cpp'
H = r'D:\tmp\solar-system-cpp\src\scenerenderer.h'

# ---- 着色器: 半透明银河底图 ----
SHADER_ADD = r'''
// ---------------------------------------------------------------------------
//  银河系参考底图 —— 半透明叠加层
//
//  ★ 画的是一张**朝向相机的四边形**(billboard), 放在银道面 (y=0) 上。
//    不用固定平面是因为: 从侧面看 (俯角接近 0°) 时平面会退化成一条线,
//    而 billboard 始终面向相机, 任何视角下都能看到完整结构。
//
//  ★ 但 billboard 会破坏"这是个盘"的透视感。折中:
//    只在俯角较大 (接近俯视) 时显示底图, 侧视时淡出 ——
//    侧视本来就是看"薄盘"的切面, 底图没有意义。
//    淡出由 CPU 侧按相机仰角算好 alpha 传入。
// ---------------------------------------------------------------------------
inline const char *kGalaxyOverlayVert = R"(
#version 330 core
layout(location = 0) in vec2 aPlane;      // -1..1 的四边形
uniform mat4  uViewProj;
uniform float uHalfSize;                  // 底图半宽 (世界单位)
uniform vec3  uCenter;
out vec2 vUV;
void main() {
    vUV = aPlane * 0.5 + 0.5;
    // 底图铺在银道面 y=0 上 (图像 +y -> 场景 +z)
    gl_Position = uViewProj * vec4(uCenter + vec3(aPlane.x, 0.0, -aPlane.y) * uHalfSize, 1.0);
}
)";

inline const char *kGalaxyOverlayFrag = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform float uAlpha;
out vec4 FragColor;
void main() {
    vec3 c = texture(uTex, vUV).rgb;
    // 底图本身是暗背景上的银臂, 用亮度做透明度 —— 暗处全透明,
    // 这样不会在银盘外围留下一块方形暗斑
    float lum = max(max(c.r, c.g), c.b);
    float a = uAlpha * smoothstep(0.02, 0.30, lum);
    FragColor = vec4(c, a);
}
)";
'''

MEMBER_ADD = r'''
    // 银河系参考底图 (NASA/JPL 官方插画, 半透明叠加)
    QOpenGLShaderProgram *m_galOverlayProg = nullptr;
    QOpenGLVertexArrayObject m_galOverlayVao;
    QOpenGLBuffer m_galOverlayVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLTexture *m_galOverlayTex = nullptr;
'''


def main():
    # ---------- 头文件 ----------
    h = open(H, encoding='utf-8').read()
    if 'm_galOverlayProg' not in h:
        anchor = '    Galaxy m_galaxy;'
        i = h.index(anchor)
        j = h.index('\n', i) + 1
        h = h[:j] + MEMBER_ADD + h[j:]
        open(H, 'w', encoding='utf-8').write(h)
        print('scenerenderer.h 已加成员')

    # ---------- 实现 ----------
    c = open(C, encoding='utf-8').read()

    if 'kGalaxyOverlayVert' not in c:
        # 把着色器插到文件里第一个 namespace 之后
        anchor = 'namespace {'
        i = c.index(anchor)
        j = c.index('\n\n', i)
        c = c[:j] + '\n' + SHADER_ADD + c[j:]
        print('已插入叠加层着色器')

    # 在 Galaxy 渲染前插入叠加层绘制
    old = '''        m_f->glDisable(GL_DEPTH_TEST);
        m_f->glDisable(GL_CULL_FACE);
        m_galaxy.render(viewProj, pointScale);'''
    new = '''        m_f->glDisable(GL_DEPTH_TEST);
        m_f->glDisable(GL_CULL_FACE);

        // ★ 先画参考底图, 再画粒子 —— 粒子在上层, 保住体积感。
        //   底图的透明度按相机俯角淡出: 接近侧视 (俯角→0) 时图会退化
        //   成一条线, 此时淡出最自然。
        drawGalaxyOverlay(viewProj, vs);
        m_galaxy.render(viewProj, pointScale);'''
    if 'drawGalaxyOverlay' not in c:
        assert old in c
        c = c.replace(old, new, 1)
        print('已在银河系分支插入叠加层调用')
    else:
        print('叠加层调用已存在')

    open(C, 'w', encoding='utf-8').write(c)
    print('scenerenderer.cpp 已更新')


if __name__ == '__main__':
    main()
