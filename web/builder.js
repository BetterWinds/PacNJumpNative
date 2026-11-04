// Pac'N-Jump APK builder. Runs entirely in the browser (or Node 18+ for tests); nothing is uploaded anywhere.
//
//   buildPack(originalApk)          -> pnj.pack bytes, made from the user's own copy of the original game's APK
//   patchApk(templateApk, pack, icons) -> the no-data app APK with the pack stored as assets/pnj.pack and the launcher icons swapped (not signed yet)
//   signApk(parts, key)             -> APK Signature Scheme v2 signature over the result (Android 7.0 and newer)
//
// The pack mapping mirrors tools/make_pack.py and platform/sdl/import.h. Keep the three in sync.

const TEX = ['bnglogo', 'bndr_logo', 'sysparts', 'stages', 'charaparts', 'stage_1_map', 'stage_1_cha', 'stage_2_map', 'stage_2_cha', 'stage_3_map', 'stage_3_cha',
  'stage_4_map', 'stage_4_cha', 'stage_5_map', 'stage_5_cha', 'stage_6_map', 'stage_6_cha', 'stage_7_map', 'stage_7_cha', 'stage_8_map', 'stage_8_cha'];
const SFX = ['balloon', 'bound', 'cancel', 'candle', 'decide', 'eat01', 'eat02', 'eat03', 'eat04', 'eat05', 'eat06', 'eat07', 'eat08', 'eat_enemy',
  'eat_item', 'eat_pow', 'gameover', 'hat', 'hiscore', 'jump', 'mappy_jump', 'miss', 'select', 'st8_warning'];
const MAP_COUNT = 42;

const te = new TextEncoder();
const u16 = (d, o) => d[o] | (d[o + 1] << 8);
const u32 = (d, o) => (d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (d[o + 3] << 24)) >>> 0;
const w16 = (v) => new Uint8Array([v & 255, (v >> 8) & 255]);
const w32 = (v) => new Uint8Array([v & 255, (v >>> 8) & 255, (v >>> 16) & 255, (v >>> 24) & 255]);
function w64(v) { const b = new Uint8Array(8); let x = BigInt(v); for (let i = 0; i < 8; i++) { b[i] = Number(x & 255n); x >>= 8n; } return b; }
function cat(...parts) {
  let n = 0; for (const p of parts) n += p.length;
  const out = new Uint8Array(n); let o = 0;
  for (const p of parts) { out.set(p, o); o += p.length; }
  return out;
}
const sha256 = async (d) => new Uint8Array(await crypto.subtle.digest('SHA-256', d));

let CRC_T;
function crc32(d) {
  if (!CRC_T) { CRC_T = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; CRC_T[n] = c >>> 0; } }
  let c = 0xFFFFFFFF;
  for (let i = 0; i < d.length; i++) c = CRC_T[(c ^ d[i]) & 255] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}

// ---------- zip reading ----------
export function parseZip(d) {
  let eocd = -1;
  for (let i = d.length - 22; i >= 0 && d.length - i <= 65557; i--) if (u32(d, i) === 0x06054b50) { eocd = i; break; }
  if (eocd < 0) throw new Error('not a zip file');
  const count = u16(d, eocd + 10), cdSize = u32(d, eocd + 12), cdOff = u32(d, eocd + 16);
  if (cdOff + cdSize > d.length) throw new Error('zip directory is damaged');
  const entries = []; let p = cdOff;
  for (let i = 0; i < count; i++) {
    if (u32(d, p) !== 0x02014b50) throw new Error('zip directory is damaged');
    const nl = u16(d, p + 28), xl = u16(d, p + 30), cl = u16(d, p + 32);
    entries.push({ cdPos: p, cdLen: 46 + nl + xl + cl, name: new TextDecoder().decode(d.subarray(p + 46, p + 46 + nl)), method: u16(d, p + 10), csize: u32(d, p + 20), usize: u32(d, p + 24), lho: u32(d, p + 42) });
    p += 46 + nl + xl + cl;
  }
  return { d, entries, cdOff, cdSize, eocd, count };
}
async function inflateRaw(src) {
  const s = new Blob([src]).stream().pipeThrough(new DecompressionStream('deflate-raw'));
  return new Uint8Array(await new Response(s).arrayBuffer());
}
export async function readEntry(z, e) {
  const d = z.d, p = e.lho;
  if (u32(d, p) !== 0x04034b50) throw new Error('bad entry ' + e.name);
  const start = p + 30 + u16(d, p + 26) + u16(d, p + 28);
  const raw = d.subarray(start, start + e.csize);
  if (e.method === 0) return raw;
  if (e.method !== 8) throw new Error('unsupported compression in ' + e.name);
  const out = await inflateRaw(raw);
  if (out.length !== e.usize) throw new Error('could not unpack ' + e.name);
  return out;
}

// ---------- pack ----------
// ---------- sprite table: compiled into classes.dex, not stored as data. Same walk as tools/dexatlas.py (read that for the details). ----------
const DEX_LEN = new Uint8Array(256).fill(1);
for (const [lo, hi, n] of [[2, 2, 2], [3, 3, 3], [5, 5, 2], [6, 6, 3], [8, 8, 2], [9, 9, 3], [0x13, 0x13, 2], [0x14, 0x14, 3], [0x15, 0x16, 2], [0x17, 0x17, 3], [0x18, 0x18, 5],
  [0x19, 0x19, 2], [0x1a, 0x1a, 2], [0x1b, 0x1b, 3], [0x1c, 0x1c, 2], [0x1f, 0x20, 2], [0x22, 0x23, 2], [0x24, 0x26, 3], [0x29, 0x29, 2], [0x2a, 0x2c, 3], [0x2d, 0x3d, 2],
  [0x44, 0x6d, 2], [0x6e, 0x72, 3], [0x74, 0x78, 3], [0x90, 0xaf, 2], [0xd0, 0xe2, 2]]) for (let i = lo; i <= hi; i++) DEX_LEN[i] = n;

export function dexAtlas(d) {
  const bad = () => new Error('Could not read the sprite table from classes.dex. Is this the 1.1.0 Android APK?');
  if (d.length < 0x70 || d[0] !== 0x64 || d[1] !== 0x65 || d[2] !== 0x78 || d[3] !== 0x0a) throw bad();
  const strN = u32(d, 0x38), strOff = u32(d, 0x3c), typeN = u32(d, 0x40), typeOff = u32(d, 0x44), methN = u32(d, 0x58), methOff = u32(d, 0x5c), clsN = u32(d, 0x60), clsOff = u32(d, 0x64);
  const uleb = (st) => { let v = 0, s = 0; for (;;) { if (st.p >= d.length || s > 28) throw bad(); const c = d[st.p++]; v |= (c & 0x7f) << s; s += 7; if (!(c & 0x80)) return v >>> 0; } };
  const str = (i) => {
    if (i >= strN || strOff + 4 * i + 4 > d.length) throw bad();
    const st = { p: u32(d, strOff + 4 * i) }; uleb(st); let e = st.p; while (e < d.length && d[e]) e++;
    return new TextDecoder('latin1').decode(d.subarray(st.p, e));
  };
  let code = 0;
  for (let c = 0; c < clsN && !code; c++) {
    const co = clsOff + 32 * c; if (co + 32 > d.length) throw bad();
    const tidx = u32(d, co), data = u32(d, co + 24);
    if (tidx >= typeN || !data || str(u32(d, typeOff + 4 * tidx)) !== 'Ljp/co/zoo/pacnjump/b/i;') continue;
    const st = { p: data }; const sf = uleb(st), inf = uleb(st), dm = uleb(st), vm = uleb(st);
    for (let i = 0; i < (sf + inf) * 2; i++) uleb(st);
    let mi = 0;
    for (let i = 0; i < dm + vm; i++) {
      mi += uleb(st); uleb(st); const cd = uleb(st);
      if (mi >= methN || methOff + 8 * mi + 8 > d.length) throw bad();
      if (cd && str(u32(d, methOff + 8 * mi + 4)) === '<clinit>') { code = cd; break; }
    }
  }
  if (!code || code + 16 > d.length) throw bad();
  const size = u32(d, code + 12); if (code + 16 + 2 * size > d.length) throw bad();
  const U = (i) => u16(d, code + 16 + 2 * i);
  const reg = new Int32Array(512), R = (i) => reg[i & 511];
  let pend = null; const rows = [];
  for (let p = 0; p < size;) {
    const w = U(p), op = w & 255, a4 = (w >> 8) & 15, b4 = w >> 12, aa = w >> 8, nx = p + DEX_LEN[op];
    if (nx > size) break;
    if (op === 0x01 || op === 0x07) reg[a4] = R(b4);
    else if (op === 0x02 || op === 0x08) reg[aa] = R(U(p + 1));
    else if (op === 0x03 || op === 0x09) reg[U(p + 1) & 511] = R(U(p + 2));
    else if (op === 0x12) reg[a4] = b4 > 7 ? b4 - 16 : b4;
    else if (op === 0x13) reg[aa] = (U(p + 1) << 16) >> 16;
    else if (op === 0x14) reg[aa] = U(p + 1) | (U(p + 2) << 16);
    else if (op === 0x15) reg[aa] = U(p + 1) << 16;
    else if (op === 0x77 && aa === 6) { const c0 = U(p + 2); pend = []; for (let i = 0; i < 6; i++) pend.push(R(c0 + i)); }
    else if (op === 0x70 && (w >> 12) === 3 && pend) { rows.push([R((U(p + 2) >> 4) & 15), pend[0], pend[1], pend[2], pend[3], pend[4]]); pend = null; }
    else if (op >= 0x0e && op <= 0x11) break;
    else if (op >= 0x28 && op <= 0x3d) throw bad();
    p = nx;
  }
  if (rows.length !== 224) throw bad();
  return te.encode(rows.map((r) => r.join(' ') + '\n').join(''));
}

// ---------- power-up jingle: wakawaka.3gp is AAC, which the engine cannot decode, so the browser decodes it and the pack carries 44.1 kHz stereo WAV ----------
export function wavFromChannels(left, right, rate) {
  const n = left.length, out = new Uint8Array(44 + n * 4), dv = new DataView(out.buffer);
  const tag = (o, t) => { for (let i = 0; i < 4; i++) out[o + i] = t.charCodeAt(i); };
  tag(0, 'RIFF'); dv.setUint32(4, 36 + n * 4, true); tag(8, 'WAVE'); tag(12, 'fmt '); dv.setUint32(16, 16, true); dv.setUint16(20, 1, true); dv.setUint16(22, 2, true);
  dv.setUint32(24, rate, true); dv.setUint32(28, rate * 4, true); dv.setUint16(32, 4, true); dv.setUint16(34, 16, true); tag(36, 'data'); dv.setUint32(40, n * 4, true);
  const q = (x) => Math.max(-32768, Math.min(32767, Math.round(x * 32767)));
  for (let i = 0; i < n; i++) { dv.setInt16(44 + i * 4, q(left[i]), true); dv.setInt16(46 + i * 4, q(right[i]), true); }
  return out;
}
async function decodeJingle(bytes) {
  if (typeof OfflineAudioContext === 'undefined') return null;
  const ctx = new OfflineAudioContext(2, 44100, 44100);                       // decodeAudioData resamples to the context rate
  const buf = await ctx.decodeAudioData(bytes.slice().buffer);
  const l = buf.getChannelData(0), r = buf.numberOfChannels > 1 ? buf.getChannelData(1) : l;
  return l.length > 4410 ? wavFromChannels(l, r, buf.sampleRate) : null;
}

export async function buildPack(apk, log = () => {}) {
  let z;
  try { z = parseZip(apk); } catch (e) { throw new Error('That file is not an APK (a zip file).'); }
  const byName = new Map(z.entries.map((e) => [e.name, e]));
  if (!byName.has('AndroidManifest.xml') || !byName.has('res/drawable-hdpi/sysparts.png')) throw new Error("That does not look like the Pac'N-Jump Android APK.");
  const files = new Map();
  for (let i = 0; i < TEX.length; i++) {
    const nm = `res/drawable-hdpi/${TEX[i]}.png`, e = byName.get(nm);
    if (!e) { if (i < 2) continue; throw new Error('Missing ' + nm); }
    const b = await readEntry(z, e);
    const w = ((b[16] << 24) | (b[17] << 16) | (b[18] << 8) | b[19]) >>> 0;
    if (i >= 2 && (b.length < 24 || w !== 1024)) throw new Error(`${nm} is not the expected 1024 px sheet. This needs the hdpi build of the game.`);
    files.set(`tex/${TEX[i]}.png`, b);
  }
  log('Sprites read');
  let maps = 0;
  for (const e of z.entries) {
    const n = e.name;
    if (!n.startsWith('res/raw/')) {
      if (n.startsWith('assets/language.')) files.set('lang/' + n.slice(7), await readEntry(z, e));
      continue;
    }
    if (n.endsWith('.map')) { files.set(`maps/${n.slice(8, -4)}.map`, await readEntry(z, e)); maps++; }
    else if (n.endsWith('.ogg')) {
      let base = n.slice(8, -4);
      if (base.startsWith('pack_action_bgm')) base = base.slice(12); else if (base === 'breeak') base = 'break';
      files.set(`audio/${base}.ogg`, await readEntry(z, e));
    }
  }
  const ww = byName.get('res/raw/wakawaka.3gp');
  if (ww) {
    try { const wav = await decodeJingle(await readEntry(z, ww)); if (wav) files.set('audio/wakawaka.wav', wav); else log('Power-up jingle skipped (this browser cannot decode it)'); }
    catch (e) { log('Power-up jingle skipped (this browser cannot decode it)'); }
  }
  { const dx = byName.get('classes.dex'); if (!dx) throw new Error('Missing classes.dex'); files.set('meta/atlas.txt', dexAtlas(await readEntry(z, dx))); }
  if (maps !== MAP_COUNT) throw new Error(`Expected ${MAP_COUNT} level maps, found ${maps}.`);
  for (const s of SFX) if (!files.has(`audio/${s}.ogg`)) throw new Error(`Missing sound audio/${s}.ogg`);
  log('Sounds and maps read');
  const meta = te.encode(JSON.stringify({ importer: 'pnj-web', apk_size: apk.length, files: files.size + 1 }));
  files.set('meta.json', meta);
  const names = [...files.keys()].sort();
  let off = 12; for (const n of names) off += 2 + te.encode(n).length + 8;
  const head = [te.encode('PNJPACK1'), w32(names.length)], body = [];
  for (const n of names) {
    const nb = te.encode(n), f = files.get(n);
    head.push(w16(nb.length), nb, w32(off), w32(f.length)); off += f.length; body.push(f);
  }
  return { pack: cat(...head, ...body), files: names.length };
}

// ---------- launcher icon: taken from the user's own APK, so the repo ships a placeholder only ----------
const ICON_DIRS = [['mdpi', 48], ['hdpi', 72], ['xhdpi', 96], ['xxhdpi', 144]];
export async function extractIcons(apk, sizes, log = () => {}) {
  // returns Map(pixel size -> PNG bytes) for the launcher sizes in `sizes`, or an empty map if this runtime cannot scale images
  const out = new Map();
  if (typeof OffscreenCanvas === 'undefined' || typeof createImageBitmap === 'undefined') return out;
  const z = parseZip(apk), byName = new Map(z.entries.map((e) => [e.name, e]));
  let best = null, bestSize = 0;
  for (const [dir, px] of ICON_DIRS) { const e = byName.get(`res/drawable-${dir}/icon.png`); if (e && px > bestSize) { best = e; bestSize = px; } }
  if (!best) return out;
  const src = await readEntry(z, best);
  const bmp = await createImageBitmap(new Blob([src], { type: 'image/png' }));
  for (const px of sizes) {
    const c = new OffscreenCanvas(px, px), g = c.getContext('2d');
    g.imageSmoothingQuality = 'high'; g.drawImage(bmp, 0, 0, px, px);
    out.set(px, new Uint8Array(await (await c.convertToBlob({ type: 'image/png' })).arrayBuffer()));
  }
  log('Launcher icon taken from your APK');
  return out;
}

// ---------- adding the pack (and icons) to the template APK ----------
const PACK_PATH = 'assets/pnj.pack';
const SIG_MAGIC = te.encode('APK Sig Block 42');
const pngWidth = (b) => (b.length > 24 && b[1] === 0x50 ? ((b[16] << 24) | (b[17] << 16) | (b[18] << 8) | b[19]) >>> 0 : 0);

export async function launcherSizes(template) {
  // the template's res/*.png files are its launcher icons; report their pixel widths
  const z = parseZip(template), sizes = new Set();
  for (const e of z.entries) if (/^res\/.*\.png$/.test(e.name)) sizes.add(pngWidth(await readEntry(z, e)));
  sizes.delete(0);
  return [...sizes];
}

export async function patchApk(template, pack, icons = new Map()) {
  let z;
  try { z = parseZip(template); } catch (e) { throw new Error('The app template (template.apk) is damaged or incomplete, so nothing can be built. Rebuild it with package.bat template.'); }
  if (z.entries.some((e) => e.name === PACK_PATH)) throw new Error('The template APK already has game data in it.');
  let start = z.cdOff;                                         // end of the local entries; a leftover signing block sits just before the directory
  if (start >= 24 && SIG_MAGIC.every((v, i) => template[start - 16 + i] === v)) {
    const size = Number(BigInt(u32(template, start - 24)) | (BigInt(u32(template, start - 20)) << 32n));
    start = start - size - 8;
  }
  // launcher icons that get replaced: same file name in the template, new bytes appended at the end
  const replaced = new Map();
  if (icons.size) for (const e of z.entries) if (/^res\/.*\.png$/.test(e.name)) { const px = pngWidth(await readEntry(z, e)); if (icons.has(px)) replaced.set(e.name, icons.get(px)); }
  const adds = [[PACK_PATH, pack, 4], ...replaced];
  let off = start; const locals = [template.subarray(0, start)], cds = [];
  for (const a of adds) {
    const nm = a[0], data = a[1], align = a[2] || 1, name = te.encode(nm), crc = crc32(data);
    const pad = (align - ((off + 30 + name.length) % align)) % align;
    const lh = cat(w32(0x04034b50), w16(10), w16(0), w16(0), w16(0), w16(33), w32(crc), w32(data.length), w32(data.length), w16(name.length), w16(pad), name, new Uint8Array(pad));
    cds.push(cat(w32(0x02014b50), w16(20), w16(10), w16(0), w16(0), w16(0), w16(33), w32(crc), w32(data.length), w32(data.length), w16(name.length), w16(0), w16(0), w16(0), w16(0), w32(0), w32(off), name));
    locals.push(lh, data); off += lh.length + data.length;
  }
  const kept = z.entries.filter((e) => !replaced.has(e.name));
  const local = cat(...locals);
  const cd = cat(...kept.map((e) => template.subarray(e.cdPos, e.cdPos + e.cdLen)), ...cds);
  const count = kept.length + adds.length;
  const eocd = (cdOffset) => cat(w32(0x06054b50), w16(0), w16(0), w16(count), w16(count), w32(cd.length), w32(cdOffset), w16(0));
  return { local, cd, eocd, icons: replaced.size };
}

// ---------- DER (just enough for a self-signed certificate) ----------
const derLen = (n) => (n < 128 ? new Uint8Array([n]) : n < 256 ? new Uint8Array([0x81, n]) : new Uint8Array([0x82, n >> 8, n & 255]));
const der = (tag, ...c) => { const b = cat(...c); return cat(new Uint8Array([tag]), derLen(b.length), b); };
const SIG_ALG = der(0x30, der(0x06, new Uint8Array([0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b])), new Uint8Array([0x05, 0x00]));
const NAME = der(0x30, der(0x31, der(0x30, der(0x06, new Uint8Array([0x55, 0x04, 0x03])), der(0x0c, te.encode("Pac'N-Jump local build")))));

async function makeCert(priv, spki) {
  const serial = crypto.getRandomValues(new Uint8Array(16)); serial[0] = (serial[0] & 0x7f) | 0x01;
  const tbs = der(0x30, der(0xa0, der(0x02, new Uint8Array([2]))), der(0x02, serial), SIG_ALG, NAME,
    der(0x30, der(0x17, te.encode('260101000000Z')), der(0x17, te.encode('491231235959Z'))), NAME, spki);
  const sig = new Uint8Array(await crypto.subtle.sign('RSASSA-PKCS1-v1_5', priv, tbs));
  return der(0x30, tbs, SIG_ALG, der(0x03, new Uint8Array([0]), sig));
}

// The signing key lives in this browser (IndexedDB) so a rebuilt APK can update an installed one. Falls back to a throwaway key.
const idb = (fn) => new Promise((res, rej) => {
  const o = indexedDB.open('pnj-builder', 1);
  o.onupgradeneeded = () => o.result.createObjectStore('k');
  o.onerror = () => rej(o.error);
  o.onsuccess = () => { const t = o.result.transaction('k', 'readwrite'), r = fn(t.objectStore('k')); t.oncomplete = () => res(r && r.result); t.onerror = () => rej(t.error); };
});
export async function getKey() {
  try { const k = await idb((s) => s.get('key')); if (k && k.priv && k.spki && k.cert) return k; } catch (e) { /* no storage: make a new key */ }
  const kp = await crypto.subtle.generateKey({ name: 'RSASSA-PKCS1-v1_5', modulusLength: 2048, publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256' }, false, ['sign', 'verify']);
  const spki = new Uint8Array(await crypto.subtle.exportKey('spki', kp.publicKey));
  const key = { priv: kp.privateKey, spki, cert: await makeCert(kp.privateKey, spki) };
  try { await idb((s) => s.put(key, 'key')); } catch (e) { /* keep going with this session's key */ }
  return key;
}

// ---------- APK Signature Scheme v2 ----------
const lp = (...parts) => { const b = cat(...parts); return cat(w32(b.length), b); };          // length-prefixed
async function contentDigest(local, cd, eocdBytes) {
  const CH = 1 << 20, digests = [];
  for (const sec of [local, cd, eocdBytes]) {
    for (let o = 0; o < sec.length; o += CH) {
      const c = sec.subarray(o, Math.min(o + CH, sec.length));
      digests.push(await sha256(cat(new Uint8Array([0xa5]), w32(c.length), c)));
    }
  }
  return sha256(cat(new Uint8Array([0x5a]), w32(digests.length), ...digests));
}
export async function signApk(parts, key) {
  const { local, cd } = parts;
  const eocdForDigest = parts.eocd(local.length);                                          // directory offset = where the signing block starts
  const dig = await contentDigest(local, cd, eocdForDigest);
  const signedData = lp(lp(lp(w32(0x0103), lp(dig))), lp(lp(key.cert)), lp());             // digests, certificates, additional attributes
  const signature = new Uint8Array(await crypto.subtle.sign('RSASSA-PKCS1-v1_5', key.priv, signedData.subarray(4)));
  const signer = lp(signedData, lp(lp(w32(0x0103), lp(signature))), lp(key.spki));
  const value = lp(signer);
  const pair = cat(w64(value.length + 4), w32(0x7109871a), value);
  const blockSize = pair.length + 8 + 16;
  const block = cat(w64(blockSize), pair, w64(blockSize), SIG_MAGIC);
  return cat(local, block, cd, parts.eocd(local.length + block.length));
}

export async function buildApk(originalApk, templateApk, log = () => {}) {
  const { pack, files } = await buildPack(originalApk, log);
  log(`Game data packed (${files} files)`);
  let icons = new Map();
  try { icons = await extractIcons(originalApk, await launcherSizes(templateApk), log); } catch (e) { log('Launcher icon skipped: ' + e.message); }
  const parts = await patchApk(templateApk, pack, icons);
  log('Added to the app');
  const key = await getKey();
  const out = await signApk(parts, key);
  log('Signed');
  return out;
}
