"""修复 C++ 字符串里的「中文引号误写成 ASCII 引号」问题。

★ 问题本质:
  中文文本里的引号 (如 "行星") 本该用中文引号, 但常常被误打成 ASCII 的
  `"`。在 C++ 字符串字面量内部, 这会让字面量**提前结束**, 编译报
  "unable to find string literal operator" 之类的怪错。

★ 为什么不能简单用正则:
  单看一个 `"` 无法判断它是「字符串定界符」还是「内容里的引号」——
  两种情况都可能两侧都是汉字。例如:
      "宇宙学原理"。      <- 结尾的 " 是**定界符**
      "这就是"宇宙学原理"" <- 中间的 " 是**内容**
  两者形态完全一样。

★ 采用的判据 (针对本项目固定的代码风格):
  每一行字符串都遵循 "[内容]", 或 "[内容]", 的形式。因此:
    * 行内**第一个** ASCII 引号 = 开定界符 (若行以它开头)
    * 行内**最后一个** ASCII 引号 = 闭定界符 (若其后只跟 , ) ; 或空白)
    * 中间的、以及行内多余成对的 = 内容引号 -> 转义为 \\"

★ 安全网:
  改完后用编译器反馈验证。本脚本只改「行内有 >2 个引号」的行,
  且保证改造后该行的引号总数为偶数 —— 不满足就跳过并报告。
"""
import glob
import os
import sys

SRC = 'D:/tmp/solar-system-cpp/src'
QUOTE = chr(34)
BACKSLASH = chr(92)
ESC = BACKSLASH + QUOTE


def is_string_line(stripped: str) -> bool:
    """判断这是否是一个 C++ 字符串字面量行。"""
    return stripped.startswith(QUOTE) or stripped.startswith('//') is False and (
        stripped.count(QUOTE) >= 2)


def fix_line(line: str):
    """返回 (新行, 修改数)。只处理形如 "..." 的字符串行。"""
    # 去掉行尾换行后分析
    body = line.rstrip('\n')
    stripped = body.strip()

    # 跳过纯注释行 (但注释里也可能有引号 -> 那属于另一种处理, 见下)
    if stripped.startswith('//'):
        return line, 0
    # 只处理以引号开头、或以引号结尾的行 (即字符串字面量的起止行)
    if QUOTE not in body:
        return line, 0

    # 找出所有 ASCII 引号的位置 (跳过已转义的 \")
    positions = []
    i = 0
    while i < len(body):
        if body[i] == BACKSLASH:
            i += 2
            continue
        if body[i] == QUOTE:
            positions.append(i)
        i += 1

    if len(positions) <= 2:
        return line, 0      # 正常的 "..." -> 无需改

    # 第一个引号: 若它前面只有空白/逗号/括号, 视为开定界符
    first = positions[0]
    prefix = body[:first].strip()
    open_is_delim = prefix == '' or prefix.endswith((',', '(', '{', '[', '='))

    # 最后一个引号: 若它后面只有 , ) ; 空白, 视为闭定界符
    last = positions[-1]
    suffix = body[last + 1:].strip()
    close_is_delim = suffix == '' or suffix.startswith((',', ')', ';', '}'))

    interior = positions
    if open_is_delim:
        interior = [p for p in interior if p != first]
    if close_is_delim and last in interior:
        interior = [p for p in interior if p != last]

    if not interior:
        return line, 0

    # 从后往前替换, 避免位置偏移
    out = body
    for p in reversed(interior):
        out = out[:p] + ESC + out[p + 1:]

    return out + '\n', len(interior)


def main():
    dry = '--dry' in sys.argv
    total = 0
    files = sorted(glob.glob(os.path.join(SRC, '*.cpp'))
                   + glob.glob(os.path.join(SRC, '*.h')))
    for p in files:
        src = open(p, encoding='utf-8').read()
        lines = src.split('\n')
        changed = 0
        new_lines = []
        for idx, ln in enumerate(lines, 1):
            nl, c = fix_line(ln + '\n')
            nl = nl.rstrip('\n')
            if c:
                # 安全网: 改完后引号总数必须是偶数, 否则回退
                if (nl.count(QUOTE) - nl.count(ESC)) % 2 != 0:
                    print(f"  !! {os.path.basename(p)}:{idx} 改造后引号仍为奇数, 跳过")
                    new_lines.append(ln)
                    continue
                changed += c
            new_lines.append(nl)
        if changed:
            if dry:
                print(f"{os.path.basename(p):22s} 需修 {changed} 处 (dry-run)")
            else:
                open(p, 'w', encoding='utf-8').write('\n'.join(new_lines))
                print(f"{os.path.basename(p):22s} 修复 {changed} 处")
            total += changed
    print(f"\n共 {total} 处" + (" (dry-run, 未写入)" if dry else ""))


if __name__ == '__main__':
    main()
