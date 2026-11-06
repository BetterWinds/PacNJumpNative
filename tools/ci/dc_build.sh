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
  # layout probe for the pack patcher: the same disc with a 1 MB and a 3 MB stand-in pack (the pack is the last file)
  head -c 1048576 /dev/urandom >p1.pack; head -c 3145728 /dev/urandom >p3.pack
  echo "/PNJ.PACK -100" >sort.txt
  for n in 1 3; do
    mkdir -p d$n; cp p$n.pack d$n/PNJ.PACK
    $M --no-banner -v 3 -m -e /work/out/pacnjump.elf -D d$n -S sort.txt -N -n "Pac'N-Jump" -a "Jon Anderson" -r 20261106 -o probe$n.cdi >>"$L" 2>&1
  done
  ls -la >>"$L" 2>&1
  echo "== diff of the two probes (byte offsets that differ, first 40)" >>"$L"
  cmp -l probe1.cdi probe3.cdi 2>&1 | wc -l >>"$L"
  cmp -l probe1.cdi probe3.cdi 2>&1 | head -40 | awk '{printf "%d ", $1-1}' >>"$L"
) || true
