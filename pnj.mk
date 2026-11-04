# Desktop build + tests.
#   make -f pnj.mk          -> build/pnj  (SDL3 shell)
#   make -f pnj.mk test     -> headless smoke test (SDL dummy video driver), writes build/selftest.bmp
#   make -f pnj.mk parity   -> JS-vs-C trace sweep (needs the dev server on :8765 and node playwright)
CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -Wno-unused-function -O2 -ffp-contract=off

build/pnj: core/pnj_core.c platform/sdl/main.c core/pnj_core.h core/pnj_maps.h core/pnj_input.h platform/sdl/atlas.h platform/sdl/menus.h platform/sdl/audio.h platform/sdl/pack.h platform/sdl/import.h platform/sdl/jingle_native.c platform/sdl/aac_dec.c platform/sdl/aac_tables.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Ithird_party/stb $$(pkg-config --cflags sdl3) core/pnj_core.c platform/sdl/main.c platform/sdl/jingle_native.c platform/sdl/aac_dec.c $$(pkg-config --libs sdl3) -lm -o $@

test: build/pnj
	SDL_VIDEODRIVER=dummy build/pnj --selftest 300 --seed 12345 --bmp build/selftest.bmp

parity:
	tools/sweep.sh 1000 25

clean:
	rm -f build/pnj build/selftest.bmp
