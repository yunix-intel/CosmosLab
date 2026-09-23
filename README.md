# 宇宙实验室 CosmosLab (C++ / QML / OpenGL)

从太阳系到可观测宇宙的三尺度教学可视化，外加恒星全链、哈勃图实测、13 条演化播放器。

## 下载（推荐：开箱即用）

- **直接用**：[Release v1.0](https://github.com/yunix-intel/CosmosLab/releases/tag/v1.0) 下载 `CosmosLab-v1.0-win64.zip`，解压双击 `cosmoslab.exe`（含全部资源与 156 条配音）。
- **从源码构建**：`git clone https://github.com/yunix-intel/CosmosLab.git`，再按需下载下方资源包，**在仓库根目录解压**（包内路径已是 `assets/...`，解压即对齐）：

| 资源包 | 内容 | 说明 |
|---|---|---|
| `CosmosLab-v1.0-assets-textures.zip` | 贴图（tex/galaxy，58MB） | 缺失时对应天体显示占位提示，不崩 |
| `CosmosLab-v1.0-assets-audio.zip` | 配音 156 条（9.6MB） | 缺失时播放按钮置灰，不崩 |
| `CosmosLab-v1.0-assets-data.zip` | lss/sn/evo 数据（2.4MB） | `assets/lss` 已在仓内，覆盖无妨 |

## 运行

```bash
# 构建 (MinGW + Qt6, 见记忆 2026-09-14)
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
./build/cosmoslab.exe
```

## 界面一览

顶栏：`演化 | 恒星 | 太阳系 | 银河系 | 宇宙` —— 前两者为 UI 视图（不切换 3D 渲染尺度），
后三者切换渲染尺度。左/右面板随视图切换。

| 视图 | 内容 |
|---|---|
| 太阳系 | 48 天体，开普勒轨道，真实比例开关，彗尾/小行星带 |
| 银河系 | Reid 2019 四臂 + 折点，13.5 万粒子，太阳位置标注 |
| 宇宙 | 24 万粒子（SDSS 实测 17 万），对数/真实比例映射开关 |
| 恒星 | B.1/B.2/B.5 共 109 条：恒星全链 53 + AGN/星系 34 + 星云/星团 22，精准/通俗双说明 |
| 演化 | 13 条剧本：恒星三链/超新星/并合/AGN/宇宙热历史/行星形成/恒星形成/双星/残骸/星团/星际介质与星团；拖动 + 0.5–8x 播放 |

叠加层：哈勃图（Pantheon+ 1701 颗，Ωm/ΩΛ 滑块 + χ²）、红移工具、详情卡（精准/通俗切换）。

## 数据来源（全部可溯源）

- 行星轨道：JPL 近似根数；小天体：JPL SBDB；卫星：JPL 卫星平均根数
- 银河系旋臂：Reid et al. 2019 (BeSSeL/VLBA 脉泽视差)
- 星系距离/红移：NED；超团：Tully et al. 2014；宇宙学参数：Planck 2018
- 超新星：Pantheon+ 2022；恒星参数：SIMBAD 常用值；AGN：EHT/原始论文常用值
- 照片：ESO/NASA 公开图库（见 `assets/galaxy/SOURCES.txt`）；无图不冒充，明确提示原因

## 自检

```bash
SS_SELFTEST=out.png SS_CARD=vega ./build/cosmoslab.exe     # 详情卡
SS_SELFTEST=out.png SS_EVO=midmass:0.55 ...                   # 演化剧本
SS_SELFTEST=out.png SS_STELLAR=1 ...                          # 恒星面板
SS_SELFTEST=out.png SS_HUBBLE=1 ...                           # 哈勃图
```

环境变量：`SS_SCALE` (0/1/2)、`SS_MAPMODE`、`SS_SDSS`、`SS_MARKER`、`SS_FOCUS`、`SS_DIST`、`SS_RES`。

## 文档

- `outputs/天体分类大纲-v1.md` —— 八编分类体系（对标 C&O/Ryden/Karttunen）
- `outputs/天体代表清单-v1.md` —— 具名代表清单
- `outputs/教学指引.md` —— 11 张图组织的三段式教学路径
- `assets/evo/narration.json` —— 78 个解说词 ID × 五键（zh 专业 / zh_pop 科普 / yue / ja / en）
