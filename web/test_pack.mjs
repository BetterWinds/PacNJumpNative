// node web/test_pack.mjs original.apk out.pack   (builds the pack only, for comparing against tools/make_pack.py)
import { readFileSync, writeFileSync } from 'node:fs';
import * as B from './builder.js';
const [orig, out] = process.argv.slice(2);
writeFileSync(out, (await B.buildPack(new Uint8Array(readFileSync(orig)))).pack);
