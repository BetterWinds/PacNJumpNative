#!/bin/bash
# usage: [STAGE=n] sweep.sh <first_seed> <count> [steps] [level] [mode]   (STAGE is 0-based; dev server on :8765, node playwright on NODE_PATH)
# Replays the web bot (or a scripted tilt) through JS and the C core; reports divergences and coverage.
cd "$(dirname "$0")/.."; mkdir -p run; ok=0; bad=0
ST=${STAGE:-0}
STEPS=${3:-3000}; LEVEL=${4:-0}; MODE=${5:-bot}
gcc -std=c99 -DPNJ_STRICT -O2 -ffp-contract=off core/pnj_core.c tools/trace_c.c -lm -o run/trace_c || exit 1
tot=0; maxh=0
for ((s=$1; s<$1+$2; s++)); do
  d=run/st${ST}_s${s}_l$LEVEL; mkdir -p $d; node tools/trace_js.js $s $ST $STEPS $MODE $d $LEVEL || continue
  run/trace_c < $d/in.txt > $d/c.json 2>$d/err.txt
  if python3 tools/compare.py $d/js.json $d/c.json --tol=1e-9 >$d/res.txt; then ok=$((ok+1)); else bad=$((bad+1)); echo "seed $s level $LEVEL: $(cat $d/res.txt)"; fi
  python3 - "$d" <<'PY'
import json,sys
r=[json.loads(l) for l in open(sys.argv[1]+'/js.json')]
print("  seed %s: steps=%d peak=%.0f power_steps=%d ghosts_seen=%d"%(sys.argv[1].split('/')[-1],len(r),-min(x[8] for x in r),sum(x[16] for x in r),max(x[18] for x in r)))
PY
done
echo "ok=$ok bad=$bad (stage $ST level $LEVEL, $STEPS steps, $MODE)"
