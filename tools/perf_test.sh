#!/bin/bash
# 宇宙视图压力测试 —— 测"粒子数 vs GPU 真实耗时"
#
# ★ 为什么要测 GPU 耗时而不是帧率:
#   场景由 onTick 的 16ms 定时器驱动, 帧率上限锁在 62.5 FPS。
#   GPU 只要低于 16ms/帧, 帧率就恒为 62.5 —— 数字完全反映不出余量。
#   必须用 glFinish 测真实耗时才能看出瓶颈在哪里。

cd /d/tmp/solar-system-cpp || exit 1

echo "========================================================================"
echo "  宇宙视图压力测试"
echo "  机器: $(nproc) 核   渲染分辨率: 1440x900 (窗口 DPR=1)"
echo "========================================================================"
printf "%-12s %-12s %-14s %s\n" "倍数" "粒子数" "GPU 平均(ms)" "纯渲染上限(FPS)"
echo "------------------------------------------------------------------------"

for M in 1 5 10 20 50 100; do
  LOG="/d/tmp/perf_${M}.log"
  SS_SCALE=2 SS_PERF=1 SS_BENCH=2200 SS_COSMOS_MULT=$M \
    ./build/solar_system.exe > "$LOG" 2>&1

  # 取最后一条 [性能] 输出 (已稳定)
  LINE=$(tr -d '\0' < "$LOG" | grep -a '\[性能\]' | tail -1)
  N=$(tr -d '\0' < "$LOG" | grep -a '压力测试' | grep -aoE '[0-9]+$' | tail -1)
  [ -z "$N" ] && N=$(tr -d '\0' < "$LOG" | grep -aoE '星系点: [0-9]+' | grep -aoE '[0-9]+' | tail -1)

  MS=$(echo "$LINE" | grep -aoE '平均 [0-9.]+ ms' | grep -aoE '[0-9.]+')
  FPS=$(echo "$LINE" | grep -aoE '上限 [0-9.]+ FPS' | grep -aoE '[0-9.]+')

  if [ -z "$MS" ]; then
    printf "%-12s %-12s %-14s %s\n" "x$M" "${N:-?}" "(未取到)" "-"
  else
    printf "%-12s %-12s %-14s %s\n" "x$M" "${N:-?}" "$MS" "$FPS"
  fi
done

echo "------------------------------------------------------------------------"
echo "完成。原始日志: /d/tmp/perf_*.log"
