/* Dreamcast toolchain smoke test (no game code): proves CI can build KallistiOS and link a program that uses the PVR, the controller and
 * the sound stream. Shows a pad-controlled textured quad over a translucent one and plays a tone. Not part of the game. */
#include <kos.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

static void *tone_cb(snd_stream_hnd_t hnd, int smp_req, int *smp_recv) {
    static int16_t buf[4096] __attribute__((aligned(32))); static int ph;
    int n = smp_req / 2; if (n > 2048) n = 2048;
    (void)hnd;
    for (int i = 0; i < n; i++) { int16_t s = (((ph++ >> 6) & 1) ? 3000 : -3000); buf[i * 2] = s; buf[i * 2 + 1] = s; }
    *smp_recv = n * 4; return buf;
}

static void quad(const pvr_poly_hdr_t *h, float x, float y, float w, float hh, uint32_t argb) {
    pvr_vertex_t v; pvr_prim((void *)h, sizeof *h);
    v.flags = PVR_CMD_VERTEX; v.z = 1.0f; v.argb = argb; v.oargb = 0;
    v.x = x;     v.y = y + hh; v.u = 0; v.v = 1; pvr_prim(&v, sizeof v);
    v.x = x;     v.y = y;      v.u = 0; v.v = 0; pvr_prim(&v, sizeof v);
    v.x = x + w; v.y = y + hh; v.u = 1; v.v = 1; pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL; v.x = x + w; v.y = y; v.u = 1; v.v = 0; pvr_prim(&v, sizeof v);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    pvr_init_params_t pp = { { PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0 }, 512 * 1024 };
    pp.autosort_disabled = 1; pp.opb_overflow_count = 3;
    pvr_init(&pp); pvr_set_bg_color(0.1f, 0.1f, 0.3f);

    const int N = 64; uint16_t px[64 * 64];
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        int a = (x + y) * 15 / (2 * N) ; int on = ((x / 8) + (y / 8)) & 1;
        px[y * N + x] = (uint16_t)(((a & 15) << 12) | ((on ? 15 : 4) << 8) | ((x * 15 / N) << 4) | (y * 15 / N));        /* ARGB4444 */
    }
    pvr_ptr_t tex = pvr_mem_malloc(N * N * 2);
    pvr_txr_load_ex(px, tex, N, N, PVR_TXRLOAD_16BPP);
    pvr_poly_cxt_t cx; pvr_poly_hdr_t ht, hc;
    pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_TWIDDLED, N, N, tex, PVR_FILTER_BILINEAR);
    cx.depth.comparison = PVR_DEPTHCMP_ALWAYS; cx.depth.write = false; cx.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&ht, &cx);
    pvr_poly_cxt_col(&cx, PVR_LIST_TR_POLY);
    cx.depth.comparison = PVR_DEPTHCMP_ALWAYS; cx.depth.write = false; cx.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&hc, &cx);

    snd_stream_init();
    snd_stream_hnd_t sh = snd_stream_alloc(tone_cb, SND_STREAM_BUFFER_MAX);
    snd_stream_start(sh, 44100, 1);

    float px0 = 280, py0 = 180;
    for (;;) {
        maple_device_t *d = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = d ? (cont_state_t *)maple_dev_status(d) : NULL;
        if (st) {
            if (st->buttons & CONT_DPAD_LEFT) px0 -= 4;
            if (st->buttons & CONT_DPAD_RIGHT) px0 += 4;
            if (st->buttons & CONT_DPAD_UP) py0 -= 4;
            if (st->buttons & CONT_DPAD_DOWN) py0 += 4;
            if (st->buttons & CONT_START) break;
        }
        snd_stream_poll(sh);
        pvr_wait_ready(); pvr_scene_begin(); pvr_list_begin(PVR_LIST_TR_POLY);
        quad(&hc, 100, 100, 440, 280, PVR_PACK_COLOR(0.5f, 1.0f, 0.8f, 0.2f));
        quad(&ht, px0, py0, 128, 128, PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f));
        pvr_list_finish(); pvr_scene_finish();
    }
    snd_stream_destroy(sh); snd_stream_shutdown();
    return 0;
}
