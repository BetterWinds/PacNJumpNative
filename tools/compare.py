#!/usr/bin/env python3
"""compare.py js.json c.json -> reports first diverging step/field. Exact equality for double builds,
--tol for float/fixed builds."""
import json, sys
names = "L x y vx vy cam593 cam594 cam595 peak seed dead nplat sumx sumy npick nghost power boost ghostsAlive gx gy picksAlive pky".split()
tol = 0.0
args = [a for a in sys.argv[1:] if not a.startswith('--tol')]
for a in sys.argv[1:]:
    if a.startswith('--tol='): tol = float(a[6:])
js = [json.loads(l) for l in open(args[0]) if l.strip()]
cc = [json.loads(l) for l in open(args[1]) if l.strip()]
n = min(len(js), len(cc))
short = len(cc) < len(js)
for i in range(n):
    for k, (a, b) in enumerate(zip(js[i], cc[i])):
        if (abs(a - b) > tol) if tol else (a != b):
            print(f"DIVERGE step {i+1} field {names[k]}: js={a!r} c={b!r}")
            sys.exit(1)
print(f"OK {n} steps identical" + (f" (tol {tol})" if tol else "") + (f"; C stopped early (generator not ported), {len(js)-n} JS steps unchecked" if short else ""))
