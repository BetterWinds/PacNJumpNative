// Dreamcast pack builder, the browser twin of `pacnjump --dcpack` (platform/sdl/main.c) and the VQ encoder in platform/sdl/dtx.h.
// It takes the pack the importer made (builder.js buildPack) and returns a pack that also holds the console textures (dc/<sheet>.dtx).
// The arithmetic follows the C code step by step (float32 via Math.fround, the same random sequence), so for the same input the output is the same file.
// node web/test_dcpack.mjs base.pack pnj.pack   compares this against a pack made by the C tool.

const TEXTURES = ['bnglogo', 'bndr_logo', 'sysparts', 'stages', 'charaparts', 'stage_1_map', 'stage_1_cha', 'stage_2_map', 'stage_2_cha', 'stage_3_map', 'stage_3_cha', 'stage_4_map', 'stage_4_cha', 'stage_5_map', 'stage_5_cha', 'stage_6_map', 'stage_6_cha', 'stage_7_map', 'stage_7_cha', 'stage_8_map', 'stage_8_cha'];
const te = new TextEncoder(), td = new TextDecoder();
const f32 = Math.fround;

// ---------- PNG (8-bit, not interlaced: what the game's sheets are) ----------
async function inflateZlib(bytes) {
  const ds = new DecompressionStream('deflate');
  const w = ds.writable.getWriter(); w.write(bytes); w.close();
  return new Uint8Array(await new Response(ds.readable).arrayBuffer());
}
export async function decodePng(b) {
  const sig = [137, 80, 78, 71, 13, 10, 26, 10];
  for (let i = 0; i < 8; i++) if (b[i] !== sig[i]) throw new Error('not a PNG');
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  let p = 8, w = 0, h = 0, depth = 0, ct = 0, il = 0, pal = null, trns = null; const idat = [];
  while (p + 8 <= b.length) {
    const len = dv.getUint32(p), type = String.fromCharCode(b[p + 4], b[p + 5], b[p + 6], b[p + 7]), d = p + 8;
    if (type === 'IHDR') { w = dv.getUint32(d); h = dv.getUint32(d + 4); depth = b[d + 8]; ct = b[d + 9]; il = b[d + 12]; }
    else if (type === 'PLTE') pal = b.subarray(d, d + len);
    else if (type === 'tRNS') trns = b.subarray(d, d + len);
    else if (type === 'IDAT') idat.push(b.subarray(d, d + len));
    else if (type === 'IEND') break;
    p = d + len + 4;
  }
  if (depth !== 8 || il !== 0) throw new Error('unsupported PNG (needs 8-bit, not interlaced)');
  const ch = ct === 6 ? 4 : ct === 2 ? 3 : ct === 4 ? 2 : (ct === 0 || ct === 3) ? 1 : 0;
  if (!ch) throw new Error('unsupported PNG colour type');
  let total = 0; for (const c of idat) total += c.length;
  const z = new Uint8Array(total); { let o = 0; for (const c of idat) { z.set(c, o); o += c.length; } }
  const raw = await inflateZlib(z), stride = w * ch, cur = new Uint8Array(h * stride);
  for (let y = 0; y < h; y++) {
    const f = raw[y * (stride + 1)], src = y * (stride + 1) + 1, dst = y * stride;
    for (let x = 0; x < stride; x++) {
      const a = x >= ch ? cur[dst + x - ch] : 0, up = y ? cur[dst - stride + x] : 0, c = (x >= ch && y) ? cur[dst - stride + x - ch] : 0;
      let v = raw[src + x];
      if (f === 1) v += a; else if (f === 2) v += up; else if (f === 3) v += (a + up) >> 1;
      else if (f === 4) { const pp = a + up - c, pa = Math.abs(pp - a), pb = Math.abs(pp - up), pc = Math.abs(pp - c); v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? up : c); }
      cur[dst + x] = v & 255;
    }
  }
  const px = new Uint8Array(w * h * 4);
  for (let i = 0; i < w * h; i++) {
    const s = i * ch, o = i * 4;
    if (ct === 6) { px[o] = cur[s]; px[o + 1] = cur[s + 1]; px[o + 2] = cur[s + 2]; px[o + 3] = cur[s + 3]; }
    else if (ct === 2) { px[o] = cur[s]; px[o + 1] = cur[s + 1]; px[o + 2] = cur[s + 2]; px[o + 3] = 255; }
    else if (ct === 0) { px[o] = px[o + 1] = px[o + 2] = cur[s]; px[o + 3] = 255; }
    else if (ct === 4) { px[o] = px[o + 1] = px[o + 2] = cur[s]; px[o + 3] = cur[s + 1]; }
    else { const k = cur[s]; px[o] = pal[k * 3]; px[o + 1] = pal[k * 3 + 1]; px[o + 2] = pal[k * 3 + 2]; px[o + 3] = trns && k < trns.length ? trns[k] : 255; }
  }
  return { w, h, px };
}

// ---------- pnj_pack4444 ----------
export function pack4444(px, w, h, shrink) {
  if (shrink < 1) shrink = 1;
  const dw = (w / shrink) | 0, dh = (h / shrink) | 0, out = new Uint16Array(dw * dh), area = shrink * shrink;
  for (let y = 0; y < dh; y++) for (let x = 0; x < dw; x++) {
    let r, g, b, a;
    if (shrink > 1) {
      let sa = 0, sr = 0, sg = 0, sb = 0;
      for (let j = 0; j < shrink; j++) for (let i = 0; i < shrink; i++) {
        const q = ((y * shrink + j) * w + (x * shrink + i)) * 4, qa = px[q + 3];
        sa += qa; sr += px[q] * qa; sg += px[q + 1] * qa; sb += px[q + 2] * qa;
      }
      if (sa) { r = Math.floor(sr / sa); g = Math.floor(sg / sa); b = Math.floor(sb / sa); } else r = g = b = 0;
      a = Math.floor(sa / area);
    } else { const p = (y * w + x) * 4; r = px[p]; g = px[p + 1]; b = px[p + 2]; a = px[p + 3]; }
    out[y * dw + x] = (Math.floor((a * 15 + 127) / 255) << 12) | (Math.floor((r * 15 + 127) / 255) << 8) | (Math.floor((g * 15 + 127) / 255) << 4) | Math.floor((b * 15 + 127) / 255);
  }
  return { lin: out, w: dw, h: dh };
}

// ---------- VQ encoder (dtx.h, pnj_vq4444) ----------
function twid(v) { let r = 0; for (let i = 0; i < 10; i++) r |= ((v >> i) & 1) << (2 * i); return r >>> 0; }
let rng = 12345;
function rand() { rng = (Math.imul(rng, 1664525) + 1013904223) >>> 0; return rng >>> 8; }
function assign(x, n, cb, out, err) {
  const c2 = new Float32Array(256);
  for (let k = 0; k < 256; k++) { let s = 0; for (let d = 0; d < 16; d++) { const c = cb[k * 16 + d]; s = f32(s + f32(c * c)); } c2[k] = s; }
  for (let i = 0; i < n; i++) {
    const vo = i * 16; let best = 1e30, bk = 0;
    best = f32(best);
    for (let k = 0; k < 256; k++) {
      const co = k * 16; let dot = 0;
      for (let d = 0; d < 16; d++) dot = f32(dot + f32(x[vo + d] * cb[co + d]));
      const dist = f32(c2[k] - f32(2 * dot));
      if (dist < best) { best = dist; bk = k; }
    }
    out[i] = bk;
    if (err) { let vv = 0; for (let d = 0; d < 16; d++) { const v = x[vo + d]; vv = f32(vv + f32(v * v)); } err[i] = f32(vv + best); }
  }
}
// lin: w*h ARGB4444 (w == h, power of two >= 8). Returns the 2048 + (w/2)*(h/2) byte VQ body.
export function vq4444(lin, w, h) {
  if (w !== h || w < 8 || (w & (w - 1)) || w > 1024) throw new Error('size does not suit the console');
  const bw = w >> 1, nb = bw * bw, X = new Float32Array(nb * 16), ix = new Int32Array(nb);
  for (let by = 0; by < bw; by++) for (let bx = 0; bx < bw; bx++) {
    const v = (by * bw + bx) * 16;
    for (let j = 0; j < 4; j++) {
      const p = lin[(2 * by + (j & 1)) * w + (2 * bx + (j >> 1))];
      X[v + j * 4] = (p >> 12) & 15; X[v + j * 4 + 1] = (p >> 8) & 15; X[v + j * 4 + 2] = (p >> 4) & 15; X[v + j * 4 + 3] = p & 15;
    }
  }
  const nt = nb < 16384 ? nb : 16384, T = new Float32Array(nt * 16), err = new Float32Array(nt), ti = new Int32Array(nt);
  const cb = new Float32Array(256 * 16), sum = new Float32Array(256 * 16), cnt = new Int32Array(256);
  rng = 12345;
  for (let i = 0; i < nt; i++) { const s = nb <= 16384 ? i : rand() % nb; T.set(X.subarray(s * 16, s * 16 + 16), i * 16); }
  for (let k = 0; k < 256; k++) { const s = rand() % nt; cb.set(T.subarray(s * 16, s * 16 + 16), k * 16); }
  for (let it = 0; it < 14; it++) {
    assign(T, nt, cb, ti, err);
    sum.fill(0); cnt.fill(0);
    for (let i = 0; i < nt; i++) { cnt[ti[i]]++; for (let d = 0; d < 16; d++) sum[ti[i] * 16 + d] = f32(sum[ti[i] * 16 + d] + T[i * 16 + d]); }
    for (let k = 0; k < 256; k++) {
      if (cnt[k]) { for (let d = 0; d < 16; d++) cb[k * 16 + d] = f32(sum[k * 16 + d] / cnt[k]); }
      else {
        let worst = 0; for (let i = 1; i < nt; i++) if (err[i] > err[worst]) worst = i;
        cb.set(T.subarray(worst * 16, worst * 16 + 16), k * 16); err[worst] = 0;
      }
    }
  }
  for (let k = 0; k < 256; k++) for (let d = 0; d < 16; d++) { const v = cb[k * 16 + d]; cb[k * 16 + d] = v < 0 ? 0 : v > 15 ? 15 : Math.trunc(f32(v + 0.5)); }
  assign(X, nb, cb, ix, null);
  const out = new Uint8Array(2048 + bw * bw);
  for (let k = 0; k < 256; k++) for (let j = 0; j < 4; j++) {
    const o = k * 16 + j * 4, p = (cb[o] << 12) | (cb[o + 1] << 8) | (cb[o + 2] << 4) | cb[o + 3];
    out[k * 8 + j * 2] = p & 255; out[k * 8 + j * 2 + 1] = p >> 8;
  }
  for (let by = 0; by < bw; by++) for (let bx = 0; bx < bw; bx++) out[2048 + (twid(by) | (twid(bx) << 1))] = ix[by * bw + bx];
  return out;
}

// ---------- pack in, pack out ----------
const u16 = (d, o) => d[o] | (d[o + 1] << 8);
const u32 = (d, o) => (d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (d[o + 3] << 24)) >>> 0;
export function readPack(d) {
  if (td.decode(d.subarray(0, 8)) !== 'PNJPACK1') throw new Error('not a pnj.pack');
  const n = u32(d, 8), ents = []; let p = 12;
  for (let i = 0; i < n; i++) { const l = u16(d, p), name = td.decode(d.subarray(p + 2, p + 2 + l)), off = u32(d, p + 2 + l), size = u32(d, p + 6 + l); ents.push({ name, data: d.subarray(off, off + size) }); p += 10 + l; }
  return ents;
}
// Encodes one sheet to a DTX2/DTX1 file body (or null if the sheet does not suit the console). Pure, so a Worker can run it.
export async function encodeSheet(png) {
  const { w, h, px } = await decodePng(png);
  const sh = w >= 512 ? 2 : 1, dw = (w / sh) | 0, dh = (h / sh) | 0;
  if ((dw & (dw - 1)) || (dh & (dh - 1)) || dw < 8 || dh < 8 || dw > 1024 || dh > 1024) return null;
  const { lin } = pack4444(px, w, h, sh), vq = dw === dh;
  const o = new Uint8Array(vq ? 8 + 2048 + (dw / 2) * (dh / 2) : 8 + dw * dh * 2);
  o.set(te.encode(vq ? 'DTX2' : 'DTX1'), 0); o[4] = dw & 255; o[5] = dw >> 8; o[6] = dh & 255; o[7] = dh >> 8;
  if (vq) o.set(vq4444(lin, dw, dh), 8); else for (let i = 0; i < lin.length; i++) { o[8 + i * 2] = lin[i] & 255; o[9 + i * 2] = lin[i] >> 8; }
  return o;
}
// base: the Uint8Array of pnj.pack. encode(name, pngBytes) -> Promise<Uint8Array|null> defaults to encodeSheet; pass a Worker-pool version to go faster.
export async function dcPack(base, log = () => {}, encode = (n, png) => encodeSheet(png)) {
  const src = readPack(base).filter((e) => !e.name.startsWith('dc/'));
  const byName = new Map(src.map((e) => [e.name, e.data]));
  const made = [];
  const jobs = TEXTURES.map(async (t) => {
    const png = byName.get(`tex/${t}.png`); if (!png) return null;
    const o = await encode(t, png); if (o) log(`${t} ready`); return o ? { name: `dc/${t}.dtx`, data: o } : null;
  });
  for (const r of await Promise.all(jobs)) if (r) made.push(r);
  if (!made.length) throw new Error('No sprite sheets found in the pack.');
  const all = [...src, ...made];
  const rank = (n) => (n.startsWith('dc/') ? 0 : n.startsWith('tex/') ? 2 : 1);
  const ord = []; for (let pass = 0; pass < 3; pass++) for (const e of all) if (rank(e.name) === pass) ord.push(e);
  let off = 12; for (const e of ord) off += 2 + te.encode(e.name).length + 8;
  let total = off; for (const e of ord) total += e.data.length;
  const out = new Uint8Array(total), dv = new DataView(out.buffer);
  out.set(te.encode('PNJPACK1'), 0); dv.setUint32(8, ord.length, true);
  let p = 12, d = off;
  for (const e of ord) {
    const nb = te.encode(e.name);
    dv.setUint16(p, nb.length, true); out.set(nb, p + 2); dv.setUint32(p + 2 + nb.length, d, true); dv.setUint32(p + 6 + nb.length, e.data.length, true);
    p += 10 + nb.length; out.set(e.data, d); d += e.data.length;
  }
  return out;
}
export { TEXTURES };
