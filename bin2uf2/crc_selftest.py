# -*- coding: utf-8 -*-
"""验证 bootloader/ware/img_crc.c 的 CRC 用法与 STM32 硬件单元语义一致。

约定: 参与校验的数据尾部不足一个字时按 0xFF(Flash 擦除态) 补齐。
img_crc.c 的做法是
    w = b0 | b1<<8 | b2<<16 | b3<<24   (小端组装, b0 为地址最低字节)
    CRC->DR = __REV(w)
硬件每写一次 DR 会按 4 字节从高到低处理, 翻转后正好等于按 b0,b1,b2,b3 顺序喂入。
"""
POLY = 0x04C11DB7
MASK = 0xFFFFFFFF

def _mk_table():
    t = []
    for i in range(256):
        c = i << 24
        for _ in range(8):
            c = ((c << 1) ^ POLY) & MASK if (c & 0x80000000) else (c << 1) & MASK
        t.append(c)
    return t
T = _mk_table()

def crc_seq(data):
    """逐字节喂入"""
    crc = MASK
    for b in data:
        crc = T[((crc >> 24) ^ b) & 0xFF] ^ ((crc << 8) & MASK)
    return crc

def rev32(x):
    return int.from_bytes(x.to_bytes(4, "little"), "big")

def crc_hw_emul(data):
    """模拟 img_crc.c 的 crc_unit_calc, 含尾部 0xFF 补齐"""
    crc = MASK
    i, n = 0, len(data)
    words = []
    while i + 4 <= n:
        words.append(data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24))
        i += 4
    if i < n:
        w = 0xFFFFFFFF
        for k in range(n - i):
            w = (w & ~(0xFF << (8 * k))) & MASK
            w |= data[i + k] << (8 * k)
        words.append(w)
    for w in words:
        r = rev32(w)
        for shift in (24, 16, 8, 0):
            crc = T[((crc >> 24) ^ ((r >> shift) & 0xFF)) & 0xFF] ^ ((crc << 8) & MASK)
    return crc

def expected(data):
    """按约定补齐后再逐字节算, 这是硬件应得到的结果"""
    pad = (-len(data)) % 4
    return crc_seq(bytes(data) + b"\xff" * pad)

import random
random.seed(7)
cases = [b"", b"123456789", bytes(range(256)), b"\xff" * 13]
for n in (4, 16, 17, 260, 1023, 4096):
    cases.append(bytes(random.randrange(256) for _ in range(n)))

bad = 0
for c in cases:
    h, e = crc_hw_emul(c), expected(c)
    if h != e:
        print("MISMATCH len=%d hw=%08x exp=%08x" % (len(c), h, e)); bad += 1

print("'123456789' ->", hex(crc_seq(b"123456789")), "(STM32 参考值 0x0376E6E7)")
print("cases=%d  hw==expected: %s" % (len(cases), "OK" if bad == 0 else "FAIL"))
