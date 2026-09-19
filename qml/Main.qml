// ============================================================================
//  宇宙实验室 CosmosLab —— QML 界面
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
    title: "宇宙实验室 · CosmosLab (C++ / QML / OpenGL)"
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

    // ---- 波长 -> 近似颜色 ----
    //
    // ★ 放在根级: 红移工具的面板和星系详情卡**都要用**。
    //   各自复制一份必然漂移 (改了色相但改了一处)。
    //
    // 物理: 400~700nm 是可见光, 之外人眼看不见 —— 返回灰色。
    //   ★ 这个"变灰"的视觉断点本身就是教学内容: 谱线移出可见光时,
    //     它不是"变成另一种颜色", 而是**彻底不可见**。
    function wlToColor(w) {
        if (w < 400 || w > 700)
            return "#4a4d57"
        let rr = 0
        let gg = 0
        let bb = 0
        if (w < 440)      { rr = -(w - 440) / 60; gg = 0; bb = 1 }
        else if (w < 490) { rr = 0; gg = (w - 440) / 50; bb = 1 }
        else if (w < 510) { rr = 0; gg = 1; bb = -(w - 510) / 20 }
        else if (w < 580) { rr = (w - 510) / 70; gg = 1; bb = 0 }
        else if (w < 645) { rr = 1; gg = -(w - 645) / 65; bb = 0 }
        else              { rr = 1; gg = 0; bb = 0 }
        return Qt.rgba(Math.max(0, Math.min(1, rr)),
                       Math.max(0, Math.min(1, gg)),
                       Math.max(0, Math.min(1, bb)), 1)
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
    // ★ 恒星视图为 UI-only (不新增渲染尺度, 复用太阳系画布):
    //   打开时隐藏三尺度各自面板, 显示恒星列表面板。
    property bool  stellarView: false
    // ★ 全局专业/科普开关 (选项一): true=专业版 (默认), false=科普版。
    //   详情卡 + 演化阶段说明 + 教学要点全部跟随此开关。
    //   解说词 TTS 按需取 zh (专业) 或 zh_pop (科普)。
    property bool  proMode: true

    Component.onCompleted: {
        // 采纳 C++ 侧的初始焦点 —— 它可能被环境变量 SS_FOCUS 覆盖,
        // 若这里硬写 "earth" 就会把 C++ 的设定顶掉。
        currentId = scene.focusId
        bodies = scene.bodyList()
        current = scene.bodyInfo(scene.focusId)
        // 测试用: SS_POP=1 启动即科普版
        if (scene.testPop)
            proMode = false
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
        visible: !root.listHidden && scene.scaleLevel === 0 && !root.stellarView

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
        visible: scene.scaleLevel === 0 && root.currentId !== "overview" && !root.stellarView

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
        // 加宽以容纳尺度切换控件 + 专业/科普开关 (6×66+间距约 420px)
        width: Math.min(parent.width - 36, 910)
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
                    : scene.scaleLevel === 1 ? "银河系"
                    : "太阳系"
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

                // ★ 全局专业/科普切换: 与演化/恒星/尺度段并列, 文字按钮。
                Rectangle {
                    width: 66; height: 26; radius: 7
                    color: root.proMode
                           ? root.cAccentSoft : Qt.rgba(0.55, 0.45, 0.15, 0.25)
                    border.width: 1
                    border.color: root.proMode
                                  ? Qt.rgba(0.37, 0.66, 1.0, 0.55)
                                  : Qt.rgba(1.0, 0.72, 0.35, 0.55)
                    Text {
                        anchors.centerIn: parent
                        text: root.proMode ? "专业版" : "科普版"
                        color: root.proMode ? "#bcd9ff" : "#ffd98a"
                        font.pixelSize: 11
                        font.bold: true
                        font.family: root.sansFont
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.proMode = !root.proMode
                    }
                }

                Rectangle {
                    width: 66; height: 26; radius: 7
                    color: evoOverlay.visible
                           ? root.cAccentSoft : Qt.rgba(1, 1, 1, 0.05)
                    border.width: 1
                    border.color: evoOverlay.visible
                                  ? Qt.rgba(0.37, 0.66, 1.0, 0.55)
                                  : Qt.rgba(1, 1, 1, 0.08)
                    Text {
                        anchors.centerIn: parent
                        text: "演化"
                        color: evoOverlay.visible ? "#bcd9ff" : root.cTextDim
                        font.pixelSize: 11
                        font.bold: evoOverlay.visible
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: evoOverlay.visible = !evoOverlay.visible
                    }
                }

                // ★ 恒星段为 UI-only 视图: 不碰 scaleLevel 状态机,
                //   只切换面板显隐。渲染仍走当前尺度画布。
                Rectangle {
                    width: 66; height: 26; radius: 7
                    color: root.stellarView
                           ? root.cAccentSoft : Qt.rgba(1, 1, 1, 0.05)
                    border.width: 1
                    border.color: root.stellarView
                                  ? Qt.rgba(0.37, 0.66, 1.0, 0.55)
                                  : Qt.rgba(1, 1, 1, 0.08)
                    Text {
                        anchors.centerIn: parent
                        text: "恒星"
                        color: root.stellarView ? "#bcd9ff" : root.cTextDim
                        font.pixelSize: 11
                        font.bold: root.stellarView
                        font.family: root.sansFont
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.stellarView = !root.stellarView
                    }
                }

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
        visible: scene.scaleLevel === 1 && !root.stellarView

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
        visible: scene.scaleLevel === 2 && !root.stellarView

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

                // ============================================================
                //  结构列表 (点击查看详情)
                // ============================================================
                //
                // ★★ 为什么必须有这个列表:
                //   详情卡组件 (galaxyCard) 早就写好了, 数据也一直是齐的
                //   (scene.cosmosStructures 返回 id/name/dist/size/kind/desc),
                //   但**从来没有任何 UI 用它** —— 结果是用户根本点不开卡片,
                //   只有测试入口 SS_CARD 能打开。这里把它接上。
                //
                // ★ 三个层级合并成一个列表 (本星系群 → 室女座团 → 大尺度结构),
                //   因为它们的顺序本身就是"由近及远"的叙事。
                //
                // ★ 位置在面板**最前面** —— 一开始放在面板底部, 结果要滚动
                //   才能看到, 可发现性太差 (详情卡做出来了却没人点得到)。
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Text {
                        text: "结构列表"
                        color: root.cText
                        font.pixelSize: 10
                        font.family: root.sansFont
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "点击查看详情"
                        color: root.cTextDim
                        font.pixelSize: 9
                        font.family: root.sansFont
                    }

                    // ---- 哈勃图入口 ----
                    // ★ 放在**可见处** (结构列表标题行), 不要塞到面板底部 ——
                    //   之前结构列表就因为放太靠下而要滚动才能看到, 教训在这。
                    Rectangle {
                        Layout.preferredWidth: 58
                        Layout.preferredHeight: 19
                        radius: 4
                        color: hubBtnMa.containsMouse
                               ? Qt.rgba(0.37, 0.66, 1.0, 0.32)
                               : Qt.rgba(0.37, 0.66, 1.0, 0.14)
                        border.width: 1
                        border.color: hubBtnMa.containsMouse
                                      ? Qt.rgba(0.55, 0.78, 1.0, 0.85)
                                      : Qt.rgba(0.45, 0.62, 0.88, 0.42)
                        Text {
                            anchors.centerIn: parent
                            text: "哈勃图"
                            color: hubBtnMa.containsMouse ? "#dceaff"
                                                          : "#9fc4f0"
                            font.pixelSize: 9
                            font.family: root.sansFont
                        }
                        MouseArea {
                            id: hubBtnMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: hubbleOverlay.visible = true
                        }
                    }
                }

                Repeater {
                    model: cosmosStructPanel.structs

                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        radius: 5
                        color: clickArea.containsMouse
                               ? Qt.rgba(0.37, 0.66, 1.0, 0.18)
                               : Qt.rgba(1, 1, 1, 0.035)
                        border.width: 1
                        border.color: clickArea.containsMouse
                                      ? Qt.rgba(0.45, 0.72, 1.0, 0.55)
                                      : Qt.rgba(1, 1, 1, 0.08)

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            // 有实景图的标一个小圆点 —— 一眼能看出
                            // 哪些点开后能看到真实照片
                            Rectangle {
                                visible: modelData.hasPhoto === true
                                width: 5; height: 5; radius: 2.5
                                color: "#7ac6ff"
                            }

                            Text {
                                text: modelData.name
                                color: root.cText
                                font.pixelSize: 10
                                font.family: root.sansFont
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: modelData.dist
                                color: root.cTextDim
                                font.pixelSize: 9
                                font.family: root.fitFont(modelData.dist)
                            }
                        }

                        MouseArea {
                            id: clickArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                galaxyCard.detail =
                                    root.cardForStruct(modelData)
                                galaxyCard.visible = true
                            }
                        }
                    }
                }

                // 列表与下方"性能开关"之间的分隔
                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 3
                    Layout.preferredHeight: 1
                    color: Qt.rgba(1, 1, 1, 0.07)
                }

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
                    // ★ 显示**实际绘制数** (scene.cosmosDrawn) 而不是
                    //   cosmosTotal —— 后者是缓冲总数, 不含"SDSS 开关"
                    //   和"星系数量"档位的削减, 直接打印会高估。
                    //   （旧版用 cosmosVisible<=0 判断, 但"全部"档时
                    //     cosmosVisible 就是 0, 无法表达 SDSS 的削减。）
                    text: "显示 " + scene.cosmosDrawn.toLocaleString()
                          + " / " + scene.cosmosTotal.toLocaleString()
                    color: root.cTextDim
                    font.pixelSize: 10
                    font.family: root.sansFont
                }
                Text {
                    Layout.fillWidth: true
                    // ★ 措辞刻意写"宇宙图层"而不是"帧率":
                    //   这个数字只是宇宙粒子图层的耗时预估,
                    //   整帧还包含约 10 个全屏 pass 的后处理链 ——
                    //   后者在 2880x1800 下才是主要瓶颈。
                    //   写成"预估帧率"会让学生以为这就是整机帧率。
                    text: "宇宙图层 约 " + scene.cosmosEstMs.toFixed(1) + " ms"
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


                // ---- SDSS 真实星系 (独立开关) ----
                //
                // ★ 为什么与"星系数量"分开:
                //   上面那个档位控制的是**程序生成的示意结构**,
                //   这里控制的是 **SDSS 巡天实测的星系位置**。
                //   两者性质不同, 观察意图也不同, 故分开控制。
                //
                // ★ 默认 50%: 实测 171,398 个 SDSS 星系在 2880x1800 下
                //   会让总粒子达 242,604, 帧率掉到约 12 FPS ——
                //   原因是星系**成团**(大量点落在同一像素, 过度绘制)。
                //   半量既能看清真实结构, 又能保持流畅。
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    text: "SDSS 实测星系"
                    color: root.cTextDim
                    font.pixelSize: 10
                    font.letterSpacing: 1
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Repeater {
                        model: [
                            { t: "关闭", p: 0.0 },
                            { t: "25%",  p: 0.25 },
                            { t: "50%",  p: 0.50 },
                            { t: "全部", p: 1.0 }
                        ]

                        Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            radius: 6

                            readonly property int targetN:
                                Math.round(scene.sdssTotal * modelData.p)
                            readonly property bool active:
                                modelData.p <= 0.0
                                    ? scene.sdssVisible <= 0
                                    : (modelData.p >= 1.0
                                       ? scene.sdssVisible >= scene.sdssTotal
                                       : scene.sdssVisible === targetN)

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
                                // ★ "关闭"用 -1 表示 (0 在 C++ 侧表示"全部")
                                onClicked: scene.sdssVisible =
                                    (modelData.p <= 0.0) ? -1
                                    : (modelData.p >= 1.0 ? scene.sdssTotal
                                                          : targetN)
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: {
                        if (scene.sdssTotal <= 0)
                            return "未载入 SDSS 星表 (assets/lss/lrg.bin)"
                        const v = scene.sdssVisible <= 0
                                  ? scene.sdssTotal : scene.sdssVisible
                        return "实测 " + v.toLocaleString() + " / "
                               + scene.sdssTotal.toLocaleString() + " 个星系"
                               + "　红移 0.60–1.00"
                    }
                    color: scene.sdssTotal > 0 ? "#7fd4a0" : "#c8a878"
                    font.pixelSize: 9
                    font.family: root.sansFont
                }

                // ★ 必须说明数据来源与覆盖范围, 否则会被误读为"全部星系"
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    visible: scene.sdssTotal > 0
                    text: "数据：SDSS DR17 eBOSS LRG 样本（实测光谱红移）。"
                          + "覆盖距离 74–111 亿光年 —— 这是巡天的观测窗口，"
                          + "不是全天完整样本。该区间之外的粒子为示意结构。"
                    color: Qt.rgba(0.56, 0.64, 0.75, 0.75)
                    font.pixelSize: 9
                    font.family: root.sansFont
                    lineHeight: 1.35
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Qt.rgba(1, 1, 1, 0.07)
                }

                // ---- 距离映射模式 (对数压缩 / 真实比例) ----
                //
                // ★ 为什么需要这个开关:
                //   宇宙要展示 0.2 Mly ~ 46.5 Gly 共六个数量级。
                //   对数压缩能一屏装下, 但远处间隔被压扁;
                //   真实比例下本星系群只有 5e-5 的占比, 屏幕上不可见 ——
                //   这在物理上是**正确**的 (它确实那么小)。
                //   把两者做成开关, 是为了让"塞进一屏付出了什么"可见。
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    text: "距离映射"
                    color: root.cTextDim
                    font.pixelSize: 10
                    font.letterSpacing: 1
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Repeater {
                        model: [
                            { t: "对数压缩", v: 0 },
                            { t: "真实比例", v: 1 }
                        ]

                        Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            radius: 6
                            readonly property bool active:
                                scene.cosmosMapMode === modelData.v
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
                                onClicked: scene.cosmosMapMode = modelData.v
                            }
                        }
                    }
                }

                // ★ 切换后的说明 —— 没有这段用户会以为程序坏了
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: scene.cosmosMapMode === 1
                          ? "真实比例：距离成比例。此时本星系群仅占画面 "
                            + "0.005%，屏幕上缩为一点 —— 这是物理事实。"
                            + "适合配合缩放观察某一区域。"
                          : "对数压缩：一屏容纳 0.2 百万光年 ~ 465 亿光年。"
                            + "代价是远处间隔被压缩，标签上的数字才是真实距离。"
                    color: scene.cosmosMapMode === 1 ? "#e8cc7a" : root.cTextDim
                    font.pixelSize: 9
                    font.family: root.sansFont
                    lineHeight: 1.35
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Qt.rgba(1, 1, 1, 0.07)
                }

                // ============================================================
                //  红移 —— 可交互演示
                //
                //  ★ 为什么不放静态图:
                //    拖动滑块看谱线实时移动, 比静态图直观得多,
                //    而且这个交互本身就在演示"红移"的定义。
                //
                //  ★★ 属性名避坑 (实测踩到, 花了十几轮才定位):
                //    初版写的是 `property real z: 0.738` ——
                //    但 **z 是 QML Item 的内建属性** (控制层叠顺序)。
                //    覆盖内建属性会让引擎在布局时崩溃, 且**日志完全为空**
                //    (QML 引擎初始化期的致命错误不走 qWarning),
                //    而 QML 编译期**不会**对属性名冲突报错。
                //    故本工具里所有相关变量都加 red 前缀。
                // ============================================================
                Item {
                    id: redshiftTool
                    Layout.fillWidth: true
                    // 高度预算 (从 0 起算):
                    //     0~20   标题 + 滑块
                    //    42~70  波长色带
                    //    46~66  谱线标记
                    //    68~107 标签文字 (最多错 3 行)
                    //   112      结论行
                    //   134~156  说明文字 (可能折行)
                    Layout.preferredHeight: 162

                    // 默认 redz = SDSS LRG 样本的中位红移 0.738
                    property real redz: 0.738

                    readonly property var lines: [
                        { n: "CaK", w: 393.4 },
                        { n: "CaH", w: 396.8 },
                        { n: "Hd",  w: 410.2 },
                        { n: "Hg",  w: 434.0 },
                        { n: "Hb",  w: 486.1 },
                        { n: "Mg",  w: 517.5 },
                        { n: "Na",  w: 589.0 },
                        { n: "Ha",  w: 656.3 }
                    ]
                    readonly property real wlMin: 380
                    readonly property real wlMax: 1350

                    readonly property int visCount: {
                        let c = 0
                        for (let i = 0; i < lines.length; ++i) {
                            if (lines[i].w * (1 + redz) <= 700) ++c
                        }
                        return c
                    }

                    // ---- 标签避让布局 ----
                    //
                    // ★ 为什么需要: 所有谱线标记都画在同一条色带上, 而高红移时
                    //   短波端的几条线 (CaK 393nm / CaH 397nm / Hd 410nm)
                    //   会挤到相近的像素列 —— 实测 z=0.738 时 CaK 与 CaH
                    //   只差 3 个像素, 标签文字直接叠在一起不可读。
                    //
                    // 做法: 按像素位置从左到右扫描, 给每条线找一个"放得下"的
                    //   行 (同一行内与前一标签至少隔 20px), 放不下就往下错一行,
                    //   最多 3 行。
                    function labelItems() {
                        const out = []
                        const W = spectrum.width
                        const lo = wlMin
                        const hi = wlMax
                        if (W <= 0)
                            return out
                        // 三行的"已占用右边界"。间距取 14px ——
                        // 字号 8 的文字实际高约 11px, 留 3px 间隙才不会叠。
                        const rowEnd = [-1e9, -1e9, -1e9]
                        for (let i = 0; i < lines.length; ++i) {
                            const sh = lines[i].w * (1 + redz)
                            if (sh < lo || sh > hi)
                                continue                 // 已移出显示范围
                            const px = W * (sh - lo) / (hi - lo)
                            let row = 0
                            while (row < 2 && px - rowEnd[row] < 22)
                                ++row
                            rowEnd[row] = px
                            out.push({ n: lines[i].n, px: px,
                                       dy: row * 14, vis: sh <= 700 })
                        }
                        return out
                    }
                    readonly property var labelLayout: labelItems()

                    // 波长 -> 近似颜色。
                    // ★ 实现在根级 (root.wlToColor) —— 星系详情卡也要用,
                    //   这里只做转发, 避免两处各写一份导致色相漂移。
                    function wlColor(w) {
                        return root.wlToColor(w)
                    }

                    Text {
                        x: 0
                        y: 0
                        text: "红移 z = " + redshiftTool.redz.toFixed(3)
                        color: root.cTextDim
                        font.pixelSize: 10
                        font.letterSpacing: 1
                    }

                    // ---- 滑块 ----
                    Item {
                        id: zSlider
                        x: 0
                        y: 20
                        width: redshiftTool.width
                        height: 16

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: zSlider.width
                            height: 3
                            radius: 2
                            color: Qt.rgba(1, 1, 1, 0.10)
                        }
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: zSlider.width
                                   * Math.min(1, redshiftTool.redz / 1.2)
                            height: 3
                            radius: 2
                            color: Qt.rgba(0.37, 0.66, 1.0, 0.55)
                        }
                        Rectangle {
                            x: zSlider.width
                               * Math.min(1, redshiftTool.redz / 1.2) - 6
                            anchors.verticalCenter: parent.verticalCenter
                            width: 12
                            height: 12
                            radius: 6
                            color: "#bcd9ff"
                            border.width: 1
                            border.color: "#6ba8ff"
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            property real lastX: 0
                            function applyAt(mx) {
                                redshiftTool.redz = Math.max(0.0,
                                    Math.min(1.2, mx / zSlider.width * 1.2))
                            }
                            onPressed: applyAt(mouseX)
                            onPositionChanged: {
                                if (pressed)
                                    applyAt(mouseX)
                            }
                        }
                    }

                    // ---- 波长色带 ----
                    Rectangle {
                        id: spectrum
                        x: 0
                        y: 46
                        width: redshiftTool.width
                        height: 20
                        radius: 3
                        gradient: Gradient {
                            GradientStop { position: 0.00
                                color: redshiftTool.wlColor(380) }
                            GradientStop { position: 0.12
                                color: redshiftTool.wlColor(450) }
                            GradientStop { position: 0.28
                                color: redshiftTool.wlColor(520) }
                            GradientStop { position: 0.45
                                color: redshiftTool.wlColor(620) }
                            GradientStop { position: 0.55
                                color: redshiftTool.wlColor(700) }
                            GradientStop { position: 1.00
                                color: "#3a3d46" }
                        }
                    }

                    // 可见光边界 (700nm)
                    Rectangle {
                        x: spectrum.width
                           * (700 - redshiftTool.wlMin)
                           / (redshiftTool.wlMax - redshiftTool.wlMin)
                        y: 42
                        width: 1
                        height: 28
                        color: Qt.rgba(1, 1, 1, 0.45)
                    }

                    // ---- 谱线标记 ----
                    //
                    // ★ delegate 内不要写 parent.xxx —— Repeater 的 delegate
                    //   运行时会被包进内部节点, parent 指向的不是 delegate 自身。
                    //   这里用 required property modelData + 显式 id。
                    // ★ 位置与避让都由 labelItems() 预先算好 (见那里的说明),
                    //   delegate 只负责画 —— 避免在 delegate 里做跨项碰撞检测。
                    Repeater {
                        model: redshiftTool.labelLayout

                        Item {
                            required property var modelData
                            // ★ 线固定在色带内 (y=46, 高 20 -> 底 66, 色带底 70),
                            //   **只有标签文字**错行。若连标记线一起错行,
                            //   下面的线会跑出色带, 看着像 bug。
                            x: modelData.px - 1
                            y: 46

                            Rectangle {
                                width: 2.5
                                height: 20
                                color: modelData.vis ? "#ffffff" : "#8a8d96"
                            }

                            Text {
                                // 居中于线。x 用 -width/2 自适应文本宽度
                                // (原来写死 width:40 + x:-9 是偏离的)。
                                // 左边界保护: 最左侧的线 (低红移时的 CaK)
                                // 标签不许越过色带左缘。
                                x: Math.max(-width / 2, -modelData.px + 2)
                                y: 22 + modelData.dy
                                text: modelData.n
                                color: modelData.vis ? "#c8d4e8" : "#7a7d86"
                                font.pixelSize: 8
                                font.family: root.sansFont
                            }
                        }
                    }

                    // ---- 结论行 ----
                    //
                    // ★ y 的取值: 标签最多错到第 3 行 (dy=28), 文字绝对底边
                    //   约 68+28+11 = 107。所以结论行必须 >= 110, 否则会被压住。
                    Text {
                        x: 0
                        y: 112
                        text: "可见光内 " + redshiftTool.visCount + " / 8 条谱线"
                              + (redshiftTool.visCount === 0
                                 ? "  — 全部移入红外" : "")
                        color: redshiftTool.visCount >= 5 ? "#8fd88f"
                             : redshiftTool.visCount >= 2 ? "#e8cc7a"
                             : "#e89090"
                        font.pixelSize: 10
                        font.family: root.sansFont
                    }

                    Text {
                        x: 0
                        y: 134
                        width: redshiftTool.width
                        wrapMode: Text.WordWrap
                        text: "拖动滑块：谱线整体向长波移动 —— 这就是红移的定义。"
                              + " z=0.738 时 Hα 从 656nm 移到 1141nm。"
                        color: root.cTextDim
                        font.pixelSize: 9
                        font.family: root.sansFont
                    }
                }

                // ★ 两个必须点明的误解
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "★ 宇宙学红移不是多普勒效应，而是空间膨胀 —— "
                          + "按 v = cz 算，z=1 就是达到光速，"
                          + "这在相对论里不可能。\n"
                          + "★ 星系看起来红多半因为它由老年恒星组成，"
                          + "红移只是叠加在上面的一层效应。"
                    color: Qt.rgba(0.82, 0.68, 0.50, 1.0)
                    font.pixelSize: 9
                    font.family: root.sansFont
                    lineHeight: 1.4
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Qt.rgba(1, 1, 1, 0.07)
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
        visible: scene.scaleLevel === 2 && !root.stellarView

        property var notes: scene.cosmosNotes()
        // ★ 跟随全局开关: 科普版用 cosmosNotesPop (一一对应)。
        property var notesPop: scene.cosmosNotesPop()

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
                // ★ 跟随全局开关, 与专业版一一对应
                model: root.proMode ? cosmosNotePanel.notes
                                    : cosmosNotePanel.notesPop

                delegate: Text {
                    required property string modelData
                    Layout.fillWidth: true
                    text: "· " + modelData
                    color: root.proMode ? root.cTextDim : "#ffd9a0"
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
        visible: scene.scaleLevel === 1 && !root.stellarView

        property var notes: []
        property var notesPop: []

        Component.onCompleted: {
            notes = scene.galaxyNotes()
            notesPop = scene.galaxyNotesPop()
        }

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
                // ★ 跟随全局开关, 与专业版一一对应
                model: root.proMode ? galaxyNotePanel.notes
                                    : galaxyNotePanel.notesPop

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
                        color: root.proMode ? root.cTextDim : "#ffd9a0"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        lineHeight: 1.35
                    }
                }
            }
        }
    }

    // ========================================================================
    //  恒星视图面板 (B.1/B.2) —— UI-only, 不新增渲染尺度
    //
    //  ★ 由 root.stellarView 控制显隐, 与 scaleLevel 正交。
    //    打开时三尺度各自面板已全部隐藏 (见各 visible 处的互斥条件),
    //    渲染画布沿用当前尺度 (教学上恒星列表不需要 3D 粒子场)。
    // ========================================================================
    StellarPanel {
        id: stellarPanel
        visible: root.stellarView
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
        visible: scene.scaleLevel === 1 && !root.stellarView && scene.sunMarkOn
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
        visible: scene.scaleLevel >= 1 && !root.stellarView
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
                        id: tagBg
                        width: labelRow.width + 12
                        height: 18
                        radius: 4

                        // ★ 只有**带 id 的标签**才可点开详情。
                        //   银河系视图的旋臂标签 (如"英仙臂") 不带 id ——
                        //   它们是结构描述, 不是可查询的天体。C++ 侧对它们
                        //   留空 id, 这里据此决定是否响应鼠标。
                        readonly property bool clickable:
                            modelData.id !== undefined
                            && modelData.id !== null
                            && modelData.id.length > 0

                        color: tagMa.containsMouse
                               ? Qt.rgba(0.13, 0.19, 0.30, 0.94)
                               : Qt.rgba(0.05, 0.06, 0.09, 0.82)
                        border.width: tagMa.containsMouse ? 2 : 1
                        border.color: tagMa.containsMouse
                                      ? Qt.rgba(0.55, 0.78, 1.0, 0.95)
                                      : (modelData.kind === "core"
                                         ? Qt.rgba(1.0, 0.82, 0.40, 0.75)
                                         : modelData.kind === "spur"
                                           ? Qt.rgba(0.50, 0.85, 0.91, 0.75)
                                           : Qt.rgba(0.72, 0.79, 0.88, 0.55))

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

                        // ★ 点击热区就放在标签底上 (而不是外面套一层) ——
                        //   标签宽度是自适应的 (labelRow.width + 12),
                        //   套在外面就得手动同步尺寸, 容易对不齐。
                        //
                        // ★ 为什么热区要覆盖整个标签而不只是那个 5px 小十字:
                        //   用户的直觉是"点名字"。5px 的圆点几乎点不中。
                        //
                        // ★★ 关键: 这个 MouseArea 必须**自己实现拖拽**。
                        //   相机拖拽原本由根级那个全屏 MouseArea 负责, 但
                        //   QML 的鼠标事件只传给最上层的 MouseArea —— 标签
                        //   一旦接受 press, 根级就收不到了, 表现为
                        //   **"鼠标正好压在标签上时拖不动视角"**。
                        //   所以这里复刻同样的拖拽逻辑 (4 行), 见下面的注释。
                        MouseArea {
                            id: tagMa
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: tagBg.clickable
                            cursorShape: pressed ? Qt.ClosedHandCursor
                                                 : Qt.PointingHandCursor

                            property real lastX: 0
                            property real lastY: 0
                            property real pressX: 0
                            property real pressY: 0
                            property bool moved: false

                            onPressed: (m) => {
                                lastX = m.x
                                lastY = m.y
                                pressX = m.x
                                pressY = m.y
                                moved = false
                            }
                            onPositionChanged: (m) => {
                                const dx = m.x - lastX
                                const dy = m.y - lastY
                                lastX = m.x
                                lastY = m.y
                                // ★ 用**累计位移**判定是否算拖拽, 不能用每帧
                                //   位移 —— 手抖 1px 就会把点击误判成拖拽。
                                //   阈值 4px 是常见的手感取值。
                                if (Math.abs(m.x - pressX) > 4
                                        || Math.abs(m.y - pressY) > 4)
                                    moved = true
                                // ★ 与根级 MouseArea 的拖拽逻辑保持一致 ——
                                //   两处若不一致, 会出现"在标签上和在空白处
                                //   拖动的手感不一样"。
                                if (m.buttons & Qt.RightButton)
                                    scene.panCamera(dx, dy)
                                else
                                    scene.rotateCamera(dx, dy)
                            }
                            onWheel: (w) => scene.zoomCamera(w.angleDelta.y / 120.0)

                            onClicked: {
                                // ★ 拖拽结束也会触发 clicked, 必须排除 ——
                                //   否则"在标签上拖一下视角"会顺带弹出详情卡。
                                if (moved)
                                    return
                                const d = root.cardForMarker(modelData)
                                if (d && d.nameCn !== undefined) {
                                    galaxyCard.detail = d
                                    galaxyCard.visible = true
                                }
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
            visible: !root.stellarView
                     && (scene.scaleLevel === 2
                         ? scene.galaxyViewWidthLy > 0.5
                         : scene.galaxyViewWidthLy > 100)
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

    // ========================================================================
    //  演化播放器入口 (顶栏分段控件旁) —— 通用时间轴引擎, 12 条剧本
    // ========================================================================
    EvoOverlay {
        id: evoOverlay
        dpr: root.screen ? root.screen.devicePixelRatio : 2.0
        testEvo: scene.testEvo
        proMode: root.proMode
        sceneObj: scene
    }

    // ========================================================================
    //  具名天体详情卡 —— 真实观测照片 + 数据
    //
    //  ★ 照片全部来自 ESO / NASA 官方图库 (来源记录见
    //    assets/galaxy/SOURCES.txt)。每一张都经过**目视确认** ——
    //    实测发现 ESO 的图库 ID 完全不可信: 同一批编号里混有
    //    望远镜照片、会议海报、人物合影、宇宙演化示意图。
    //
    //  ★ 找不到图时不显示假图, 而是明确提示"暂无实景图"并说明原因。
    //    这对以下两类是**必须**的:
    //      - 普通椭圆星系 (M86/M49): 没有单独的清晰观测
    //      - 超星系团/巨壁/空洞: 它们是速度场或密度场的**边界**,
    //        物理上不存在"一张照片"
    // ========================================================================
    Rectangle {
        id: galaxyCard
        anchors.fill: parent
        z: 200
        visible: false
        color: Qt.rgba(0.02, 0.03, 0.05, 0.72)

        // 详情数据 (由列表项点击时填入, 或由 location() 返回)
        // 文本版本由全局 root.proMode 决定, 卡片自身不再持有开关。
        property var detail: ({})

        // 测试用: SS_CARD=<id> 时启动即打开 (供自动化截图验证)
        Component.onCompleted: {
            const t = scene.testCard
            if (t && t.length > 0) {
                // 先按星系 id 查; 查不到就当作大尺度结构的名字再试一次 ——
                // 后者没有 id, 只能靠名字匹配。再查不到试 B.1/B.2 新增条目。
                // 三条路都走 root 里的函数, 保证与用户点击时**完全一致**。
                let d = location(t)
                if (!d || d.nameCn === undefined)
                    d = root.cardForStructByName(t)
                // ★ B.1/B.2 新增条目: SS_CARD 同样直达 (与面板点击同路径)
                if (!d || d.nameCn === undefined)
                    d = root.stellarAgnDetail(t)
                if (!d || d.nameCn === undefined)
                    d = root.stellarAgnDetail(t)
                galaxyCard.detail = d
                galaxyCard.visible = true
            }
        }

        // 点背景关闭
        MouseArea {
            anchors.fill: parent
            onClicked: galaxyCard.visible = false
        }

        Rectangle {
            id: card
            anchors.centerIn: parent
            width: Math.min(parent.width - 80, 560)
            height: cardCol.implicitHeight + 36
            radius: 12
            color: Qt.rgba(0.055, 0.078, 0.125, 0.97)
            border.width: 1
            border.color: Qt.rgba(0.37, 0.66, 1.0, 0.30)

            // 吞掉点击, 避免穿透到背景关闭
            MouseArea { anchors.fill: parent }

            ColumnLayout {
                id: cardCol
                anchors.fill: parent
                anchors.margins: 18
                spacing: 10

                // ---- 标题行 ----
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: (galaxyCard.detail.nameCn || "")
                        color: "#dce8ff"
                        font.pixelSize: 17
                        font.bold: true
                        font.family: root.sansFont
                    }
                    Text {
                        text: (galaxyCard.detail.nameEn || "")
                        color: root.cTextDim
                        font.pixelSize: 11
                        font.family: root.sansFont
                        Layout.alignment: Qt.AlignBottom
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "✕"
                        color: root.cTextDim
                        font.pixelSize: 15
                        Layout.alignment: Qt.AlignTop
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -8
                            cursorShape: Qt.PointingHandCursor
                            onClicked: galaxyCard.visible = false
                        }
                    }
                }

                // ---- 照片区 ----
                //
                // ★ 有照片: 按原始比例显示, 高度上限 260 避免撑破卡片。
                // ★ 无照片: 显示一个等高的提示框 —— 用明确的文字说明,
                //   不留给用户"图没加载出来"的错觉。
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: photoArea.height
                    color: "transparent"

                    Item {
                        id: photoArea
                        width: parent.width
                        height: {
                            const p = galaxyCard.detail.photo
                            if (p && p.length > 0)
                                return Math.min(photoImg.implicitHeight
                                                * (width / Math.max(photoImg.implicitWidth, 1)),
                                            260)
                            return 96          // 无图时的提示框高度
                        }

                        Image {
                            id: photoImg
                            anchors.fill: parent
                            // ★ 本地路径必须加 file:/// 前缀, 否则 Image
                            //   会把 "D:/..." 当成相对 URL 而加载失败
                            source: {
                                const p = galaxyCard.detail.photo
                                return (p && p.length > 0)
                                       ? "file:///" + p : ""
                            }
                            fillMode: Image.PreserveAspectFit
                            visible: source !== ""
                            asynchronous: true
                        }

                        // 无照片提示
                        Rectangle {
                            anchors.fill: parent
                            visible: !photoImg.visible
                            radius: 8
                            color: Qt.rgba(1, 1, 1, 0.035)
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.10)

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 5
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "暂无实景图"
                                    color: "#c8a878"
                                    font.pixelSize: 13
                                    font.family: root.sansFont
                                }
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: card.width - 100
                                    wrapMode: Text.WordWrap
                                    horizontalAlignment: Text.AlignHCenter
                                    text: (galaxyCard.detail.noPhotoWhy
                                           || "该天体暂无单独的高质量观测图像。")
                                    color: root.cTextDim
                                    font.pixelSize: 10
                                    font.family: root.sansFont
                                }
                            }
                        }
                    }
                }

                // ---- 数据行 ----
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 14
                    rowSpacing: 4

                    Repeater {
                        model: {
                            const d = galaxyCard.detail
                            const rows = []
                            // ★ 两种距离来源要分先后:
                            //   distText —— **结构列表给的现成文本**。大尺度
                            //     结构的距离跨 1 亿~12 亿光年, 若统一换算成
                            //     "百万光年"会变成一串难读的大数 (如 1200)。
                            //   dist —— **星系数据的数值** (单位: 百万光年)。
                            if (d.distText !== undefined)
                                rows.push({ k: "距离", v: d.distText })
                            else if (d.dist !== undefined)
                                rows.push({ k: "距离", v: d.dist > 1e-9
                                            ? d.dist.toFixed(2) + " 百万光年"
                                            : "我们所在" })
                            if (d.sizeText !== undefined)
                                rows.push({ k: "尺度", v: d.sizeText })
                            else if (d.diameter !== undefined)
                                rows.push({ k: "直径", v: d.diameter.toFixed(1) + " 千光年" })
                            if (d.massLog !== undefined)
                                rows.push({ k: "恒星质量", v: "10^"
                                            + d.massLog.toFixed(2) + " 太阳质量" })
                            // ★ B.1/B.2 新增条目字段 (直接显示, 无需换算):
                            //   spec(光谱型/类型) / teffText / massText /
                            //   cat(分组) —— 由 C++ 侧拼好文本, QML 只搬运。
                            if (d.spec !== undefined)
                                rows.push({ k: "类型", v: d.spec })
                            else if (d.typeText !== undefined)
                                rows.push({ k: "类型", v: d.typeText })
                            if (d.teffText !== undefined)
                                rows.push({ k: "有效温度", v: d.teffText })
                            if (d.massText !== undefined)
                                rows.push({ k: "质量", v: d.massText })
                            if (d.cat !== undefined)
                                rows.push({ k: "分组", v: d.cat })
                            return rows
                        }

                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.columnSpan: 1
                            spacing: 6
                            Text {
                                text: modelData.k
                                color: root.cTextDim
                                font.pixelSize: 10
                                font.family: root.sansFont
                                Layout.preferredWidth: 62
                            }
                            Text {
                                text: modelData.v
                                color: root.cText
                                font.pixelSize: 10
                                font.family: root.fitFont(modelData.v)
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                // ---- 红移与谱线位移 ----
                //
                // ★★ 为什么不做"整条可见光谱上的位移":
                //   这些近距星系的 |z| 都 < 0.005 —— Hα (656.3nm) 的位移
                //   最多 2.8nm。在 380~700nm 的整段光谱上只占 **0.4% 宽度**,
                //   **肉眼完全看不出来**, 画出来等于没画。
                //   所以这里**放大到 Hα 附近 ±8nm 的窗口**, 并在标签里
                //   写明"放大 x 倍", 避免让人误以为位移真有这么大。
                //
                // ★ 为什么不画彩虹色带: 648~664nm 整段都是纯红,
                //   色带在这个窗口里**不含任何信息**。所以改用天文学家
                //   真正看到的东西 —— **连续谱上的一条吸收线**,
                //   它的位置就是测红移的依据。
                Item {
                    id: redshiftBlock
                    Layout.fillWidth: true
                    // ★ 高度自适应: 上四行固定占 66px, 解读行按**实际文本高度**
                    //   累加。写死高度是不行的 —— "推算红移 + 已移出可见光"
                    //   两种情况叠加时解读会折成 4 行, 会盖住下面的描述文字。
                    Layout.preferredHeight: 66 + interpText.implicitHeight + 6
                    visible: galaxyCard.detail.redshift !== undefined

                    // ★ 属性名用 gz 而不是 z —— z 是 QML Item 的**内建属性**
                    //   (层叠顺序), 覆盖它会让进程静默退出且日志全空。
                    readonly property real gz:
                        galaxyCard.detail.redshift !== undefined
                        ? galaxyCard.detail.redshift : 0

                    readonly property real cKms: 299792.458   // 光速 km/s
                    readonly property real haRest: 656.3      // Hα 静止波长 nm
                    readonly property real haObs: haRest * (1 + gz)
                    readonly property real velKms: gz * cKms

                    // ★★ 显示窗口必须**自适应**两端 (静止位置与实测位置)。
                    //
                    //   固定窗口是不行的: 大尺度结构的 z 可达 0.069,
                    //   Hα 移到约 701nm —— 早已跑出"±8nm"这种固定窗口,
                    //   吸收线会画到条外, 看起来像"线不见了"。
                    //   实测踩到: 固定窗口时 M31/M87 正常, 但点开斯隆巨壁
                    //   只见一条虚线没有吸收线, 差点误判成计算错误。
                    readonly property real wLo: Math.min(haRest, haObs) - 3.0
                    readonly property real wHi: Math.max(haRest, haObs) + 3.0

                    function wl2x(w) {
                        return (w - wLo) / (wHi - wLo) * strip.width
                    }

                    // 是否已移出可见光 (700nm) —— 只有大尺度结构会遇到。
                    // ★ 这是个很好的教学点: 在宇宙学尺度上,
                    //   连 Hα 这种可见光里最强的谱线都会被推出可见范围。
                    readonly property bool outOfVisible: haObs > 700.0

                    readonly property string zText:
                        "红移 z = " + (gz >= 0 ? "+" : "−")
                        + Math.abs(gz).toFixed(6)

                    // 红移 -> 偏红; 蓝移 -> 偏蓝
                    readonly property color zColor:
                        gz < -0.0001 ? "#7ab8ff"
                      : gz >  0.0001 ? "#ff9a7a"
                      : root.cTextDim

                    readonly property string velText:
                        Math.abs(velKms) < 1
                        ? "相对静止"
                        : (velKms > 0 ? "退行 " : "接近 ")
                          + Math.abs(velKms).toFixed(0) + " km/s"

                    readonly property string zoomText:
                        "窗口 " + wLo.toFixed(1) + " – " + wHi.toFixed(1) + " nm"
                        + (outOfVisible ? " · ⚠ 已超出可见光"
                                        : " · 放大显示")

                    // ---- 第一行: z 值 + 速度 ----
                    Text {
                        x: 0; y: 0
                        text: redshiftBlock.zText
                        color: redshiftBlock.zColor
                        font.pixelSize: 11
                        font.family: root.monoFont
                    }
                    Text {
                        x: 210; y: 0
                        text: redshiftBlock.velText
                        color: root.cTextDim
                        font.pixelSize: 10
                        font.family: root.sansFont
                    }

                    // ---- 连续谱 + 吸收线 ----
                    //
                    // ★ 视觉要点: 连续谱要**暖白且亮**, 吸收线要**很黑**,
                    //   两者对比强烈才像"光谱"而不是"灰色进度条"。
                    //   实测第一版用 (0.86,0.84,0.80,0.30) 的灰太低,
                    //   整条看起来就是一根灰条, 毫无"光"的感觉。
                    Rectangle {
                        id: strip
                        x: 0
                        y: 20
                        width: redshiftBlock.width
                        height: 18
                        radius: 3
                        // 连续谱: 偏暖的亮白 (恒星光的平均色; 略偏红
                        // 是因为窗口在 Hα 附近, 本来就偏长波端)
                        color: Qt.rgba(0.98, 0.94, 0.86, 0.88)
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.22)
                    }

                    // ---- 波长刻度 ----
                    // ★ 没有刻度的话, "放大 ±8nm"这张图无法读出任何数值,
                    //   只能看出"线动了一下"。三根短刻度标出窗口的
                    //   左沿 / 静止位置 / 右沿。
                    Repeater {
                        model: [
                            { w: redshiftBlock.wLo },
                            { w: redshiftBlock.haRest },
                            { w: redshiftBlock.wHi }
                        ]

                        Rectangle {
                            required property var modelData
                            x: redshiftBlock.wl2x(modelData.w) - 0.5
                            y: 38
                            width: 1
                            height: 4
                            color: Qt.rgba(1, 1, 1, 0.32)
                        }
                    }

                    // 静止位置 (z=0 时 Hα 应在的地方) —— 虚线式参照
                    Rectangle {
                        x: redshiftBlock.wl2x(redshiftBlock.haRest) - 0.5
                        y: 16
                        width: 1
                        height: 26
                        color: Qt.rgba(1, 1, 1, 0.38)
                    }

                    // 实测位置 —— 一条**暗吸收线**, 就是天文学家测量的对象。
                    // 加横跨上下的长度, 让它明显区别于刻度。
                    Rectangle {
                        x: redshiftBlock.wl2x(redshiftBlock.haObs) - 2.5
                        y: 16
                        width: 5
                        height: 26
                        color: Qt.rgba(0.03, 0.04, 0.06, 0.95)
                    }
                    // 吸收线两侧的亮缘 —— 强化"这是一条线"而不是"一块黑斑"
                    Rectangle {
                        x: redshiftBlock.wl2x(redshiftBlock.haObs) - 4
                        y: 16
                        width: 1.5
                        height: 26
                        color: Qt.rgba(1, 1, 1, 0.85)
                    }
                    Rectangle {
                        x: redshiftBlock.wl2x(redshiftBlock.haObs) + 2.5
                        y: 16
                        width: 1.5
                        height: 26
                        color: Qt.rgba(1, 1, 1, 0.85)
                    }

                    // ---- 第三行: 读数 + 放大说明 ----
                    Text {
                        x: 0; y: 48
                        text: "Hα  " + redshiftBlock.haRest.toFixed(1)
                              + " → " + redshiftBlock.haObs.toFixed(1) + " nm"
                        color: root.cText
                        font.pixelSize: 10
                        font.family: root.monoFont
                    }
                    Text {
                        x: 210; y: 49
                        text: redshiftBlock.zoomText
                        color: Qt.rgba(0.62, 0.70, 0.82, 0.75)
                        font.pixelSize: 9
                        font.family: root.sansFont
                    }

                    // ---- 第四行: 解读 ----
                    //
                    // ★ 措辞刻意保守: 只陈述**可以从数据直接推出**的结论,
                    //   不替用户推断成因 —— 近距星系的红移是"宇宙膨胀 +
                    //   本动速度"的叠加, 两者的拆分需要更多观测数据。
                    //
                    // ★★ 必须区分**实测**与**推算**:
                    //   星系的 z 来自单条光谱的测量; 大尺度结构没有单条
                    //   光谱可测, z 由哈勃定律算出 (C++ 侧带 redshiftDerived
                    //   标志)。混为一谈会让人以为"结构也被测过光谱"。
                    Text {
                        id: interpText
                        x: 0; y: 66
                        width: redshiftBlock.width
                        wrapMode: Text.WordWrap
                        text: {
                            const g = redshiftBlock.gz
                            if (galaxyCard.detail.redshiftDerived === true)
                                return "★ 推算的膨胀红移 —— 在这个尺度上, "
                                     + "宇宙膨胀已完全主导, 星系自身运动只是零头。"
                                     + " (由 z = H₀·d/c 算出, 非光谱实测)"
                                     + (redshiftBlock.outOfVisible
                                        ? "\n★ 注意 Hα 已被推到 700nm 之外 —— "
                                          + "在宇宙学距离上, 可见光谱线会整体移入红外, "
                                          + "这正是高红移巡天要在红外波段做的原因。"
                                        : "")
                            if (g < -0.0001)
                                return "★ 蓝移 —— 它正朝我们接近。"
                                     + "在这么近的距离上, 宇宙膨胀的贡献极小, "
                                     + "观测到的移动主要来自星系自身的运动。"
                            if (g > 0.0001)
                                return "★ 红移 —— 谱线整体向长波端移动, "
                                     + "这是它正在远离我们的证据。"
                            return "★ 红移为零基准 —— 银河系是测量其他天体红移的参照。"
                        }
                        color: Qt.rgba(0.82, 0.68, 0.50, 1.0)
                        font.pixelSize: 9
                        font.family: root.sansFont
                        lineHeight: 1.35
                    }
                }

                // ---- 描述 (跟随全局专业/科普开关) ----
                //
                // ★ 选项一: 详情卡不再自带切换按钮, 统一跟随 root.proMode。
                //   有 pop 的条目才区分显示; 无 pop 的条目 (既有星系/结构)
                //   两种模式显示同一 desc。
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: (!root.proMode && galaxyCard.detail.pop !== undefined)
                          ? galaxyCard.detail.pop
                          : (galaxyCard.detail.desc || "")
                    color: root.proMode ? root.cText : "#ffe6b8"
                    font.pixelSize: 11
                    font.family: root.sansFont
                    lineHeight: 1.45
                }

                // ---- 图片来源标注 (仅在有照片时) ----
                Text {
                    Layout.fillWidth: true
                    // ★ 必须显式转 bool。写 `return p && p.length > 0` 时,
                    //   若 p 是 undefined, `&&` 短路后返回的是 undefined 而非
                    //   false, QML 会报 "Unable to assign [undefined] to bool"。
                    visible: {
                        const p = galaxyCard.detail.photo
                        return (p !== undefined && p !== null && p.length > 0)
                    }
                    text: "图片来源: ESO / NASA 公开图库 (详见 assets/galaxy/SOURCES.txt)"
                    color: Qt.rgba(0.56, 0.64, 0.75, 0.60)
                    font.pixelSize: 9
                    font.family: root.sansFont
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    // ---- 测试用: 模拟点击 3D 标签 (SS_MARKER) ----
    //
    // ★★ 为什么不复用上面 SS_CARD 那条路径:
    //   那条是**直查数据表**, 不经过标签。而这里要验证的是
    //   "标签上的 id 有没有正确从 C++ 传到 QML" —— 只有走
    //   scene.galaxyLabels 再调 cardForMarker 才能真正覆盖这段链路。
    //
    // ★ 必须等标签列表**真的填充**后再查: 标签是渲染线程算完投影后
    //   emit 过来的, Component.onCompleted 时还是空数组。
    Timer {
        id: markerProbe
        interval: 400
        repeat: true
        running: scene.testMarker.length > 0
        property bool done: false

        onTriggered: {
            if (markerProbe.done)
                return
            const list = scene.galaxyLabels
            if (!list || list.length === 0)
                return                      // 还没投影出来, 再等一轮

            const want = scene.testMarker
            let hit = null
            for (let i = 0; i < list.length; ++i) {
                const L = list[i]
                if (L.text === want || L.id === want) {
                    hit = L
                    break
                }
            }
            markerProbe.done = true

            if (!hit) {
                console.log("[拾取测试] 未匹配到标签: " + want
                            + "  (标签总数 " + list.length + ")")
                return
            }
            console.log("[拾取测试] 命中标签 text=" + hit.text
                        + "  id=" + hit.id + "  kind=" + hit.kind)
            const d = root.cardForMarker(hit)
            if (d && d.nameCn !== undefined) {
                console.log("[拾取测试] 卡片数据 OK: " + d.nameCn
                            + "  redshift=" + d.redshift
                            + "  derived=" + d.redshiftDerived)
                galaxyCard.detail = d
                galaxyCard.visible = true
            } else {
                console.log("[拾取测试] cardForMarker 返回空 —— 链路失败")
            }
        }
    }

    // ========================================================================
    //  哈勃图面板 —— 用真实超新星数据检验宇宙膨胀
    //
    //  ★★ 为什么这件事在教学上是"质变":
    //    之前的宇宙视图是**展示**宇宙长什么样; 这里是**用数据检验模型** ——
    //    拖动 Ωm, 曲线变形、χ² 变化, 能亲眼看到哪个模型被数据接受、
    //    哪个被排除。这是从"看"到"做科学"的那一步。
    //
    //  ★ 数据: Pantheon+ (2022), 1701 颗 Ia 型超新星。
    //    它们是**标准烛光** —— 距离由视亮度独立测定, 与红移无关。
    //    这一点是整张图的立足点: 若改用 SDSS 星系 (距离由红移推出),
    //    就是循环论证, 参数取什么值都会"符合"。
    // ========================================================================
    Rectangle {
        id: hubbleOverlay
        anchors.fill: parent
        visible: false
        z: 180
        color: Qt.rgba(0.02, 0.03, 0.05, 0.90)

        // 点背景关闭
        MouseArea {
            anchors.fill: parent
            onClicked: hubbleOverlay.visible = false
        }

        // ---- 数据与状态 ----
        property var  hd: ({})
        property real om: 0.315
        property real ol: 0.685
        property real chi2: 0
        property real omBest: 0.351
        property real omLo: 0.30
        property real omHi: 0.40

        // 参考模型的 χ² (固定值, 启动时算一次)
        property real chi2Best: 0
        property real chi2Planck: 0
        property real chi2Eds: 0
        property real chi2Empty: 0

        property bool ready: false

        // ---- 坐标范围 ----
        readonly property real lz0: -3.0      // log10(z) 下限 (z = 0.001)
        readonly property real lz1: 0.40      // 上限 (z = 2.5)
        readonly property real mu0: 28.4
        readonly property real mu1: 47.6
        readonly property int  plotW: 820
        readonly property int  plotH: 380
        readonly property real dpr: root.screen ? root.screen.devicePixelRatio : 1

        // 当前曲线用的积分表与零点 (绘图需要)
        property var  curTable: null
        property real curC: 0

        // ------------------------------------------------------------------
        //  宇宙学计算
        // ------------------------------------------------------------------

        // E(z) = H(z)/H0, 含曲率项 Ωk = 1 - Ωm - ΩΛ
        function ezOf(z, m, l) {
            const omk = 1.0 - m - l
            const q = 1.0 + z
            return Math.sqrt(m * q * q * q + omk * q * q + l)
        }

        // ★★ 累积积分表 —— 让 χ² 能实时算的关键
        //
        //   朴素做法: 对 1701 颗超新星**各做一次**数值积分 (每个 ~80 步)
        //   = 13.6 万次 E(z) 求值, 拖滑块会明显卡。
        //   这里改成: 在一条预先算好的网格上做**累积 Simpson**,
        //   得到 cum[i] = ∫₀^{z_i} dz/E(z); 之后任意 z 只需插值。
        //   2000 点网格 → 约 6000 次求值, 快 20 倍以上。
        //
        //   ★ 网格必须 **log 等距**而不是线性:
        //     红移跨 0.0012~2.26 (= 三个半数量级), 线性网格在低 z 端
        //     第一个格子就跨过整段低红移数据; log 网格让每个区间的
        //     **相对**宽度一致, Simpson 的相对误差在各处均匀。
        function buildTable(m, l) {
            const N = 2000
            const za = 1e-4          // 起点: 更低的贡献可忽略
            const zb = 2.6
            const lr = Math.log(zb / za) / N
            const zs = []
            const inv = []
            for (let i = 0; i <= N; ++i) {
                const z = za * Math.exp(lr * i)
                zs.push(z)
                inv.push(1.0 / ezOf(z, m, l))
            }
            const cum = [0.0]
            for (let i = 1; i <= N; ++i) {
                const h = zs[i] - zs[i - 1]
                const zm = 0.5 * (zs[i] + zs[i - 1])
                const fm = 1.0 / ezOf(zm, m, l)
                cum.push(cum[i - 1] + h / 6.0 * (inv[i - 1] + 4.0 * fm + inv[i]))
            }
            return { cum: cum, N: N, lr: lr, za: za }
        }

        // 共动距离 (以 c/H0 为单位)。在 log 空间线性插值 ——
        // 网格本身就是 log 等距的, 在这个空间插值误差最小。
        function dcOf(t, z) {
            if (!t || z <= 0)
                return 0.0
            let f = Math.log(z / t.za) / t.lr
            if (f <= 0) f = 0
            if (f >= t.N) f = t.N - 1
            const i = Math.floor(f)
            const r = f - i
            return t.cum[i] * (1.0 - r) + t.cum[i + 1] * r
        }

        // 拟合: χ² 与边缘化后的零点 C, 并回传所用积分表
        //
        // ★ C 必须作为自由参数: 它同时吸收 H0 与超新星绝对星等 M,
        //   二者在哈勃图上**完全简并** (只差一个常数), 固定任何一个
        //   都会得出错误结论。
        // ★ 用**解析边缘化**: 给定 Ωm/ΩΛ 时 χ² 对 C 是二次式, 极小点
        //   有闭式解 C* = Σw(μ−m)/Σw, 比网格扫描省掉一整个维度。
        function fitWith(m, l) {
            const t = buildTable(m, l)
            const n = hd.logz.length
            let sw = 0.0, swd = 0.0
            const mv = []
            for (let i = 0; i < n; ++i) {
                const z = Math.pow(10, hd.logz[i])
                const dl = (1.0 + z) * dcOf(t, z)
                const v = 5.0 * Math.log10(Math.max(dl, 1e-12))
                mv.push(v)
                const w = 1.0 / (hd.err[i] * hd.err[i])
                sw += w
                swd += w * (hd.mu[i] - v)
            }
            const C = sw > 0 ? swd / sw : 0.0
            let c2 = 0.0
            for (let i = 0; i < n; ++i) {
                const r = hd.mu[i] - mv[i] - C
                c2 += r * r / (hd.err[i] * hd.err[i])
            }
            return { chi2: c2, C: C, table: t }
        }

        // 重算当前参数的 χ² 与曲线
        function refresh() {
            if (!ready)
                return
            const r = fitWith(om, ol)
            chi2 = r.chi2
            curTable = r.table
            curC = r.C
            fgCv.requestPaint()
        }

        // 复位到最佳拟合
        function resetToBest() {
            omSlider.setValue(omBest)
            olSlider.setValue(1.0 - omBest)
        }

        Component.onCompleted: {
            const d = scene.hubbleData()
            if (d && d.n !== undefined && d.n > 0) {
                hd = d
                omBest = d.omBest
                omLo = d.omLo
                omHi = d.omHi
                om = d.omBest
                ol = 1.0 - d.omBest
                ready = true

                // 参考模型的 χ² 只算一次 (不随滑块变化)
                chi2Planck = fitWith(0.315, 0.685).chi2
                chi2Eds = fitWith(1.0, 0.0).chi2
                chi2Empty = fitWith(0.0, 0.0).chi2
                chi2Best = fitWith(omBest, 1.0 - omBest).chi2

                refresh()
                bgCv.requestPaint()
            }
        }

        // 画布坐标变换
        function cx(logz) { return (logz - lz0) / (lz1 - lz0) * plotW }
        function cy(mu)   { return plotH - (mu - mu0) / (mu1 - mu0) * plotH }

        // 给定模型画曲线所需的零点 (与 fitWith 同一套约定)
        function zeroPointOf(t, m, l) {
            let sw = 0.0, swd = 0.0
            for (let i = 0; i < hd.logz.length; ++i) {
                const z = Math.pow(10, hd.logz[i])
                const dl = (1.0 + z) * dcOf(t, z)
                const v = 5.0 * Math.log10(Math.max(dl, 1e-12))
                const w = 1.0 / (hd.err[i] * hd.err[i])
                sw += w
                swd += w * (hd.mu[i] - v)
            }
            return sw > 0 ? swd / sw : 0.0
        }

        // 主卡片
        Rectangle {
            id: hubCard
            anchors.centerIn: parent
            width: hubbleOverlay.plotW + 56
            height: hubCol.implicitHeight + 40
            radius: 12
            color: Qt.rgba(0.055, 0.078, 0.125, 0.98)
            border.width: 1
            border.color: Qt.rgba(0.37, 0.66, 1.0, 0.35)

            MouseArea { anchors.fill: parent }     // 吞点击, 不穿透到背景

            Column {
                id: hubCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 20
                spacing: 10

                // ---- 标题行 ----
                Row {
                    spacing: 12
                    Text {
                        text: "哈勃图"
                        color: root.cText
                        font.pixelSize: 20
                        font.bold: true
                        font.family: root.sansFont
                    }
                    Text {
                        anchors.baseline: parent.children[0].baseline
                        text: "用真实超新星数据检验宇宙膨胀模型"
                        color: root.cTextDim
                        font.pixelSize: 12
                        font.family: root.sansFont
                    }
                    Text {
                        anchors.baseline: parent.children[0].baseline
                        visible: hubbleOverlay.ready
                        text: "Pantheon+ · " + hubbleOverlay.hd.n
                              + " 颗 Ia 型超新星 (2022)"
                        color: Qt.rgba(0.58, 0.76, 0.98, 0.95)
                        font.pixelSize: 11
                        font.family: root.sansFont
                    }
                    Item { width: 200; height: 1 }
                    Text {
                        anchors.baseline: parent.children[0].baseline
                        text: "✕"
                        color: hoverClose.containsMouse ? "#ff9a9a" : root.cTextDim
                        font.pixelSize: 18
                        MouseArea {
                            id: hoverClose
                            anchors.fill: parent
                            anchors.margins: -8
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: hubbleOverlay.visible = false
                        }
                    }
                }

                // ---- 绘图区 (两个 Canvas 叠加) ----
                Item {
                    width: hubbleOverlay.plotW
                    height: hubbleOverlay.plotH
                    // 左边留出放 y 轴数字的位置
                    x: 44

                    // 底层: 网格 + 坐标轴 + 数据点 (静态)
                    Canvas {
                        id: bgCv
                        anchors.fill: parent
                        renderStrategy: Canvas.Immediate
                        // ★ canvasSize 设成物理分辨率, 告诉 Canvas 按高清渲染。
                        //   但**不要**再手动 ctx.scale(dpr, dpr) ——
                        //   Qt 已自动把绘图坐标 (逻辑尺寸 820x380) 映射到它。
                        //   多乘一次会把所有坐标放大一倍, 表现为
                        //   "只画出一部分、数据点全在画布外"。
                        canvasSize: Qt.size(width * hubbleOverlay.dpr,
                                            height * hubbleOverlay.dpr)

                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()

                            const W = hubbleOverlay.plotW
                            const H = hubbleOverlay.plotH
                            const ov = hubbleOverlay

                            // 网格
                            ctx.strokeStyle = "rgba(255,255,255,0.07)"
                            ctx.lineWidth = 1
                            const zs = [0.001, 0.003, 0.01, 0.03, 0.1, 0.3, 1.0, 2.5]
                            for (let i = 0; i < zs.length; ++i) {
                                const x = ov.cx(Math.log10(zs[i]))
                                ctx.beginPath(); ctx.moveTo(x, 0)
                                ctx.lineTo(x, H); ctx.stroke()
                            }
                            for (let v = 30; v <= 47; v += 2) {
                                const y = ov.cy(v)
                                ctx.beginPath(); ctx.moveTo(0, y)
                                ctx.lineTo(W, y); ctx.stroke()
                            }

                            // 数据点
                            if (ov.ready) {
                                const d = ov.hd
                                ctx.fillStyle = "rgba(150,210,255,0.5)"
                                for (let i = 0; i < d.logz.length; ++i) {
                                    const x = ov.cx(d.logz[i])
                                    const y = ov.cy(d.mu[i])
                                    ctx.fillRect(x - 1, y - 1, 2, 2)
                                }
                            }

                            // 边框
                            ctx.strokeStyle = "rgba(120,150,190,0.45)"
                            ctx.lineWidth = 1.5
                            ctx.strokeRect(0.5, 0.5, W - 1, H - 1)

                            // 轴标签
                            ctx.fillStyle = "rgba(160,180,205,0.9)"
                            ctx.font = "11px Consolas, monospace"
                            for (let i = 0; i < zs.length; ++i) {
                                const x = ov.cx(Math.log10(zs[i]))
                                ctx.fillText(String(zs[i]), x - 13, H + 17)
                            }
                            for (let v = 30; v <= 46; v += 4) {
                                ctx.fillText(String(v), -32, ov.cy(v) + 4)
                            }
                            ctx.font = "12px 'Microsoft YaHei'"
                            ctx.fillStyle = "rgba(180,196,218,0.95)"
                            ctx.fillText("红移 z", W - 46, H + 38)
                        }
                    }

                    // 上层: 理论曲线 (参数变化时重绘)
                    Canvas {
                        id: fgCv
                        anchors.fill: parent
                        renderStrategy: Canvas.Immediate
                        // 同上: 不要再 ctx.scale(dpr, dpr)
                        canvasSize: Qt.size(width * hubbleOverlay.dpr,
                                            height * hubbleOverlay.dpr)

                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            const ov = hubbleOverlay
                            if (!ov.ready || !ov.curTable)
                                return

                            // 画一条理论曲线 (N 点折线)
                            function curve(t, C, color, width) {
                                ctx.strokeStyle = color
                                ctx.lineWidth = width
                                ctx.beginPath()
                                const N = 140
                                for (let i = 0; i <= N; ++i) {
                                    const lz = ov.lz0 + (ov.lz1 - ov.lz0) * i / N
                                    const z = Math.pow(10, lz)
                                    const dl = (1.0 + z) * ov.dcOf(t, z)
                                    const mu = 5.0 * Math.log10(Math.max(dl, 1e-12)) + C
                                    const x = ov.cx(lz)
                                    const y = ov.cy(mu)
                                    if (i === 0) ctx.moveTo(x, y)
                                    else ctx.lineTo(x, y)
                                }
                                ctx.stroke()
                            }

                            // 参考模型 (细线, 各自用**自己的**零点)
                            const tEds = ov.buildTable(1.0, 0.0)
                            curve(tEds, ov.zeroPointOf(tEds, 1.0, 0.0),
                                  "rgba(255,118,108,0.8)", 2)
                            const tEmp = ov.buildTable(0.0, 0.0)
                            curve(tEmp, ov.zeroPointOf(tEmp, 0.0, 0.0),
                                  "rgba(255,205,112,0.8)", 2)

                            // 当前模型 (粗线)
                            curve(ov.curTable, ov.curC, "#7ee28a", 3.5)
                        }
                    }
                }

                // ---- Ωm 滑块 ----
                Row {
                    spacing: 10
                    Text {
                        text: "Ωm"
                        color: root.cText
                        font.pixelSize: 12
                        font.family: root.monoFont
                        width: 26
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Slider {
                        id: omSlider
                        width: 260
                        from: 0.0; to: 1.0; stepSize: 0.005
                        value: 0.315
                        anchors.verticalCenter: parent.verticalCenter
                        onMoved: {
                            hubbleOverlay.om = value
                            omDebounce.restart()
                        }
                    }
                    Text {
                        text: hubbleOverlay.om.toFixed(3)
                        color: root.cText
                        font.pixelSize: 12
                        font.family: root.monoFont
                        width: 52
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // ---- ΩΛ 滑块 ----
                Row {
                    spacing: 10
                    Text {
                        text: "ΩΛ"
                        color: root.cText
                        font.pixelSize: 12
                        font.family: root.monoFont
                        width: 26
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Slider {
                        id: olSlider
                        width: 260
                        from: 0.0; to: 1.0; stepSize: 0.005
                        value: 0.685
                        anchors.verticalCenter: parent.verticalCenter
                        onMoved: {
                            hubbleOverlay.ol = value
                            omDebounce.restart()
                        }
                    }
                    Text {
                        text: hubbleOverlay.ol.toFixed(3)
                        color: root.cText
                        font.pixelSize: 12
                        font.family: root.monoFont
                        width: 52
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // 防抖: 拖动时不要每一帧都重算 (一次完整重算约 6000 次
                // 积分求值 + 1701 次插值, 每帧都算会掉帧)
                Timer {
                    id: omDebounce
                    interval: 90
                    onTriggered: hubbleOverlay.refresh()
                }

                // ---- 读数区 ----
                Rectangle {
                    width: hubbleOverlay.plotW + 16
                    height: readCol.implicitHeight + 20
                    radius: 6
                    color: Qt.rgba(1, 1, 1, 0.04)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.09)

                    Column {
                        id: readCol
                        x: 12; y: 10
                        spacing: 5

                        // 当前 χ²
                        Row {
                            spacing: 14
                            Text {
                                text: "当前模型  χ² = "
                                      + hubbleOverlay.chi2.toFixed(0)
                                color: "#7ee28a"
                                font.pixelSize: 12
                                font.family: root.monoFont
                            }
                            Text {
                                text: {
                                    const d = hubbleOverlay.chi2
                                          - hubbleOverlay.chi2Best
                                    if (d < 1.0)
                                        return "Δχ² = " + d.toFixed(1)
                                              + "  ← 与最佳拟合无法区分"
                                    if (d < 4.0)
                                        return "Δχ² = " + d.toFixed(0)
                                              + "  ·  可接受"
                                    if (d < 25.0)
                                        return "Δχ² = " + d.toFixed(0)
                                              + "  ·  数据不太支持"
                                    return "Δχ² = " + d.toFixed(0)
                                          + "  ·  已被数据排除"
                                }
                                color: {
                                    const d = hubbleOverlay.chi2
                                          - hubbleOverlay.chi2Best
                                    return d < 4.0 ? "#8fd88f"
                                         : d < 25.0 ? "#e8cc7a"
                                         : "#e89090"
                                }
                                font.pixelSize: 11
                                font.family: root.sansFont
                            }
                        }

                        Text {
                            text: "Ωk = 1 − Ωm − ΩΛ = "
                                  + (1.0 - hubbleOverlay.om
                                     - hubbleOverlay.ol).toFixed(3)
                            color: root.cTextDim
                            font.pixelSize: 10
                            font.family: root.monoFont
                        }

                        // 参考模型对比表
                        Text {
                            text: "参考模型"
                            color: root.cText
                            font.pixelSize: 11
                            font.family: root.sansFont
                            topPadding: 4
                        }
                        Repeater {
                            model: {
                                const o = hubbleOverlay
                                return [
                                    { n: "最佳拟合 (本数据)",
                                      s: "Ωm = " + o.omBest.toFixed(3),
                                      c: o.chi2Best, col: "#7ee28a" },
                                    { n: "Planck 2018 (CMB)",
                                      s: "Ωm = 0.315", c: o.chi2Planck,
                                      col: "#be96ff" },
                                    { n: "爱因斯坦-德西特 (减速)",
                                      s: "Ωm = 1, ΩΛ = 0", c: o.chi2Eds,
                                      col: "#ff766c" },
                                    { n: "空宇宙",
                                      s: "Ωm = 0, ΩΛ = 0", c: o.chi2Empty,
                                      col: "#ffcd70" }
                                ]
                            }
                            delegate: Row {
                                required property var modelData
                                spacing: 0
                                Rectangle {
                                    width: 3; height: 13; radius: 1.5
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: modelData.col
                                }
                                Text {
                                    width: 168
                                    leftPadding: 8
                                    text: modelData.n
                                    color: root.cTextDim
                                    font.pixelSize: 10
                                    font.family: root.sansFont
                                }
                                Text {
                                    width: 100
                                    text: "χ² = " + modelData.c.toFixed(0)
                                    color: modelData.col
                                    font.pixelSize: 10
                                    font.family: root.monoFont
                                }
                                Text {
                                    text: "Δχ² = "
                                          + (modelData.c
                                             - hubbleOverlay.chi2Best).toFixed(0)
                                    color: root.cTextDim
                                    font.pixelSize: 10
                                    font.family: root.monoFont
                                }
                            }
                        }
                    }
                }

                // ---- 结论 + 复位按钮 ----
                Row {
                    spacing: 14
                    Text {
                        width: hubbleOverlay.plotW - 30
                        wrapMode: Text.WordWrap
                        text: "★ 拖动滑块改变宇宙学参数，看曲线如何变形。"
                              + "数据点明显**拒绝**红色的减速宇宙 —— "
                              + "它的 Δχ² 高达 "
                              + (hubbleOverlay.chi2Eds
                                 - hubbleOverlay.chi2Best).toFixed(0)
                              + "。这就是宇宙加速膨胀的定量证据。"
                        color: Qt.rgba(0.82, 0.68, 0.50, 1.0)
                        font.pixelSize: 10
                        font.family: root.sansFont
                        lineHeight: 1.35
                    }
                    Rectangle {
                        width: 74; height: 26; radius: 5
                        anchors.verticalCenter: parent.verticalCenter
                        color: resetMa.containsMouse
                               ? Qt.rgba(0.37, 0.66, 1.0, 0.28)
                               : Qt.rgba(1, 1, 1, 0.06)
                        border.width: 1
                        border.color: Qt.rgba(0.5, 0.7, 1.0, 0.4)
                        Text {
                            anchors.centerIn: parent
                            text: "复位"
                            color: root.cText
                            font.pixelSize: 11
                            font.family: root.sansFont
                        }
                        MouseArea {
                            id: resetMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: hubbleOverlay.resetToBest()
                        }
                    }
                }
            }
        }
    }

    // 测试用: SS_HUBBLE=1 时启动即打开哈勃图面板
    Timer {
        interval: 300
        running: hubbleOverlay.ready && !hubbleOverlay.visible
                 && scene.testHubble
        repeat: false
        onTriggered: {
            hubbleOverlay.visible = true
            console.log("[哈勃图] 已打开: " + hubbleOverlay.hd.n + " 点, χ² = "
                        + hubbleOverlay.chi2.toFixed(1)
                        + ", Ωm最佳 = " + hubbleOverlay.omBest)
        }
    }

    // 查询某天体的详情 (id 为空时返回列表项自带的信息)
    //
    // ★ 为什么要分两条路:
    //   具名星系 (M31/M87/...) 的完整数据在 C++ 的 cosmosdata 里,
    //   要通过 scene.galaxyDetail(id) 取;
    //   而大尺度结构 (拉尼亚凯亚/巨壁/空洞) **没有照片也没有 id**,
    //   它们的描述就在列表项里 —— 直接沿用即可, 避免再走一趟 C++。
    function location(id) {
        if (!id || id.length === 0)
            return ({})

        const d = scene.galaxyDetail(id)
        if (!d || d.nameCn === undefined)
            return ({})

        // 类型文字
        const names = ["螺旋星系", "椭圆星系", "不规则星系",
                       "矮星系", "环状星系"]
        d.typeText = (d.type !== undefined && d.type >= 0 && d.type < 5)
                     ? names[d.type] : "星系"

        // ★ 无图时给出**具体原因** —— 比一句"暂无"更有教学价值
        if (!d.photo || d.photo.length === 0) {
            d.noPhotoWhy = (d.type === 1)
                ? "这是室女座星系团中的椭圆星系, 目前没有单独的高分辨率观测图像。"
                : "该天体暂无单独的高质量观测图像。"
        }
        return d
    }

    // 结构列表项 -> 详情卡数据
    //
    // ★ 为什么不复用 location(): 大尺度结构**没有 id**,
    //   scene.galaxyDetail() 查不到它们 (C++ 侧返回空表)。
    //   它们的全部信息就在列表项里, 直接搬运即可。
    //
    // ★ 但**具名星系仍要走 C++** —— 因为实测红移 (redshift) 和照片
    //   只在 C++ 的 cosmosdata 里, 列表项里没有。混用会导致点开 M31
    //   看不到红移区块。
    function cardForStruct(item) {
        if (!item)
            return ({})

        // 有 id -> 具名星系, 走完整数据源
        if (item.id && item.id.length > 0) {
            const d = location(item.id)
            if (d && d.nameCn !== undefined)
                return d
        }

        // 无 id -> 大尺度结构
        if (item.redshift === undefined)
            return ({})       // 数据未就绪

        return ({
            nameCn: item.name,
            nameEn: item.en,
            desc: item.desc,
            distText: item.dist,
            // ★ 剥掉前缀再交给卡片。
            //   C++ 的 size 字段是给**列表**用的, 自带 "尺度 "/"直径 " 前缀;
            //   而卡片左侧本来就有一列标签 ("尺度"), 直接用会变成
            //   "尺度    尺度 14 亿光年" —— 实测踩到过。
            sizeText: (item.size || "").replace(/^(尺度|直径)\s*/, ""),
            // ★ 推算红移 (哈勃定律), 与星系的实测红移区分开。
            //   卡片据此切换解读措辞 (见 redshiftBlock 里的说明)。
            redshift: item.redshift,
            redshiftDerived: true,
            // 没有 photo 字段 -> 卡片走"暂无实景图"分支, 并给出原因
            noPhotoWhy: "这是大尺度结构 —— 由星系的速度场与密度场"
                      + "划定的边界, 不是能被拍下来的单个天体。"
        })
    }

    // 按名字找大尺度结构 (仅测试入口用)
    function cardForStructByName(name) {
        const list = scene.cosmosStructures()
        for (let i = 0; i < list.length; ++i) {
            const it = list[i]
            if (it.name === name || it.en === name)
                return cardForStruct(it)
        }
        return ({})
    }

    // B.1/B.2 新增条目详情: 先试恒星链, 再试 AGN-星系-暂现源。
    // ★ 复用 galaxyCard: 卡片字段 (nameCn/desc/pop/distText/spec/
    //   teffText/massText/cat/noPhotoWhy) 两边已对齐, 无需新卡片。
    function stellarAgnDetail(id) {
        if (!id || id.length === 0)
            return ({})
        let d = scene.stellarDetail(id)
        if (d && d.nameCn !== undefined)
            return d
        d = scene.agnDetail(id)
        if (d && d.nameCn !== undefined)
            return d
        // ★ B.5 星云/星团: 第三顺位 (id 空间独立, 无冲突)
        d = scene.ismDetail(id)
        if (d && d.nameCn !== undefined)
            return d
        return ({})
    }

    // 3D 场景里的标签 -> 详情卡数据
    //
    // ★ 与"结构列表点击"的区别: 标签里**只有** 名字 / 距离文本 / id,
    //   没有 desc、没有红移 —— 必须回查数据表补齐。
    //   所以这里不能直接喂给 cardForStruct()。
    //
    // ★ 两条查表路径的先后不能颠倒:
    //   先试星系 (location) —— 它给的是**实测红移**, 信息最全;
    //   再试大尺度结构 —— 它们没有数据表 id, 用英文名匹配。
    function cardForMarker(m) {
        if (!m)
            return ({})

        const id = (m.id !== undefined && m.id !== null) ? m.id : ""

        if (id.length > 0) {
            const g = location(id)
            if (g && g.nameCn !== undefined)
                return g
            const s = cardForStructByName(id)
            if (s && s.nameCn !== undefined)
                return s
        }

        // 退路: 用显示名再试一次 (nameCn)
        return cardForStructByName(m.text || "")
    }
}

