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

# ---- disc image experiment: failures here never fail the build; the log goes out with the artifacts ----
(
  set +e
  L=/work/out/disc-log.txt
  echo "== which tools" >"$L"
  for t in makeip scramble mkisofs genisoimage xorriso mkdcdisc cdi4dc sh-elf-objcopy kos-objcopy; do printf '%s: ' "$t" >>"$L"; command -v "$t" >>"$L" 2>&1 || echo missing >>"$L"; done
  ls "$KOS_BASE/utils/makeip" "$KOS_BASE/utils/scramble" >>"$L" 2>&1
  echo "== apk search" >>"$L"
  apk search libisofs cdrkit xorriso 2>&1 | head -20 >>"$L"
  apk add --no-cache meson ninja build-base libisofs-dev >>"$L" 2>&1
  git clone --depth 1 https://gitlab.com/simulant/mkdcdisc.git /tmp/mkdcdisc >>"$L" 2>&1
  (cd /tmp/mkdcdisc && meson setup build >>"$L" 2>&1 && ninja -C build >>"$L" 2>&1)
  ls -la /tmp/mkdcdisc/build >>"$L" 2>&1
  /tmp/mkdcdisc/build/mkdcdisc --help >>"$L" 2>&1
) || true
