"""解析 SDSS eBOSS 的 FITS 星表，提取星系的真实三维位置。

★★ 数据来源 (权威、可查证):
   SDSS DR17 官方数据站:
     https://data.sdss.org/sas/dr17/eboss/lss/catalogs/DR16/

   文件:
     eBOSS_LRG_clustering_data-NGC-vDR16.fits   7.4 MB  (北银极)
     eBOSS_LRG_clustering_data-SGC-vDR16.fits   4.6 MB  (南银极)

   这是什么: **eBOSS 巡天 (TDSS/eBOSS 项目) 的 LRG (亮红星系) 聚类样本**。
   每个 LRG 都有**光谱测定的红移 z**, 由红移可换算成**共动距离** ——
   于是我们得到真实的星系三维位置, 而不是程序生成的假分布。

   ★ 为什么选 LRG:
     LRG 是"亮红星系", 多为大质量椭圆星系, 在 z~0.6-1.0 范围内
     构成最亮的示踪物, 且**成团性强** —— 正好用来展示宇宙网的纤维与巨壁。

★★ 为什么不用 astropy:
   本机 pip 安装被沙箱终止。而 FITS 的结构并不复杂:
     * 头文件是 2880 字节一块的 ASCII 卡片 (key=value 定宽格式)
     * 数据区的字节序、位宽由头文件里的 BITPIX/NAXIS 决定
   自己解析 200 行就够, 且**零依赖**, 反而更稳。

★ 关键字段 (eBOSS LRG clustering 星表):
   RA, DEC     —— 赤经赤纬 (度)
   Z           —— 红移 (光谱测定)
   WEIGHT_SYSTOT / WEIGHT_CP / WEIGHT_NOZ
                —— 系统权重 (用于校正观测选择效应, 见下方说明)
   NGC/SGC     —— 由文件名区分南北银极

用法:
    python parse_sdss.py --probe         只打印表头与字段
    python parse_sdss.py --dump <n>      导出前 n 个星系 (调试)
    python parse_sdss.py --export        导出为 C++ 可读的二进制
"""
import math
import os
import struct
import sys

SRC = r'D:\tmp\sdss'
OUT_CPP = r'D:\tmp\solar-system-cpp\assets\lss'

# ---- 宇宙学参数 (Planck 2018, 与项目其他部分保持一致) ----
# 用于把红移换算成共动距离
H0 = 67.4          # km/s/Mpc
OM0 = 0.315
C_KMS = 299792.458  # 光速 km/s


def read_fits(path, max_rows=None, hdu_index=None):
    """极简 FITS 读取器。返回 (fields, rows, naxis2)。

    ★★ 必须找**含数据的那个 HDU**, 不能用第一个。
      实测踩到: eBOSS 星表的第一个 HDU 是空的 (NAXIS=0) ——
      FITS 常把主 HDU 留空, 真正的表在第二个 HDU (EXTENSION)。
      只看第一个 HDU 会报 "NAXIS=0" 而误判成"文件格式不对"。

    ★ HDU 的排布规则:
      每个 HDU = 头文件 (2880 的整数倍) + 数据区 (2880 的整数倍, 可能为 0)
      要跳到下一个 HDU, 必须**按数据区大小对齐**, 而不是直接往后读。
    """
    with open(path, 'rb') as f:
        # 逐个 HDU 前进, 直到找到 NAXIS>=1 的那个
        for _ in range(8):                    # 最多找 8 个 HDU
            start = f.tell()

            # ---- 读这个 HDU 的头文件 ----
            header = b''
            while True:
                block = f.read(2880)
                if not block:
                    raise ValueError('文件在头文件结束前就没了')
                header += block
                if b'END' in block:
                    break

            cards = {}
            for i in range(0, len(header), 80):
                card = header[i:i + 80].decode('ascii', 'ignore')
                if card.startswith('END'):
                    break
                key = card[:8].strip()
                if not key or key in ('COMMENT', 'HISTORY', ''):
                    continue
                if card[8:10] == '= ':
                    v = card[10:].split('/')[0].strip()
                    # ★★ 字符串值要**去掉两侧的单引号**。
                    #   FITS 写法是  TTYPE1  = 'RA      ' / 注释
                    #   引号闭合在注释之前, 上面的 '/' 切分把
                    #   "'RA      '" 整个留下了 —— 于是字典键带引号,
                    #   后面按 'RA' 查永远取不到, 表现为"读出来全是 0"。
                    #   TFORM 同理会变成 "'D       '", 导致 code 取到空格、
                    #   字段宽度算错, 整个字节偏移全乱。
                    if len(v) >= 2 and v.startswith("'") and v.endswith("'"):
                        v = v[1:-1].strip()
                    cards[key] = v

            naxis = int(cards.get('NAXIS', 0))
            has_data = cards.get('XTENSION') is not None or naxis >= 1
            # 表数据的字节数 (用于计算下一个 HDU 的位置)
            if naxis >= 1:
                gcount = int(cards.get('GCOUNT', 1))
                pcount = int(cards.get('PCOUNT', 0))
                naxisn = [int(cards.get('NAXIS%d' % i, 0))
                          for i in range(1, naxis + 1)]
                nbytes = gcount * (pcount + (1 if naxisn else 0))
                for v in naxisn:
                    nbytes *= v
                bitpix = abs(int(cards.get('BITPIX', 8)))
                nbytes = nbytes * bitpix // 8
            else:
                nbytes = 0
                if has_data or cards.get('XTENSION'):
                    nbytes = 0

            # 表数据 (BINTABLE)
            if 'BINTABLE' in cards.get('XTENSION', '') or naxis == 2:
                hdr_len = len(header)
                data_start = start + hdr_len
                return _read_table(f, cards, data_start, max_rows)

            # 不是表 -> 跳到下一个 HDU (数据区按 2880 对齐)
            skip = ((nbytes + 2879) // 2880) * 2880
            f.seek(start + len(header) + skip)

        raise ValueError('未找到含数据的 HDU')


def _read_table(f, cards, data_start, max_rows):
    """解析 BINTABLE 的数据区。"""
    naxis1 = int(cards.get('NAXIS1', 0))     # 每行字节数
    naxis2 = int(cards.get('NAXIS2', 0))     # 行数
    if naxis1 <= 0 or naxis2 <= 0:
        raise ValueError('空表 NAXIS1=%d NAXIS2=%d' % (naxis1, naxis2))
    cards = dict(cards)
    with open(f.name, 'rb') as g:
        g.seek(data_start)

        # ---- 3) 字段定义 ----
        tfields = int(cards.get('TFIELDS', 0))
        fields = []          # (name, fmt, repeat, offset)
        offset = 0
        for i in range(1, tfields + 1):
            tform = cards.get('TFORM%d' % i, '').strip().strip("'")
            # ★★ TTYPE 的值是**定宽字符串**, 带尾随空格 ——
            #   实测头文件里是 "TTYPE1  = 'RA      '"。
            #   不 strip 的话字典键会是 'RA      ', 而后面的代码按 'RA' 查,
            #   永远取到默认值 0 —— 表现为"读出来的全是 0",
            #   很容易误判成"字节偏移算错了"。
            ttype = cards.get('TTYPE%d' % i, 'COL%d' % i).strip().strip("'")
            # TFORM 形如 'D' / 'E' / '1J' / '10A'
            j = 0
            while j < len(tform) and tform[j].isdigit():
                j += 1
            repeat = int(tform[:j]) if j > 0 else 1
            code = tform[j:j + 1]

            # 字节宽度
            wmap = {'D': 8, 'E': 4, 'J': 4, 'I': 2, 'K': 8, 'B': 1, 'A': 1}
            width = wmap.get(code, 1) * repeat

            # FITS 大端序
            fmt_map = {'D': '>d', 'E': '>f', 'J': '>i', 'I': '>h', 'K': '>q'}
            fields.append({
                'name': ttype, 'code': code, 'repeat': repeat,
                'offset': offset, 'width': width,
                'fmt': fmt_map.get(code),
            })
            offset += width

        rowbytes = offset
        nrows = naxis2 if max_rows is None else min(naxis2, max_rows)

        # ---- 4) 读数据 ----
        rows = []
        for r in range(nrows):
            raw = g.read(rowbytes)
            if len(raw) < rowbytes:
                break
            row = {}
            for fd in fields:
                if fd['fmt'] is None:
                    continue
                v = struct.unpack_from(fd['fmt'], raw, fd['offset'])[0]
                row[fd['name']] = v
            rows.append(row)
        return fields, rows, naxis2


def comoving_distance_mpc(z):
    """红移 -> 共动距离 (Mpc)。用简单的梯形积分算标准 ΛCDM 公式。

    ★ 为什么不用 z*c/H0 的线性近似:
      在 z=1 时线性近似会低估约 30%。eBOSS LRG 的 z 到 1.0,
      用近似会把远处的纤维"压扁", 破坏大尺度结构的形状。
    """
    if z <= 0:
        return 0.0
    n = 256
    dz = z / n
    total = 0.0
    for i in range(n + 1):
        zz = i * dz
        e = math.sqrt(OM0 * (1 + zz) ** 3 + (1 - OM0))
        w = 1.0 if i in (0, n) else (4.0 if i % 2 else 2.0)
        total += w / e
    return (C_KMS / H0) * (dz / 3.0) * total     # Simpson 公式


def main():
    if '--probe' in sys.argv:
        for fn in sorted(os.listdir(SRC)):
            if not fn.endswith('.fits'):
                continue
            p = os.path.join(SRC, fn)
            print('=== %s  (%.1f MB)' % (fn, os.path.getsize(p) / 1048576))
            fields, rows, total = read_fits(p, max_rows=3)
            print('  总行数 %d, 字段 %d 个:' % (total, len(fields)))
            for fd in fields:
                print('     %-24s %s x%d' % (fd['name'], fd['code'], fd['repeat']))
            print('  前 3 行关键字段:')
            for r in rows:
                print('     RA=%.4f DEC=%.4f Z=%.4f  d=%.0f Mly'
                      % (r.get('RA', 0), r.get('DEC', 0), r.get('Z', 0),
                         comoving_distance_mpc(r.get('Z', 0)) * 3.2616))
            print()
        return

    if '--export' in sys.argv:
        os.makedirs(OUT_CPP, exist_ok=True)
        recs = []
        for fn in sorted(os.listdir(SRC)):
            if not fn.endswith('.fits'):
                continue
            fields, rows, total = read_fits(os.path.join(SRC, fn))
            print('%s: %d 行' % (fn, len(rows)))
            for r in rows:
                z = r.get('Z', -1.0)
                # ★ 只保留有红移且在天文合理范围的
                if z is None or z <= 0.001 or z > 1.2:
                    continue
                ra = r.get('RA', None)
                dec = r.get('DEC', None)
                if ra is None or dec is None:
                    continue
                # 系统权重: 用于校正观测选择效应。不用的话,
                # 视野边界的星系密度会被人为压低, 宇宙网出现假空洞。
                w = 1.0
                for k in ('WEIGHT_SYSTOT', 'WEIGHT_CP', 'WEIGHT_NOZ'):
                    if k in r:
                        w *= r[k]
                if w <= 0 or w > 100:
                    w = 1.0
                recs.append((ra, dec, z, w))

        print()
        print('有效星系 %d 个' % len(recs))
        zs = [r[2] for r in recs]
        print('红移范围 %.3f ~ %.3f (中位 %.3f)'
              % (min(zs), max(zs), sorted(zs)[len(zs)//2]))
        ds = [comoving_distance_mpc(r[2]) * 3.2616 for r in recs]
        print('共动距离 %.0f ~ %.0f Mly' % (min(ds), max(ds)))

        # 写二进制: 每记录 4 个 float (ra, dec, z, w)
        out = os.path.join(OUT_CPP, 'lrg.bin')
        with open(out, 'wb') as f:
            f.write(struct.pack('<I', len(recs)))
            for ra, dec, z, w in recs:
                f.write(struct.pack('<4f', ra, dec, z, w))
        print('已写 %s  (%.1f MB)' % (out, os.path.getsize(out) / 1048576))
        return

    if '--dump' in sys.argv:
        i = sys.argv.index('--dump')
        n = int(sys.argv[i + 1]) if i + 1 < len(sys.argv) else 10
        for fn in sorted(os.listdir(SRC))[:1]:
            if not fn.endswith('.fits'):
                continue
            fields, rows, total = read_fits(os.path.join(SRC, fn), max_rows=n)
            print('%s  前 %d 行:' % (fn, n))
            for r in rows:
                print('   RA %8.4f  DEC %8.4f  Z %.4f  d %6.0f Mly'
                      % (r.get('RA', 0), r.get('DEC', 0), r.get('Z', 0),
                         comoving_distance_mpc(r.get('Z', 0)) * 3.2616))
        return

    print(__doc__)


if __name__ == '__main__':
    main()
