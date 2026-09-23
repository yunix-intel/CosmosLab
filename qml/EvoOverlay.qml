// ============================================================================
//  EvoOverlay.qml —— 通用演化时间轴播放引擎 + 全演化剧本
//
//  架构 (三层, 新增演化只需加剧本数据):
//    1. 播放引擎 (本文件上半): 拖动 / 播放暂停 / 倍速 / 线性对数切换 / 阶段跳转
//    2. 演化剧本 (scripts 属性): 每条含阶段划分、物理时间、说明、解说词 ID
//    3. 视觉映射 (drawViz): 按 viz 类型把物理时间映射到 Canvas 示意
//
//  ★ 解说词只留接口: 每阶段带 narrId (如 "evo.mid.s3"), 文本四语种以后填。
//    见 assets/evo/narration_stub.json (ID 清单, 内容待写)。
//
//  ★ Canvas 高 DPI: 只设 canvasSize, 不手动 ctx.scale (dpr 陷阱, 见记忆)。
//  ★ 方法名小写开头 (QML 不允许大写开头的方法名)。
// ============================================================================

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: evo
    anchors.fill: parent
    visible: false
    z: 190
    color: Qt.rgba(0.02, 0.03, 0.05, 0.90)

    // 由 Main.qml 传入 (避免跨文件 id 引用)
    property real dpr: 2.0
    property string testEvo: ""
    // ★ 全局专业/科普开关 (由 Main.qml 的 root.proMode 传入):
    //   true=专业版 desc, false=科普版 pop (无 pop 的阶段回退到 desc)。
    property bool proMode: true
    // ★ 配音调用的 scene 对象 (由 Main.qml 传入 scene 本体, 同 testEvo 模式)。
    property var sceneObj: null

    // ---- 引擎状态 ----
    property int cur: 0
    property real prog: 0.0          // 0..1, 时间轴位置
    property bool playing: false
    property real speed: 1.0
    property bool logMode: true      // 仅当剧本 allowLog 时有效
    property real baseDur: 45.0      // 1x 全程秒数

    // ---- 配色 (自包含, 与 Main.qml 主题一致) ----
    readonly property color tx: "#e8eef9"
    readonly property color txDim: "#8fa3bf"
    readonly property color accent: "#5ea9ff"
    readonly property color good: "#7ee28a"
    readonly property color warn: "#e8cc7a"
    readonly property color bad: "#e89090"

    // ========================================================================
    //  演化剧本 (全部 12 条; 数值取常用量级, 来源见 desc)
    // ========================================================================
    property var scripts: [
        {
            id: "lowmass", title: "低质量恒星演化", sub: "0.3 M☉ 红矮星 · 主序极长",
            unit: "Gyr", tMin: 0, tMax: 250, allowLog: false, useLog: false,
            viz: "hr",
            stages: [
                { t: 0,   label: "零龄主序", desc: "氢聚变点火, 进入主序。0.3 M☉ 全对流, 燃料利用充分。", narr: "evo.low.s0", pop: "一颗小红星点着了, 开始烧氢。它个头小, 烧得慢, 特别省燃料。",
                  track: { teff: 3400, logL: -2.0, rad: 0.30 } },
                { t: 100, label: "主序中期", desc: "仍在平稳烧氢。红矮星主序寿命可达数千亿年 —— 宇宙年龄内走不完。", narr: "evo.low.s1", pop: "一千亿年过去, 它还在不紧不慢地烧。宇宙的年纪都不够它过完一生。",
                  track: { teff: 3350, logL: -2.1, rad: 0.31 } },
                { t: 200, label: "主序晚期", desc: "氢接近耗尽, 缓慢收缩增温。注意: 这是理论外推, 尚未实测到此类终点。", narr: "evo.low.s2", pop: "两千亿岁, 燃料快见底, 它慢慢缩成一团。注意: 这是算出来的, 没人亲眼见过结局。",
                  track: { teff: 3300, logL: -2.2, rad: 0.33 } },
                { t: 250, label: "He 白矮星 (理论终点)", desc: "直接塌缩为 He 白矮星 —— 单星通道在宇宙年龄内到不了, 系理论预言。", narr: "evo.low.s3", pop: "最后缩成一颗氦白矮星 —— 但这只是纸上预言, 宇宙还没老到能见证它。",
                  track: { teff: 6000, logL: -3.0, rad: 0.02 } }
            ]
        },
        {
            id: "midmass", title: "类太阳恒星一生", sub: "1 M☉ · 主序约 100 亿年",
            unit: "Gyr", tMin: 0, tMax: 15, allowLog: false, useLog: false,
            viz: "hr",
            stages: [
                { t: 0,    label: "零龄主序", desc: "氢聚变点火。有效温度约 5778 K, 光度 1 L☉。", narr: "evo.mid.s0", pop: "一颗和太阳一样的恒星出生了, 一切从这里开始。",
                  track: { teff: 5778, logL: 0.0, rad: 1.0 } },
                { t: 4.6,  label: "太阳现在", desc: "46 亿岁。核心氢已消耗约一半, 光度比初生时高约 30%。", narr: "evo.mid.s1", pop: "46亿岁, 这就是太阳的今天: 人到中年, 烧掉了一半燃料, 比年轻时亮了三成。",
                  track: { teff: 5778, logL: 0.0, rad: 1.0 } },
                { t: 10,   label: "红巨星支", desc: "核心氢耗尽, 氢壳层燃烧, 包层膨胀。半径可达上百 R☉。", narr: "evo.mid.s2", pop: "100亿岁, 燃料烧空, 外壳猛涨, 变成红巨星, 一口能吞掉好几个地球轨道。",
                  track: { teff: 4000, logL: 2.0, rad: 30.0 } },
                { t: 11,   label: "氦闪 · 水平支", desc: "简并 He 核点火 (氦闪) 后进入平稳氦燃烧, 落在水平支/红团簇。", narr: "evo.mid.s3", pop: "核心的氦被点着, 闪了一下, 然后安稳下来, 在赫罗图上找了个新位置。",
                  track: { teff: 5000, logL: 1.7, rad: 10.0 } },
                { t: 11.2, label: "渐近巨星支", desc: "He/H 双壳层燃烧 + 热脉冲, 强星风抛射包层 (超风可达 1e-4 M☉/yr 量级)。", narr: "evo.mid.s4", pop: "晚年大喘气, 一阵阵往外抛衣服, 风大到一年能吹掉十万分之一太阳。",
                  track: { teff: 3000, logL: 3.7, rad: 300.0 } },
                { t: 11.25, label: "行星状星云", desc: "包层被剥离、电离发光, 持续约 1 万年 —— 宇宙尺度上一瞬。", narr: "evo.mid.s5", pop: "抛掉的外衣被照亮, 成了行星状星云 —— 只亮一万年, 转瞬即逝。",
                  track: { teff: 100000, logL: 3.0, rad: 0.1 } },
                { t: 15,   label: "C-O 白矮星冷却", desc: "剩余约 0.6 M☉ 碳氧核, 无聚变, 靠余热冷却。冷却序列可定年龄。", narr: "evo.mid.s6", pop: "最后剩个碳氧内核, 慢慢冷却, 就是白矮星。看它多暗, 就能猜它几岁。",
                  track: { teff: 8000, logL: -2.5, rad: 0.01 } }
            ]
        },
        {
            id: "massive", title: "大质量恒星一生", sub: "20 M☉ · 全寿命约 1000 万年",
            unit: "Myr", tMin: 0, tMax: 12, allowLog: false, useLog: false,
            viz: "hr",
            stages: [
                { t: 0,   label: "零龄主序 (O 型)", desc: "O7V, 有效温度约 35000 K, 光度约 1e5 L☉。CNO 循环主导。", narr: "evo.mas.s0", pop: "一颗20倍太阳质量的巨星登场, 又亮又烫, 但活不长。",
                  track: { teff: 35000, logL: 5.0, rad: 8.0 } },
                { t: 8,   label: "主序末", desc: "核心氢耗尽。仅 800 万年 —— 大质量星活得短而亮。", narr: "evo.mas.s1", pop: "才800万年, 燃料烧光。太阳要100亿年走的路, 它几百万年就跑完了。",
                  track: { teff: 30000, logL: 5.2, rad: 12.0 } },
                { t: 8.8, label: "超巨星 (红/蓝)", desc: "外层膨胀为红超巨星 (或蓝超巨, 依质量损失与金属丰度)。Ne–O–Si 逐级燃烧, 每级更快。", narr: "evo.mas.s2", pop: "外壳吹成大气球, 变成超巨星。肚子里烧完一层换一层, 越烧越快。",
                  track: { teff: 3600, logL: 5.5, rad: 800.0 } },
                { t: 9.3, label: "沃尔夫–拉叶阶段", desc: "强星风剥掉氢包层, 露出 He/C/O 核心 (WC/WN)。质量损失率可达 1e-5–1e-4 M☉/yr。", narr: "evo.mas.s3", pop: "大风把外衣剥光, 露出滚烫的内核 —— 沃尔夫-拉叶阶段, 宇宙里最猛的风。",
                  track: { teff: 60000, logL: 5.3, rad: 5.0 } },
                { t: 9.5, label: "核坍缩 · 超新星", desc: "铁核坍缩, Ⅱb/Ⅰb 型 (包层已被剥离则无氢线)。中微子带走约 99% 能量。", narr: "evo.mas.s4", pop: "铁核塌了, 大爆炸。因为外衣早没了, 光谱里看不到氢 —— 这就是一b型。",
                  track: { teff: 20000, logL: 8.0, rad: 50.0 } },
                { t: 12,  label: "黑洞遗迹", desc: "20 M☉ 太阳金属丰度下大概率成黑洞 (中子星/黑洞分界约 20–25 M☉, 只给区间)。", narr: "evo.mas.s5", pop: "最后剩个黑洞。几倍太阳质量以上就压不住了, 只能给个区间, 给不出精确数。",
                  track: { teff: 0, logL: -6.0, rad: 0.0 } }
            ]
        },
        {
            id: "sn", title: "超新星爆发", sub: "Ⅰa 型 (SN 2011fe 式) · 光变 + 膨胀",
            unit: "day", tMin: 0, tMax: 300, allowLog: false, useLog: false,
            viz: "sn",
            stages: [
                { t: 0,   label: "爆发 · 早期增亮", desc: "热核失控/激波加热, 光度急升。Ⅰa 无氢线是分类判据。", narr: "evo.sn.s0", pop: "炸了!亮度猛涨。光谱里没有氢 —— 这是一a型的身份证。" },
                { t: 19,  label: "B 波段极大", desc: "约 19 天达峰, M ≈ −19.3。这是标准烛光定标点 (Phillips 关系: 宽–亮相关)。", narr: "evo.sn.s1", pop: "约19天亮到顶, 这是最亮的时刻, 也是量距离的标尺。" },
                { t: 30,  label: "近红外次极大", desc: "正常 Ⅰa 在红外有二次隆起 (B 极大后约 15–20 天), 源于 Fe 族元素电离态变化。", narr: "evo.sn.s2", pop: "约30天, 红外又鼓个小包 —— 铁元素变脸留下的指纹, 正常一a才有。" },
                { t: 60,  label: "过渡段", desc: "光球退入 Fe 核, 颜色转红。", narr: "evo.sn.s3", pop: "60天, 开始走下坡路, 颜色发红, 亮度指数下降。" },
                { t: 150, label: "钴衰变尾", desc: "⁵⁶Co → ⁵⁶Fe (半衰期 77.2 d) 供能, 光变按指数衰减。", narr: "evo.sn.s4", pop: "150天, 靠钴衰变撑着发光 —— 光的尾巴就是元素在变身。" },
                { t: 300, label: "遗迹阶段", desc: "抛射物稀薄成星云相, 禁线主导。光变进入 ⁵⁶Co+⁵⁷Co 混合尾 (比纯 ⁵⁶Co 慢); 数百年后成超新星遗迹 (如 Tycho)。", narr: "evo.sn.s5", pop: "300天后只剩稀薄的烟雾, 几百年后会变成第谷那样的遗迹。" }
            ]
        },
        {
            id: "merger", title: "星系并合", sub: "银河系 + 仙女座未来 · 触须星系为现在进行时",
            unit: "Gyr", tMin: 0, tMax: 8, allowLog: false, useLog: false,
            viz: "merger",
            stages: [
                { t: 0, label: "现在", desc: "相距约 770 kpc, 以约 110 km/s 接近。M31 蓝移 z = −0.001 是直接证据。", narr: "evo.mrg.s0", pop: "现在: 银河系和仙女座隔着77万秒差距靠近, 仙女座正朝我们来, 这就是证据。" },
                { t: 4, label: "第一次近心点", desc: "潮汐尾拉出, 星暴被触发。类比触须星系 (Arp 244, Toomre 序列中期)。", narr: "evo.mrg.s1", pop: "约40亿年后第一次擦肩: 拉出长尾巴, 点燃星暴。触须星系就是现场直播。" },
                { t: 6, label: "并合", desc: "核球合并, 盘结构被打乱, 星暴达峰后淬灭开始。", narr: "evo.mrg.s2", pop: "约60亿年后撞在一起: 旋臂搅碎, 星暴烧到最旺然后熄火。" },
                { t: 8, label: "椭圆遗迹 Milkomeda", desc: "弛豫为大椭圆星系。太阳系届时命运不确定 —— 大概率被甩到外晕。", narr: "evo.mrg.s3", pop: "80亿年后变成一个大椭圆星系。太阳大概率被甩到郊区, 但自己没事。" }
            ]
        },
        {
            id: "agn", title: "AGN 喷流生命周期", sub: "射电星系尺度 · M87 / 天鹅座 A 类",
            unit: "Myr", tMin: 0, tMax: 120, allowLog: false, useLog: false,
            viz: "agn",
            stages: [
                { t: 0,   label: "宁静椭圆星系", desc: "中心超大质量黑洞沉睡, 无喷流。M–σ 关系已就位。", narr: "evo.agn.s0", pop: "一座安静的椭圆星系, 中心黑洞在睡觉, 没有喷流。" },
                { t: 8,   label: "并合供气触发", desc: "气体流入核区 (~pc 尺度), 吸积盘点亮, 宽线区出现。", narr: "evo.agn.s1", pop: "撞车送来气体, 黑洞开饭, 盘子点亮 —— 活动星系核开机了。" },
                { t: 12,  label: "喷流开启", desc: "相对论性喷流打通 kpc 尺度 (M87 喷流约 5 kpc)。视角决定分类: 正对即耀变体。", narr: "evo.agn.s2", pop: "喷流打穿几千秒差距, 和梅西耶八七的差不多。正对着看, 它就是耀变体 —— 角度决定身份。" },
                { t: 40,  label: "巨瓣 + 反馈", desc: "瓣达百 kpc 级 (天鹅座 A 级约 120 kpc), 激波加热星系周气体 —— AGN 反馈淬灭恒星形成 (候选机制)。", narr: "evo.agn.s3", pop: "瓣长到几十万光年, 把周围气体加热, 星系可能就此停造星。注意: 只是候选解释。" },
                { t: 120, label: "遗迹瓣", desc: "燃料耗尽, 喷流熄灭, 瓣辐射老化变陡。", narr: "evo.agn.s4", pop: "饭吃完, 喷流熄火, 只剩老瓣在射电波段慢慢变暗, 星系重归安静。" }
            ]
        },
        {
            id: "cosmic", title: "宇宙热历史", sub: "大爆炸至今 138 亿年 · 对数/线性可切换",
            unit: "s", tMin: 0.1, tMax: 4.35e17, allowLog: true, useLog: true,
            viz: "cosmic",
            stages: [
                { t: 1,      label: "太初核合成", desc: "1 秒–3 分钟: ⁴He 质量分数约 25%, D/H 定重子密度 —— 与 CMB 独立一致。", narr: "evo.cos.s0", pop: "大爆炸后1秒到3分钟: 造出第一批氦, 分量约25% —— 大爆炸最硬的预言, 和微波背景对得上。", temp: "1e10 K" },
                { t: 1.6e12,  label: "物质–辐射相等", desc: "约 5 万年: 物质密度超过辐射, 扰动开始增长 (结构形成的起点)。", narr: "evo.cos.s1", pop: "约5万年, 物质超过辐射, 引力说了算, 小疙瘩开始长大 —— 结构形成的起点。", temp: "9000 K" },
                { t: 1.2e13,  label: "复合 · CMB 退耦", desc: "38 万年, z ≈ 1090, T ≈ 3000 K → 今天 2.7255 K。我们能看到的最古老的光。", narr: "evo.cos.s2", pop: "38万年, 电子和质子牵手, 光终于能跑了 —— 这就是今天2.7度的微波背景, 我们能看到的最老的光。", temp: "3000 K" },
                { t: 6.0e15,  label: "第一代恒星", desc: "约 2 亿年: Pop Ⅲ 点亮 (理论预言为主)。黑暗时代结束。", narr: "evo.cos.s3", pop: "约2亿年, 第一代恒星点亮, 黑暗时代结束。主要是理论预言, 还没实锤。", temp: "60 K" },
                { t: 2.5e16,  label: "再电离", desc: "z ~ 6–10: 星系紫外光子电离星系际氢 (Gunn–Peterson 谷为证)。", narr: "evo.cos.s4", pop: "星系的紫外线把星系之间的氢电离, 类星体光谱里的黑谷就是证据。", temp: "30 K" },
                { t: 2.5e17,  label: "加速膨胀开始", desc: "z ~ 0.6 (约 79 亿年): 暗能量主导。1998 超新星证据; 本项目哈勃图 Δχ² 实测复现。", narr: "evo.cos.s5", pop: "暗能量接管, 膨胀加速。1998年超新星发现的, 本项目的哈勃图也能算出来。", temp: "10 K" },
                { t: 4.35e17, label: "现在", desc: "138 亿年。Planck 2018: H₀ = 67.4, Ωm = 0.315, 平直宇宙。", narr: "evo.cos.s6", pop: "138亿年后的今天。但两个哈勃常数对不上, 差5个标准差 —— 宇宙学最大的麻烦。", temp: "2.7255 K" }
            ]
        },
        {
            id: "planet", title: "行星系统形成", sub: "类太阳系 · 盘 → 迁移 → 碎屑",
            unit: "Myr", tMin: 0, tMax: 12, allowLog: false, useLog: false,
            viz: "planet",
            stages: [
                { t: 0,  label: "Ⅱ类盘", desc: "气体+尘埃盘, 质量约 1% 恒星。ALMA 已成像大量环缝结构。", narr: "evo.pln.s0", pop: "气体尘埃盘, 质量只有恒星的百分之一。毫米波望远镜拍到的环缝, 就是行星在刻痕。" },
                { t: 1,  label: "尘埃生长 · 星子", desc: "微米尘埃聚成 km 级星子 (卵石吸积/引力不稳定, 机制仍在竞争)。", narr: "evo.pln.s1", pop: "尘埃抱团长大成星子。怎么长的还有争议, 没定论。" },
                { t: 3,  label: "寡头生长 · 巨行星核", desc: "约 10 M⊕ 固核触发 runaway 气体吸积。", narr: "evo.pln.s2", pop: "长到10倍地球质量, 开始疯狂吸气, 气态巨行星的核就位了。" },
                { t: 5,  label: "气体耗散 · 迁移", desc: "盘气体散去; Ⅰ/Ⅱ型迁移可造热木星 (飞马座 51b) 或大距 (HR 8799)。", narr: "evo.pln.s3", pop: "约500万年气体散光。有的留在外侧, 有的搬进内侧成热木星 —— 同一起点, 两种人生。" },
                { t: 8,  label: "类地行星收尾", desc: "巨撞击阶段 (月球形成式)。", narr: "evo.pln.s4", pop: "类地行星收尾靠大撞击, 月球就是这么撞出来的。" },
                { t: 12, label: "碎屑盘", desc: "残余小天体碰撞级联, 红外超明显 (织女型)。", narr: "evo.pln.s5", pop: "剩下的碎渣互相碰撞, 红外线下很亮。柯伊伯带就是老年的碎屑盘。" }
            ]
        },
        {
            id: "protostar", title: "恒星形成", sub: "低质量单星 · 包层耗散序列",
            unit: "Myr", tMin: 0.01, tMax: 3, allowLog: true, useLog: false,
            viz: "protostar",
            stages: [
                { t: 0.01, label: "前恒星核", desc: "分子云核, Jeans 判据量级成立才坍缩 (只给量级, 不给单值)。", narr: "evo.psf.s0", pop: "分子云核, 够重才塌。只讲量级, 不给精确数 —— 形状和磁场都会影响。" },
                { t: 0.1,  label: "0 类", desc: "深埋包层, L_bol 低、T_bol < 70 K。双极外向流最强。", narr: "evo.psf.s1", pop: "0类: 包在厚茧里, 又暗又冷, 两头喷流最猛 —— 婴儿在打喷嚏。" },
                { t: 0.3,  label: "Ⅰ 类", desc: "包层+盘并存, T_bol 70–650 K。", narr: "evo.psf.s2", pop: "I类: 茧还在, 盘露头了, 过渡形态。" },
                { t: 0.8,  label: "Ⅱ 类 (经典金牛T)", desc: "盘主导, Hα 强发射 + 紫外超。", narr: "evo.psf.s3", pop: "二类: 茧散了, 盘当家, 还在吃料, 发射线很强。" },
                { t: 3,    label: "Ⅲ 类 (弱线)", desc: "盘散去, 恒星表面主导。", narr: "evo.psf.s4", pop: "三类: 盘也没了, 露出恒星表面, 恒星形成毕业, 接下来慢慢收缩。" }
            ]
        },
        {
            id: "binary", title: "双星旋进并合", sub: "双中子星 (GW170817 式) · 距并合时间",
            unit: "logyr", tMin: -7, tMax: 9, allowLog: false, useLog: false,
            viz: "binary",
            stages: [
                { t: 9,  label: "宽双星", desc: "两颗 O/B 主序星, 周期年量级。", narr: "evo.bin.s0", pop: "一对大质量双胞胎, 周期按年算, 故事开场。" },
                { t: 6,  label: "包层共有", desc: "第一次超新星后 + 包层共有抛射, 轨道大幅收缩 (效率 αλ 高度不确定, 只给定性)。", narr: "evo.bin.s1", pop: "一颗炸了, 另一颗膨胀把伴星吞了, 轨道在共用包层里猛缩。具体缩多少, 算不准, 只讲定性。" },
                { t: 1,  label: "双中子星", desc: "周期小时量级, 引力波缓慢带走角动量 (Hulse–Taylor 式衰减)。", narr: "evo.bin.s2", pop: "剩双中子星, 周期按小时算, 引力波慢慢偷走能量, 越转越快。" },
                { t: -5.5, label: "啁啾段 (最后约 100 秒)", desc: "频率扫过 LIGO 带 (数十–数百 Hz)。GW170817 啁啾约 100 秒。", narr: "evo.bin.s3", pop: "最后100秒啁啾: 频率从几十扫到几百赫兹, 那次并合响了约100秒 —— 人类听到的宇宙声音。" },
                { t: -7, label: "并合 · 千新星", desc: "AT2017gfo: r 过程元素 (金、铂) 起源直接证据 + 短 GRB 170817A。", narr: "evo.bin.s4", pop: "撞上了!炸出金银铂, 还附赠短伽马暴 —— 多信使天文学开张。" }
            ]
        },
        {
            id: "remnant", title: "残骸冷却 · 自转减慢", sub: "白矮星冷却 + 脉冲星 P–Pdot",
            unit: "logyr", tMin: 3, tMax: 10, allowLog: false, useLog: false,
            viz: "remnant",
            stages: [
                { t: 3,  label: "年轻残骸 (千年)", desc: "蟹状星云脉冲星 P = 33 ms; 白矮星刚诞生 L ~ 1e-2 L☉。", narr: "evo.rmn.s0", pop: "千年尺度: 蟹状星云那颗每秒转30圈; 新生的白矮星只有百分之一太阳亮。" },
                { t: 6,  label: "百万年", desc: "脉冲星周期显著增长 (磁偶极辐射制动, n = 3 理想情形); 白矮星冷却序列可定年龄。", narr: "evo.rmn.s1", pop: "百万年: 脉冲星越转越慢; 白矮星越凉越暗, 暗到什么程度能反推年龄。" },
                { t: 8,  label: "亿年", desc: "普通脉冲星多已越过死亡线; 白矮星 L ~ 1e-4 L☉。", narr: "evo.rmn.s2", pop: "亿年: 多数脉冲星熄火; 白矮星只剩万分之一太阳亮度。" },
                { t: 10, label: "百亿年", desc: "黑矮星理论存在、宇宙年龄内到不了 (与低质量链呼应)。毫秒脉冲星可稳定到此时钟精度。", narr: "evo.rmn.s3", pop: "百亿年: 黑矮星理论上有, 但宇宙还没老到那份上。毫秒脉冲星倒是稳如钟。" }
            ]
        },
        {
            id: "cluster", title: "星团演化", sub: "典型疏散团 1000 M☉ · 蒸发与核坍缩",
            unit: "Myr", tMin: 1, tMax: 10000, allowLog: true, useLog: true,
            viz: "cluster",
            stages: [
                { t: 1,     label: "嵌入团", desc: "仍埋在分子云中, 气体占主导 (如 Trapezium)。", narr: "evo.clu.s0", pop: "还埋在云里的婴儿星团, 气体说了算, 和猎户座梯形一样。" },
                { t: 3,     label: "气体排出", desc: "大质量星反馈吹散气体; 若效率低则团瓦解 (婴儿死亡率)。", narr: "evo.clu.s1", pop: "约300万年吹散气体, 吹不干净就散伙 —— 多数星团死在这一关。" },
                { t: 100,   label: "弛豫 · 蒸发", desc: "两体弛豫 + 潮汐剥离, 低质量星优先逃逸 (质量分层)。昴星团 (~1 亿年) 在此段。", narr: "evo.clu.s2", pop: "约1亿年, 小星先跑路, 团越蒸发越小。昴星团正在这里。" },
                { t: 1000,  label: "核坍缩或瓦解", desc: "毕星团 (~7 亿年) 正在瓦解; 致密球状团可核坍缩 (M15 式)。", narr: "evo.clu.s3", pop: "约10亿年, 要么散架(毕星团正在散), 要么缩成一团。" },
                { t: 10000, label: "球状团暮年", desc: "幸存者如 M13 (~120 亿年): 年龄 = 宇宙年龄下限 (自洽性检查)。", narr: "evo.clu.s4", pop: "100亿年, 剩下的老年球状团, 它们的年龄就是宇宙年龄的下限。" }
            ]
        },
        {
            // ★ B.5 ISM/星团认知链: 非物理时间, t 为讲解序号 (0-10)。
            //   发射→反射→暗→行星状→遗迹→分子云/泡→疏散→球状→星协→三裂综合→巴纳德环。
            id: "ism", title: "星际介质与星团", sub: "认三种光 · 从苗圃到化石",
            unit: "step", tMin: 0, tMax: 10, allowLog: false, useLog: false,
            viz: "ism",
            stages: [
                { t: 0,  label: "发射星云", desc: "H II 区: 大质量星紫外电离 (M42, 梯形星团为电灯)。", narr: "evo.ism.s0", pop: "发射星云是氢被点亮: 猎户座大星云最近, 梯形星团就是电灯。" },
                { t: 1,  label: "反射星云", desc: "散射借光 (M45 偶遇尘埃, 非诞生地; 蓝光散射更强)。", narr: "evo.ism.s1", pop: "昴星团蓝汪汪不是自己发光, 是照亮了路过的尘埃。记住, 不是诞生地。" },
                { t: 2,  label: "暗星云", desc: "致密尘埃剪影 (马头 B33; 煤袋最冷, 无大质量形成)。", narr: "evo.ism.s2", pop: "马头挡住亮星云形成剪影, 煤袋最冷。暗不是空, 是尘埃太密。" },
                { t: 3,  label: "行星状星云", desc: "AGB 抛射 + 中心星电离 (M57/M27/NGC 7293, 约 1 万年)。", narr: "evo.ism.s3", pop: "指环、哑铃、螺旋都是死星吹的泡泡, 中心白矮星照亮, 只亮一万年。" },
                { t: 4,  label: "超新星遗迹", desc: "Cas A (IIb, 消光无目击) / 面纱 (2 万年壳层); 成分验尸爆发。", narr: "evo.ism.s4", pop: "仙后座A三百年前炸的, 尘埃挡住没人看见; 面纱是两万年前的涟漪。" },
                { t: 5,  label: "分子云与泡", desc: "金牛座云 (低质量形成) / 本地泡 (身处其中) / 银心环反常低效。", narr: "evo.ism.s5", pop: "金牛座分子云专生小星; 我们就住在一个古老爆炸吹出的泡泡里。" },
                { t: 6,  label: "疏散星团", desc: "昴 (1 亿年) / 毕 (7 亿年瓦解中) / 英仙双 (1300 万年); 色星等定年。", narr: "evo.ism.s6", pop: "昴星团一亿岁, 毕星团正在散, 双星团还带着红超巨。" },
                { t: 7,  label: "球状星团", desc: "M13 (~120 亿年, 年龄下限) / ω Cen (多星族) / 47 Tuc (毫秒脉冲星)。", narr: "evo.ism.s7", pop: "M13一百多亿岁, 半人马座欧米茄可能是被吃掉的矮星系核。" },
                { t: 8,  label: "星协", desc: "Sco-Cen (最近 OB 协, 心宿二成员; 成协膨胀定年)。", narr: "evo.ism.s8", pop: "天蝎半人马是刚散伙的帮派, 心宿二就是成员, 本地泡可能是它炸的。" },
                { t: 9,  label: "三裂综合", desc: "M20 一图三光: 发射红 + 反射蓝 + 暗缝 (分类即认光)。", narr: "evo.ism.s9", pop: "三裂星云一张图三种光: 红发光、蓝反光、黑的是尘埃缝。" },
                { t: 10, label: "巴纳德环", desc: "猎户座 10 度超级泡 (古老 SN + 星风); 泡是反馈化石。", narr: "evo.ism.s10", pop: "巴纳德环是古老爆炸的年轮, 猎户座腰间的超级泡。" }
            ]
        }
    ]

    // ---- 时间映射 ----
    function tNow() {
        const s = scripts[cur]
        if (s.useLog && s.allowLog)
            return s.tMin * Math.pow(s.tMax / s.tMin, prog)
        return s.tMin + (s.tMax - s.tMin) * prog
    }
    function progOf(t) {
        const s = scripts[cur]
        if (s.useLog && s.allowLog)
            return Math.log(t / s.tMin) / Math.log(s.tMax / s.tMin)
        return (t - s.tMin) / (s.tMax - s.tMin)
    }
    function stageIndex() {
        const s = scripts[cur]
        const t = tNow()
        let k = 0
        for (let i = 0; i < s.stages.length; ++i)
            if (t >= s.stages[i].t) k = i
        return k
    }

    // ---- 时间格式化 ----
    function fmtT(v) {
        const s = scripts[cur]
        if (s.unit === "s") {
            if (v < 60) return v.toPrecision(2) + " s"
            if (v < 3600) return (v / 60).toPrecision(3) + " min"
            if (v < 86400) return (v / 3600).toPrecision(3) + " hr"
            if (v < 3.156e7) return (v / 86400).toPrecision(3) + " d"
            if (v < 3.156e10) return (v / 3.156e7).toPrecision(3) + " yr"
            if (v < 3.156e13) return (v / 3.156e10).toPrecision(3) + " kyr"
            if (v < 3.156e16) return (v / 3.156e13).toPrecision(3) + " Myr"
            return (v / 3.156e16).toPrecision(3) + " Gyr"
        }
        if (s.unit === "logyr") {
            if (v < 0) {
                const sec = Math.pow(10, v) * 3.156e7
                if (sec < 60) return sec.toPrecision(2) + " s 前并合"
                if (sec < 3600) return (sec / 60).toPrecision(2) + " min 前并合"
                return (sec / 3600).toPrecision(2) + " hr 前并合"
            }
            if (v < 3) return Math.pow(10, v).toPrecision(2) + " yr"
            if (v < 6) return (Math.pow(10, v) / 1e3).toPrecision(2) + " kyr"
            if (v < 9) return (Math.pow(10, v) / 1e6).toPrecision(2) + " Myr"
            return (Math.pow(10, v) / 1e9).toPrecision(2) + " Gyr"
        }
        if (s.unit === "step") {
            // ★ B.5 ISM 认知链: 非物理时间, 显示"第 N 站 / 共 11 站"
            return "第 " + Math.round(v) + " 站 / 共 11 站"
        }
        return v.toPrecision(3) + " " + s.unit
    }

    // ---- 恒星颜色 (黑体近似分段) ----
    function starCol(teff) {
        if (teff <= 0) return "#000000"
        if (teff < 3500) return "#ff9a5c"
        if (teff < 5000) return "#ffcf87"
        if (teff < 6000) return "#ffedbe"
        if (teff < 7500) return "#fbf8e8"
        if (teff < 10000) return "#cad8ff"
        if (teff < 30000) return "#aec6ff"
        return "#9db4ff"
    }

    // ---- HR 轨迹插值 (按物理时间) ----
    function trackAt(stages, t) {
        let a = stages[0].track, b = stages[stages.length - 1].track
        let ta = stages[0].t, tb = stages[stages.length - 1].t
        for (let i = 0; i < stages.length - 1; ++i) {
            if (t >= stages[i].t && t <= stages[i + 1].t) {
                a = stages[i].track; b = stages[i + 1].track
                ta = stages[i].t; tb = stages[i + 1].t
                break
            }
        }
        const f = tb > ta ? (t - ta) / (tb - ta) : 0
        function lerp(x, y) { return x + (y - x) * f }
        function lerpLog(x, y) {
            if (x <= 0 || y <= 0) return lerp(x, y)
            return Math.exp(Math.log(x) + (Math.log(y) - Math.log(x)) * f)
        }
        return {
            teff: lerp(a.teff, b.teff),
            logL: lerp(a.logL, b.logL),
            rad: lerpLog(Math.max(a.rad, 1e-4), Math.max(b.rad, 1e-4))
        }
    }

    // ---- Ⅰa 光变模板 (天数 → M, SN 2011fe 式) ----
    // ★ 钴尾按 ⁵⁶Co 半衰期 77.2 d = 0.98 mag/100d 构造, 不用折线近似。
    //   100 天之前沿用 2011fe 实测分段 (60–100 天仍在向钴尾过渡);
    //   100 天之后纯指数衰减 (锚定 -15.2)。
    function snMag(d) {
        const pts = [[0,-13.0],[5,-17.0],[10,-18.8],[19,-19.3],[30,-18.2],
                     [60,-16.5],[100,-15.2]]
        if (d <= 0) return -13.0
        for (let i = 0; i < pts.length - 1; ++i) {
            if (d <= pts[i + 1][0]) {
                const f = (d - pts[i][0]) / (pts[i + 1][0] - pts[i][0])
                return pts[i][1] + (pts[i + 1][1] - pts[i][1]) * f
            }
        }
        // 钴衰变尾: M(d) = -15.2 + 0.0098*(d-100)
        return -15.2 + 0.0098 * (d - 100)
    }

    // ---- 播放控制 ----
    function setProg(v) {
        prog = Math.max(0, Math.min(1, v))
        vizCv.requestPaint()
    }
    function togglePlay() { playing = !playing }
    // ★ 停止配音: playNarration 传空 id 即停 (见 sceneitem.cpp)。
    //   sceneObj 为空时静默跳过, 绝不抛异常 —— 之前这里缺定义,
    //   点阶段按钮抛 ReferenceError, 后续 setProg 直接不执行。
    function stopNarr() {
        try {
            if (evo.sceneObj)
                evo.sceneObj.playNarration("", false)
        } catch (e) { console.warn("[演化] 停配音失败: " + e) }
    }
    function selectScript(i) {
        cur = i; prog = 0; playing = false
        stopNarr()
        const s = scripts[i]
        logMode = s.useLog
        vizCv.requestPaint()
    }

    Timer {
        id: tick
        interval: 50; repeat: true; running: evo.playing && evo.visible
        onTriggered: {
            let np = evo.prog + (0.05 / evo.baseDur) * evo.speed
            if (np >= 1) { np = 1; evo.playing = false }
            evo.setProg(np)
        }
    }

    // ---- 测试入口 SS_EVO=<scriptId>[:<prog01>] ----
    Timer {
        interval: 400; repeat: true
        running: evo.testEvo.length > 0 && !evo.visible
        property bool done: false
        onTriggered: {
            if (done) return
            const parts = evo.testEvo.split(":")
            let idx = 0
            for (let i = 0; i < scripts.length; ++i)
                if (scripts[i].id === parts[0]) idx = i
            evo.selectScript(idx)
            if (parts.length > 1) evo.setProg(parseFloat(parts[1]))
            else evo.setProg(0.5)
            evo.visible = true
            done = true
            console.log("[演化] 已打开: " + scripts[idx].id
                        + " 进度=" + evo.prog.toFixed(2)
                        + " 阶段=" + scripts[idx].stages[evo.stageIndex()].label)
        }
    }

    // 点背景关闭
    MouseArea {
        anchors.fill: parent
        onClicked: { evo.playing = false; evo.stopNarr(); evo.visible = false }
    }

    // ---- 主卡片 ----
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 60, 980)
        height: Math.min(parent.height - 60, 720)
        radius: 12
        color: Qt.rgba(0.055, 0.078, 0.125, 0.98)
        border.width: 1
        border.color: Qt.rgba(0.37, 0.66, 1.0, 0.35)

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 10

            // ---- 标题行 ----
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Text {
                    text: "天体演化播放器"
                    color: evo.tx
                    font.pixelSize: 18
                    font.bold: true
                    font.family: "Microsoft YaHei"
                }
                Text {
                    text: "可拖动 · 多倍速 · 解说词接口已预留"
                    color: evo.txDim
                    font.pixelSize: 11
                    font.family: "Microsoft YaHei"
                    Layout.alignment: Qt.AlignBottom
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: "✕"
                    color: evo.txDim
                    font.pixelSize: 15
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -8
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { evo.playing = false; evo.stopNarr(); evo.visible = false }
                    }
                }
            }

            // ---- 剧本选择 ----
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: evo.scripts
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: 108; height: 26; radius: 7
                        color: evo.cur === index
                               ? Qt.rgba(0.37, 0.66, 1.0, 0.30)
                               : Qt.rgba(1, 1, 1, 0.05)
                        border.width: 1
                        border.color: evo.cur === index
                                      ? Qt.rgba(0.37, 0.66, 1.0, 0.55)
                                      : Qt.rgba(1, 1, 1, 0.08)
                        Text {
                            anchors.centerIn: parent
                            text: modelData.title
                            color: evo.cur === index ? "#bcd9ff" : evo.txDim
                            font.pixelSize: 10
                            font.family: "Microsoft YaHei"
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: evo.selectScript(index)
                        }
                    }
                }
            }

            // ---- 可视化 ----
            Canvas {
                id: vizCv
                Layout.fillWidth: true
                Layout.preferredHeight: 330
                renderStrategy: Canvas.Immediate
                canvasSize: Qt.size(width * evo.dpr, height * evo.dpr)
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    evo.drawViz(ctx, width, height)
                }
            }

            // ---- 时间轴 ----
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Text {
                    text: evo.fmtT(evo.tNow())
                    color: evo.accent
                    font.pixelSize: 13
                    font.family: "Consolas, monospace"
                    Layout.preferredWidth: 150
                }
                Slider {
                    id: progSlider
                    Layout.fillWidth: true
                    from: 0; to: 1; stepSize: 0.001
                    value: evo.prog
                    onMoved: evo.setProg(value)
                }
                Text {
                    text: scripts[cur].stages[stageIndex()].label
                    color: evo.tx
                    font.pixelSize: 12
                    font.family: "Microsoft YaHei"
                    Layout.preferredWidth: 190
                    elide: Text.ElideRight
                }
            }

            // ---- 播放控制 ----
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Rectangle {
                    width: 76; height: 26; radius: 7
                    color: evo.playing ? Qt.rgba(0.37, 0.66, 1.0, 0.30)
                                       : Qt.rgba(1, 1, 1, 0.05)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    Text {
                        anchors.centerIn: parent
                        text: evo.playing ? "❚❚ 暂停" : "▶ 播放"
                        color: evo.playing ? "#bcd9ff" : evo.txDim
                        font.pixelSize: 11
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: evo.togglePlay()
                    }
                }
                Repeater {
                    model: [0.5, 1, 2, 4, 8]
                    delegate: Rectangle {
                        required property var modelData
                        width: 52; height: 26; radius: 7
                        color: Math.abs(evo.speed - modelData) < 1e-9
                               ? Qt.rgba(0.37, 0.66, 1.0, 0.30)
                               : Qt.rgba(1, 1, 1, 0.05)
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        Text {
                            anchors.centerIn: parent
                            text: modelData + "x"
                            color: Math.abs(evo.speed - modelData) < 1e-9
                                   ? "#bcd9ff" : evo.txDim
                            font.pixelSize: 11
                            font.family: "Consolas, monospace"
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: evo.speed = modelData
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    visible: scripts[cur].allowLog
                    width: 110; height: 26; radius: 7
                    color: Qt.rgba(1, 1, 1, 0.05)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    Text {
                        anchors.centerIn: parent
                        text: evo.logMode ? "对数时间" : "线性时间"
                        color: evo.txDim
                        font.pixelSize: 11
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            evo.logMode = !evo.logMode
                            scripts[cur].useLog = evo.logMode
                            vizCv.requestPaint()
                        }
                    }
                }
            }

            // ---- 阶段说明 + 解说词接口 ----
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 86
                radius: 6
                color: Qt.rgba(1, 1, 1, 0.04)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.09)
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 4
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        // ★ 跟随全局开关: 科普版优先 pop, 无 pop 回退 desc。
                        text: {
                            const st = scripts[cur].stages[stageIndex()]
                            return (!evo.proMode && st.pop !== undefined)
                                   ? st.pop : st.desc
                        }
                        color: evo.proMode ? Qt.rgba(0.82, 0.68, 0.50, 1.0)
                                           : Qt.rgba(1.0, 0.85, 0.60, 1.0)
                        font.pixelSize: 11
                        font.family: "Microsoft YaHei"
                        lineHeight: 1.35
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: "解说词 " + scripts[cur].stages[stageIndex()].narr
                                  + (evo.proMode ? " · 男音专业版" : " · 女音科普版")
                            color: evo.txDim
                            font.pixelSize: 10
                            font.family: "Consolas, monospace"
                        }
                        // ★ 配音播放: proMode=true->A男音_ pro.mp3,
                        //   false->B女音 _pop.mp3。文件缺失时 C++ 返回空串,
                        //   按钮置灰一次(下次切换阶段恢复)。
                        Rectangle {
                            Layout.preferredWidth: 64
                            Layout.preferredHeight: 22
                            radius: 5
                            color: narrBtnMa.containsMouse
                                   ? Qt.rgba(0.37, 0.66, 1.0, 0.30)
                                   : Qt.rgba(1, 1, 1, 0.06)
                            border.width: 1
                            border.color: Qt.rgba(0.5, 0.7, 1.0, 0.4)
                            Text {
                                anchors.centerIn: parent
                                text: "▶ 播放"
                                color: "#d7e6ff"
                                font.pixelSize: 10
                                font.family: "Microsoft YaHei"
                            }
                            MouseArea {
                                id: narrBtnMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    const st = scripts[cur].stages[stageIndex()]
                                    if (evo.sceneObj)
                                        evo.sceneObj.playNarration(st.narr,
                                                                   !evo.proMode)
                                }
                            }
                        }
                    }
                }
            }

            // ---- 阶段跳转 ----
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: scripts[cur].stages
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: 120; height: 24; radius: 6
                        color: evo.stageIndex() === index
                               ? Qt.rgba(0.37, 0.66, 1.0, 0.25)
                               : Qt.rgba(1, 1, 1, 0.04)
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: evo.stageIndex() === index ? "#bcd9ff" : evo.txDim
                            font.pixelSize: 10
                            font.family: "Microsoft YaHei"
                            elide: Text.ElideRight
                            width: 110
                            horizontalAlignment: Text.AlignHCenter
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                evo.playing = false
                                evo.stopNarr()
                                evo.setProg(evo.progOf(modelData.t))
                            }
                        }
                    }
                }
            }
        }
    }

    // ========================================================================
    //  视觉映射 (按 viz 类型; 全部示意, 物理量级正确)
    // ========================================================================
    function drawViz(ctx, W, H) {
        const s = scripts[cur]
        const t = tNow()
        const n = prog
        ctx.fillStyle = "rgba(8,12,20,1)"
        ctx.fillRect(0, 0, W, H)
        if (s.viz === "hr") drawHR(ctx, W, H, s, t)
        else if (s.viz === "sn") drawSN(ctx, W, H, s, t)
        else if (s.viz === "merger") drawMerger(ctx, W, H, n)
        else if (s.viz === "agn") drawAGN(ctx, W, H, n, t)
        else if (s.viz === "cosmic") drawCosmic(ctx, W, H, s, t)
        else if (s.viz === "planet") drawPlanet(ctx, W, H, n, t)
        else if (s.viz === "protostar") drawProtostar(ctx, W, H, n, t)
        else if (s.viz === "binary") drawBinary(ctx, W, H, n, t)
        else if (s.viz === "remnant") drawRemnant(ctx, W, H, n, t)
        else if (s.viz === "cluster") drawCluster(ctx, W, H, n, t)
        else if (s.viz === "ism") drawISM(ctx, W, H, s, t)
        // 边框
        ctx.strokeStyle = "rgba(120,150,190,0.4)"
        ctx.lineWidth = 1
        ctx.strokeRect(0.5, 0.5, W - 1, H - 1)
    }

    function drawHR(ctx, W, H, s, t) {
        // 左: 赫罗图轨迹; 右: 恒星圆 (半径对数映射, 颜色按 Teff)
        const mx = 46, my = 14, pw = W * 0.52, ph = H - 60
        const tr = trackAt(s.stages, t)
        function X(te) {  // logTeff 3.3..4.7, 左热右冷
            const l = Math.log10(Math.max(te, 1000))
            return mx + (4.7 - l) / (4.7 - 3.3) * pw
        }
        function Y(lL) { return my + (6.5 - lL) / (6.5 - (-4.5)) * ph }
        ctx.strokeStyle = "rgba(255,255,255,0.10)"
        for (let g = -4; g <= 6; g += 2) {
            ctx.beginPath(); ctx.moveTo(mx, Y(g)); ctx.lineTo(mx + pw, Y(g)); ctx.stroke()
        }
        // 全轨迹 (暗) + 已走过 (亮)
        ctx.lineWidth = 1.5
        ctx.strokeStyle = "rgba(150,180,220,0.30)"
        ctx.beginPath()
        for (let i = 0; i < s.stages.length; ++i) {
            const p = s.stages[i].track
            const x = X(p.teff), y = Y(p.logL)
            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
        }
        ctx.stroke()
        ctx.strokeStyle = "#7ee28a"
        ctx.lineWidth = 2.5
        ctx.beginPath()
        let started = false
        for (let i = 0; i < s.stages.length; ++i) {
            if (s.stages[i].t > t) break
            const p = s.stages[i].track
            const x = X(p.teff), y = Y(p.logL)
            if (!started) { ctx.moveTo(x, y); started = true } else ctx.lineTo(x, y)
        }
        const cx = X(tr.teff), cy = Y(tr.logL)
        ctx.lineTo(cx, cy); ctx.stroke()
        ctx.fillStyle = "#ffffff"
        ctx.beginPath(); ctx.arc(cx, cy, 4, 0, 6.2832); ctx.fill()
        ctx.fillStyle = "rgba(160,180,205,0.9)"
        ctx.font = "11px 'Microsoft YaHei'"
        ctx.fillText("赫罗图 (示意)", mx, H - 12)
        ctx.fillText("Teff→", mx + pw - 44, H - 12)
        // 右: 恒星圆
        const rx = mx + pw + (W - mx - pw) / 2
        const ry = H / 2 - 10
        const rr = tr.rad <= 0 ? 3 : 8 + 46 * Math.log10(1 + tr.rad) / Math.log10(301)
        if (tr.rad > 0) {
            ctx.fillStyle = starCol(tr.teff)
            ctx.beginPath(); ctx.arc(rx, ry, Math.min(rr, 90), 0, 6.2832); ctx.fill()
        } else {
            ctx.fillStyle = "#000000"
            ctx.beginPath(); ctx.arc(rx, ry, 14, 0, 6.2832); ctx.fill()
            ctx.strokeStyle = "#ff9a9a"
            ctx.beginPath(); ctx.arc(rx, ry, 20, 0, 6.2832); ctx.stroke()
            ctx.fillStyle = "#ff9a9a"
            ctx.fillText("黑洞 (无辐射示意)", rx - 52, ry + 40)
        }
        ctx.fillStyle = "rgba(220,232,255,0.95)"
        ctx.font = "12px 'Microsoft YaHei'"
        const info = tr.rad > 0
            ? tr.teff.toFixed(0) + "K 光度10的" + tr.logL.toFixed(1) + "次方太阳 半径" + tr.rad.toPrecision(2) + "太阳"
            : "遗迹: 黑洞"
        ctx.fillText(info, rx - 150, H - 34)
    }

    function drawSN(ctx, W, H, s, t) {
        // 左: 光变曲线; 右: 膨胀壳层
        const mx = 46, my = 14, pw = W * 0.52, ph = H - 60
        function X(d) { return mx + d / 300 * pw }
        function Y(m) { return my + (-12.0 - m) / (-12.0 - (-19.6)) * ph }
        ctx.strokeStyle = "rgba(255,255,255,0.10)"
        for (let m = -19; m <= -13; m += 2) {
            ctx.beginPath(); ctx.moveTo(mx, Y(m)); ctx.lineTo(mx + pw, Y(m)); ctx.stroke()
        }
        ctx.strokeStyle = "#ffcd70"
        ctx.lineWidth = 2.5
        ctx.beginPath()
        for (let d = 0; d <= 300; d += 3) {
            const x = X(d), y = Y(snMag(d))
            if (d === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
        }
        ctx.stroke()
        // 已走部分高亮
        ctx.strokeStyle = "#7ee28a"
        ctx.lineWidth = 3
        ctx.beginPath()
        for (let d = 0; d <= Math.min(t, 300); d += 3) {
            const x = X(d), y = Y(snMag(d))
            if (d === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
        }
        ctx.stroke()
        const px = X(Math.min(t, 300)), py = Y(snMag(Math.min(t, 300)))
        ctx.fillStyle = "#ffffff"
        ctx.beginPath(); ctx.arc(px, py, 4, 0, 6.2832); ctx.fill()
        ctx.fillStyle = "rgba(160,180,205,0.9)"
        ctx.font = "11px 'Microsoft YaHei'"
        ctx.fillText("Ⅰa 光变 (M)", mx, H - 12)
        // 右: 壳层 —— 半径按 v·t 换算 (Ⅰa 抛射物典型速度约 15000 km/s)。
        // ★ 钳在示意半径内: 60 天时真实半径约 2.6 光天, 300 天约 13 光天;
        //   不钳的话晚期壳层会撑破画布。读数仍给真实换算值。
        const rx = mx + pw + (W - mx - pw) / 2, ry = H / 2 - 10
        const ltDay = 0.05 * Math.min(t, 300)   // v=15000 km/s 换算光天
        // ★ 视觉半径放大: 真实 3 光天在示意画布上只有几个像素,
        //   用 8x 系数让壳层在 60 天时约 30px 可见。读数仍给真实换算值。
        const rr = 8 + Math.min(ltDay, 15) * 8
        const grd = ctx.createRadialGradient(rx, ry, 2, rx, ry, rr)
        grd.addColorStop(0, "rgba(255,220,160,0.95)")
        grd.addColorStop(0.5, "rgba(255,150,80,0.45)")
        grd.addColorStop(1, "rgba(255,120,60,0.05)")
        ctx.fillStyle = grd
        ctx.beginPath(); ctx.arc(rx, ry, rr, 0, 6.2832); ctx.fill()
        ctx.fillStyle = "rgba(220,232,255,0.95)"
        ctx.font = "12px 'Microsoft YaHei'"
        ctx.fillText("M=" + snMag(Math.min(t, 300)).toFixed(1)
                     + "  R约" + ltDay.toFixed(1) + "光天(半径放大8倍显示)", rx - 150, H - 34)
    }

    function drawMerger(ctx, W, H, n) {
        // 间距 770→0 kpc (近心点处潮汐尾最强)
        const sep = 770 * (1 - n)
        const tail = Math.exp(-Math.pow((n - 0.5) / 0.16, 2))
                   + 0.7 * Math.exp(-Math.pow((n - 0.75) / 0.12, 2))
        const cx = W / 2, cy = H / 2 - 8
        const dx = 40 + (1 - n) * (W * 0.32)
        function galaxy(x, squeeze) {
            ctx.fillStyle = "rgba(150,190,255,0.85)"
            ctx.beginPath(); ctx.ellipse(x, cy, 46, 46 * squeeze, 0, 0, 6.2832); ctx.fill()
            ctx.fillStyle = "rgba(255,240,210,0.95)"
            ctx.beginPath(); ctx.ellipse(x, cy, 14, 14 * squeeze, 0, 0, 6.2832); ctx.fill()
        }
        // 潮汐尾 (近心点前后)
        ctx.strokeStyle = "rgba(150,200,255," + (0.15 + 0.6 * Math.min(tail, 1)).toFixed(2) + ")"
        ctx.lineWidth = 5
        ctx.beginPath()
        ctx.moveTo(cx - dx - 40, cy - 10)
        ctx.quadraticCurveTo(cx - dx - 120, cy - 90 * tail - 20, cx - dx - 170, cy - 110 * tail - 10)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(cx + dx + 40, cy + 10)
        ctx.quadraticCurveTo(cx + dx + 120, cy + 90 * tail + 20, cx + dx + 170, cy + 110 * tail + 10)
        ctx.stroke()
        galaxy(cx - dx, 0.55); galaxy(cx + dx, 0.55)
        ctx.fillStyle = "rgba(220,232,255,0.95)"
        ctx.font = "12px 'Microsoft YaHei'"
        ctx.fillText("间距约" + sep.toFixed(0) + " kpc", 24, H - 34)
        ctx.fillStyle = "rgba(160,180,205,0.9)"
        ctx.font = "11px 'Microsoft YaHei'"
        ctx.fillText("潮汐尾强度示意 (近心点最强)", 24, H - 14)
    }

    function drawAGN(ctx, W, H, n, t) {
        // 喷流长度 0→150 kpc (对数观感; CygA 瓣约 120 kpc, M87 约 5 kpc,
        // 上限取大值包络, 读数给当时值, 不虚标终点)。
        const jet = 150 * Math.pow(n, 1.6)
        const cx = W / 2, cy = H / 2 - 6
        const px = Math.min(W * 0.42, 30 + jet * 1.1)
        // 瓣
        ctx.fillStyle = "rgba(150,170,255,0.25)"
        ctx.beginPath(); ctx.ellipse(cx - px, cy, 46, 26, 0, 0, 6.2832); ctx.fill()
        ctx.beginPath(); ctx.ellipse(cx + px, cy, 46, 26, 0, 0, 6.2832); ctx.fill()
        // 喷流
        ctx.strokeStyle = "rgba(140,220,255,0.9)"
        ctx.lineWidth = 3
        ctx.beginPath(); ctx.moveTo(cx - 8, cy); ctx.lineTo(cx - px, cy); ctx.stroke()
        ctx.beginPath(); ctx.moveTo(cx + 8, cy); ctx.lineTo(cx + px, cy); ctx.stroke()
        // 核 + 盘
        ctx.fillStyle = "rgba(255,240,210,0.9)"
        ctx.beginPath(); ctx.ellipse(cx, cy, 30, 10, 0, 0, 6.2832); ctx.fill()
        ctx.fillStyle = "#000000"
        ctx.beginPath(); ctx.arc(cx, cy, 7, 0, 6.2832); ctx.fill()
        ctx.strokeStyle = "#ff9a9a"
        ctx.beginPath(); ctx.arc(cx, cy, 7, 0, 6.2832); ctx.stroke()
        ctx.fillStyle = "rgba(220,232,255,0.95)"
        ctx.font = "12px Consolas, monospace"
        // ★ 读数行避免 "~" 与 "·" 同 Consolas 混排 CJK ——
        //   实测混合字体下波浪线/中圆点会显示为乱码/ tofu。
        //   改用"约"与逗号, CJK 交给雅黑, 数字仍用等宽。
        ctx.font = "12px 'Microsoft YaHei'"
        // ★ 功率量级随规模走: M87 级约 1e44, CygA 级约 1e45–1e46。
        //   用喷流长度分档, 不在小喷流时虚标大功率。
        const pjet = jet < 20 ? "1e43–44" : (jet < 80 ? "1e44–45" : "1e45–46")
        ctx.fillText("喷流约" + jet.toFixed(0) + " kpc,功率约" + pjet + " erg/s(量级)", 24, H - 34)
        ctx.fillStyle = "rgba(160,180,205,0.9)"
        ctx.font = "11px 'Microsoft YaHei'"
        ctx.fillText("正对喷流即耀变体 (统一模型·视角决定分类)", 24, H - 14)
    }

    function drawCosmic(ctx, W, H, s, t) {
        // 对数时间轴 + 事件 ticks + 温度读数
        const mx = 46, my = 60, pw = W - 92
        function X(tt) {
            if (s.useLog) return mx + Math.log(tt / s.tMin) / Math.log(s.tMax / s.tMin) * pw
            return mx + (tt - s.tMin) / (s.tMax - s.tMin) * pw
        }
        // 时代底色 (示意): 热→透明→恒星点亮
        const grd = ctx.createLinearGradient(mx, 0, mx + pw, 0)
        grd.addColorStop(0, "rgba(255,180,120,0.30)")
        grd.addColorStop(0.45, "rgba(255,120,80,0.10)")
        grd.addColorStop(0.62, "rgba(80,120,255,0.10)")
        grd.addColorStop(1, "rgba(120,180,255,0.18)")
        ctx.fillStyle = grd
        ctx.fillRect(mx, my - 34, pw, 96)
        // 事件 ticks
        ctx.font = "10px 'Microsoft YaHei'"
        for (let i = 0; i < s.stages.length; ++i) {
            const x = X(s.stages[i].t)
            const hit = t >= s.stages[i].t
            ctx.strokeStyle = hit ? "#7ee28a" : "rgba(160,180,205,0.5)"
            ctx.lineWidth = hit ? 2.5 : 1.5
            ctx.beginPath(); ctx.moveTo(x, my - 30); ctx.lineTo(x, my + 44); ctx.stroke()
            ctx.fillStyle = hit ? "#bcd9ff" : "rgba(160,180,205,0.75)"
            ctx.fillText(s.stages[i].label, Math.min(x - 24, mx + pw - 90), my + 60)
        }
        // 当前 marker
        const nx = X(Math.max(t, s.tMin))
        ctx.fillStyle = "#ffffff"
        ctx.beginPath(); ctx.arc(nx, my + 6, 5, 0, 6.2832); ctx.fill()
        const st = s.stages[stageIndex()]
        ctx.fillStyle = "rgba(220,232,255,0.95)"
        ctx.font = "13px Consolas, monospace"
        ctx.fillText("t = " + fmtT(t) + "   T ~ " + (st.temp || "?"), 24, H - 56)
        ctx.fillStyle = "rgba(160,180,205,0.9)"
        ctx.font = "11px 'Microsoft YaHei'"
        const note = s.useLog ? "对数时间轴 (早期事件被压成一条线是诚实的代价, 见宇宙视图同款说明)"
                              : "线性时间轴 (早期几分钟占不到一个像素 —— 这本身就是教学点)"
        ctx.fillText(note, 24, H - 34)
        ctx.fillText((!evo.proMode && st.pop !== undefined) ? st.pop : st.desc, 24, H - 14)
    }

    function drawPlanet(ctx, W, H, n, t) {
        // 侧视盘 + 行星生长/迁移 (示意)
        const cx = W / 2, cy = H / 2
        const gas = Math.max(0, 1 - t / 5)   // 气体 5 Myr 散去
        ctx.fillStyle = "rgba(150,170,220," + (0.10 + 0.25 * gas).toFixed(2) + ")"
        ctx.beginPath(); ctx.ellipse(cx, cy, W * 0.42, 26 + 30 * gas, 0, 0, 6.2832); ctx.fill()
        // 恒星
        ctx.fillStyle = "#ffedbe"
        ctx.beginPath(); ctx.arc(cx, cy, 12, 0, 6.2832); ctx.fill()
        // 三颗行星: 内岩质 / 气态巨行星(迁移) / 外冰巨星
        const g1 = Math.min(1, Math.max(0, (t - 3) / 3))
        const g2 = Math.min(1, Math.max(0, (t - 1) / 4))
        const jx = cx + (W * 0.30 - n * W * 0.16) * (t > 5 ? 1 : 1) // 迁移 inward 示意
        function planet(x, r, col) {
            ctx.fillStyle = col
            ctx.beginPath(); ctx.arc(x, cy - 4, Math.max(2, r), 0, 6.2832); ctx.fill()
        }
        planet(cx - W * 0.22, 2 + 3 * g1, "#ff9a7a")
        planet(jx - W * 0.05, 3 + 7 * g2, "#e8b96a")
        planet(cx + W * 0.30, 2 + 4 * Math.min(1, t / 8), "#9fd0ff")
        // 间隙
        if (g2 > 0.5) {
            ctx.strokeStyle = "rgba(0,0,0,0.55)"
            ctx.lineWidth = 8
            ctx.beginPath(); ctx.ellipse(jx - W * 0.05, cy, 26, 20, 0, 0, 6.2832); ctx.stroke()
        }
        ctx.fillStyle = "rgba(220,232,255,0.95)"
        ctx.font = "12px 'Microsoft YaHei'"
        ctx.fillText("t=" + t.toPrecision(3) + "百万年 气体余量约" + (gas * 100).toFixed(0) + "%", 24, H - 34)
        ctx.fillStyle = "rgba(160,180,205,0.9)"
        ctx.font = "11px 'Microsoft YaHei'"
        ctx.fillText("迁移分叉: 内移成热木星 (51 Peg b) 或留在外侧 (HR 8799)", 24, H - 14)
    }

    function drawProtostar(ctx, W, H, n, t) {
        const cx = W / 2, cy = H / 2 - 6
        const env = Math.max(0, 1 - t / 1.2)   // 包层约 1 Myr 散去
        const er = 20 + 120 * env
        const grd = ctx.createRadialGradient(cx, cy, 4, cx, cy, er)
        grd.addColorStop(0, "rgba(255,230,180,0.9)")
        grd.addColorStop(0.4, "rgba(200,150,110," + (0.15 + 0.4 * env).toFixed(2) + ")")
        grd.addColorStop(1, "rgba(120,100,90,0.03)")
        ctx.fillStyle = grd
        ctx.beginPath(); ctx.arc(cx, cy, er, 0, 6.2832); ctx.fill()
        // 双极外向流: 强度随包层耗散而减弱 (Ⅱ/Ⅲ 类只剩残余),
        // 长度钳在画布内 —— 否则早期会顶穿上下边, 像一条"贯穿线"。
        const jetFrac = Math.min(1, t / 0.5) * (0.15 + 0.85 * env)
        const jet = Math.min(H * 0.38, 30 + 130 * jetFrac)
        ctx.strokeStyle = "rgba(140,200,255," + (0.15 + 0.55 * jetFrac).toFixed(2) + ")"
        ctx.lineWidth = 4
        ctx.beginPath(); ctx.moveTo(cx, cy - 8); ctx.lineTo(cx, cy - 8 - jet); ctx.stroke()
        ctx.beginPath(); ctx.moveTo(cx, cy + 8); ctx.lineTo(cx, cy + 8 + jet); ctx.stroke()
        ctx.fillStyle = "rgba(220,232,255,0.95)"
        ctx.font = "12px 'Microsoft YaHei'"
        ctx.fillText("包层余量约" + (env * 100).toFixed(0) + "%", 24, H - 34)
        ctx.fillStyle = "rgba(160,180,205,0.9)"
        ctx.font = "11px 'Microsoft YaHei'"
        ctx.fillText("0→Ⅰ→Ⅱ→Ⅲ: 包层耗散序列 (T_bol 上升)", 24, H - 14)
    }

    function drawBinary(ctx, W, H, n, t) {
        // t 为 log10(距并合年); 分离示意加速收缩 + 啁啾正弦
        const cx = W * 0.28, cy = H / 2 - 10
        const sep = 12 + 90 * Math.pow(Math.max(0, (t + 7) / 16), 1.8)
        ctx.fillStyle = "#cad8ff"
        ctx.beginPath(); ctx.arc(cx - sep / 2, cy, 9, 0, 6.2832); ctx.fill()
        ctx.beginPath(); ctx.arc(cx + sep / 2, cy, 9, 0, 6.2832); ctx.fill()
        ctx.strokeStyle = "rgba(150,180,255,0.35)"
        ctx.beginPath(); ctx.ellipse(cx, cy, sep / 2 + 8, (sep / 2 + 8) * 0.6, 0, 0, 6.2832); ctx.stroke()
        // 右: 啁啾波 (频率随 n 增高)
        const mx = W * 0.52, pw = W * 0.44, my = 30, ph = H - 90
        ctx.strokeStyle = "#7ee28a"
        ctx.lineWidth = 2
        ctx.beginPath()
        for (let i = 0; i <= 200; ++i) {
            const f = i / 200
            const fr = 2 + 26 * Math.pow(f * (0.2 + 0.8 * n), 1.5)
            const x = mx + f * pw
            const y = my + ph / 2 + Math.sin(f * fr * 6.2832) * ph * 0.32 * (0.3 + 0.7 * f)
            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
        }
        ctx.stroke()
        ctx.fillStyle = "rgba(220,232,255,0.95)"
        ctx.font = "12px 'Microsoft YaHei'"
        ctx.fillText(t < 0 ? "距并合" + fmtT(t) : "距并合10的" + t.toFixed(1) + "次方年", 24, H - 34)
        ctx.fillStyle = "rgba(160,180,205,0.9)"
        ctx.font = "11px 'Microsoft YaHei'"
        ctx.fillText("终点: GW170817 + 千新星 AT2017gfo (金、铂起源)", 24, H - 14)
    }

    function drawRemnant(ctx, W, H, n, t) {
        // 左: 白矮星冷却 (log 年龄 – log 光度); 右: 脉冲星周期增长
        const lw = W * 0.44
        function panel(x0, title) {
            ctx.strokeStyle = "rgba(255,255,255,0.12)"
            ctx.strokeRect(x0, 24, lw, H - 90)
            ctx.fillStyle = "rgba(160,180,205,0.9)"
            ctx.font = "11px 'Microsoft YaHei'"
            ctx.fillText(title, x0 + 8, 42)
        }
        panel(24, "白矮星冷却 (L–年龄)")
        panel(24 + lw + 24, "脉冲星自转减慢 (P–年龄)")
        // WD: logL  -1.5 → -4.5 over logyr 6→10 (示意直线)
        const wdx = 24 + (t - 3) / 7 * lw
        const wdL = -1.5 - (t - 6) / 4 * 3.0
        ctx.fillStyle = "#cad8ff"
        ctx.beginPath(); ctx.arc(Math.min(wdx, 24 + lw - 4), 60 + ((-1.0 - wdL) / 4.5) * (H - 140), 5, 0, 6.2832); ctx.fill()
        ctx.strokeStyle = "rgba(160,180,220,0.5)"
        ctx.beginPath(); ctx.moveTo(28, 70); ctx.lineTo(24 + lw - 4, H - 80); ctx.stroke()
        // Pulsar: P 0.033 → 数秒 (log)
        const logP = -1.48 + (t - 3) / 7 * 2.0
        const pxx = 24 + lw + 24 + (t - 3) / 7 * lw
        ctx.fillStyle = "#7ee28a"
        ctx.beginPath(); ctx.arc(Math.min(pxx, 24 + 2 * lw + 20), 60 + ((logP + 1.6) / 2.4) * (H - 140), 5, 0, 6.2832); ctx.fill()
        ctx.fillStyle = "rgba(220,232,255,0.95)"
        ctx.font = "12px 'Microsoft YaHei'"
        ctx.fillText("白矮星光度10的" + wdL.toFixed(1) + "次方太阳 · 脉冲星周期"
                     + Math.pow(10, logP).toPrecision(2) + "秒", 24, H - 34)
        ctx.fillStyle = "rgba(160,180,205,0.9)"
        ctx.font = "11px 'Microsoft YaHei'"
        ctx.fillText("黑矮星宇宙年龄内到不了 · 毫秒脉冲星守时到百亿年", 24, H - 14)
    }

    function drawCluster(ctx, W, H, n, t) {
        // 星数蒸发 + 半质量半径 (示意); t 为 Myr (log 模式)
        const bound = Math.max(0.05, 1 - 0.55 * Math.log10(Math.max(t, 1)) / 4)
        const rh = 2 + 6 * Math.min(1, Math.log10(Math.max(t, 1)) / 3)
        const cx = W / 2, cy = H / 2 - 8
        const R = 40 + rh * 12
        const N = Math.round(130 * bound)
        for (let i = 0; i < N; ++i) {
            const a = (i * 2.39996) % 6.2832
            const r = R * Math.sqrt(((i * 0.618034) % 1))
            const x = cx + Math.cos(a) * r
            const y = cy + Math.sin(a) * r * 0.8
            const b = 120 + ((i * 37) % 100)
            ctx.fillStyle = "rgba(" + b + "," + (b + 20) + ",255,0.8)"
            ctx.fillRect(x, y, 2, 2)
        }
        ctx.strokeStyle = "rgba(150,180,255,0.4)"
        ctx.beginPath(); ctx.arc(cx, cy, R, 0, 6.2832); ctx.stroke()
        ctx.fillStyle = "rgba(220,232,255,0.95)"
        ctx.font = "12px 'Microsoft YaHei'"
        ctx.fillText("束缚星数约" + (bound * 100).toFixed(0) + "% 半质量半径约"
                     + rh.toFixed(1) + "秒差距", 24, H - 34)
        ctx.fillStyle = "rgba(160,180,205,0.9)"
        ctx.font = "11px 'Microsoft YaHei'"
        ctx.fillText("低质量星优先逃逸 (质量分层) · M13 年龄即宇宙年龄下限", 24, H - 14)
    }

    function drawISM(ctx, W, H, s, t) {
        // ★ B.5 三色示意: 红=发射(H II) / 蓝=反射(散射) / 暗=尘埃剪影。
        //   当前站高亮, 其余压暗 —— "认三种光"的视觉锚点。
        const k = stageIndex()
        const cx = W / 2, cy = H / 2 - 8
        // 背景星场
        for (let i = 0; i < 90; ++i) {
            const a = (i * 2.39996) % 6.2832
            const r = 60 + ((i * 0.618034) % 1) * (Math.min(W, H) * 0.38)
            const b = 100 + ((i * 37) % 120)
            ctx.fillStyle = "rgba(" + b + "," + b + "," + b + ",0.35)"
            ctx.fillRect(cx + Math.cos(a) * r, cy + Math.sin(a) * r * 0.7, 1.5, 1.5)
        }
        // 三团: 发射(红) / 反射(蓝) / 暗(黑幕+描边)
        const cols = [["255,120,120", "发射 H II"], ["140,180,255", "反射 散射"], ["20,22,30", "暗 剪影"]]
        for (let j = 0; j < 3; ++j) {
            const x = cx + (j - 1) * 130
            const on = (k <= 2 && j === k) || (k > 2)  // 前3站逐一点亮, 之后全亮
            ctx.fillStyle = "rgba(" + cols[j][0] + "," + (on ? "0.55" : "0.14") + ")"
            ctx.beginPath(); ctx.ellipse(x, cy, 52, 40, 0, 0, 6.2832); ctx.fill()
            ctx.strokeStyle = on ? "rgba(255,255,255,0.5)" : "rgba(160,180,205,0.25)"
            ctx.lineWidth = on ? 2 : 1
            ctx.beginPath(); ctx.ellipse(x, cy, 52, 40, 0, 0, 6.2832); ctx.stroke()
            ctx.fillStyle = on ? "rgba(220,232,255,0.95)" : "rgba(160,180,205,0.5)"
            ctx.font = "11px 'Microsoft YaHei'"
            ctx.fillText(cols[j][1], x - 30, cy + 58)
        }
        ctx.fillStyle = "rgba(220,232,255,0.95)"
        ctx.font = "12px 'Microsoft YaHei'"
        ctx.fillText("第 " + k + " 站 · " + s.stages[k].label, 24, H - 34)
        ctx.fillStyle = "rgba(160,180,205,0.9)"
        ctx.font = "11px 'Microsoft YaHei'"
        ctx.fillText("红发光 · 蓝反光 · 黑是尘埃缝", 24, H - 14)
    }
}
