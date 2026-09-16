// ============================================================================
//  shaders.h —— 全部 GLSL 源码
//
//  移植自 Python 版 core/shaders.py。保留其光照模型的关键特征:
//    * 晨昏线用 smoothstep 柔化 (比 max(NdotL,0) 更接近真实大气散射)
//    * 冷色环境光 + 暖色阳光的对比
//    * 晨昏线附近的暖色偏折 (模拟大气折射使日落呈红)
//    * 大气临边辉光 (rim)
//  本次不移植: 夜面城市灯光、镜面高光贴图、HDR/Bloom 后处理链。
//
//  注意: Qt6 的 QOpenGLShaderProgram 可以直接用字符串源码, 不需要 .qsb ——
//        那是 QML ShaderEffect 的要求, 与这里的原生 GL 管线无关。
// ============================================================================

#pragma once

namespace shaders {

// ---------------------------------------------------------------------------
//  全屏四边形 (背景 / 后期处理)
// ---------------------------------------------------------------------------
inline const char *kFullscreenVert = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

// ---------------------------------------------------------------------------
//  星空背景 —— 银河纹理 + 程序化星点
//
//  用逆视图投影把屏幕坐标还原成视线方向, 再按方向采样银河纹理。
//  星点用方向量化的哈希生成, 这样旋转相机时星星固定在天球上。
// ---------------------------------------------------------------------------
inline const char *kSkyboxFrag = R"(
#version 330 core
in vec2 vUV;
uniform mat4  uInvViewProj;
uniform vec3  uCamPos;
uniform sampler2D uMilkyWay;
uniform float uHasMilkyWay;
uniform float uExposure;
out vec4 FragColor;

vec3 rayDir(vec2 uv, mat4 invVP) {
    vec4 p = invVP * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    return normalize(p.xyz / p.w - uCamPos);
}

void main() {
    vec3 d = rayDir(vUV, uInvViewProj);

    // 底色: 极暗的深空蓝。
    // ★ 这条链后面有 ACES + gamma 编码, 会显著抬升暗部 —— 写 0.004 时
    //   最终屏幕上是约 5% 的灰雾, 背景就\"不黑\"了 (实测正是如此)。
    //   必须取 5e-4 量级, ACES 压缩后 gamma 编码出来才 <2%, 是真正的深空。
    vec3 col = vec3(0.0005, 0.0007, 0.0016);

    // 银河带 (纹理是 sRGB 存储, 须转线性后再进 HDR 缓冲)
    if (uHasMilkyWay > 0.5) {
        float u = atan(d.z, d.x) / 6.28318530718 + 0.5;
        float v = asin(clamp(d.y, -1.0, 1.0)) / 3.14159265359 + 0.5;
        col += pow(texture(uMilkyWay, vec2(u, v)).rgb, vec3(2.0)) * 0.42;
    }

    // 程序化星点: 按方向量化 + 哈希
    vec3 g = floor(d * 620.0);
    float h = fract(sin(dot(g, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
    float star = smoothstep(0.9975, 0.99995, h);
    // 亮星略有暖冷差异
    float t = fract(h * 91.7);
    vec3 tint = mix(vec3(0.75, 0.85, 1.0), vec3(1.0, 0.88, 0.72), t);
    col += tint * star * 1.35;

    FragColor = vec4(col * uExposure, 1.0);
}
)";

// ---------------------------------------------------------------------------
//  行星
// ---------------------------------------------------------------------------
inline const char *kPlanetVert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;

uniform mat4 uModel;
uniform mat4 uViewProj;

// 屏幕空间最小尺寸 —— 真实比例模式的关键
//
// 数学事实: 真实比例下地球半径 6371 km = 0.0064 场景单位, 在 320 单位
// 视距下的角直径只有 4e-5 rad, 在 1800px 高的画面上约 0.07 像素 ——
// 完全不可见。这不是 bug, 是尺度本身的含义。
//
// 但教学软件若什么都不显示就没有意义。业界通行做法 (Celestia、
// Stellarium) 是给天体一个**屏幕空间最小尺寸**: 轨道与间距严格真实,
// 球体在过小时按像素下限放大以便观察, 并在 UI 上说明。
uniform vec3  uBodyCenter;   // 天体中心 (世界坐标)
uniform float uBodyRadius;   // 天体半径 (世界单位)
uniform float uMinPixelR;    // 最小屏幕半径 (像素), 0 = 不限制
uniform float uProjScale;    // = viewportHeight / (2·tan(fov/2))

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vTangent;

void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);

    // 屏幕空间最小尺寸: 半径投影到像素后若小于下限, 按比例放大顶点偏移。
    // 只放大几何, 不改变位置 uBodyCenter —— 所以轨道位置仍然精确。
    if (uMinPixelR > 0.0 && uBodyRadius > 0.0) {
        vec4 centerClip = uViewProj * vec4(uBodyCenter, 1.0);
        float w = max(centerClip.w, 1e-6);
        float pixelR = uProjScale * uBodyRadius / w;
        if (pixelR < uMinPixelR) {
            float grow = uMinPixelR / max(pixelR, 1e-6);
            wp.xyz = uBodyCenter + (wp.xyz - uBodyCenter) * grow;
        }
    }

    vWorldPos = wp.xyz;
    vNormal   = normalize(mat3(uModel) * aNormal);
    vTangent  = normalize(mat3(uModel) * aTangent);
    vUV = aUV;
    gl_Position = uViewProj * wp;
}
)";

inline const char *kPlanetFrag = R"(
#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec3 vTangent;

uniform sampler2D uAlbedo;
uniform sampler2D uNormalMap;
uniform float uHasNormalMap;
uniform float uNormalScale;

uniform vec3  uSunPos;
uniform vec3  uSunColor;
uniform float uSunIntensity;

uniform vec3  uAmbientColor;
uniform float uAmbientStrength;

uniform vec3  uCamPos;
uniform vec3  uBaseColor;
uniform float uHasTexture;

uniform float uAtmoRim;        // 临边辉光强度
uniform vec3  uAtmoColor;
uniform float uEmissive;       // 恒星自发光

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uSunPos - vWorldPos);
    vec3 V = normalize(uCamPos - vWorldPos);

    // ---- 切线空间法线贴图 ----
    if (uHasNormalMap > 0.5) {
        vec3 T = normalize(vTangent - N * dot(N, vTangent));
        vec3 B = cross(N, T);
        vec3 nt = texture(uNormalMap, vUV).xyz * 2.0 - 1.0;
        nt.xy *= uNormalScale;
        N = normalize(mat3(T, B, N) * nt);
    }

    float NdotL = dot(N, L);
    float NdotV = max(dot(N, V), 0.0);

    // 晨昏线柔化: 比 max(NdotL,0) 更接近真实的大气散射过渡
    float diff = smoothstep(-0.08, 0.22, NdotL);

    vec3 albedo = uHasTexture > 0.5 ? texture(uAlbedo, vUV).rgb : uBaseColor;
    albedo = pow(albedo, vec3(2.2));                 // sRGB -> 线性

    // 冷色环境光, 与暖阳形成对比
    vec3 ambient = uAmbientColor * uAmbientStrength;
    vec3 color = albedo * (ambient + uSunColor * diff * uSunIntensity);

    // 晨昏线附近的暖色偏折 (大气折射使日落呈红)
    float terminator = smoothstep(0.0, 0.45, NdotL)
                     * (1.0 - smoothstep(0.35, 0.9, NdotL));
    color += albedo * vec3(1.0, 0.42, 0.14) * terminator * 0.35;

    // 大气临边辉光: 视线越掠射越亮, 且只在向阳侧可见
    float rim = pow(1.0 - NdotV, 3.0);
    color += uAtmoColor * rim * uAtmoRim * smoothstep(-0.15, 0.5, NdotL);

    // 恒星自发光 (太阳)
    color += albedo * uEmissive;

    FragColor = vec4(color, 1.0);
}
)";

// ---------------------------------------------------------------------------
//  环系
//
//  环的 UV 为「径向 + 角度」二维: UV.x = 0 内缘 / 1 外缘, UV.y = 绕一圈。
//  早期版本每段都映射 UV.x 0->1 且 UV.y 固定 0.5, 导致 1D 径向纹理每段
//  重复形成放射状条纹。改用二维 UV 后自然环绕无接缝。
// ---------------------------------------------------------------------------
inline const char *kRingVert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uViewProj;

out vec3 vWorldPos;
out vec2 vUV;

void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vUV = aUV;
    gl_Position = uViewProj * wp;
}
)";

inline const char *kRingFrag = R"(
#version 330 core

in vec3 vWorldPos;
in vec2 vUV;

uniform sampler2D uRingTex;
uniform float uHasTexture;
uniform vec4  uRingColor;

uniform vec3  uSunPos;
uniform float uSunIntensity;
uniform vec3  uPlanetPos;
uniform float uPlanetRadius;   // 场景单位
uniform float uAmbient;

uniform vec3  uCamPos;

out vec4 FragColor;

void main() {
    vec4 c = uHasTexture > 0.5 ? texture(uRingTex, vUV) : vec4(1.0);
    vec4 col = c * uRingColor;

    vec3 L = normalize(uSunPos - vWorldPos);
    float lit = max(dot(vec3(0.0, 1.0, 0.0), L), 0.0);

    // ---- 行星本影 ----
    // 正确判据: P 在影中 <=> ① 从行星看 P 在背光侧 ② P 到行星-太阳轴线的
    // 垂距 < 行星半径。
    // 早期版本用 dot(C-P, L) > 0 判断, 在 P 位于向阳侧时也成立
    // (因为 C-P 指向背离太阳的方向), 把朝阳环面误判为本影 —— 表现为环上
    // 一块扇形缺口, 看起来像几何缺失。
    vec3 toSun = normalize(uSunPos - uPlanetPos);
    vec3 rel   = vWorldPos - uPlanetPos;
    float along = dot(rel, toSun);
    float perp  = length(rel - toSun * along);
    float shadow = (along < 0.0 && perp < uPlanetRadius) ? 1.0 : 0.0;
    // 半影过渡: 本影不完全归零 —— 真实环粒子会散射环境光,
    // 归零会让阴影区与黑背景融为一体。
    float shade = mix(1.0, 0.16, smoothstep(uPlanetRadius * 1.25,
                                            uPlanetRadius * 0.75, perp)
                                 * shadow);

    float lum = uAmbient + lit * uSunIntensity * shade;
    FragColor = vec4(col.rgb * lum, col.a * uRingColor.a);
}
)";

// ---------------------------------------------------------------------------
//  轨道线
// ---------------------------------------------------------------------------
inline const char *kOrbitVert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uViewProj;
uniform vec3 uCamPos;
out float vFade;
void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
    // 远处的轨道线淡出, 避免全屏蛛网
    float d = length(aPos - uCamPos);
    vFade = 1.0 - smoothstep(1200.0, 4200.0, d);
}
)";

inline const char *kOrbitFrag = R"(
#version 330 core
in float vFade;
uniform vec3  uColor;
uniform float uOpacity;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor, uOpacity * vFade);
}
)";

// ---------------------------------------------------------------------------
//  大气壳 (加法混合的背面球)
// ---------------------------------------------------------------------------
inline const char *kAtmoVert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uViewProj;
out vec3 vWorldPos;
out vec3 vNormal;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vNormal   = normalize(mat3(uModel) * aNormal);
    gl_Position = uViewProj * wp;
}
)";

inline const char *kAtmoFrag = R"(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
uniform vec3  uSunPos;
uniform vec3  uCamPos;
uniform vec3  uAtmoColor;
uniform float uOpacity;
uniform float uPower;
out vec4 FragColor;
void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = normalize(uSunPos - vWorldPos);
    // 背面壳: 菲涅耳越掠射越亮
    float fres = pow(1.0 - abs(dot(N, V)), uPower);
    float lit  = smoothstep(-0.25, 0.55, dot(-N, L));
    float a = fres * lit * uOpacity;
    FragColor = vec4(uAtmoColor * a, a);
}
)";

// ===========================================================================
//  HDR 后处理链
//
//  移植自 Python 版 core/postfx.py + core/shaders.py。
//
//  没有这条链的话, 光照强度必须压在 1.0 以下才不会死白, 于是行星永远
//  灰蒙蒙、太阳也没有耀光。有了 HDR + Bloom + ACES 之后, 光照可以开到
//  3.2 (平方反比), 超亮部分由 ACES 优雅滚降, 溢出的能量变成 Bloom 光晕,
//  这才是「好看」的来源。
// ===========================================================================

// --- 亮部提取 (4 点盒式 + 软膝阈值) ---
inline const char *kBloomPrefilterFrag = R"(
#version 330 core
in vec2 vUV;

uniform sampler2D uScene;
uniform float uThreshold;
uniform float uSoftKnee;
uniform vec2  uTexel;

out vec4 FragColor;

vec3 fetch(vec2 uv) { return texture(uScene, uv).rgb; }

void main() {
    // 4 点盒式采样, 减少闪烁
    vec3 c = fetch(vUV) * 0.5;
    c += fetch(vUV + vec2( uTexel.x,  uTexel.y)) * 0.125;
    c += fetch(vUV + vec2(-uTexel.x,  uTexel.y)) * 0.125;
    c += fetch(vUV + vec2( uTexel.x, -uTexel.y)) * 0.125;
    c += fetch(vUV + vec2(-uTexel.x, -uTexel.y)) * 0.125;

    // 软膝阈值: 亮部平滑过渡, 避免硬切边
    float br = max(c.r, max(c.g, c.b));
    float knee = uThreshold * uSoftKnee + 1e-5;
    float soft = clamp(br - uThreshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    float w = max(soft, br - uThreshold) / max(br, 1e-5);

    FragColor = vec4(c * w, 1.0);
}
)";

// --- 降采样 (13-tap, COD 风格) ---
inline const char *kBloomDownFrag = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uSrc;
uniform vec2 uTexel;
out vec4 FragColor;

vec3 s(vec2 uv) { return texture(uSrc, uv).rgb; }

void main() {
    vec2 t = uTexel;
    vec3 a = s(vUV + vec2(-2.0,  2.0) * t);
    vec3 b = s(vUV + vec2( 0.0,  2.0) * t);
    vec3 c = s(vUV + vec2( 2.0,  2.0) * t);
    vec3 d = s(vUV + vec2(-2.0,  0.0) * t);
    vec3 e = s(vUV);
    vec3 f = s(vUV + vec2( 2.0,  0.0) * t);
    vec3 g = s(vUV + vec2(-2.0, -2.0) * t);
    vec3 h = s(vUV + vec2( 0.0, -2.0) * t);
    vec3 i = s(vUV + vec2( 2.0, -2.0) * t);

    vec3 j = s(vUV + vec2(-1.0,  1.0) * t);
    vec3 k = s(vUV + vec2( 1.0,  1.0) * t);
    vec3 l = s(vUV + vec2(-1.0, -1.0) * t);
    vec3 m = s(vUV + vec2( 1.0, -1.0) * t);

    vec3 col = e * 0.125;
    col += (a + c + g + i) * 0.03125;
    col += (b + d + f + h) * 0.0625;
    col += (j + k + l + m) * 0.125;
    FragColor = vec4(col, 1.0);
}
)";

// --- 升采样 (3x3 tent filter, 加法混合叠加) ---
inline const char *kBloomUpFrag = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uSrc;
uniform vec2  uTexel;
uniform float uRadius;
out vec4 FragColor;

void main() {
    vec2 t = uTexel * uRadius;
    vec3 col = vec3(0.0);
    col += texture(uSrc, vUV + vec2(-1.0,  1.0) * t).rgb * 0.0625;
    col += texture(uSrc, vUV + vec2( 0.0,  1.0) * t).rgb * 0.125;
    col += texture(uSrc, vUV + vec2( 1.0,  1.0) * t).rgb * 0.0625;
    col += texture(uSrc, vUV + vec2(-1.0,  0.0) * t).rgb * 0.125;
    col += texture(uSrc, vUV                        ).rgb * 0.25;
    col += texture(uSrc, vUV + vec2( 1.0,  0.0) * t).rgb * 0.125;
    col += texture(uSrc, vUV + vec2(-1.0, -1.0) * t).rgb * 0.0625;
    col += texture(uSrc, vUV + vec2( 0.0, -1.0) * t).rgb * 0.125;
    col += texture(uSrc, vUV + vec2( 1.0, -1.0) * t).rgb * 0.0625;
    FragColor = vec4(col, 1.0);
}
)";

// --- 合成: 场景 + Bloom + 镜头光斑 + 暗角 + ACES + 颗粒 ---
inline const char *kCompositeFrag = R"(
#version 330 core
in vec2 vUV;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomStrength;
uniform float uExposure;
uniform float uVignette;
uniform float uGrain;
uniform float uTime;
uniform float uSunScreenX;      // 太阳屏幕位置 (用于光斑)
uniform float uSunScreenY;
uniform float uSunVisible;
uniform float uFlareStrength;
uniform vec2  uResolution;

out vec4 FragColor;

// ---- ACES Filmic (Narkowicz 近似) ----
vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ---- 镜头光斑: 沿太阳-屏幕中心连线的鬼影 ----
float ghost(vec2 uv, vec2 pos, float size) {
    vec2 d = uv - pos;
    float r = length(d) / size;
    return exp(-r * r * 3.5) * (1.0 - smoothstep(0.85, 1.0, r));
}

void main() {
    vec3 scene = texture(uScene, vUV).rgb;
    vec3 bloom = texture(uBloom, vUV).rgb;

    scene += bloom * uBloomStrength;

    // ---- 镜头光斑 ----
    if (uSunVisible > 0.5 && uFlareStrength > 0.0) {
        vec2 sunPos = vec2(uSunScreenX, uSunScreenY);
        float aspect = uResolution.x / max(uResolution.y, 1.0);

        // 以屏幕中心为原点, 太阳的镜像方向
        vec2 center = vec2(0.5);
        vec2 dir = center - sunPos;

        float flare = 0.0;
        // 一串沿轴鬼影, 尺寸与颜色各异
        flare += ghost(vUV, sunPos + dir * 0.35, 0.040) * 0.85;
        flare += ghost(vUV, sunPos + dir * 0.62, 0.030) * 0.55;
        flare += ghost(vUV, sunPos + dir * 0.95, 0.075) * 0.35;
        flare += ghost(vUV, sunPos + dir * 1.28, 0.045) * 0.45;
        flare += ghost(vUV, sunPos + dir * 1.65, 0.020) * 0.60;

        // 横向拉伸的 anamorphic 光晕 —— 必须很快衰减,
        // 否则会形成贯穿全屏的亮线 (早期版本 exp(-|dx|*2.2) 衰减过慢)
        vec2 d = (vUV - sunPos) * vec2(aspect, 1.0);
        float streak = exp(-abs(d.y) * 260.0) * exp(-abs(d.x) * 40.0) * 0.50;

        vec3 flareCol = mix(vec3(1.0, 0.86, 0.62), vec3(0.66, 0.78, 1.0), vUV.y);
        scene += flareCol * (flare + streak) * uFlareStrength;
    }

    // ---- 暗角 ----
    vec2 vc = (vUV - 0.5) * vec2(uResolution.x / max(uResolution.y, 1.0), 1.0);
    float vig = 1.0 - uVignette * dot(vc, vc) * 1.15;
    scene *= clamp(vig, 0.0, 1.0);

    // ---- 曝光 + 色调映射 ----
    vec3 mapped = aces(scene * uExposure);

    // ---- 胶片颗粒 (在色调映射后叠加, 更接近真实胶片) ----
    float n = fract(sin(dot(vUV * uResolution + uTime * 137.0,
                            vec2(12.9898, 78.233))) * 43758.5453);
    mapped += (n - 0.5) * uGrain;

    FragColor = vec4(pow(mapped, vec3(1.0 / 2.2)), 1.0);
}
)";

} // namespace shaders
