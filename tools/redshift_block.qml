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
                    Layout.preferredHeight: 134

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

                    // 波长 -> 近似颜色。400~700nm 可见, 之外转灰
                    // (这个视觉断点本身就是教学内容)
                    function wlColor(w) {
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
                    Repeater {
                        model: redshiftTool.lines

                        Rectangle {
                            required property var modelData
                            readonly property real shifted:
                                modelData.w * (1 + redshiftTool.redz)
                            x: spectrum.width
                               * (shifted - redshiftTool.wlMin)
                               / (redshiftTool.wlMax - redshiftTool.wlMin) - 1
                            y: 46
                            width: 2.5
                            height: 20
                            visible: shifted >= redshiftTool.wlMin
                                     && shifted <= redshiftTool.wlMax
                            color: shifted <= 700 ? "#ffffff" : "#8a8d96"

                            Text {
                                x: -9
                                y: 22
                                width: 40
                                horizontalAlignment: Text.AlignHCenter
                                text: modelData.n
                                color: shifted <= 700 ? "#c8d4e8" : "#7a7d86"
                                font.pixelSize: 8
                                font.family: root.sansFont
                            }
                        }
                    }

                    // ---- 结论行 ----
                    Text {
                        x: 0
                        y: 92
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
                        y: 112
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

