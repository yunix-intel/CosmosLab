# CosmosLab 全面测试矩阵 (T01-T24)
# 用法: python3 tools/fulltest.py [--exe PATH] [--out DIR]
# 每项: 跑 cosmoslab.exe 无头自检, 断言: 退出码0 + 出图非空 + 日志无QML错误
import argparse, os, re, subprocess, sys, time

EXE_DEFAULT = "D:/tmp/solar-system-cpp/build/cosmoslab.exe"
QML_ERR = re.compile(r"ReferenceError|TypeError|Unable to assign|module .* is not installed|QML.*Error", re.I)

CASES = [
    # --- A. 三尺度渲染 ---
    ("T01", "太阳系默认", {}, "t01_solar.png"),
    ("T02", "银河系", {"SS_SCALE": "1"}, "t02_galaxy.png"),
    ("T03", "宇宙对数", {"SS_SCALE": "2"}, "t03_cosmos.png"),
    ("T04", "宇宙真实比例", {"SS_SCALE": "2", "SS_MAPMODE": "1"}, "t04_truescale.png"),
    ("T05", "宇宙无SDSS", {"SS_SCALE": "2", "SS_SDSS": "0"}, "t05_nosdss.png"),
    # --- B. 详情卡: 恒星/AGN/ISM 各抽 ---
    ("T06", "卡-织女星", {"SS_CARD": "vega"}, "t06_vega.png"),
    ("T07", "卡-黑洞", {"SS_CARD": "cyg_x1"}, "t07_cygx1.png"),
    ("T08", "卡-系外行星", {"SS_CARD": "peg51b"}, "t08_peg51b.png"),
    ("T09", "卡-类星体", {"SS_CARD": "qso273"}, "t09_3c273.png"),
    ("T10", "卡-发射星云", {"SS_CARD": "m42"}, "t10_m42.png"),
    ("T11", "卡-球状星团", {"SS_CARD": "m13"}, "t11_m13.png"),
    ("T12", "卡-本地泡", {"SS_CARD": "localbubble"}, "t12_localbubble.png"),
    # --- C. 13 演化剧本全覆盖 ---
    ("T13", "演化-类太阳", {"SS_EVO": "midmass:0.55"}, "t13_midmass.png"),
    ("T14", "演化-大质量", {"SS_EVO": "massive:0.7"}, "t14_massive.png"),
    ("T15", "演化-低质量", {"SS_EVO": "lowmass:0.5"}, "t15_lowmass.png"),
    ("T16", "演化-超新星", {"SS_EVO": "sn:0.5"}, "t16_sn.png"),
    ("T17", "演化-宇宙热历史", {"SS_EVO": "cosmic:0.8"}, "t17_cosmic.png"),
    ("T18", "演化-AGN", {"SS_EVO": "agn:0.7"}, "t18_agn.png"),
    ("T19", "演化-双星并合", {"SS_EVO": "binary:0.9"}, "t19_binary.png"),
    ("T20", "演化-ISM新剧本", {"SS_EVO": "ism:0.5"}, "t20_ism.png"),
    # --- D. 面板与模式 ---
    ("T21", "恒星面板", {"SS_STELLAR": "1"}, "t21_stellar.png"),
    ("T22", "哈勃图", {"SS_HUBBLE": "1"}, "t22_hubble.png"),
    ("T23", "科普版M42", {"SS_CARD": "m42", "SS_POP": "1"}, "t23_pop.png"),
    ("T24", "轨道求解", {"SS_ORBTEST": "1"}, None),
]

def run(exe, env_extra, out, timeout=120):
    env = dict(os.environ)
    env["PATH"] = "C:/msys64/ucrt64/bin;C:/Windows/System32;C:/Windows"
    env.update(env_extra)
    if out:
        env["SS_SELFTEST"] = out
    t0 = time.time()
    r = subprocess.run([exe], capture_output=True, text=True, cwd=os.path.dirname(exe), env=env, timeout=timeout)
    return r, time.time() - t0

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=EXE_DEFAULT)
    ap.add_argument("--out", default="D:/tmp/cosmos_test")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    ok, fail = [], []
    for tid, name, extra, shot in CASES:
        out = os.path.join(a.out, shot) if shot else None
        try:
            r, dt = run(a.exe, extra, out)
            errs = QML_ERR.findall(r.stdout + r.stderr)
            shot_ok = True
            if out:
                shot_ok = os.path.exists(out) and os.path.getsize(out) > 20000
            passed = (r.returncode == 0) and not errs and shot_ok
            detail = f"rc={r.returncode} {dt:.0f}s"
            if out:
                detail += f" shot={'OK' if shot_ok else 'BAD/MISSING'}"
            if errs:
                detail += f" QMLERR={errs[:2]}"
            (ok if passed else fail).append((tid, name, detail))
            print(f"[{'PASS' if passed else 'FAIL'}] {tid} {name}: {detail}", flush=True)
        except subprocess.TimeoutExpired:
            fail.append((tid, name, "TIMEOUT"))
            print(f"[FAIL] {tid} {name}: TIMEOUT", flush=True)
        except Exception as e:
            fail.append((tid, name, f"EXC {e}"))
            print(f"[FAIL] {tid} {name}: EXC {e}", flush=True)
    print(f"\n==== {len(ok)}/{len(CASES)} 通过, {len(fail)} 失败 ====")
    for t, n, d in fail:
        print(f"  FAIL {t} {n}: {d}")
    return 1 if fail else 0

sys.exit(main())
