// ============================================================================
//  太阳系模拟器 —— QML 界面
//
//  C++ 侧提供 SolarScene (原生 OpenGL 渲染节点 + 天体数据);
//  QML 侧负责全部界面与交互, 这是 Qt 官方推荐的现代组合。
//
//  与 Python 版的关系: Python 版用 QPainter 自绘玻璃拟态面板, 这里改用
//  QML 的声明式写法 —— 同样的视觉, 更少代码, 且自带动画与状态绑定。
// ============================================================================

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 导入本模块 —— C++ 侧用 QML_NAMED_ELEMENT 注册的类型 (SolarScene)
// 就属于这个模块。缺这一行会报 "SolarScene is not a type"。
import SolarSystem 1.0

ApplicationWindow {
    id: root
    width: 1440; height: 900
    minimumWidth: 1024
    minimumHeight: 640
    visible: true
    // ★ 自检模式下从第一帧就透明 —— 避免渲染窗口闪现在用户屏幕上。
    //   见 main.cpp 的 ssHeadless 上下文属性说明。
    //   (不能改成 visible: false: 窗口不可见时 Qt 会跳过场景图渲染,
    //    grabWindow() 只能抓到空白。)
    opacity: ssHeadless ? 0 : 1
    title: "太阳系模拟器 · Solar System Simulator (C++ / QML / OpenGL)"
    color: "#05070d"


    // ---- 字体规范 ----
    //
    // ★ 统一原则 (此前是散落各处的硬编码, 导致中文被等宽字体渲染、
    //   字距松散, 且各面板风格不一致):
    //     monoFont  -> 数值/日期/代号 (等宽才能对齐)
    //     sansFont  -> 中文文本、名称、标签 (CJK 字形完整、字距正常)
    //
    //   Qt 会按逗号列表依次尝试, 取第一个可用的。
    readonly property string monoFont: "Consolas, Cascadia Mono, Menlo, monospace"

    // ★★ 中文字体只用**一个**确定的字体, 不写回退链。
    //
    //   实测踩到: 写成 "Microsoft YaHei UI, Microsoft YaHei, ..." 时,
    //   中文出现**重影/发虚** —— 字形边缘叠了一层暗色描边, 像渲染两次。
    //   原因是回退链里多个字体的度量不同, Qt 在同一段文本里混用了
    //   不同字体的字形 (某些字号下 Windows 的字体链接会这样)。
    //
    //   本机可用: msyh.ttc (微软雅黑) / Deng.ttf (等线) / simhei.ttf (黑体)
    //   微软雅黑最普适, 只写它本身。
    readonly property string sansFont: "Microsoft YaHei"

    // ★★ 按内容自动选字体。
    //
    //   问题: 有些字段的值**既有数字又有中文** ——
    //     银河系面板的 "105,700 光年"、宇宙面板的 "13.8 亿光年"。
    //   这些如果套 monoFont (Consolas), Consolas 没有中文字形,
    //   Qt 会回退到某个衬线 CJK 字体, 结果同一面板里
    //   数字是无衬线、中文是衬线, 风格割裂 (实测就是这样)。
    //
    //   规则: 只要含 CJK 字符就用无衬线, 纯 ASCII 才用等宽。
    //   这样数字列仍然对齐 (同类字段格式一致), 中文也不会串字体。
    function fitFont(t) {
        if (t === undefined || t === null)
            return sansFont
        return /[一-鿿　-〿＀-￯]/.test(String(t))
               ? sansFont : monoFont
    }

    // ---- 配色 (玻璃拟态) ----
    readonly property color cPanel:      Qt.rgba(0.055, 0.078, 0.125, 0.72)
    readonly property color cPanelSolid: Qt.rgba(0.055, 0.078, 0.125, 0.96)
    readonly property color cBorder:     Qt.rgba(0.45, 0.62, 0.88, 0.16)
    readonly property color cText:       "#e8eef9"
    readonly property color cTextDim:    "#8fa3bf"
    readonly property color cAccent:     "#5ea9ff"
    readonly property color cAccentSoft: Qt.rgba(0.37, 0.66, 1.0, 0.18)

    property var  bodies: []
    property var  current: ({})
    property string currentId: "earth"
    property bool  listHidden: false

    Component.onCompleted: {
        // 采纳 C++ 侧的初始焦点 —— 它可能被环境变量 SS_FOCUS 覆盖,
        // 若这里硬写 "earth" 就会把 C++ 的设定顶掉。
        currentId = scene.focusId
        bodies = scene.bodyList()
        current = scene.bodyInfo(scene.focusId)
    }

    // ========================================================================
    //  主视口 —— C++ 原生 OpenGL 渲染
    // ========================================================================
    SolarScene {
        id: scene
        // 供 C++ 侧 findChild 定位 —— QML 的 id 不是属性, C++ 读不到。
        objectName: "scene"
        anchors.fill: parent
        // 注意: 这里**不要**写 focusId: root.currentId ——
        // 那样会在初始化时把 C++ 侧的初始焦点 (可被 SS_FOCUS 覆盖) 顶掉。
        // 焦点由 C++ 持有, QML 只通过 focusOn() 请求切换。
    }

    function refresh() {
        current = scene.bodyInfo(scene.focusId)
        currentId = scene.focusId
    }

    // 详情里的日心距/轨道速度随时间变化, 定时刷新
    Timer {
        interval: 600; running: true; repeat: true
        onTriggered: {
            root.current = scene.bodyInfo(scene.focusId)
            root.currentId = scene.focusId
        }
    }

    // ---- 相机交互 ----
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        property real lastX: 0
        property real lastY: 0
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor

        onPressed: (m) => { lastX = m.x; lastY = m.y }
        onPositionChanged: (m) => {
            const dx = m.x - lastX
            const dy = m.y - lastY
            lastX = m.x; lastY = m.y
            if (m.buttons & Qt.RightButton)
                scene.panCamera(dx, dy)
            else
                scene.rotateCamera(dx, dy)
        }
        onWheel: (w) => scene.zoomCamera(w.angleDelta.y / 120.0)
    }

    // ========================================================================
    //  面板基类
    // ========================================================================
    component GlassPanel: Rectangle {
        color: root.cPanel
        border.color: root.cBorder
        border.width: 1
        radius: 12
    }

    component SectionTitle: Text {
        color: root.cTextDim
        font.pixelSize: 11
        font.letterSpacing: 0.8
    }

    component ChipButton: Rectangle {
        id: chip
        property string label: ""
        property bool active: false
        signal clicked()

        implicitWidth: chipText.implicitWidth + 20
        implicitHeight: 26
        radius: 7
        color: active ? root.cAccentSoft : Qt.rgba(1, 1, 1, 0.045)
        border.width: 1
        border.color: active ? Qt.rgba(0.37, 0.66, 1.0, 0.55)
                             : Qt.rgba(1, 1, 1, 0.07)

        Text {
            id: chipText
            anchors.centerIn: parent
            text: chip.label
            color: chip.active ? "#bcd9ff" : root.cTextDim
            font.pixelSize: 11
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: chip.clicked()
        }
    }

    // ========================================================================
    //  左面板
    // ========================================================================
    GlassPanel {
        id: leftPanel
        x: 18; y: 74
        width: 268
        height: parent.height - 74 - 46
        // 银河系尺度用另一套面板 (见下方 galaxyPanel)
        visible: !root.listHidden && scene.scaleLevel === 0

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            // ---------------- 时间控制 ----------------
            SectionTitle { text: "时间控制" }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                ChipButton {
                    label: scene.paused ? "▶ 继续" : "❚❚ 暂停"
                    active: scene.paused
                    onClicked: scene.paused = !scene.paused
                }
                ChipButton {
                    label: "回到此刻"
                    onClicked: scene.setTimeToNow()
                }
            }

            Text {
                text: "流速"
                color: root.cTextDim
                font.pixelSize: 11
            }

            Flow {
                Layout.fillWidth: true
                spacing: 6

                Repeater {
                    model: [
                        { t: "实时",  s: 1.0 },
                        { t: "1分/秒", s: 60.0 },
                        { t: "1时/秒", s: 3600.0 },
                        { t: "1天/秒", s: 86400.0 },
                        { t: "1月/秒", s: 2592000.0 },
                        { t: "1年/秒", s: 31536000.0 }
                    ]
                    ChipButton {
                        required property var modelData
                        label: modelData.t
                        active: Math.abs(scene.timeScale - modelData.s) < 1e-6 && !scene.paused
                        onClicked: scene.timeScale = modelData.s
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true; height: 1
                color: Qt.rgba(1, 1, 1, 0.07)
            }

            // ---------------- 显示 ----------------
            SectionTitle { text: "显示" }

            CheckBox {
                text: "轨道线"
                checked: scene.showOrbits
                onToggled: scene.showOrbits = checked
                palette.windowText: root.cText
            }
            CheckBox {
                text: "行星环"
                checked: scene.showRings
                onToggled: scene.showRings = checked
            }
            CheckBox {
                text: "大气层"
                checked: scene.showAtmo
                onToggled: scene.showAtmo = checked
            }

            // ---- 真实比例 ----
            // 教学上极重要: 艺术压缩让内外行星同框, 但也扭曲了尺度认知。
            // 开启后按 1:1 呈现, 学生能看到"地球在 1 AU 处有多小"。
            CheckBox {
                text: "真实比例 (1:1)"
                checked: scene.realScale
                onToggled: scene.realScale = checked
            }

            Text {
                Layout.fillWidth: true
                visible: scene.realScale
                text: "已关闭艺术压缩。轨道半径与天体间距为 1:1 真实比例；球体尺寸已放大以便观察（真实尺寸下不足一个像素，无法显示）。"
                color: Qt.rgba(1.0, 0.80, 0.35, 0.9)
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                lineHeight: 1.3
            }

            Rectangle {
                Layout.fillWidth: true; height: 1
                color: Qt.rgba(1, 1, 1, 0.07)
            }

            // ---------------- 天体列表 ----------------
            SectionTitle {
                text: "天体 · " + root.bodies.length
            }

            ListView {
                id: bodyList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.bodies
                spacing: 2

                delegate: Rectangle {
                    required property var modelData

                    width: bodyList.width
                    height: 38
                    radius: 8
                    color: (modelData.id === root.currentId)
                           ? root.cAccentSoft : "transparent"
                    border.width: modelData.id === root.currentId ? 1 : 0
                    border.color: Qt.rgba(0.37, 0.66, 1.0, 0.45)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 9

                        Rectangle {
                            width: 12; height: 12; radius: 6
                            color: modelData.color
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.25)
                        }

                        // ★ 中英文名: 两行都**左对齐**, 左边缘严格对齐。
                        //   之前中英文各自居中, 加上中文用等宽字体带字距补偿,
                        //   两行的左边缘是错开的, 视觉上很乱。
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Text {
                                text: modelData.name
                                color: root.cText
                                font.pixelSize: 13
                                font.family: root.sansFont
                                font.bold: modelData.id === root.currentId
                                horizontalAlignment: Text.AlignLeft
                                Layout.fillWidth: true
                            }
                            Text {
                                text: modelData.en
                                color: root.cTextDim
                                font.pixelSize: 10
                                font.family: root.sansFont
                                horizontalAlignment: Text.AlignLeft
                                Layout.fillWidth: true
                            }
                        }

                        // ★ 分类标签: **固定宽度 + 右对齐**, 让所有行的标签
                        //   形成整齐的一列 (之前宽度随中文名浮动, 参差不齐)
                        Text {
                            text: {
                                const k = modelData.kind
                                if (k === "star") return "恒星"
                                if (k === "moon") return "卫星"
                                return "行星"
                            }
                            color: Qt.rgba(0.56, 0.64, 0.75, 0.75)
                            font.pixelSize: 10
                            font.family: root.sansFont
                            Layout.preferredWidth: 34
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            scene.focusOn(modelData.id)
                            root.currentId = modelData.id
                            root.current = scene.bodyInfo(modelData.id)
                        }
                    }
                }
            }
        }
    }

    // ========================================================================
    //  右面板 —— 选中天体详情
    // ========================================================================
    GlassPanel {
        id: rightPanel
        width: 300
        height: infoColumn.implicitHeight + 28
        x: parent.width - width - 18
        y: parent.height - height - 46

        // "overview" 是全景视角, 并没有某个具体天体可显示 ——
        // 早期这里不做判断, 结果是一整块面板全是占位符 "—", 看着像坏了。
        // 银河系尺度下由 galaxyStatPanel 接管, 这里同样要隐藏。
        visible: scene.scaleLevel === 0 && root.currentId !== "overview"

        ColumnLayout {
            id: infoColumn
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            RowLayout {
                spacing: 9
                Rectangle {
                    width: 16; height: 16; radius: 8
                    color: root.current.color !== undefined ? root.current.color : "#888"
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.3)
                }
                ColumnLayout {
                    spacing: 0
                    Text {
                        text: root.current.name !== undefined ? root.current.name : ""
                        color: root.cText
                        font.pixelSize: 16
                        font.bold: true
                    }
                    Text {
                        text: root.current.en !== undefined ? root.current.en : ""
                        color: root.cTextDim
                        font.pixelSize: 10
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true; height: 1
                color: Qt.rgba(1, 1, 1, 0.08)
            }

            Repeater {
                model: [
                    { k: "半径",   v: "radius" },
                    { k: "质量",   v: "mass" },
                    { k: "自转",   v: "rotation" },
                    { k: "日心距", v: "distAu" },
                    { k: "轨道速度", v: "speed" },
                    { k: "反照率", v: "albedo" },
                    { k: "表面重力", v: "gravity" },
                    { k: "逃逸速度", v: "escape" }
                ]

                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: modelData.k
                        color: root.cTextDim
                        font.pixelSize: 11
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: root.current[modelData.v] !== undefined
                              ? root.current[modelData.v] : "—"
                        color: root.cText
                        font.pixelSize: 11
                        font.family: root.fitFont(root.current[modelData.v])
                    }
                }
            }
        }
    }

    // ========================================================================
    //  顶栏
    // ========================================================================
    GlassPanel {
        x: 18; y: 18
        // 加宽以容纳尺度切换控件
        width: Math.min(parent.width - 36, 840)
        height: 44

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 12

            ColumnLayout {
                spacing: -1
                Text {
                    text: scene.scaleLevel === 2 ? "宇宙大尺度结构"
                    : scene.scaleLevel === 1 ? "银河系模拟器"
                    : "太阳系模拟器"
                    color: root.cText
                    font.pixelSize: 14
                    font.bold: true
                }
                Text {
                    text: "C++ / Qt 6 · QML 界面 · OpenGL 3.3 Core 渲染"
                    color: root.cTextDim
                    font.pixelSize: 9
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: scene.dateText
                color: root.cAccent
                font.pixelSize: 14
                font.family: root.fitFont(scene.dateText)
            }

            // ---------------- 尺度切换 ----------------
            //
            // 用**分段控件**而不是下拉框: 选项直接可见, 用户一眼就知道
            // 这里还有「银河系」视图。下拉框收起时看不出有第二个尺度,
            // 这正是之前"没看到可选"的原因。
            Rectangle {
                Layout.preferredWidth: 4
                Layout.preferredHeight: 1
                color: "transparent"
            }

            Row {
                spacing: 3

                Repeater {
                    model: [
                        { t: "太阳系", v: 0 },
                        { t: "银河系", v: 1 },
                        { t: "宇宙",   v: 2 }
                    ]

                    Rectangle {
                        required property var modelData
                        width: 66; height: 26; radius: 7
                        color: scene.scaleLevel === modelData.v
                               ? root.cAccentSoft : Qt.rgba(1, 1, 1, 0.05)
                        border.width: 1
                        border.color: scene.scaleLevel === modelData.v
                                      ? Qt.rgba(0.37, 0.66, 1.0, 0.55)
                                      : Qt.rgba(1, 1, 1, 0.08)

                        Text {
                            anchors.centerIn: parent
                            text: modelData.t
                            color: scene.scaleLevel === modelData.v
                                   ? "#bcd9ff" : root.cTextDim
                            font.pixelSize: 11
                            font.bold: scene.scaleLevel === modelData.v
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: scene.scaleLevel = modelData.v
                        }
                    }
                }
            }
        }
    }

    // ========================================================================
    //  银河系尺度面板
    // ========================================================================

    // 左侧: 结构与尺度数值表
    GlassPanel {
        id: galaxyStructPanel
        x: 18; y: 74
        width: 288
        height: parent.height - 74 - 46
        visible: scene.scaleLevel === 1

        property var g: ({})

        Component.onCompleted: g = scene.galaxyInfo()

        Flickable {
            anchors.fill: parent
            anchors.margins: 14
            contentHeight: gsCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: gsCol
                width: parent.width
                spacing: 9

                SectionTitle { text: "银河系结构" }

                Repeater {
                    model: [
                        { k: "直径",     v: "diameter" },
                        { k: "薄盘厚度", v: "diskThickness" },
                        { k: "核球半径", v: "bulgeRadius" },
                        { k: "中央棒长度", v: "barLength" },
                        { k: "旋臂",     v: "arms" }
                    ]

                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: modelData.k
                            color: root.cTextDim
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: galaxyStructPanel.g[modelData.v] || "—"
                            color: root.cText
                            font.pixelSize: 11
                            font.family: root.fitFont(galaxyStructPanel.g[modelData.v] || "—")
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; height: 1
                    color: Qt.rgba(1, 1, 1, 0.07)
                }

                SectionTitle { text: "太阳的位置" }

                Repeater {
                    model: [
                        { k: "距银心",   v: "sunDistance" },
                        { k: "绕行速度", v: "sunSpeed" },
                        { k: "银河年",   v: "galacticYear" },
                        { k: "已绕行",   v: "sunOrbits" }
                    ]

                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: modelData.k
                            color: root.cTextDim
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: galaxyStructPanel.g[modelData.v] || "—"
                            color: "#ffdd88"
                            font.pixelSize: 11
                            font.family: root.fitFont(galaxyStructPanel.g[modelData.v] || "—")
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; height: 1
                    color: Qt.rgba(1, 1, 1, 0.07)
                }

                SectionTitle { text: "组成" }

                Repeater {
                    model: [
                        { k: "恒星数",   v: "starCount" },
                        { k: "总质量",   v: "mass" },
                        { k: "银心黑洞", v: "blackHole" }
                    ]

                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: modelData.k
                            color: root.cTextDim
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: galaxyStructPanel.g[modelData.v] || "—"
                            color: root.cText
                            font.pixelSize: 11
                            font.family: root.fitFont(galaxyStructPanel.g[modelData.v] || "—")
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; height: 1
                    color: Qt.rgba(1, 1, 1, 0.07)
                }

                // 视角提示
                Text {
                    Layout.fillWidth: true
                    text: "本视图为 1:1 真实比例：银盘直径 10.6 万光年，薄盘厚度仅 1000 光年，相差 100 多倍。"
                    color: Qt.rgba(1.0, 0.80, 0.35, 0.95)
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }
            }
        }
    }

    // ========================================================================
    //  宇宙尺度 · 结构与尺度面板
    //
    //  ★ 这里的核心教学任务是**建立尺度感**。宇宙视图的距离用对数映射,
    //    所以面板必须给出真实数字, 否则学生会以为"远处的星系挨得更近"。
    //    结构层级表 (行星系 → 宇宙网) 是逐级放大的锚点。
    // ========================================================================
    GlassPanel {
        id: cosmosStructPanel
        x: 18; y: 74
        width: 288
        height: parent.height - 74 - 46
        visible: scene.scaleLevel === 2

        // ★ 用属性初始化式绑定, 而不是 Component.onCompleted 赋值。
        //   onCompleted 在**首次绑定求值之后**才跑, 于是 Repeater 的
        //   model 数组首次求值时数据还是空的, 会刷一屏
        //   "Unable to assign [undefined] to QString" 警告。
        //   写成 property var x: scene.xxx() 就没有这个时间差。
        property var info: scene.cosmosInfo()
        property var structs: scene.cosmosStructures()

        // ★ 性能开关的反馈数据。必须显式引用 scene.cosmosVisible 来建立
        //   依赖 —— scene.cosmosPerf() 是普通函数调用, 不会自动触发绑定
        //   更新; 少了这一句, 拖动档位后数字不刷新。
        // ★ 依赖 scene.cosmosVisible: C++ 侧在粒子总数就绪时会发
        //   cosmosVisibleChanged 信号 (见 SolarScene::onTick), 触发重算。
        //   这样启动时序与档位切换都能正确刷新。
        // ★ 性能数据直接绑定 scene 的 Q_PROPERTY。
        //   这四个属性由 C++ 的 onTick 持续更新并 emit 通知,
        //   QML 只需绑定 —— 不需要任何 Timer 或函数调用。
        //   (之前试过函数返回值 / var 对象 / QML Timer, 都不可靠;
        //    见 sceneitem.h 里 cosmosTotal 处的教训记录)
        Flickable {
            anchors.fill: parent
            anchors.margins: 14
            contentHeight: cosCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: cosCol
                width: parent.width - 4
                spacing: 9

                // ---- 星系数量档位 (性能开关) ----
                //
                // ★ 为什么做成开关: 场景由 onTick 的 16ms 定时器驱动,
                //   帧率上限锁在 62.5 FPS, 所以帧率数字看不出 GPU 余量。
                //   实测本机的边际成本约 18.8 us / 千粒子 (填充率主导):
                //     10 万 -> 2.4 ms     100 万 -> 19 ms
                //     260 万 -> 49 ms     480 万 -> 90 ms
                //   (260 万即 SDSS 全部星系样本的规模)
                //   用户按需在"结构完整"与"流畅"之间取舍。
                //
                // ★ 切换是**零成本**的: 顶点数据一次上传后不再变动,
                //   只改 glDrawArrays 的 count。不重传、不重建 VAO。
                //
                // ★ 低档位不是"随机丢一半": 数据按重要性顺序生成
                //   (纤维 → 空洞边缘 → 背景填充), 截断天然保留宇宙网骨架。
                Text {
                    text: "星系数量"
                    color: root.cTextDim
                    font.pixelSize: 10
                    font.letterSpacing: 1
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Repeater {
                        model: [
                            { t: "10%",  p: 0.10 },
                            { t: "25%",  p: 0.25 },
                            { t: "50%",  p: 0.50 },
                            { t: "全部", p: 1.00 }
                        ]

                        Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            radius: 6

                            readonly property int targetN:
                                Math.round(scene.cosmosTotal
                                           * modelData.p)
                            readonly property bool active:
                                modelData.p >= 1.0
                                    ? scene.cosmosVisible <= 0
                                    : scene.cosmosVisible === targetN

                            color: active ? root.cAccentSoft
                                          : Qt.rgba(1, 1, 1, 0.05)
                            border.width: 1
                            border.color: active
                                          ? Qt.rgba(0.37, 0.66, 1.0, 0.55)
                                          : Qt.rgba(1, 1, 1, 0.08)

                            Text {
                                anchors.centerIn: parent
                                text: modelData.t
                                color: parent.active ? "#bcd9ff" : root.cTextDim
                                font.pixelSize: 10
                                font.family: root.sansFont
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: scene.cosmosVisible =
                                    (modelData.p >= 1.0) ? 0 : targetN
                            }
                        }
                    }
                }

                // 实时反馈: 当前粒数 + 预估耗时。颜色随帧率预警。
                Text {
                    Layout.fillWidth: true
                    // ★ scene.cosmosVisible <= 0 表示"全部", 此时实际显示数
                    //   就是总数 —— 不能直接打印 0
                    text: "显示 "
                          + (scene.cosmosVisible <= 0
                             ? scene.cosmosTotal.toLocaleString()
                             : scene.cosmosVisible.toLocaleString())
                          + " / " + scene.cosmosTotal.toLocaleString()
                    color: root.cTextDim
                    font.pixelSize: 10
                    font.family: root.sansFont
                }
                Text {
                    Layout.fillWidth: true
                    text: "预估 " + scene.cosmosEstMs.toFixed(1) + " ms"
                          + "  ·  " + Math.round(scene.cosmosEstFps) + " FPS"
                    color: scene.cosmosEstFps > 55 ? "#8fd88f"
                         : scene.cosmosEstFps > 30 ? "#e8cc7a"
                         : "#e89090"
                    font.pixelSize: 10
                    font.family: root.monoFont
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Qt.rgba(1, 1, 1, 0.07)
                }

                Text {
                    text: "宇宙学参数"
                    color: root.cTextDim
                    font.pixelSize: 10
                    font.letterSpacing: 1
                }

                Repeater {
                    model: [
                        { k: "宇宙年龄",     v: cosmosStructPanel.info.age },
                        { k: "哈勃常数",     v: cosmosStructPanel.info.h0 },
                        { k: "暗能量占比",   v: cosmosStructPanel.info.omegaLambda, hot: true },
                        { k: "物质总占比",   v: cosmosStructPanel.info.omegaM },
                        { k: "重子物质占比", v: cosmosStructPanel.info.omegaB },
                        { k: "CMB 温度",     v: cosmosStructPanel.info.cmb },
                        { k: "CMB 红移",     v: cosmosStructPanel.info.cmbZ },
                        { k: "复合时期",     v: cosmosStructPanel.info.recombT },
                        { k: "可观测半径",   v: cosmosStructPanel.info.obsRadius },
                        { k: "可观测直径",   v: cosmosStructPanel.info.obsDia },
                        { k: "星系总数",     v: cosmosStructPanel.info.galaxies }
                    ]

                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            text: modelData.k
                            color: root.cTextDim
                            font.pixelSize: 10
                        }
                        Text {
                            text: modelData.v ? String(modelData.v) : "—"
                            color: modelData.hot ? "#ffb4a2" : root.cText
                            font.pixelSize: 10
                            font.family: root.fitFont(modelData.v ? String(modelData.v) : "—")
                        }
                    }
                }

                // ---- 结构层级 ----
                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    height: 1
                    color: Qt.rgba(0.35, 0.42, 0.55, 0.35)
                }

                Text {
                    text: "结构层级"
                    color: root.cTextDim
                    font.pixelSize: 10
                    font.letterSpacing: 1
                }

                Repeater {
                    model: cosmosStructPanel.info.hierarchy

                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            text: modelData.lvl
                            color: root.cText
                            font.pixelSize: 10
                        }
                        Text {
                            text: modelData.size
                            color: root.cTextDim
                            font.pixelSize: 10
                            font.family: root.fitFont(modelData.size)
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    height: 1
                    color: Qt.rgba(0.35, 0.42, 0.55, 0.35)
                }

                Text {
                    text: "大尺度结构"
                    color: root.cTextDim
                    font.pixelSize: 10
                    font.letterSpacing: 1
                }

                Repeater {
                    model: cosmosStructPanel.structs

                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 1

                        Text {
                            text: modelData.name
                            color: modelData.kind === 2 ? "#a8bcd8"
                                 : modelData.kind === 0 ? "#d4a5ff"
                                 : "#ffb4a2"
                            font.pixelSize: 10
                            font.bold: true
                        }
                        Text {
                            text: modelData.dist + "  ·  " + modelData.size
                            color: root.cTextDim
                            font.pixelSize: 9
                            font.family: root.fitFont(modelData.dist + "  ·  " + modelData.size)
                        }
                    }
                }

                // ---- 对数映射说明 (必须明说, 否则学生会误读) ----
                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    Layout.preferredHeight: noteTxt.implicitHeight + 16
                    radius: 5
                    color: Qt.rgba(0.55, 0.38, 0.15, 0.22)
                    border.width: 1
                    border.color: Qt.rgba(1.0, 0.72, 0.35, 0.35)

                    Text {
                        id: noteTxt
                        anchors.fill: parent
                        anchors.margins: 8
                        text: "★ 距离为对数映射：远处间隔被压缩，" +
                              "标注中的数字才是真实距离。"
                        color: Qt.rgba(1.0, 0.82, 0.50, 0.95)
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        lineHeight: 1.3
                    }
                }
            }
        }
    }

    // 右侧: 宇宙教学要点
    GlassPanel {
        id: cosmosNotePanel
        width: 330
        height: cosNoteCol.implicitHeight + 28
        x: parent.width - width - 18
        y: parent.height - height - 46
        visible: scene.scaleLevel === 2

        property var notes: scene.cosmosNotes()

        ColumnLayout {
            id: cosNoteCol
            anchors.fill: parent
            anchors.margins: 14
            spacing: 7

            RowLayout {
                spacing: 8
                Rectangle {
                    width: 9; height: 9; radius: 4.5
                    color: "#ffb4a2"
                }
                Text {
                    text: "宇宙大尺度结构"
                    color: root.cText
                    font.pixelSize: 13
                    font.bold: true
                }
                Text {
                    text: "Large-Scale Structure"
                    color: root.cTextDim
                    font.pixelSize: 10
                }
            }

            Repeater {
                model: cosmosNotePanel.notes

                delegate: Text {
                    required property string modelData
                    Layout.fillWidth: true
                    text: "· " + modelData
                    color: root.cTextDim
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                }
            }
        }
    }

    // 右侧: 教学要点
    GlassPanel {
        id: galaxyNotePanel
        width: 330
        height: noteCol.implicitHeight + 28
        x: parent.width - width - 18
        y: parent.height - height - 46
        visible: scene.scaleLevel === 1

        property var notes: []

        Component.onCompleted: notes = scene.galaxyNotes()

        ColumnLayout {
            id: noteCol
            anchors.fill: parent
            anchors.margins: 14
            spacing: 9

            RowLayout {
                spacing: 9
                Rectangle {
                    width: 16; height: 16; radius: 8
                    color: "#c9a94e"
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.3)
                }
                ColumnLayout {
                    spacing: 0
                    Text {
                        text: "银河系"
                        color: root.cText
                        font.pixelSize: 16
                        font.bold: true
                    }
                    Text {
                        text: "Milky Way · 棒旋星系 (SBbc)"
                        color: root.cTextDim
                        font.pixelSize: 10
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true; height: 1
                color: Qt.rgba(1, 1, 1, 0.08)
            }

            Repeater {
                model: galaxyNotePanel.notes

                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 7

                    Text {
                        text: "•"
                        color: root.cAccent
                        font.pixelSize: 12
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: root.cTextDim
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        lineHeight: 1.35
                    }
                }
            }
        }
    }

    // ========================================================================
    //  银河系 · 太阳位置标注
    //
    //  画在 QML 叠加层而不是 GL 里, 因为 OpenGL core profile 的线宽上限
    //  通常只有 1px —— 细线画在 12 万粒子的星场上完全看不见 (实测如此)。
    //  QML 这边可以画粗描边 + 文字标签, 教学效果也好得多。
    // ========================================================================
    Item {
        id: galaxySunMarker
        visible: scene.scaleLevel === 1 && scene.sunMarkOn
        // 位置由 C++ 投影得到 (归一化 0..1)
        x: scene.sunMarkX * root.width
        y: scene.sunMarkY * root.height

        // 淡入淡出, 避免出现/消失时突兀
        opacity: visible ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 180 } }

        // 十字准星: 4 条短粗线, 中心留出圆环的位置
        Repeater {
            model: [
                { dx: 0,  dy: -1, len: 26 },   // 上
                { dx: 0,  dy:  1, len: 26 },   // 下
                { dx: -1, dy:  0, len: 26 },   // 左
                { dx: 1,  dy:  0, len: 26 }    // 右
            ]

            Rectangle {
                required property var modelData
                width:  modelData.dy === 0 ? modelData.len : 2
                height: modelData.dy === 0 ? 2 : modelData.len
                x: modelData.dx < 0 ? -modelData.len - 13
                   : (modelData.dx > 0 ? 13 : -1)
                y: modelData.dy < 0 ? -modelData.len - 13
                   : (modelData.dy > 0 ? 13 : -1)
                color: "#ffdc5e"
                // 深色描边让标记在亮旋臂上也能读出来
                border.width: 1
                border.color: Qt.rgba(0, 0, 0, 0.85)
            }
        }

        // 中心圆环
        Rectangle {
            width: 22; height: 22; radius: 11
            x: -11; y: -11
            color: "transparent"
            border.width: 2
            border.color: "#ffdc5e"
            // 外圈再加一层深色, 保证在亮背景上依然清晰
            Rectangle {
                anchors.centerIn: parent
                width: 26; height: 26; radius: 13
                color: "transparent"
                border.width: 1
                border.color: Qt.rgba(0, 0, 0, 0.75)
                z: -1
            }
        }

        // 文字标签
        Rectangle {
            x: 20; y: -34
            width: labelText.implicitWidth + 16
            height: 22
            radius: 6
            color: Qt.rgba(0.06, 0.06, 0.09, 0.88)
            border.width: 1
            border.color: Qt.rgba(1.0, 0.86, 0.37, 0.55)

            Text {
                id: labelText
                anchors.centerIn: parent
                text: "太阳系 · 距银心 2.6 万光年"
                color: "#ffdc5e"
                font.pixelSize: 11
            }
        }
    }

    // ========================================================================
    //  银河系 · 旋臂 / 银心 / 猎户支 标注
    //
    //  ★ 这些标注是银河系视图的教学价值所在。没有它们, 学生看到的只是
    //    "一团有旋臂的粒子", 不知道哪条是英仙臂、太阳在哪条臂上。
    //
    //  ★ "猎户支" 特意用不同样式 (虚框/青色) 与主旋臂区分 ——
    //    太阳所在的猎户支严格说不是主旋臂, 而是一条次级结构。
    //    把它标成主旋臂是常见的科普错误。
    // ========================================================================
    Item {
        id: galaxyLabelLayer
        anchors.fill: parent
        // ★ 银河系 (1) 与宇宙 (2) 都要显示标注层。
        //   初版只写了 === 1, 于是宇宙视图里所有标注都不出现 ——
        //   而画面本身是正常的, 很容易误判成"投影算错了"。
        visible: scene.scaleLevel >= 1
        z: 5

        Repeater {
            model: scene.galaxyLabels

            delegate: Item {
                required property var modelData
                x: modelData.x * root.width
                y: modelData.y * root.height
                visible: x > 30 && x < root.width - 30
                         && y > 30 && y < root.height - 30

                // 标注锚点小十字
                Rectangle {
                    width: 5; height: 5; radius: 2.5
                    x: -2.5; y: -2.5
                    color: modelData.kind === "core" ? "#ffd166"
                         : modelData.kind === "spur" ? "#7fd8e8"
                         : modelData.kind === "supercluster" ? "#ffb4a2"
                         : modelData.kind === "void" ? "#8fa8c8"
                         : modelData.kind === "wall" ? "#d4a5ff"
                         : "#c9d4e4"
                    border.width: 1
                    border.color: Qt.rgba(0, 0, 0, 0.8)
                }

                Column {
                    x: 9
                    y: -9
                    spacing: 1

                    Rectangle {
                        width: labelRow.width + 12
                        height: 18
                        radius: 4
                        color: Qt.rgba(0.05, 0.06, 0.09, 0.82)
                        border.width: 1
                        border.color: modelData.kind === "core"
                                      ? Qt.rgba(1.0, 0.82, 0.40, 0.75)
                                      : modelData.kind === "spur"
                                        ? Qt.rgba(0.50, 0.85, 0.91, 0.75)
                                        : Qt.rgba(0.72, 0.79, 0.88, 0.55)

                        Row {
                            id: labelRow
                            anchors.centerIn: parent
                            spacing: 5

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.text
                                color: modelData.kind === "core" ? "#ffd166"
                                     : modelData.kind === "spur" ? "#7fd8e8"
                                     : modelData.kind === "supercluster" ? "#ffb4a2"
                                     : modelData.kind === "void" ? "#a8bcd8"
                                     : modelData.kind === "wall" ? "#d4a5ff"
                                     : "#dce4f0"
                                font.pixelSize: 11
                                font.bold: modelData.kind === "core"
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                visible: modelData.sub !== ""
                                text: modelData.sub
                                color: Qt.rgba(0.62, 0.68, 0.78, 0.9)
                                font.pixelSize: 9
                            }
                        }
                    }
                }
            }
        }

        // ---- 光年比例尺 (底部居中) ----
        // 教学上很关键: 让"这个视野有多大"变成可读的数字, 而不是
        // 只能靠"看起来很大"来判断。
        Item {
            id: scaleBar
            // 阈值同样按尺度分派: 宇宙视图的数值天然大得多
            visible: scene.scaleLevel === 2
                     ? scene.galaxyViewWidthLy > 0.5
                     : scene.galaxyViewWidthLy > 100
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 52

            // 选一个"好看"的整数刻度: 把视野宽度折到 1/4 再取整
            readonly property real barLy: {
                var target = scene.galaxyViewWidthLy * 0.25;
                var mag = Math.pow(10, Math.floor(Math.log(target) / Math.LN10));
                var n = target / mag;
                if (n >= 5) n = 5; else if (n >= 2) n = 2; else n = 1;
                return n * mag;
            }
            readonly property real barPx:
                scene.galaxyViewWidthLy > 0
                ? barLy / scene.galaxyViewWidthLy * root.width : 0

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: barRect.top
                anchors.bottomMargin: 4
                text: {
                    var ly = scaleBar.barLy;
                    // ★★ 单位必须按尺度分派 —— 两套差距 6 个数量级:
                    //   银河系视图: galaxyViewWidthLy 单位是**光年**
                    //   宇宙视图:   galaxyViewWidthLy 单位是**百万光年**
                    //   (后者由 C++ 的 Cosmos::distToScene 对数映射反推得到)
                    //
                    //   初版把宇宙尺度也当成光年处理, 于是 200 亿光年被显示成
                    //   "20000 光年" —— 差了 6 个数量级, 这是致命的读数错误。
                    if (scene.scaleLevel === 2) {
                        // ly 的单位是 Mly (百万光年)
                        // 1 Mly = 100 万光年 = 0.01 亿光年
                        if (ly >= 100) return (ly / 100).toFixed(0) + " 亿光年";
                        return ly.toFixed(0) + " 百万光年";
                    }
                    if (ly >= 1000)
                        return (ly / 1000).toFixed(ly % 1000 === 0 ? 0 : 1) + " 千光年";
                    return ly.toFixed(0) + " 光年";
                }
                color: Qt.rgba(0.78, 0.83, 0.90, 0.95)
                font.pixelSize: 11
            }

            Rectangle {
                id: barRect
                width: Math.max(20, scaleBar.barPx)
                height: 2
                color: Qt.rgba(0.78, 0.83, 0.90, 0.9)
                // 两端刻度竖线
                Rectangle {
                    width: 1; height: 7; y: -2.5
                    anchors.left: parent.left
                    color: Qt.rgba(0.78, 0.83, 0.90, 0.9)
                }
                Rectangle {
                    width: 1; height: 7; y: -2.5
                    anchors.right: parent.right
                    color: Qt.rgba(0.78, 0.83, 0.90, 0.9)
                }
            }
        }
    }

    // ========================================================================
    //  底栏
    // ========================================================================
    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 20
        anchors.bottomMargin: 16
        text: scene.scaleLevel === 2
              ? "拖动旋转 · 滚轮缩放 (距离为对数映射, 标注给出真实距离)"
              : scene.scaleLevel === 1
                ? "拖动旋转 · 右键拖动平移 · 滚轮缩放 (可拉远观察整个星系)"
                : "拖动旋转 · 右键拖动平移 · 滚轮缩放"
        color: Qt.rgba(0.45, 0.52, 0.62, 0.85)
        font.pixelSize: 10
    }
}
