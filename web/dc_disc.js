// Puts a pnj.pack into the Dreamcast disc template built by CI (tools/ci/dc_build.sh).
// CI builds the whole disc with mkdcdisc around a stand-in PNJ.PACK of a fixed size, and writes down where that file's sectors sit in the image
// (pacnjump-dc.json: offset of the first sector, sector size, count). Here the real pack replaces the stand-in's sectors and every sector's EDC and ECC
// are computed again, so the disc is the same one mkdcdisc would have written for that pack. Nothing else in the image changes.
// node web/test_dcdisc.mjs template.cdi manifest.json pack mkdcdisc-built.cdi   checks the result against an image mkdcdisc built from the same pack.

const EDC_LUT = new Uint32Array(256), ECC_F = new Uint8Array(256), ECC_B = new Uint8Array(256);
for (let i = 0; i < 256; i++) {
  let e = i; for (let j = 0; j < 8; j++) e = (e >>> 1) ^ (e & 1 ? 0xD8018001 : 0); EDC_LUT[i] = e >>> 0;
  const j = (i << 1) ^ (i & 0x80 ? 0x11D : 0); ECC_F[i] = j & 255; ECC_B[i ^ (j & 255)] = i;
}
function edc(buf, from, to) { let e = 0; for (let i = from; i < to; i++) e = ((e >>> 8) ^ EDC_LUT[(e ^ buf[i]) & 255]) >>> 0; return e; }
function eccBlock(s, majorCount, minorCount, majorMult, minorInc, dest) {
  const size = majorCount * minorCount;
  for (let major = 0; major < majorCount; major++) {
    let index = (major >> 1) * majorMult + (major & 1), a = 0, b = 0;
    for (let minor = 0; minor < minorCount; minor++) {
      const t = s[0x0C + index]; index += minorInc; if (index >= size) index -= size;
      a ^= t; b ^= t; a = ECC_F[a];
    }
    a = ECC_B[ECC_F[a] ^ b];
    s[dest + major] = a; s[dest + major + majorCount] = a ^ b;
  }
}
// raw: a 2352 byte sector holding subheader + data at 16..2072. Fills EDC (2072) and ECC P (0x81C) and Q (0x8C8) the way Mode 2 Form 1 wants them (header counted as zero).
function sealSector(raw) {
  const e = edc(raw, 16, 2072); raw[2072] = e & 255; raw[2073] = (e >>> 8) & 255; raw[2074] = (e >>> 16) & 255; raw[2075] = e >>> 24;
  raw[12] = raw[13] = raw[14] = raw[15] = 0;
  eccBlock(raw, 86, 24, 2, 86, 0x81C); eccBlock(raw, 52, 43, 86, 88, 0x8C8);
}
export function patchCdi(template, m, pack) {
  const cap = m.sectors * 2048;
  if (pack.length > cap) throw new Error(`The game data is ${pack.length} bytes and the disc template has room for ${cap}.`);
  const out = new Uint8Array(template), raw = new Uint8Array(2352);
  for (let i = 0; i < m.sectors; i++) {
    const base = m.offset + i * m.stride, pay = base + m.pre, from = i * 2048, n = Math.max(0, Math.min(2048, pack.length - from));
    out.fill(0, pay, pay + 2048);
    if (n > 0) out.set(pack.subarray(from, from + n), pay);
    if (m.stride === 2048) continue;
    raw.fill(0);
    raw.set(out.subarray(pay - 8, pay), 16); raw.set(out.subarray(pay, pay + 2048), 24);
    sealSector(raw);
    out.set(raw.subarray(2072, 2352), pay + 2048);
  }
  return out;
}
