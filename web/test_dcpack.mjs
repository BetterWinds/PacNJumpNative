// node web/test_dcpack.mjs base.pack [expected.pack]   builds the Dreamcast pack in JS; with a second file, compares it to the one the C tool (--dcpack) made
import { readFileSync, writeFileSync } from 'node:fs';
import { dcPack } from './dc_pack.js';
const [base, want, out] = process.argv.slice(2);
const t0 = Date.now();
const got = await dcPack(new Uint8Array(readFileSync(base)), (m) => process.stderr.write(m + '\n'));
console.log(`built ${got.length} bytes in ${((Date.now() - t0) / 1000).toFixed(1)} s`);
if (out) writeFileSync(out, got);
if (want) {
  const w = new Uint8Array(readFileSync(want)); let diff = 0, first = -1;
  if (w.length !== got.length) console.log(`size differs: C ${w.length}, JS ${got.length}`);
  for (let i = 0; i < Math.min(w.length, got.length); i++) if (w[i] !== got[i]) { if (first < 0) first = i; diff++; }
  console.log(diff ? `${diff} bytes differ, first at ${first}` : 'identical to the C tool output');
  process.exit(diff || w.length !== got.length ? 1 : 0);
}
