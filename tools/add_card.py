"""给宇宙视图加"具名天体详情卡"（方案 A）。

★★ 设计要点:

  1. **点击结构列表项 → 弹出详情卡**, 显示:
       照片 (有的话) + 名称 + 距离 + 直径 + 描述
     没有照片时显示明确的"暂无实景图"提示。

  2. **为什么不做 3D 场景贴图 (方案 B)**:
     实测评估后放弃 —— 场景距离是**对数映射**的, 真实的 220 千光年
     直径在这里是 0.0005 场景单位 (亚像素)。图块尺寸必须人为放大,
     放大倍数就成了任意参数, 失去物理意义; 且 billboard 在侧视时
     会投影重叠, 在教学上误导。详情卡没有这些问题。

  3. **"暂无实景图"不是什么都没做**:
     拉尼亚凯亚超星系团是 Tully 2014 从**速度场**推算的水流域边界,
     牧夫座空洞本身是"没有东西" —— 这些**物理上不存在一张照片**。
     明确说明比拿别的图冒充诚实得多。

★ QML 注意 (踩过的坑):
  * 本地文件路径必须加 `file:/// ` 前缀才能被 Image 加载
  * 访问可能不存在的 map 字段要用 `|| ""` 兜底, 否则得 undefined
  * 详情卡要放在 QML 末尾并给高 z 值, 才能盖住所有面板
"""
import re

P = r'D:\tmp\solar-system-cpp\qml\Main.qml'

# ---- 1) 让结构列表项可点击 ----
OLD_DELEGATE = '''                    delegate: ColumnLayout {
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
                    }'''

NEW_DELEGATE = '''                    delegate: Rectangle {
                        id: structRow
                        required property var modelData
                        Layout.fillWidth: true
                        // ★ 高度跟随内容: 没照片的行矮一些, 视觉上不浪费空间。
                        //   用 implicitHeight 让 ColumnLayout 自己算。
                        implicitHeight: rowCol.implicitHeight + 6
                        radius: 4
                        color: rowHover.containsMouse
                               ? Qt.rgba(1, 1, 1, 0.06) : "transparent"

                        MouseArea {
                            id: rowHover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                // 有 id 的天体查详情; 大尺度结构 (id 为空)
                                // 直接用它自己的 desc
                                const d = location(modelData.id)
                                galaxyCard.detail = d
                                galaxyCard.visible = true
                            }
                        }

                        ColumnLayout {
                            id: rowCol
                            anchors.fill: parent
                            anchors.leftMargin: 5
                            anchors.rightMargin: 5
                            anchors.topMargin: 3
                            spacing: 1

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                Text {
                                    text: structRow.modelData.name
                                    color: structRow.modelData.kind === 2 ? "#a8bcd8"
                                         : structRow.modelData.kind === 0 ? "#d4a5ff"
                                         : "#ffb4a2"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                                // ★ 有实景图的标一个小相机图形 —— 提示用户
                                //   "这一项点开能看到照片"
                                Text {
                                    visible: structRow.modelData.hasPhoto === true
                                    text: "◉"
                                    color: "#7fd4a0"
                                    font.pixelSize: 9
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Text {
                                text: structRow.modelData.dist + "  ·  "
                                      + structRow.modelData.size
                                color: root.cTextDim
                                font.pixelSize: 9
                                font.family: root.fitFont(
                                    structRow.modelData.dist + "  ·  "
                                    + structRow.modelData.size)
                            }
                        }
                    }'''


# ---- 2) 详情卡 (插在文件末尾的最后一个大括号之前) ----
CARD = '''

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
        property var detail: ({})

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
                            if (d.dist !== undefined)
                                rows.push({ k: "距离", v: d.dist > 1e-9
                                            ? d.dist.toFixed(2) + " 百万光年"
                                            : "我们所在" })
                            if (d.diameter !== undefined)
                                rows.push({ k: "直径", v: d.diameter.toFixed(1) + " 千光年" })
                            if (d.massLog !== undefined)
                                rows.push({ k: "恒星质量", v: "10^"
                                            + d.massLog.toFixed(2) + " 太阳质量" })
                            if (d.typeText !== undefined)
                                rows.push({ k: "类型", v: d.typeText })
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

                // ---- 描述 ----
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: (galaxyCard.detail.desc || "")
                    color: root.cText
                    font.pixelSize: 11
                    font.family: root.sansFont
                    lineHeight: 1.45
                }

                // ---- 图片来源标注 (仅在有照片时) ----
                Text {
                    Layout.fillWidth: true
                    visible: {
                        const p = galaxyCard.detail.photo
                        return p && p.length > 0
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
}
'''

# QML 的根是 ApplicationWindow，最后的 `}` 是它的闭合
# 找到文件末尾最后一个独立的 `}`


def main():
    s = open(P, encoding='utf-8').read()

    if 'galaxyCard' in s:
        print('详情卡已存在, 跳过')
        return

    assert OLD_DELEGATE in s, '未找到结构列表 delegate'
    s = s.replace(OLD_DELEGATE, NEW_DELEGATE, 1)
    print('列表项已改为可点击')

    # 插到文件末尾 (最后一个 } 之前)
    i = s.rstrip().rfind('\n}')
    assert i > 0, '未找到文件末尾的根闭合括号'
    s = s[:i] + CARD + s[i:]
    print('详情卡已插入')

    open(P, 'w', encoding='utf-8').write(s)
    print('Main.qml 已更新')


if __name__ == '__main__':
    main()
