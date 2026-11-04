#!/usr/bin/env python3
"""Reads the sprite rectangle table out of the original game's classes.dex.

The table is not data in the APK, it is compiled into the static initialiser of one class (jp/co/zoo/pacnjump/b/i):
224 x  new Sprite(tex, Rect.a(x, y, w, h, tw, th)).  This walks that initialiser instruction by instruction, tracking
only constants and register moves, and reads the arguments off each call. Nothing else in the dex is interpreted.

Output text format (what goes into the pack as meta/atlas.txt): one sprite per line, "tex x y w h tw", in table order.
The C importer (platform/sdl/import.h) and the browser builder (web/builder.js) do the same walk; keep the three in sync.
"""
import struct

CLASS = b'Ljp/co/zoo/pacnjump/b/i;'
COUNT = 224

# instruction length in 16-bit code units for opcodes 0x00..0xff (Dalvik, dex 035)
_LEN = [1] * 256
for _lo, _hi, _n in [(0x02, 0x02, 2), (0x03, 0x03, 3), (0x05, 0x05, 2), (0x06, 0x06, 3), (0x08, 0x08, 2), (0x09, 0x09, 3), (0x13, 0x13, 2), (0x14, 0x14, 3),
                     (0x15, 0x16, 2), (0x17, 0x17, 3), (0x18, 0x18, 5), (0x19, 0x19, 2), (0x1a, 0x1a, 2), (0x1b, 0x1b, 3), (0x1c, 0x1c, 2), (0x1f, 0x20, 2),
                     (0x22, 0x23, 2), (0x24, 0x26, 3), (0x29, 0x29, 2), (0x2a, 0x2c, 3), (0x2d, 0x3d, 2), (0x44, 0x6d, 2), (0x6e, 0x72, 3), (0x74, 0x78, 3),
                     (0x90, 0xaf, 2), (0xd0, 0xe2, 2)]:
    for _i in range(_lo, _hi + 1): _LEN[_i] = _n


class DexError(Exception):
    pass


def _uleb(b, p):
    v = 0; s = 0
    while True:
        c = b[p]; p += 1; v |= (c & 0x7f) << s; s += 7
        if not c & 0x80: return v, p


def _find_clinit(b):
    if b[:4] != b'dex\n': raise DexError('classes.dex is not a dex file')
    str_n, str_off, type_n, type_off, _, _, fld_n, fld_off, meth_n, meth_off, cls_n, cls_off = struct.unpack_from('<12I', b, 0x38)

    def string(i):
        p = struct.unpack_from('<I', b, str_off + 4 * i)[0]; _, p = _uleb(b, p); e = b.index(b'\0', p); return b[p:e]
    def type_name(i): return string(struct.unpack_from('<I', b, type_off + 4 * i)[0])
    def method_name(i): return string(struct.unpack_from('<I', b, meth_off + 8 * i + 4)[0])   # method_id: class u16, proto u16, name u32

    for c in range(cls_n):
        cidx, _, _, _, _, _, data_off, _ = struct.unpack_from('<8I', b, cls_off + 32 * c)
        if type_name(cidx) != CLASS or not data_off: continue
        p = data_off; sf, p = _uleb(b, p); inf, p = _uleb(b, p); dm, p = _uleb(b, p); vm, p = _uleb(b, p)
        for _ in range((sf + inf) * 2): _, p = _uleb(b, p)
        mi = 0
        for _ in range(dm + vm):
            d, p = _uleb(b, p); _, p = _uleb(b, p); code, p = _uleb(b, p); mi += d
            if method_name(mi) == b'<clinit>' and code: return code
    raise DexError('this APK is not the version the engine knows (sprite table class not found)')


def read_atlas(dex):
    b = dex
    code = _find_clinit(b)
    size = struct.unpack_from('<I', b, code + 12)[0]
    u = struct.unpack_from('<%dH' % size, b, code + 16)
    reg = {}; pend = None; out = []; p = 0
    while p < size:
        w = u[p]; op = w & 0xff; n = _LEN[op]
        a4 = (w >> 8) & 15; b4 = w >> 12; aa = w >> 8
        if op in (0x01, 0x07): reg[a4] = reg.get(b4, 0)              # move, move-object
        elif op in (0x02, 0x08): reg[aa] = reg.get(u[p + 1], 0)      # move/from16
        elif op in (0x03, 0x09): reg[u[p + 1]] = reg.get(u[p + 2], 0)
        elif op == 0x12: reg[a4] = b4 - 16 if b4 > 7 else b4         # const/4
        elif op == 0x13: v = u[p + 1]; reg[aa] = v - 65536 if v > 32767 else v
        elif op == 0x14: v = u[p + 1] | u[p + 2] << 16; reg[aa] = v - (1 << 32) if v >> 31 else v
        elif op == 0x15: reg[aa] = u[p + 1] << 16
        elif op == 0x77 and aa == 6:                                 # invoke-static/range, six int args: Rect.a(x, y, w, h, tw, th)
            c0 = u[p + 2]; pend = [reg.get(c0 + i, 0) for i in range(6)]
        elif op == 0x70 and (w >> 12) == 3 and pend:                 # invoke-direct Sprite.<init>(this, tex, rect)
            tex = reg.get((u[p + 2] >> 4) & 15, 0)
            out.append((tex, pend[0], pend[1], pend[2], pend[3], pend[4])); pend = None
        elif op in (0x0e, 0x0f, 0x10, 0x11): break
        elif 0x28 <= op <= 0x3d: raise DexError('unexpected branch in the sprite table initialiser')
        p += n
    if len(out) != COUNT: raise DexError('found %d sprites, expected %d (unsupported APK version)' % (len(out), COUNT))
    return out


def atlas_text(dex):
    return ''.join('%d %d %d %d %d %d\n' % r for r in read_atlas(dex)).encode()


if __name__ == '__main__':
    import sys, zipfile
    z = zipfile.ZipFile(sys.argv[1]); sys.stdout.write(atlas_text(z.read('classes.dex')).decode())
