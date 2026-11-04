// Replays random touch scripts through the web pad logic and the C input model; requires dev server on :8765.
const { chromium } = require('playwright'); const { execFileSync } = require('child_process');
(async () => {
  let s = 12345; const rnd = () => ((s = (s * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff);
  const script = [];
  for (let g = 0; g < 60; g++) {
    const x = Math.round(rnd() * 320), y = Math.round(rnd() * 480);
    script.push(['d', x, y]);
    let cx = x, cy = y;
    for (let i = 0; i < 40; i++) {
      if (i % 5 === 0) { cx = Math.round(rnd() * 320); cy = Math.round(rnd() * 480); script.push(['m', cx, cy]); }
      script.push(['t']);
    }
    script.push(['u'], ['t']);
  }
  const cin = script.map(e => e.join(' ')).join('\n') + '\n';
  const c = execFileSync(process.argv[2], { input: cin }).toString().trim().split('\n').map(Number);
  const b = await chromium.launch(); const pg = await b.newPage();
  await pg.goto('http://localhost:8765/index.html'); await pg.waitForFunction(() => window.__pnj && window.__pnj.ready);
  const js = await pg.evaluate(script => {
    const P = window.__pnj, pad = P.pad, BAR = { x: 160, y: 432, w: 200 }, out = [];
    pad.id = -1;
    for (const e of script) {
      if (e[0] === 'd') {   // same arithmetic as padDown(), minus the DOM event
        const [, lx, ly] = e, onBar = Math.abs(lx - BAR.x) <= BAR.w / 2 + 10 && Math.abs(ly - 432) <= 30;
        if (!onBar && ly > 400) continue;
        pad.id = 1; pad.bar = onBar; pad.x = lx; pad.y = ly; pad.ax = onBar ? BAR.x : lx; pad.ay = onBar ? 432 : ly;
      } else if (e[0] === 'm') { if (pad.id >= 0) { pad.x = e[1]; pad.y = e[2]; } }
      else if (e[0] === 'u') pad.id = -1;
      else out.push(P.padValue());
    }
    return out;
  }, script);
  await b.close();
  // Math.hypot (V8) and libm hypot can differ in the last bit, so compare with a 1e-12 tolerance
  let bad = 0, maxd = 0; for (let i = 0; i < js.length; i++) { const d = Math.abs(js[i] - c[i]); maxd = Math.max(maxd, d); if (d > 1e-12 && bad++ < 3) console.log('DIFF tick', i, js[i], c[i]); }
  console.log(bad ? `FAIL ${bad}/${js.length}` : `OK ${js.length} ticks match (max diff ${maxd.toExponential(1)})`);
})();
