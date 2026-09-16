"""还原注释中被误加的转义 (\\" -> ")，字符串里的保持不变。

★ 起因: 之前用正则把「字符串内部的中文引号误写成 ASCII 引号」批量修正时,
  把注释里的引号也一起转义了。注释里出现 \\" 是纯噪声, 影响可读性。

★ 为什么需要状态机:
  不能简单地全局替换 —— 字符串里的 \\" 是必需的 (否则字面量会提前结束),
  而注释里的 \\" 是多余的。必须逐字符跟踪当前处于
  代码区 / 字符串 / 字符常量 / 行注释 / 块注释 中的哪一种。
"""
import glob
import os

SRC = 'D:/tmp/solar-system-cpp/src'

BACKSLASH = chr(92)     # 反斜杠, 避免本文件自身的转义混乱
QUOTE = chr(34)         # 双引号
APOS = chr(39)          # 单引号


def fix(src: str):
    out = []
    i = 0
    n = len(src)
    in_str = False
    in_chr = False
    in_line_c = False
    in_block_c = False
    changed = 0

    while i < n:
        c = src[i]

        if in_line_c:
            if c == BACKSLASH and i + 1 < n and src[i + 1] == QUOTE:
                out.append(QUOTE); i += 2; changed += 1; continue
            out.append(c)
            if c == '\n':
                in_line_c = False
            i += 1
            continue

        if in_block_c:
            if c == '*' and i + 1 < n and src[i + 1] == '/':
                out.append('*/'); i += 2; in_block_c = False; continue
            if c == BACKSLASH and i + 1 < n and src[i + 1] == QUOTE:
                out.append(QUOTE); i += 2; changed += 1; continue
            out.append(c); i += 1
            continue

        if in_str:
            if c == BACKSLASH and i + 1 < n:
                out.append(src[i:i + 2]); i += 2; continue
            if c == QUOTE:
                in_str = False
            out.append(c); i += 1
            continue

        if in_chr:
            if c == BACKSLASH and i + 1 < n:
                out.append(src[i:i + 2]); i += 2; continue
            if c == APOS:
                in_chr = False
            out.append(c); i += 1
            continue

        # 普通代码区
        if c == '/' and i + 1 < n and src[i + 1] == '/':
            out.append('//'); i += 2; in_line_c = True; continue
        if c == '/' and i + 1 < n and src[i + 1] == '*':
            out.append('/*'); i += 2; in_block_c = True; continue
        if c == QUOTE:
            in_str = True; out.append(c); i += 1; continue
        if c == APOS:
            in_chr = True; out.append(c); i += 1; continue
        out.append(c); i += 1

    return ''.join(out), changed


def main():
    total = 0
    for p in sorted(glob.glob(os.path.join(SRC, '*.cpp'))
                    + glob.glob(os.path.join(SRC, '*.h'))):
        s = open(p, encoding='utf-8').read()
        s2, c = fix(s)
        if c:
            open(p, 'w', encoding='utf-8').write(s2)
            print(f"{os.path.basename(p):22s} 注释还原 {c} 处")
            total += c
    print(f"\n共还原 {total} 处")


if __name__ == '__main__':
    main()
