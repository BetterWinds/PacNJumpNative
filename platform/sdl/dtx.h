/* Dreamcast texture files ("dc/<sheet>.dtx" in the pack). The console decodes nothing at run time: the file is the sheet already halved to the size it is
 * drawn at, and the loader only has to copy it into video memory.
 *   bytes 0..3  "DTX1"      4..5  width (u16 LE)      6..7  height (u16 LE)      then width * height ARGB4444 values (u16 LE), top row first
 *   bytes 0..3  "DTX2"      same header, then the PowerVR's own VQ format: a 256-entry codebook (2048 bytes, each entry a 2x2 block of four ARGB4444 values)
 *                           and then one index byte per 2x2 block, both in twiddled order. Square power-of-two sizes only. 1/8 the size of DTX1 in video memory,
 *                           which is what lets every sheet stay resident, so nothing is read from the disc during play.
 * pnj_pack4444 is shared: the tool side (--dcpack, desktop) and the console's PNG fallback (dc_gfx.h) both use it, so they agree to the bit. */
#ifndef PNJ_DTX_H
#define PNJ_DTX_H
#include <stdint.h>
/* RGBA8 (w x h) to ARGB4444, shrunk by an integer factor with an alpha-weighted box average. out holds (w / shrink) * (h / shrink) values. */
static void pnj_pack4444(const unsigned char *px, int w, int h, int shrink, uint16_t *out) {
    if (shrink < 1) shrink = 1;
    int dw = w / shrink, dh = h / shrink;
    for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
        unsigned r, g, b, a;
        if (shrink > 1) {
            unsigned sa = 0, sr = 0, sg = 0, sb = 0;
            for (int j = 0; j < shrink; j++) for (int i = 0; i < shrink; i++) {
                const unsigned char *q = px + ((size_t)(y * shrink + j) * w + (x * shrink + i)) * 4;
                sa += q[3]; sr += q[0] * q[3]; sg += q[1] * q[3]; sb += q[2] * q[3];
            }
            if (sa) { r = sr / sa; g = sg / sa; b = sb / sa; } else r = g = b = 0;
            a = sa / (unsigned)(shrink * shrink);
        } else { const unsigned char *p = px + ((size_t)y * w + x) * 4; r = p[0]; g = p[1]; b = p[2]; a = p[3]; }
        out[(size_t)y * dw + x] = (uint16_t)((((a * 15 + 127) / 255) << 12) | (((r * 15 + 127) / 255) << 8) | (((g * 15 + 127) / 255) << 4) | ((b * 15 + 127) / 255));
    }
}
#ifndef PNJ_DC
/* ---------- VQ encoder (tool side only) ----------
 * Twiddle order puts the four pixels of every 2x2 block next to each other, so a block's vector is four consecutive twiddled pixels and its index
 * is the block's own twiddled position. That is the layout pvrtex from KallistiOS writes (checked against it bit for bit). The codebook is a k-means
 * (Lloyd) fit over the blocks' 16 channel values, trained on a sample and then used to index every block. */
#include <stdlib.h>
#include <string.h>
static uint32_t pnj_twid(uint32_t v) { uint32_t r = 0; for (int i = 0; i < 10; i++) r |= ((v >> i) & 1u) << (2 * i); return r; }
static uint32_t vq_rng = 12345u;
static uint32_t vq_rand(void) { vq_rng = vq_rng * 1664525u + 1013904223u; return vq_rng >> 8; }
static void vq_assign(const float *x, int n, const float *cb, int *out, float *err) {
    float c2[256]; for (int k = 0; k < 256; k++) { float s = 0; for (int d = 0; d < 16; d++) s += cb[k * 16 + d] * cb[k * 16 + d]; c2[k] = s; }
    for (int i = 0; i < n; i++) {
        const float *v = x + (size_t)i * 16; float best = 1e30f; int bk = 0;
        for (int k = 0; k < 256; k++) {
            const float *c = cb + k * 16; float dot = 0;
            for (int d = 0; d < 16; d++) dot += v[d] * c[d];
            float dist = c2[k] - 2 * dot; if (dist < best) { best = dist; bk = k; }
        }
        out[i] = bk; if (err) { float vv = 0; for (int d = 0; d < 16; d++) vv += v[d] * v[d]; err[i] = vv + best; }
    }
}
/* lin: w*h ARGB4444, w == h, a power of two >= 8. out receives 2048 + (w / 2) * (h / 2) bytes. Returns 1 on success. */
static int pnj_vq4444(const uint16_t *lin, int w, int h, unsigned char *out) {
    if (w != h || w < 8 || (w & (w - 1)) || w > 1024) return 0;
    int bw = w / 2, nb = bw * bw;
    float *X = (float *)malloc((size_t)nb * 16 * sizeof(float)); int *ix = (int *)malloc((size_t)nb * sizeof(int));
    if (!X || !ix) { free(X); free(ix); return 0; }
    for (int by = 0; by < bw; by++) for (int bx = 0; bx < bw; bx++) {
        float *v = X + ((size_t)by * bw + bx) * 16;
        for (int j = 0; j < 4; j++) {                                   /* pixel order inside a block: y bit low, x bit high (twiddle order) */
            uint16_t p = lin[(size_t)(2 * by + (j & 1)) * w + (2 * bx + (j >> 1))];
            v[j * 4 + 0] = (float)((p >> 12) & 15); v[j * 4 + 1] = (float)((p >> 8) & 15); v[j * 4 + 2] = (float)((p >> 4) & 15); v[j * 4 + 3] = (float)(p & 15);
        }
    }
    int nt = nb < 16384 ? nb : 16384;
    float *T = (float *)malloc((size_t)nt * 16 * sizeof(float)), *err = (float *)malloc((size_t)nt * sizeof(float)); int *ti = (int *)malloc((size_t)nt * sizeof(int));
    float cb[256 * 16], sum[256 * 16]; int cnt[256];
    if (!T || !err || !ti) { free(X); free(ix); free(T); free(err); free(ti); return 0; }
    vq_rng = 12345u;
    for (int i = 0; i < nt; i++) memcpy(T + (size_t)i * 16, X + (size_t)(nb <= 16384 ? (uint32_t)i : vq_rand() % (uint32_t)nb) * 16, 16 * sizeof(float));
    for (int k = 0; k < 256; k++) memcpy(cb + k * 16, T + (size_t)(vq_rand() % (uint32_t)nt) * 16, 16 * sizeof(float));
    for (int it = 0; it < 14; it++) {
        vq_assign(T, nt, cb, ti, err);
        memset(sum, 0, sizeof sum); memset(cnt, 0, sizeof cnt);
        for (int i = 0; i < nt; i++) { cnt[ti[i]]++; for (int d = 0; d < 16; d++) sum[ti[i] * 16 + d] += T[(size_t)i * 16 + d]; }
        for (int k = 0; k < 256; k++) {
            if (cnt[k]) { for (int d = 0; d < 16; d++) cb[k * 16 + d] = sum[k * 16 + d] / cnt[k]; }
            else { int worst = 0; for (int i = 1; i < nt; i++) if (err[i] > err[worst]) worst = i;           /* an empty cell takes the worst-fitted sample */
                   memcpy(cb + k * 16, T + (size_t)worst * 16, 16 * sizeof(float)); err[worst] = 0; }
        }
    }
    for (int k = 0; k < 256; k++) for (int d = 0; d < 16; d++) { float v = cb[k * 16 + d]; cb[k * 16 + d] = v < 0 ? 0 : v > 15 ? 15 : (float)(int)(v + 0.5f); }   /* the file stores 4-bit channels */
    vq_assign(X, nb, cb, ix, NULL);
    for (int k = 0; k < 256; k++) for (int j = 0; j < 4; j++) {
        const float *c = cb + k * 16 + j * 4;
        uint16_t p = (uint16_t)(((int)c[0] << 12) | ((int)c[1] << 8) | ((int)c[2] << 4) | (int)c[3]);
        out[k * 8 + j * 2] = (unsigned char)(p & 255); out[k * 8 + j * 2 + 1] = (unsigned char)(p >> 8);
    }
    for (int by = 0; by < bw; by++) for (int bx = 0; bx < bw; bx++) out[2048 + (pnj_twid((uint32_t)by) | (pnj_twid((uint32_t)bx) << 1))] = (unsigned char)ix[by * bw + bx];
    free(X); free(ix); free(T); free(err); free(ti); return 1;
}
#endif
#endif
