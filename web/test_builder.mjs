// node web/test_builder.mjs original.apk template.apk out.apk
import { readFileSync, writeFileSync } from 'node:fs';
import * as B from './builder.js';
const [orig, tmpl, out] = process.argv.slice(2);
const t0 = Date.now();
const apk = await B.buildApk(new Uint8Array(readFileSync(orig)), new Uint8Array(readFileSync(tmpl)), (m) => console.log(' ', m));
writeFileSync(out, apk);
console.log('wrote', out, apk.length, 'bytes in', Date.now() - t0, 'ms');
