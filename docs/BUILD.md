# 宇宙实验室 CosmosLab —— 构建指南

> 目标：别人 clone 仓库后能从源码构建出 `cosmoslab.exe`。
> 已验证环境：Windows 11 + MSYS2 UCRT64（cmake 4.3.3 / g++ 16.1.0 / Qt 6.11.1）。

## 1. 前置依赖（MSYS2 UCRT64）

```bat
pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-declarative mingw-w64-ucrt-x86_64-qt6-multimedia mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make
```

说明：
- Qt 模块：`qt6-base`（Core/Gui/OpenGL）+ `qt6-declarative`（Qml/Quick/QuickControls2）+ `qt6-multimedia`（配音播放，可选）。
- `CMakeLists.txt` 用 `qt_add_executable` / `qt_add_qml_module`，需 Qt 6.3+。
- 配音播放走 WinMM `mciSendString`（系统库 `winmm`，MinGW 自带，无新增依赖）。

## 2. 构建

```bat
cd D:\tmp\solar-system-cpp
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

产物：`build/cosmoslab.exe`。开发期直接运行即可（资源走三级回退：`exe旁assets/` → `build/../assets/` → 仓库 `assets/`，见 `src/assetroot.h`）。

## 3. 资源（assets）

- 源码仓内：`assets/evo/narration.json`（78 解说词 ID）+ `assets/galaxy|tex|lss|sn` 贴图与数据。
- 大文件（Release 附件，不在 git 内）：贴图包、配音包（`assets/audio/` 156 条 mp3 + `manifest.json`）、绿色包（`cosmoslab.exe` + DLL + 全量 assets，开箱即用）。
- 配音缺失时软件正常运行：`playNarration` 返回空串，播放按钮置灰（见 `src/sceneitem.cpp`）。

## 4. 自检

```bat
SS_SELFTEST=out.png SS_CARD=vega ./build/cosmoslab.exe     # 详情卡
SS_SELFTEST=out.png SS_EVO=midmass:0.55 ...                   # 演化剧本
SS_SELFTEST=out.png SS_STELLAR=1 ...                          # 恒星面板
SS_SELFTEST=out.png SS_HUBBLE=1 ...                           # 哈勃图
```

完整 24 项矩阵：`python3 tools/fulltest.py --out <目录>`（要求 PATH 含 ucrt64/bin）。

## 5. 打绿色包（维护者）

```bat
windeployqt --release --qmldir qml build/cosmoslab.exe --dir D:/tmp/solar-app
```

再把 `assets/` 全量拷入 `solar-app/assets/`。注意 `windeployqt` 会漏 MinGW 运行时与 ICU（`libstdc++-6.dll` 等），需从 `C:/msys64/ucrt64/bin` 手工补齐（见记忆 2026-09-19 绿色包记录）。
