# ============================================================================
#  hubble_fit.py —— 从真实 Ia 型超新星数据拟合宇宙学参数
#
#  ★★ 为什么必须用超新星而不是 SDSS 星系:
#     哈勃图要检验的是 **v–d 关系**, 所以两者必须**独立测量**。
#     SDSS 的 LRG 只有光谱红移 —— 距离是**从红移推**出来的。
#     拿它画 v–d 图等于 "用 z 算 d 再去看 d 和 z 的关系", 是循环论证,
#     无论宇宙学参数取什么值, 图都会"符合"。
#     Ia 型超新星是**标准烛光**: 距离由视亮度独立测定, 与红移无关。
#
#  数据: Pantheon+ (2022) —— 1701 颗 Ia 型超新星, 红移 0.001~2.26
#        来源: https://github.com/PantheonPlusSH0ES/DataRelease
#
#  方法 (与 Perlmutter/Riess 的原始分析一致):
#    μ_model(z) = 5·log10(D_L(z; Ωm, ΩΛ)) + C
#    其中 D_L 是无量纲光度距离 (以 c/H0 为单位),
#    ★ C 同时吸收 H0 与超新星绝对星等 M —— 二者在哈勃图上**完全简并**,
#      所以必须作为冗余参数一起拟合, 否则会拟合出没有意义的 H0。
# ============================================================================
import math
import os
import sys

import numpy as np

# ---- 常量 ----
C_KMS = 299792.458
DATA = r'D:\tmp\sn\Pantheon+SH0ES.dat'


# ---------------------------------------------------------------------------
#  宇宙学: 无量纲光度距离
# ---------------------------------------------------------------------------
def e_z(z, om, ol):
    """E(z) = H(z)/H0。含曲率项 Ωk = 1 - Ωm - ΩΛ。"""
    return math.sqrt(om * (1.0 + z) ** 3 + (1.0 - om - ol) * (1.0 + z) ** 2 + ol)


def dl_over_dh(z, om, ol, n=400):
    """D_L / (c/H0) —— 无量纲光度距离。

    ★ 必须数值积分: E(z) 没有初等原函数。
      用 Simpson 法, 步长自适应于 z 的量级 ——
      固定步长在 z 跨 0.001~2.26 时要么远的太糙、要么近的太浪费。
    """
    if z <= 0.0:
        return 0.0
    # 对 1/E(z) 积分
    lo, hi = 0.0, z
    if n % 2:
        n += 1
    h = (hi - lo) / n
    s = 1.0 / e_z(lo, om, ol) + 1.0 / e_z(hi, om, ol)
    for i in range(1, n):
        w = 4.0 if i % 2 else 2.0
        s += w / e_z(lo + i * h, om, ol)
    dc = s * h / 3.0                      # 共动距离 (无量纲)
    return (1.0 + z) * dc                 # 光度距离 = (1+z)·共动距离


# ---------------------------------------------------------------------------
#  读数据
# ---------------------------------------------------------------------------
def load_pantheon(path):
    with open(path, encoding='utf-8') as f:
        hdr = f.readline().split()
        cols = {name: i for i, name in enumerate(hdr)}
        z, mu, err = [], [], []
        for ln in f:
            p = ln.split()
            if len(p) < 12:
                continue
            try:
                zz = float(p[cols['zHD']])        # ★ 用 zHD (已扣除本动速度)
                mm = float(p[cols['MU_SH0ES']])
                ee = float(p[cols['MU_SH0ES_ERR_DIAG']])
            except (ValueError, KeyError):
                continue
            if zz <= 0 or ee <= 0 or not math.isfinite(mm):
                continue
            z.append(zz); mu.append(mm); err.append(ee)
    return np.array(z), np.array(mu), np.array(err)


# ---------------------------------------------------------------------------
#  χ² —— C 解析边缘化
# ---------------------------------------------------------------------------
def chi2_over_C(z, mu, err, om, ol):
    """对 μ 的**零点 C** 解析边缘化后的 χ²。

    ★ 为什么解析而不是数值扫描 C:
      给定 Ωm/ΩΛ 时, 模型对 C 是**线性**的
      (μ_th = 5log10 D_L + C), χ² 对 C 是二次式, 有闭式极小点:
          C* = Σ w_i (μ_i − 5log10 D_i) / Σ w_i,   w_i = 1/σ_i²
      解析解既快又精确, 比网格扫描省掉一整个维度。
      (这也是宇宙学里拟合 H0/M 的标准做法)
    """
    inv = 1.0 / (err * err)
    m = np.array([5.0 * math.log10(dl_over_dh(zz, om, ol)) for zz in z])
    w = inv.sum()
    if w <= 0:
        return 1e30, 0.0
    cstar = (inv * (mu - m)).sum() / w
    r = mu - m - cstar
    return float((inv * r * r).sum()), float(cstar)


def main():
    if not os.path.isfile(DATA):
        print('缺少数据:', DATA); return 1

    z, mu, err = load_pantheon(DATA)
    print('超新星数量: %d' % len(z))
    print('红移范围:   %.4f ~ %.4f  (中位 %.3f)' % (z.min(), z.max(), np.median(z)))
    print('距离模数:   %.2f ~ %.2f' % (mu.min(), mu.max()))
    print()

    # ---- 1) 平坦宇宙下扫描 Ωm ----
    print('=== 平坦宇宙假设 (Ωm + ΩΛ = 1) ===')
    print('%8s %12s' % ('Ωm', 'χ²'))
    oms = np.arange(0.0, 1.001, 0.05)
    chis = []
    for om in oms:
        c, _ = chi2_over_C(z, mu, err, om, 1.0 - om)
        chis.append(c)
    chis = np.array(chis)
    for om, c in zip(oms, chis):
        bar = '█' * int(max(0, (c - chis.min()) ** 0.5 * 3))
        print('%8.2f %12.1f  %s' % (om, c, bar[:44]))

    ibest = int(np.argmin(chis))
    print()
    print('最佳拟合 Ωm = %.2f   (χ²/dof = %.3f)'
          % (oms[ibest], chis[ibest] / (len(z) - 2)))

    # 精细化: 在最佳点附近再扫一轮
    lo = max(0.0, oms[ibest] - 0.05)
    hi = min(1.0, oms[ibest] + 0.05)
    fine = np.linspace(lo, hi, 41)
    cf = np.array([chi2_over_C(z, mu, err, o, 1.0 - o)[0] for o in fine])
    jbest = int(np.argmin(cf))
    om_best = float(fine[jbest])
    print('精细化后 Ωm = %.4f' % om_best)

    # Δχ² = 1 给出 1σ 区间
    sig = fine[cf <= cf.min() + 1.0]
    if len(sig) >= 2:
        print('1σ 区间 (Δχ²=1): %.3f ~ %.3f' % (sig.min(), sig.max()))

    # ---- 2) 关键对比: 三种宇宙 ----
    print()
    print('=== 三种模型的 χ² 对比 ===')
    for name, om, ol in [
        ('ΛCDM     (Ωm=0.315, ΩΛ=0.685)', 0.315, 0.685),
        ('最佳拟合 (flat)',                om_best, 1.0 - om_best),
        ('爱因斯坦-德西特 (Ωm=1)',          1.0, 0.0),
        ('空宇宙   (Ωm=0)',                0.0, 0.0),
    ]:
        c, cc = chi2_over_C(z, mu, err, om, ol)
        print('  %-32s χ² = %9.1f   C = %8.4f' % (name, c, cc))
    c1, _ = chi2_over_C(z, mu, err, 1.0, 0.0)
    c0, _ = chi2_over_C(z, mu, err, 0.0, 0.0)
    print()
    print('★ Δχ² (EdS 相对最佳) = %.0f  ->  排除爱因斯坦-德西特宇宙' % (c1 - cf.min()))
    print('★ Δχ² (空宇宙相对最佳) = %.0f' % (c0 - cf.min()))
    print()
    print('★ 结论: 若宇宙是减速膨胀 (EdS), Δχ² 应接近 0。')
    print('  实测 Δχ² 高达 %.0f —— 这就是"宇宙在加速膨胀"的定量表述,' % (c1 - cf.min()))
    print('  也是 2011 年诺贝尔物理学奖的依据。')

    # ---- 3) 导出给 app ----
    out = r'D:\tmp\solar-system-cpp\assets\sn\pantheon.bin'
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'wb') as f:
        f.write(b'HUBB\x01')                      # 魔数 + 版本
        f.write(np.array([len(z)], dtype='<i4').tobytes())
        for arr in (z, mu, err):
            f.write(arr.astype('<f4').tobytes())
    print()
    print('已导出 %s  (%.1f KB)' % (out, os.path.getsize(out) / 1024))

    return 0


if __name__ == '__main__':
    sys.exit(main())
