#!/bin/sh
# Builds web/aac.wasm from platform/sdl/aac_dec.c with plain clang and wasm-ld (no emscripten, no libc).
# The web builder uses it for the power-up jingle when the browser has no AAC decoder of its own.
set -e
cd "$(dirname "$0")/../.."
F="--target=wasm32 -O2 -ffreestanding -fno-builtin -nostdlib -fno-math-errno"
clang $F -I tools/aac_wasm/stub -I platform/sdl -c platform/sdl/aac_dec.c -o /tmp/aac_dec.o
clang $F -c tools/aac_wasm/shim.c -o /tmp/aac_shim.o
wasm-ld --no-entry --export=alloc --export=decode --export=out_ptr --export=out_len --export=memory --allow-undefined -o web/aac.wasm /tmp/aac_dec.o /tmp/aac_shim.o
