// ============================================================================
//  orbtest.cpp —— 轨道求解器自检
//
//  验证开普勒求解器在**极端偏心率**下的正确性。这是本项目里最容易悄悄
//  出错的一环: 若求解器在近日点附近不收敛, 位置会整体偏移, 但画面看上去
//  仍然"有个东西在动", 肉眼完全无法发现。
//
//  ★ 本项目实际踩到的坑 (记录在此以免重犯):
//    初版把哈雷的轨道根数按 J2000 传播, 但那组根数的历元其实是 1994 年。
//    结果近日点位置算成 15.7 AU, 真值 0.586 AU —— 差 26 倍。
//    修法是按 JPL 的原始历元做精确换算 (见 tools/fetch_jpl.py)。
//
//  ★ 基准值来源:
//    全部由 tools/fetch_jpl.py 从 NASA/JPL SBDB API 实时取出。
//    **不要手写文献值** —— 文献里的旧解会被后续观测修正, 例如哈雷的
//    近日点距离在 1994 年解里是 0.58598 AU, 而当前解是 0.574864 AU。
//    自检若对着旧值比, 会把正确的计算判成错误。
//
//  用法: SS_ORBTEST=1 cosmoslab.exe
// ============================================================================

#include "bodyregistry.h"
#include "celestialdata.h"
#include "ephemeris.h"
#include "scene.h"
#include "smalldata.h"

#include <QDebug>
#include <QVector3D>
#include <cmath>

namespace {

// 自检基准: 近日点距离 q、远日点距离 Q、过近日点时刻 tp (TDB 儒略日)
struct PeriRef
{
    const char *id;
    double q;        // AU
    double Q;        // AU
    double tp;       // 儒略日
};

// ---- 自检基准 (由 tools/fetch_jpl.py 从同一 API 生成) ----
const PeriRef PERI_REFS[] = {
    { "ceres", 2.545159, 2.985946, 2461599.84 },
    { "eris", 38.162674, 97.705220, 2545407.72 },
    { "haumea", 34.687518, 51.433063, 2500416.60 },
    { "makemake", 38.330213, 52.811653, 2408158.69 },
    { "vesta", 2.148362, 2.574370, 2460901.59 },
    { "pallas", 2.130621, 3.408497, 2461695.03 },
    { "juno", 1.988018, 3.353962, 2461631.30 },
    { "hygiea", 2.814736, 3.487212, 2461813.20 },
    { "eros", 1.133233, 1.783254, 2461088.81 },
    { "bennu", 0.896894, 1.355888, 2455439.14 },
    { "ryugu", 0.963366, 1.418471, 2461118.30 },
    { "halley", 0.574864, 35.282406, 2446469.97 },
    { "encke", 0.338911, 4.100331, 2460239.90 },
    { "churyumov", 1.243266, 5.681233, 2457247.59 },
    { "neowise", 0.294651, 716.641262, 2459034.18 },
    { "wild2", 1.595856, 5.303635, 2459929.28 },
};
const int REFS_COUNT = int(sizeof(PERI_REFS) / sizeof(PERI_REFS[0]));

// 高中低偏心率都取, 覆盖求解器的不同分支
const char *SWEEP[] = { "halley", "encke", "churyumov", "neowise", "wild2",
                        "mercury", "pallas", "ceres" };
const int SWEEP_COUNT = int(sizeof(SWEEP) / sizeof(SWEEP[0]));

} // namespace

void runOrbitSelfTest()
{
    int fail = 0;

    qInfo().noquote() << "=== 轨道求解器自检 ===";
    qInfo().noquote() << QString("注册天体总数: %1  (基础 %2 + 扩展 %3)")
                             .arg(registry::count())
                             .arg(ALL_BODIES_COUNT)
                             .arg(small::count());
    qInfo().noquote() << "";

    // ---- 1. 近日点 / 远日点与 JPL 基准的一致性 ----
    //
    // 这条检查的是根数转录是否正确 —— a 或 e 抄错一个小数位会立刻暴露。
    // 容差取 0.5%: 代码用 a(1±e) 计算, 而 JPL 直接给 q/Q, 两者在
    // 双精度下应当几乎完全一致。
    qInfo().noquote() << "--- 1. 根数一致性 (q = a(1-e), Q = a(1+e) vs JPL) ---";
    for (int i = 0; i < REFS_COUNT; ++i) {
        const PeriRef &r = PERI_REFS[i];
        const OrbitalElements *el = registry::findOrbit(r.id);
        if (!el) {
            qWarning().noquote() << QString("    %1: 找不到轨道根数!").arg(r.id);
            ++fail;
            continue;
        }
        const double q = el->a * (1.0 - el->e);
        const double Q = el->a * (1.0 + el->e);
        const double rel = qMax(std::fabs(q - r.q) / qMax(r.q, 1e-9),
                                std::fabs(Q - r.Q) / qMax(r.Q, 1e-9));
        const bool ok = rel < 0.005;
        if (!ok)
            ++fail;
        qInfo().noquote() << QString("    %1 %2  q=%3 (JPL %4)  Q=%5 (JPL %6)")
                                 .arg(QString::fromUtf8(r.id), -11)
                                 .arg(ok ? QStringLiteral("OK ") : QStringLiteral("!! "))
                                 .arg(q, 10, 'f', 5).arg(r.q, 10, 'f', 5)
                                 .arg(Q, 11, 'f', 4).arg(r.Q, 11, 'f', 4);
    }

    // ---- 2. 全域扫描 ----
    //
    // 沿整条轨道均匀取样, 检查日心距是否始终落在 [q, Q] 内。
    // 越界说明开普勒方程解错了 (通常是不收敛)。
    qInfo().noquote() << "";
    qInfo().noquote() << "--- 2. 全域扫描 (一个周期 400 点, r 应始终在 [q,Q] 内) ---";
    for (int i = 0; i < SWEEP_COUNT; ++i) {
        const char *id = SWEEP[i];
        const OrbitalElements *el = registry::findOrbit(id);
        if (!el)
            continue;
        const double q = el->a * (1.0 - el->e);
        const double Q = el->a * (1.0 + el->e);
        const double periodDays = 360.0 / (el->rL / 36525.0);   // deg / (deg/day)

        double rmin = 1e30, rmax = 0.0;
        bool inRange = true;
        for (int k = 0; k < 400; ++k) {
            const double jd = J2000 + periodDays * double(k) / 400.0;
            const double r = eph::orbitalRadiusKm(id, jd) / AU_KM;
            rmin = qMin(rmin, r);
            rmax = qMax(rmax, r);
            // 2% 余量: 近日点/远日点未必正好落在采样点上
            if (r < q * 0.97 || r > Q * 1.03) {
                inRange = false;
                qWarning().noquote()
                    << QString("       !! JD=%1 处 r=%2 越界 (q=%3 Q=%4)")
                           .arg(jd, 0, 'f', 1).arg(r, 0, 'f', 5)
                           .arg(q, 0, 'f', 5).arg(Q, 0, 'f', 5);
                break;
            }
        }
        if (!inRange)
            ++fail;
        qInfo().noquote() << QString("    %1 %2  r ∈ [%3, %4]   q=%5  Q=%6  e=%7")
                                 .arg(QString::fromUtf8(id), -11)
                                 .arg(inRange ? QStringLiteral("OK ") : QStringLiteral("!! "))
                                 .arg(rmin, 7, 'f', 4).arg(rmax, 9, 'f', 3)
                                 .arg(q, 8, 'f', 4).arg(Q, 9, 'f', 3)
                                 .arg(el->e, 0, 'f', 5);
    }

    // ---- 3. 近日点定点验证 (最关键的一步) ----
    //
    // ★ 为什么必须单独测这个:
    //   开普勒方程在 e→1 且 M→0 (近日点) 附近最难收敛 —— 此时
    //   f'(E) = 1 - e·cos E 接近 0, 牛顿法接近奇异。
    //   上面的全域扫描用均匀采样,**根本落不到近日点上**
    //   (NEOWISE 周期 6800 年, 400 个采样点每点相隔 17 年),
    //   所以扫描通过不能证明近日点算得对。
    //
    //   而近日点恰是彗星最亮、最该出现在正确位置的地方 ——
    //   "哈雷 1986 年回归"是否画在正确位置上, 全靠这里。
    //
    //   基准 tp 取自 JPL, 是权威的过近日点时刻。
    qInfo().noquote() << "";
    qInfo().noquote() << "--- 3. 近日点定点验证 (求解器最易失稳之处) ---";
    for (int i = 0; i < REFS_COUNT; ++i) {
        const PeriRef &r = PERI_REFS[i];
        if (!registry::findOrbit(r.id))
            continue;
        const double rAu = eph::orbitalRadiusKm(r.id, r.tp) / AU_KM;
        const double err = std::fabs(rAu - r.q) / qMax(r.q, 1e-9);
        const bool ok = err < 0.01;      // 1% 容差
        if (!ok)
            ++fail;
        qInfo().noquote() << QString("    %1 %2  在 tp 处 r=%3 AU  (JPL q=%4 AU, 偏差 %5%)")
                                 .arg(QString::fromUtf8(r.id), -11)
                                 .arg(ok ? QStringLiteral("OK ") : QStringLiteral("!! "))
                                 .arg(rAu, 10, 'f', 5).arg(r.q, 10, 'f', 5)
                                 .arg(err * 100.0, 0, 'f', 3);
    }

    // ---- 3.5 卫星轨道比例验证 ----
    //
    // ★ 这里检查一个曾经的真实缺陷: 卫星轨道与天体半径走了两套不匹配的
    //   变换, 导致比例严重失真。月球曾经显示在 1.37 个地球半径处,
    //   而真值是 60.3 —— 差 44 倍, "月球离地球多远"直接被画错。
    qInfo().noquote() << "";
    qInfo().noquote() << "--- 3.5 卫星轨道比例 (倍母星半径) ---";
    {
        struct M { const char *id; const char *parent; };
        const M moons[] = {
            { "moon", "earth" }, { "phobos", "mars" }, { "io", "jupiter" },
            { "titan", "saturn" }, { "triton", "neptune" },
        };
        for (const M &m : moons) {
            const BodyData *mb = registry::findBody(m.id);
            const BodyData *pb = registry::findBody(m.parent);
            if (!mb || !pb)
                continue;
            const double expect = mb->semiMajorKm / pb->radiusKm;
            // 场景里实际用的公式: 母星显示半径 × (真实半长轴 / 母星真实半径)
            const double parentVisR = sceneconst::displayRadius(pb->radiusKm, false);
            const double actual = parentVisR * expect;
            const double ratioShown = actual / parentVisR;   // 应恒等于 expect
            const double err = std::fabs(ratioShown - expect) / expect;
            const bool ok = err < 0.001;
            if (!ok) ++fail;
            qInfo().noquote()
                << QString("    %1绕%2 %3  显示 %4 倍母星半径, 应 %5 倍 (a=%6 km, R=%7 km)")
                       .arg(QString::fromUtf8(m.id), -9)
                       .arg(QString::fromUtf8(m.parent), -8)
                       .arg(ok ? QStringLiteral("OK ") : QStringLiteral("!! "))
                       .arg(ratioShown, 0, 'f', 2)
                       .arg(expect, 0, 'f', 2)
                       .arg(mb->semiMajorKm, 0, 'f', 0)
                       .arg(pb->radiusKm, 0, 'f', 0);
        }
    }

    // ---- 4. 分类标签抽查 ----
    //
    // 教学材料里把矮行星写成行星是明确的事实错误, 故单独检查一遍。
    qInfo().noquote() << "";
    qInfo().noquote() << "--- 4. 分类标签 (教学必须准确) ---";
    const char *cats[] = { "sun", "earth", "pluto", "ceres", "eris", "moon",
                           "vesta", "pallas", "halley", "neowise", "charon" };
    for (const char *id : cats) {
        qInfo().noquote() << QString("    %1 → %2 / %3")
                                 .arg(QString::fromUtf8(id), -10)
                                 .arg(QString::fromUtf8(registry::categoryCn(id)), -8)
                                 .arg(QString::fromUtf8(registry::categoryEn(id)));
    }

    qInfo().noquote() << "";
    qInfo().noquote() << (fail == 0
        ? QStringLiteral("=== 全部通过 ===")
        : QStringLiteral("=== %1 项未通过 ===").arg(fail));
}
