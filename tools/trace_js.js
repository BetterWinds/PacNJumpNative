// usage: node trace_js.js <seed> <stage> <n> <tiltmode> <outdir> [level]   (needs the dev server on :8765)
// tiltmode: sine | bang | zero | rand | bot (the web test bot drives; its tilt is recorded and replayed in C)
const { chromium } = require('playwright');
const fs = require('fs');
(async () => {
  const [seed, stage, n, mode, out, level] = [+process.argv[2], +process.argv[3], +process.argv[4], process.argv[5] || 'sine', process.argv[6] || '.', +(process.argv[7] || 0)];
  const tilts = [];
  for (let i = 0; i < n; i++) {
    let t = 0;
    if (mode === 'sine') t = 0.5 * Math.sin(i * 0.07 + seed);
    else if (mode === 'bang') t = (Math.floor(i / 23) % 3 - 1) * 0.5;
    else if (mode === 'zero') t = 0;
    else if (mode === 'rand') { t = Math.sin(i * 12.9898 + seed) * 43758.5453; t = (t - Math.floor(t)) - 0.5; }
    tilts.push(t);
  }
  const b = await chromium.launch(); const pg = await b.newPage({ viewport: { width: 400, height: 700 } });
  await pg.goto(`http://localhost:8765/index.html?stage=${stage}&seed=${seed}&level=${level}` + (mode === 'bot' ? '&bot=1' : ''));
  await pg.waitForFunction(() => window.__pnj && window.__pnj.ready, null, { timeout: 20000 });
  const res = await pg.evaluate(({ seed, stage, tilts, bot }) => {
    const P = window.__pnj;
    seed_set(seed);
    function seed_set(s) { window.eval('seed = ' + (s >>> 0)); }
    P.start();
    window.eval('seed = ' + (seed >>> 0) + '>>>0');
    const rows = [], used = [];
    if (bot) { const o = window.botTilt; window.botTilt = () => { const v = o(); used.push(v); return v; }; }
    for (const t of tilts) {
      const before = used.length;
      P.step(t);
      if (bot && used.length === before) used.push(0);
      const G = P.G;
      let sx = 0, sy = 0, na = 0;
      for (const pl of G.plats) if (pl.alive) { sx += pl.x; sy += pl.y; na++; }
      let gx = 0, gy = 0, ga = 0, ky = 0, ka = 0;
      for (const g of G.ghosts) if (g.alive) { gx += g.x; gy += g.y; ga++; }
      for (const k of G.picks) if (k.alive) { ky += k.y; ka++; }
      rows.push([G.L, G.p.x, G.p.y, G.p.vx, G.p.vy, G.cam593, G.cam594, G.cam595, G.peak, window.eval('seed'), G.p.dead ? 1 : 0, na, sx, sy, G.picks.length, G.ghosts.length, G.p.power ? 1 : 0, G.p.boost ? 1 : 0, ga, gx, gy, ka, ky]);
      if (G.p.dead && G.p.deadT <= 0) break;
    }
    return { rows, used };
  }, { seed, stage, tilts, bot: mode === 'bot' });
  const rows = res.rows; if (mode === 'bot') { tilts.length = 0; res.used.slice(0, rows.length).forEach(v => tilts.push(v)); }
  fs.writeFileSync(`${out}/in.txt`, `${seed} ${stage} ${rows.length} ${level}\n` + tilts.map(t => t.toPrecision(17)).join('\n') + '\n');
  fs.writeFileSync(`${out}/js.json`, rows.map(r => JSON.stringify(r)).join('\n') + '\n');
  await b.close();
})();
