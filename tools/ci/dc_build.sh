#!/bin/sh
# Runs inside the pcercuei/dreamcast-toolchain image (see .github/workflows/dreamcast.yml). /work is the repo; results go to /work/out.
set -e
apk add --no-cache bash git make cmake python3 >/dev/null 2>&1 || true
mkdir -p /opt/toolchains/dc/kos && cd /opt/toolchains/dc/kos
git init -q && git remote add origin https://github.com/KallistiOS/KallistiOS.git
git fetch -q --depth 1 origin "$KOS_COMMIT" && git checkout -q FETCH_HEAD
cp doc/environ.sh.sample environ.sh
. ./environ.sh
make -j"$(nproc)" >/tmp/kos-build.log 2>&1 || { tail -50 /tmp/kos-build.log; exit 1; }
cd /work
mkdir -p out
kos-cc -O2 -o out/dc_smoke.elf platform/dc/dc_smoke.c
kos-cc -O2 -ffp-contract=off -Wall -Wno-unused-function -Wno-unused-variable -DPNJ_DC=1 -DPNJ_FLOAT=1 -DPNJ_BYO=1 -DPNJ_VERSION="\"$PNJ_VERSION\"" \
  -Icore -Ithird_party/stb -o out/pacnjump.elf core/pnj_core.c platform/sdl/main.c -lm
ls -la out

# ---- disc images: failures here never fail the build; the log goes out with the artifacts ----
(
  set +e
  L=/work/out/disc-log.txt
  apk add --no-cache meson ninja build-base libisofs-dev >>"$L" 2>&1
  git clone --depth 1 https://gitlab.com/simulant/mkdcdisc.git /tmp/mkdcdisc >>"$L" 2>&1
  (cd /tmp/mkdcdisc && meson setup build >>"$L" 2>&1 && ninja -C build >>"$L" 2>&1)
  M=/tmp/mkdcdisc/build/mkdcdisc
  mkdir -p /tmp/probe && cd /tmp/probe
  # no game data on a CI disc: the game shows its "game data not found" screen
  $M --no-banner -v 2 -m -N -e /work/out/pacnjump.elf -n "Pac'N-Jump" -a "Jon Anderson" -o /work/out/pacnjump-nodata.cdi >>"$L" 2>&1
  ls -la /work/out >>"$L" 2>&1
  # ---- the disc template for the web builder (web/dc_disc.js) ----
  # The whole disc is built around a stand-in PNJ.PACK of a fixed size (16 MiB, room for the real pack with some to spare). The browser swaps in the real pack's sectors and redoes their EDC/ECC.
  # Each stand-in sector starts with a marker, so the script below can find where the file landed in the image and write that down (pacnjump-dc.json).
  SLOT=16777216
  python3 - "$SLOT" <<'PY' >>"$L" 2>&1
import sys, struct, os
n = int(sys.argv[1]); os.makedirs('/tmp/probe/dt', exist_ok=True); os.makedirs('/tmp/probe/dr', exist_ok=True)
with open('/tmp/probe/dt/PNJ.PACK', 'wb') as f:
    for k in range(n // 2048): f.write((b'PNJSLOT\0' + struct.pack('<I', k)).ljust(2048, b'\0'))
with open('/tmp/probe/dr/PNJ.PACK', 'wb') as f: f.write(os.urandom(n - 777))     # random and a little short of the slot: the check disc
PY
  echo "/PNJ.PACK -100" >sort.txt
  for d in dt dr; do
    $M --no-banner -v 2 -m -e /work/out/pacnjump.elf -D /tmp/probe/$d -S sort.txt -N -n "Pac'N-Jump" -a "Jon Anderson" -r 20261106 -o /tmp/probe/$d.cdi >>"$L" 2>&1
  done
  cp /tmp/probe/dt.cdi /work/out/pacnjump-dc.cdi
  python3 - "$SLOT" <<'PY' >>"$L" 2>&1
import sys, json, struct
n = int(sys.argv[1]) // 2048
d = open('/tmp/probe/dt.cdi', 'rb').read()
m0 = b'PNJSLOT\0' + struct.pack('<I', 0); m1 = b'PNJSLOT\0' + struct.pack('<I', 1); mL = b'PNJSLOT\0' + struct.pack('<I', n - 1)
o0 = d.find(m0); o1 = d.find(m1); oL = d.find(mL)
assert o0 > 0 and o1 > o0 and d.count(m0) == 1, (o0, o1)
stride = o1 - o0
assert oL == o0 + (n - 1) * stride, (oL, o0, stride, n)
pre = {2336: 8, 2352: 24, 2048: 0}[stride]
man = {'file': 'pacnjump-dc.cdi', 'size': len(d), 'slot': {'offset': o0 - pre, 'stride': stride, 'pre': pre, 'sectors': n}}
json.dump(man, open('/work/out/pacnjump-dc.json', 'w'))
print('manifest', man)
PY
  ls -la /tmp/probe /work/out >>"$L" 2>&1
  # the check: the browser code patching the template with the random pack must give mkdcdisc's own image of that pack
  apk add --no-cache nodejs >>"$L" 2>&1
  echo "== JS patcher vs mkdcdisc" >>"$L"
  node /work/web/test_dcdisc.mjs /tmp/probe/dt.cdi /work/out/pacnjump-dc.json /tmp/probe/dr/PNJ.PACK /tmp/probe/dr.cdi >>"$L" 2>&1
  echo "check exit: $?" >>"$L"
) || true
