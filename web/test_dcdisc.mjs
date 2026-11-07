import { readFileSync } from 'node:fs';
import { patchCdi } from './dc_disc.js';
const [tpl, man, pack, want] = process.argv.slice(2);
const m = JSON.parse(readFileSync(man, 'utf8')).slot, t = new Uint8Array(readFileSync(tpl)), p = new Uint8Array(readFileSync(pack)), w = new Uint8Array(readFileSync(want));
const t0 = Date.now(), got = patchCdi(t, m, p);
console.log(`patched ${got.length} bytes in ${Date.now() - t0} ms; mkdcdisc image is ${w.length}`);
if (got.length !== w.length) { console.log('SIZE MISMATCH'); process.exit(1); }
const lo = m.offset, hi = m.offset + m.sectors * m.stride; let inSlot = 0, outSlot = 0, firstOut = -1, firstIn = -1;
for (let i = 0; i < got.length; i++) if (got[i] !== w[i]) { if (i >= lo && i < hi) { inSlot++; if (firstIn < 0) firstIn = i; } else { outSlot++; if (firstOut < 0) firstOut = i; } }
console.log(`differences inside the pack's sectors: ${inSlot}${inSlot ? ' first at ' + firstIn : ''}; outside them: ${outSlot}${outSlot ? ' first at ' + firstOut : ''}`);
process.exit(inSlot ? 1 : 0);
