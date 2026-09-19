// ============================================================================
//  StellarPanel.qml —— B.1/B.2 恒星链 + AGN 分组列表面板
//
//  ★ 顶栏"恒星"段为 UI-only 视图 (不新增渲染尺度, 复用太阳系画布):
//    由 root.stellarView 控制显隐, 与 scaleLevel 正交。
//  ★ 列表直接绑定 scene.stellarList()/agnList(); 点击走
//    root.stellarAgnDetail(id) 复用 galaxyCard (字段已对齐)。
// ============================================================================

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: stellarPanel
    x: 18; y: 74
    width: 288
    height: parent.height - 74 - 46
    radius: 12
    color: Qt.rgba(0.055, 0.078, 0.125, 0.72)
    border.width: 1
    border.color: Qt.rgba(0.45, 0.62, 0.88, 0.16)

    property var stellar: []
    property var agns: []
    property var isms: []
    property string tab: "star"   // star | agn | ism

    Component.onCompleted: {
        stellar = scene.stellarList()
        agns = scene.agnList()
        isms = scene.ismList()
        // 测试用: SS_STELLAR=1 启动即显示恒星面板 (供自动化截图验证)
        if (scene.testStellar) {
            root.stellarView = true
            console.log("[恒星] 已打开: " + stellar.length + " 恒星条目, "
                        + agns.length + " AGN条目, "
                        + isms.length + " ISM/星团条目")
        }
    }

    function openDetail(id) {
        const d = root.stellarAgnDetail(id)
        if (d && d.nameCn !== undefined) {
            galaxyCard.detail = d
            galaxyCard.visible = true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 9

        Text {
            text: "恒星 · 活动星系"
            color: root.cText
            font.pixelSize: 13
            font.bold: true
            font.family: root.sansFont
        }

        // ---- 分组切换 ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: [
                    { t: "恒星链", v: "star" },
                    { t: "AGN/星系", v: "agn" },
                    { t: "星云星团", v: "ism" }
                ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    radius: 6
                    color: stellarPanel.tab === modelData.v
                           ? root.cAccentSoft : Qt.rgba(1, 1, 1, 0.05)
                    border.width: 1
                    border.color: stellarPanel.tab === modelData.v
                                  ? Qt.rgba(0.37, 0.66, 1.0, 0.55)
                                  : Qt.rgba(1, 1, 1, 0.08)
                    Text {
                        anchors.centerIn: parent
                        text: modelData.t
                        color: stellarPanel.tab === modelData.v
                               ? "#bcd9ff" : root.cTextDim
                        font.pixelSize: 10
                        font.family: root.sansFont
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: stellarPanel.tab = modelData.v
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: stellarPanel.tab === "star"
                  ? "B.1 恒星全链 53 条 · 点击查看精准/通俗双说明"
                  : (stellarPanel.tab === "agn"
                     ? "B.2/B.3 AGN 与星系 34 条 · 候选与争议已注记"
                     : "B.5 星云/星团 22 条 · 发射/反射/暗/行星状/遗迹/星团")
            color: root.cTextDim
            font.pixelSize: 9
            font.family: root.sansFont
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: listCol.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: listCol
                width: parent.width - 4
                spacing: 5

                Repeater {
                    model: stellarPanel.tab === "star" ? stellarPanel.stellar
                           : (stellarPanel.tab === "agn" ? stellarPanel.agns
                                                         : stellarPanel.isms)
                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        radius: 5
                        color: stClick.containsMouse
                               ? Qt.rgba(0.37, 0.66, 1.0, 0.18)
                               : Qt.rgba(1, 1, 1, 0.035)
                        border.width: 1
                        border.color: stClick.containsMouse
                                      ? Qt.rgba(0.45, 0.72, 1.0, 0.55)
                                      : Qt.rgba(1, 1, 1, 0.08)
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            anchors.topMargin: 4
                            anchors.bottomMargin: 4
                            spacing: 1
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text {
                                    text: modelData.name
                                    color: root.cText
                                    font.pixelSize: 10
                                    font.family: root.sansFont
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: modelData.cat
                                    color: root.cAccent
                                    font.pixelSize: 9
                                    font.family: root.sansFont
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text {
                                    text: modelData.spec !== undefined ? modelData.spec : modelData.en
                                    color: root.cTextDim
                                    font.pixelSize: 9
                                    font.family: root.sansFont
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: modelData.dist
                                    color: root.cTextDim
                                    font.pixelSize: 9
                                    font.family: root.fitFont(modelData.dist)
                                }
                            }
                        }
                        MouseArea {
                            id: stClick
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: stellarPanel.openDetail(modelData.id)
                        }
                    }
                }
            }
        }
    }
}
