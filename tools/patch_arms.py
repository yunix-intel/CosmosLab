"""把旋臂粒子生成改为按 Reid 2019 的逐臂参数（螺距角 + 折点）。

★ 旧逻辑: 4 条臂, 统一螺距 12°, 均匀分布在 0/90/180/270°
★ 新逻辑: 6 段结构 (4 条主臂 + 外臂 + 猎户支), 每段用自己的
          螺距角与折点, 位置由 R(β) 解析式算出

R(β) = Rk · exp(-(β - βk) · tan(ψ))
  β 为银心方位角 (太阳方向为 0°, 从北看顺时针)
  ψ 取 ψ前 (β<βk) 或 ψ后 (β>βk)
场景方位角 φ = φ_sun + β
"""

C = r'D:\tmp\solar-system-cpp\src\galaxy.cpp'

OLD_START = '    // ==== 3. 旋臂 (4 条对数螺旋) ===='
OLD_END = '                put(x, y, z, cr, cg, cb,\n                    0.6f + uni(rng) * 1.1f,\n                    brightness(0.14f, 0.72f));\n            }'

NEW = r'''    // ==== 3. 旋臂 —— 参数取自 Reid et al. 2019 Table 2 ====
    //
    //  ★ 每条臂有自己的**螺距角**与**折点 (kink)**, 不再用统一螺距。
    //    实测螺距角范围 8.7°~19.5°, 差异显著 —— 用统一值会丢失
    //    "矩尺臂陡、英仙臂缓"这一真实特征。
    //
    //  R(β) = Rk · exp(-(β - βk) · tan(ψ)),  ψ 按 β 是否越过 βk 选取
    //  场景方位角 φ = φ_sun + β   (两者都是从北银极看的顺时针方位)
    {
        const float phiSun = float(gx::kOrionSpurAngleDeg * M_PI / 180.0);
        const int perArm = 11000;          // × 6 段 ≈ 66000 颗

        for (int ai = 0; ai < gx::kArmSpiralCount; ++ai) {
            const gx::ArmSpiral &A = gx::kArmSpiral[ai];
            const double bk = A.betaKinkDeg;
            const double tkPre  = std::tan(A.pitchPreDeg  * M_PI / 180.0);
            const double tkPost = std::tan(A.pitchPostDeg * M_PI / 180.0);

            for (int i = 0; i < perArm; ++i) {
                // 沿 β 采样。用 pow 让内侧略密 (内臂更亮更紧致)
                const float t  = std::pow(uni(rng), 0.85f);
                const double beta = A.betaBeginDeg
                                  + (A.betaEndDeg - A.betaBeginDeg) * double(t);
                const double dbeta = beta - bk;
                const double tk = dbeta >= 0.0 ? tkPost : tkPre;
                const double R  = A.rKinkLy * std::exp(-dbeta * tk);   // ly

                const float rr = float(R / gx::kLyPerUnit);            // 场景单位

                // 臂的径向宽度: 表值为含 90% 示踪物的全宽, 取一半作 σ
                const float sigR = float(A.widthLy / gx::kLyPerUnit) * 0.5f;

                // ★ 臂宽随半径略微展开 (真实旋臂外侧更松散),
                //   但不能太大, 否则相邻臂会连成一片白带。
                const float sig = sigR * (0.75f + 0.85f * (rr / rDisk));

                const float dRad = gauss(rng) * sig;
                const float dAng = gauss(rng) * sig / qMax(rr, 4.0f);

                const float r2 = qMax(0.5f, rr + dRad);
                const float a2 = phiSun + float(beta * M_PI / 180.0) + dAng;

                const float x = r2 * std::cos(a2);
                const float z = r2 * std::sin(a2);
                const float y = gauss(rng) * diskTh * (0.55f + 0.5f * (rr / rDisk));

                // 年轻蓝白星为主; 少量 HII 区呈粉红
                float cr = 0.70f, cg = 0.82f, cb = 1.00f;
                if (uni(rng) < 0.045f) {
                    cr = 1.00f; cg = 0.62f; cb = 0.72f;
                } else {
                    const float v = uni(rng) * 0.22f;
                    cr += v; cg += v * 0.8f; cb -= v * 0.35f;
                }

                put(x, y, z, cr, cg, cb,
                    0.6f + uni(rng) * 1.1f,
                    brightness(0.14f, 0.72f));
            }'''


def main():
    s = open(C, encoding='utf-8').read()
    a = s.index(OLD_START)
    b = s.index(OLD_END, a) + len(OLD_END)
    s = s[:a] + NEW + s[b:]
    open(C, 'w', encoding='utf-8').write(s)
    print('旋臂生成已改为逐臂参数 (Reid 2019)')


if __name__ == '__main__':
    main()
