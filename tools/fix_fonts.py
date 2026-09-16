"""统一 QML 字体与列表对齐。

★★ 两个问题:

  **问题 A: 字体混乱**
   项目里 8 处硬编码 `font.family: "Consolas, Menlo, monospace"`,
   而其余文本不设 family 走系统默认。后果:
     * 中文文本被等宽字体渲染 —— 笔画间出现多余空隙, 看起来松散
     * 银河系/宇宙面板的**说明文字**(整段中文)也套了等宽,
       变成衬线感, 与太阳系面板不一致

   ★ 正确原则:
       数值/日期/代号  -> 等宽 (对齐需要, 且数字宽度一致)
       中文文本/名称   -> 系统无衬线 (CJK 字形完整)
       英文标签       -> 系统无衬线

  **问题 B: 天体列表对齐**
   实测发现三处不齐:
     * 中文名与英文名的左边缘不齐 (中文有字距补偿)
     * 分类标签"行星"随中文名宽度浮动, 未右对齐
     * 中文被等宽字体拉松

   ★ 修正:
     * 中英文两行都用 `horizontalAlignment: Text.AlignLeft` 且父容器
       左对齐 (ColumnLayout 默认已左对齐, 但要显式设 text 对齐;
       更关键的是**去掉任何隐式居中**)
     * 分类标签固定宽度 + 右对齐, 形成整齐的一列
     * 中文名不用等宽

用法:
    python fix_fonts.py --check
    python fix_fonts.py
"""
import os
import re
import sys

P = r'D:\tmp\solar-system-cpp\qml\Main.qml'

# ---- 字体常量定义 (插到 ApplicationWindow 的属性区) ----
FONT_PROPS = '''
    // ---- 字体规范 ----
    //
    // ★ 统一原则 (此前是散落各处的硬编码, 导致中文被等宽字体渲染、
    //   字距松散, 且各面板风格不一致):
    //     monoFont  -> 数值/日期/代号 (等宽才能对齐)
    //     sansFont  -> 中文文本、名称、标签 (CJK 字形完整、字距正常)
    //
    //   Qt 会按逗号列表依次尝试, 取第一个可用的。
    readonly property string monoFont: "Consolas, Cascadia Mono, Menlo, monospace"
    readonly property string sansFont: "Microsoft YaHei UI, Microsoft YaHei, PingFang SC, Noto Sans CJK SC, Segoe UI, sans-serif"
'''


def main():
    dry = '--check' in sys.argv

    s = open(P, encoding='utf-8').read()
    orig = s

    if 'readonly property string monoFont' in s:
        print('字体常量已存在, 跳过定义')
    else:
        marker = '    // ---- 配色 (玻璃拟态) ----'
        assert marker in s, '未找到配色注释锚点'
        s = s.replace(marker, FONT_PROPS + '\n' + marker, 1)
        print('已插入字体常量 (monoFont / sansFont)')

    # ---- 把所有硬编码 monospace 改为引用常量 ----
    n = s.count('font.family: "Consolas, Menlo, monospace"')
    s = s.replace('font.family: "Consolas, Menlo, monospace"',
                  'font.family: root.monoFont')
    print('替换硬编码 monospace: %d 处' % n)

    # ---- 天体列表: 中英文名左对齐 + 分类标签定宽右对齐 ----
    old_col = '''                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Text {
                                text: modelData.name
                                color: root.cText
                                font.pixelSize: 13
                                font.bold: modelData.id === root.currentId
                            }
                            Text {
                                text: modelData.en
                                color: root.cTextDim
                                font.pixelSize: 10
                            }
                        }

                        Text {
                            text: {
                                const k = modelData.kind
                                if (k === "star") return "恒星"
                                if (k === "moon") return "卫星"
                                return "行星"
                            }
                            color: Qt.rgba(0.56, 0.64, 0.75, 0.75)
                            font.pixelSize: 10
                        }'''

    new_col = '''                        // ★ 中英文名: 两行都**左对齐**, 左边缘严格对齐。
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
                        }'''

    if old_col in s:
        s = s.replace(old_col, new_col, 1)
        print('已修正天体列表对齐')
    else:
        print('警告: 未找到天体列表布局代码')

    if s == orig:
        print('无改动')
        return
    if dry:
        print('(--check 模式, 未写入)')
        return
    open(P, 'w', encoding='utf-8').write(s)
    print('已写入 %s' % P)


if __name__ == '__main__':
    main()
