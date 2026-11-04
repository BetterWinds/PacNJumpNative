/* Small AAC-LC decoder for one file: the power-up jingle (res/raw/wakawaka.3gp) in the original APK.
 * It reads an MP4/3GP file, decodes AAC-LC (mono or stereo, long and short windows, M/S, intensity, noise substitution, TNS, pulse data)
 * and returns a 16-bit WAV in memory. It does not do SBR, PS, LTP, gain control, or the error-resilient profiles; those return NULL and the
 * importer then tries the operating system's decoder or ffmpeg instead. Plain C, no dependencies, so it behaves the same on every platform.
 *
 * Written from ISO/IEC 14496-3. The Huffman codebooks and band layouts in aac_tables.h come from the public domain JAAD decoder. */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "aac_tables.h"

unsigned char *pnj_aac_decode(const unsigned char *file, size_t size, size_t *out_size);

#define PI 3.14159265358979323846
#ifdef AAC_TEST
static int STAT[12];   /* seq0..3, tns, pulse, ms, intensity, noise, cpe, sce */
#define STAT_INC(i) (STAT[i]++)
#else
#define STAT_INC(i) ((void)0)
#endif

/* ---------- bit reader ---------- */
typedef struct { const unsigned char *d; size_t nbits, pos; int err; } br_t;
static unsigned br_get(br_t *b, int n) {
    unsigned v = 0;
    while (n-- > 0) {
        if (b->pos >= b->nbits) { b->err = 1; v <<= 1; continue; }
        v = (v << 1) | ((b->d[b->pos >> 3] >> (7 - (b->pos & 7))) & 1); b->pos++;
    }
    return v;
}
static int hc_find(br_t *b, const aac_hc *t, int cnt) {
    int off = 0, len = t[0].n; unsigned cw = br_get(b, len);
    while (cw != t[off].c) {
        if (++off >= cnt || b->err) { b->err = 1; return 0; }
        int j = t[off].n - len; len = t[off].n; cw = (cw << j) | br_get(b, j);
    }
    return off;
}

/* ---------- decoder state ---------- */
enum { LONG_SEQ, START_SEQ, SHORT_SEQ, STOP_SEQ };
typedef struct { int seq, shape, max_sfb, ngroups, glen[8], nwin; const short *swb; int nswb; } ics_t;
typedef struct {
    ics_t info; int gain;
    int cb[8][64], sf[8][64];
    int q[1024]; float spec[1024];
    int pulse_n, pulse_pos[4], pulse_amp[4];
    int tns, n_filt[8], f_len[8][3], f_ord[8][3], f_dir[8][3]; float f_coef[8][3][20];
    float ov[1024]; int prev_shape;                          /* kept between frames */
} chan_t;
typedef struct {
    int rate_idx, chans; chan_t ch[2]; unsigned rng; int sbr_seen;
    float wl[2][1024], ws[2][128];                            /* rising window halves: [0] sine, [1] Kaiser-Bessel derived */
    float cos_l[8192], cos_s[1024], pow43[8192 + 16];
} dec_t;

static double bessel_i0(double x) { double s = 1, t = 1; for (int k = 1; k < 60; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; }
static void kbd(float *w, int n, double alpha) {              /* rising half of length n */
    double *k = (double *)malloc((size_t)(n + 1) * sizeof *k), sum = 0, acc = 0;
    for (int i = 0; i <= n; i++) { double r = 2.0 * i / n - 1; k[i] = bessel_i0(PI * alpha * sqrt(1 - r * r)); sum += k[i]; }
    for (int i = 0; i < n; i++) { acc += k[i]; w[i] = (float)sqrt(acc / sum); }
    free(k);
}
static void dec_init(dec_t *D) {
    for (int i = 0; i < 1024; i++) D->wl[0][i] = (float)sin(PI / 2048 * (i + 0.5));
    for (int i = 0; i < 128; i++) D->ws[0][i] = (float)sin(PI / 256 * (i + 0.5));
    kbd(D->wl[1], 1024, 4.0); kbd(D->ws[1], 128, 6.0);
    for (int i = 0; i < 8192; i++) D->cos_l[i] = (float)cos(2 * PI * i / 8192);
    for (int i = 0; i < 1024; i++) D->cos_s[i] = (float)cos(2 * PI * i / 1024);
    for (int i = 0; i < 8192 + 16; i++) D->pow43[i] = (float)pow((double)i, 4.0 / 3.0);
}

/* ---------- syntax ---------- */
static int read_ics_info(br_t *b, dec_t *D, ics_t *I, int common) {
    (void)common;
    br_get(b, 1);
    I->seq = (int)br_get(b, 2); I->shape = (int)br_get(b, 1);
    I->ngroups = 1; I->glen[0] = 1;
    if (I->seq == SHORT_SEQ) {
        I->max_sfb = (int)br_get(b, 4);
        for (int i = 0; i < 7; i++) { if (br_get(b, 1)) I->glen[I->ngroups - 1]++; else I->glen[I->ngroups++] = 1; }
        I->nwin = 8; I->swb = AAC_SWB_SHORT[D->rate_idx]; I->nswb = AAC_NSWB_SHORT[D->rate_idx];
    } else {
        I->max_sfb = (int)br_get(b, 6); I->nwin = 1;
        I->swb = AAC_SWB_LONG[D->rate_idx]; I->nswb = AAC_NSWB_LONG[D->rate_idx];
        if (br_get(b, 1)) return 0;                           /* predictor data: Main profile, not supported */
    }
    return !b->err && I->max_sfb <= I->nswb;
}

static void read_spectrum(br_t *b, chan_t *C) {
    const ics_t *I = &C->info; int gw = 0;
    memset(C->q, 0, sizeof C->q);
    for (int g = 0; g < I->ngroups; g++) {
        for (int s = 0; s < I->max_sfb; s++) {
            int cb = C->cb[g][s], width = I->swb[s + 1] - I->swb[s];
            if (cb == 0 || cb >= 13) continue;
            int pair = cb >= 5, uns = (cb == 3 || cb == 4 || cb >= 7), num = pair ? 2 : 4;
            for (int w = 0; w < I->glen[g]; w++) {
                int *q = C->q + (gw + w) * 128 + I->swb[s];
                for (int k = 0; k < width; k += num) {
                    int i = hc_find(b, AAC_HCB[cb - 1], AAC_HCB_N[cb - 1]), v[4];
                    for (int j = 0; j < num; j++) v[j] = AAC_HCB[cb - 1][i].v[j];
                    if (uns) for (int j = 0; j < num; j++) if (v[j] && br_get(b, 1)) v[j] = -v[j];
                    if (cb == 11) for (int j = 0; j < 2; j++) if (abs(v[j]) == 16) {
                        int neg = v[j] < 0, n = 4; while (br_get(b, 1) && !b->err && n < 24) n++;
                        int e = (int)br_get(b, n) | (1 << n); v[j] = neg ? -e : e;
                    }
                    for (int j = 0; j < num; j++) q[k + j] = v[j];
                    if (b->err) return;
                }
            }
        }
        gw += I->glen[g];
    }
}

static int read_ics(br_t *b, dec_t *D, chan_t *C, int common) {
    ics_t *I = &C->info;
    C->gain = (int)br_get(b, 8);
    if (!common && !read_ics_info(b, D, I, 0)) return 0;
    /* section data */
    int bits = I->seq == SHORT_SEQ ? 3 : 5, esc = (1 << bits) - 1;
    memset(C->cb, 0, sizeof C->cb);
    for (int g = 0; g < I->ngroups; g++) {
        int k = 0;
        while (k < I->max_sfb) {
            int cb = (int)br_get(b, 4), end = k, inc;
            if (cb == 12) return 0;
            while ((inc = (int)br_get(b, bits)) == esc && !b->err) end += inc;
            end += inc;
            if (end > I->max_sfb || b->err) return 0;
            for (; k < end; k++) C->cb[g][k] = cb;
        }
    }
    /* scale factors */
    int sfv = C->gain, noise = C->gain - 90, isp = 0, first_noise = 1;
    for (int g = 0; g < I->ngroups; g++) for (int s = 0; s < I->max_sfb; s++) {
        int cb = C->cb[g][s];
        if (cb == 0) { C->sf[g][s] = 0; continue; }
        if (cb >= 14) { isp += AAC_HCB_SF[hc_find(b, AAC_HCB_SF, 121)].v[0] - 60; C->sf[g][s] = isp; }
        else if (cb == 13) {
            if (first_noise) { noise += (int)br_get(b, 9) - 256; first_noise = 0; }
            else noise += AAC_HCB_SF[hc_find(b, AAC_HCB_SF, 121)].v[0] - 60;
            C->sf[g][s] = noise;
        } else { sfv += AAC_HCB_SF[hc_find(b, AAC_HCB_SF, 121)].v[0] - 60; if (sfv < 0 || sfv > 255) return 0; C->sf[g][s] = sfv; }
        if (b->err) return 0;
    }
    /* pulse data */
    C->pulse_n = 0; STAT_INC(I->seq);
    if (br_get(b, 1)) {
        STAT_INC(5);
        if (I->seq == SHORT_SEQ) return 0;
        C->pulse_n = (int)br_get(b, 2) + 1; int sb = (int)br_get(b, 6);
        if (sb >= I->nswb) return 0;
        int pos = I->swb[sb];
        for (int i = 0; i < C->pulse_n; i++) { pos += (int)br_get(b, 5); C->pulse_pos[i] = pos; C->pulse_amp[i] = (int)br_get(b, 4); }
    }
    /* TNS */
    C->tns = (int)br_get(b, 1);
    if (C->tns) {
        STAT_INC(4);
        int sh = I->seq == SHORT_SEQ;
        for (int w = 0; w < I->nwin; w++) {
            C->n_filt[w] = (int)br_get(b, sh ? 1 : 2);
            if (!C->n_filt[w]) continue;
            int res = (int)br_get(b, 1);
            for (int f = 0; f < C->n_filt[w]; f++) {
                C->f_len[w][f] = (int)br_get(b, sh ? 4 : 6); int ord = C->f_ord[w][f] = (int)br_get(b, sh ? 3 : 5);
                if (ord > 20) return 0;
                if (ord) {
                    C->f_dir[w][f] = (int)br_get(b, 1); int comp = (int)br_get(b, 1), len = res + 3 - comp;
                    for (int i = 0; i < ord; i++) {
                        int s = (int)br_get(b, len); if (s >= (1 << (len - 1))) s -= 1 << len;
                        double iq = (((1 << (res + 2)) + (s >= 0 ? -0.5 : 0.5)) / (PI / 2));
                        C->f_coef[w][f][i] = (float)sin(s / iq);
                    }
                }
            }
        }
    }
    if (br_get(b, 1)) return 0;                               /* gain control: not supported */
    read_spectrum(b, C);
    if (b->err) return 0;
    /* pulses go onto the quantised values */
    for (int i = 0; i < C->pulse_n; i++) {
        int p = C->pulse_pos[i]; if (p > 1023) return 0;
        if (C->q[p] > 0) C->q[p] += C->pulse_amp[i]; else C->q[p] -= C->pulse_amp[i];
    }
    return 1;
}

/* ---------- dequantisation, stereo, TNS ---------- */
static void dequant(dec_t *D, chan_t *C) {
    const ics_t *I = &C->info; int gw = 0;
    memset(C->spec, 0, sizeof C->spec);
    for (int g = 0; g < I->ngroups; g++) {
        for (int s = 0; s < I->max_sfb; s++) {
            int cb = C->cb[g][s], width = I->swb[s + 1] - I->swb[s];
            for (int w = 0; w < I->glen[g]; w++) {
                int off = (gw + w) * 128 + I->swb[s];
                if (cb == 0 || cb >= 14) continue;
                if (cb == 13) {                               /* noise substitution */
                    STAT_INC(8);
                    double en = 0; float *x = C->spec + off;
                    for (int k = 0; k < width; k++) { D->rng = D->rng * 1664525u + 1013904223u; x[k] = (float)(int32_t)D->rng; en += (double)x[k] * x[k]; }
                    double sc = pow(2.0, C->sf[g][s] / 4.0) / sqrt(en > 0 ? en : 1);
                    for (int k = 0; k < width; k++) x[k] = (float)(x[k] * sc);
                    continue;
                }
                double gain = pow(2.0, (C->sf[g][s] - 100) / 4.0);
                for (int k = 0; k < width; k++) {
                    int v = C->q[off + k], a = abs(v);
                    double m = a < 8192 + 16 ? D->pow43[a] : pow((double)a, 4.0 / 3.0);
                    C->spec[off + k] = (float)((v < 0 ? -m : m) * gain);
                }
            }
        }
        gw += I->glen[g];
    }
}

static void tns_apply(chan_t *C, int rate_idx) {
    static const unsigned char MAXL[12] = { 31, 31, 34, 40, 42, 51, 46, 46, 42, 42, 42, 39 }, MAXS[12] = { 9, 9, 10, 14, 14, 14, 14, 14, 14, 14, 14, 14 };
    const ics_t *I = &C->info; int sh = I->seq == SHORT_SEQ, mmm = sh ? MAXS[rate_idx] : MAXL[rate_idx]; if (I->max_sfb < mmm) mmm = I->max_sfb;
    for (int w = 0; w < I->nwin; w++) {
        int bottom = I->nswb;
        for (int f = 0; f < C->n_filt[w]; f++) {
            int top = bottom; bottom = top - C->f_len[w][f]; if (bottom < 0) bottom = 0;
            int ord = C->f_ord[w][f]; if (!ord) continue;
            int s0 = I->swb[bottom < mmm ? bottom : mmm], s1 = I->swb[top < mmm ? top : mmm];
            if (s1 <= s0) continue;
            float a[21], t[21]; a[0] = 1;
            for (int m = 1; m <= ord; m++) {                  /* reflection coefficients to filter coefficients */
                float k = C->f_coef[w][f][m - 1];
                for (int i = 1; i < m; i++) t[i] = a[i] + k * a[m - i];
                for (int i = 1; i < m; i++) a[i] = t[i];
                a[m] = k;
            }
            float *x = C->spec + w * 128; int dir = C->f_dir[w][f];
            for (int n = 0; n < s1 - s0; n++) {
                int i = dir ? s1 - 1 - n : s0 + n; float acc = x[i];
                for (int j = 1; j <= ord && j <= n; j++) acc -= a[j] * x[dir ? i + j : i - j];
                x[i] = acc;
            }
        }
    }
}

/* ---------- filterbank ---------- */
static void imdct(dec_t *D, const float *X, int M, float *z) {   /* M coefficients -> 2M samples */
    int n0x2 = M + 1;                                        /* 2 * n0 */
    const float *cs = M == 1024 ? D->cos_l : D->cos_s; int mask = M == 1024 ? 8191 : 1023;
    double sc = 2.0 / (2.0 * M);
    int lo = M == 1024 ? M / 2 : 0, hi = M == 1024 ? M / 2 + M : 2 * M;     /* long frames: compute the middle half, the rest follows by symmetry */
    for (int n = lo; n < hi; n++) {
        double acc = 0; int base = 2 * n + n0x2;
        for (int k = 0; k < M; k++) acc += X[k] * cs[(base * (2 * k + 1)) & mask];
        z[n] = (float)(acc * sc);
    }
    if (M == 1024) {
        for (int j = 0; j < M / 2; j++) { z[M / 2 - 1 - j] = -z[M / 2 + j]; z[3 * M / 2 + j] = z[3 * M / 2 - 1 - j]; }
    }
}
static void synth(dec_t *D, chan_t *C, float *out) {          /* 1024 new samples */
    const ics_t *I = &C->info; float z[2048], acc[2048]; int cur = I->shape, prev = C->prev_shape;
    if (I->seq != SHORT_SEQ) {
        imdct(D, C->spec, 1024, z);
        for (int n = 0; n < 1024; n++) {
            float w = I->seq == STOP_SEQ ? (n < 448 ? 0.f : n < 576 ? D->ws[prev][n - 448] : 1.f) : D->wl[prev][n];
            z[n] *= w;
        }
        for (int j = 0; j < 1024; j++) {
            float w = I->seq == START_SEQ ? (j < 448 ? 1.f : j < 576 ? D->ws[cur][127 - (j - 448)] : 0.f) : D->wl[cur][1023 - j];
            z[1024 + j] *= w;
        }
        for (int i = 0; i < 1024; i++) { out[i] = C->ov[i] + z[i]; C->ov[i] = z[1024 + i]; }
    } else {
        memset(acc, 0, sizeof acc);
        for (int w = 0; w < 8; w++) {
            float y[256]; imdct(D, C->spec + w * 128, 128, y);
            int sp = w == 0 ? prev : cur;
            for (int n = 0; n < 128; n++) { y[n] *= D->ws[sp][n]; y[128 + n] *= D->ws[cur][127 - n]; }
            for (int n = 0; n < 256; n++) acc[448 + 128 * w + n] += y[n];
        }
        for (int i = 0; i < 1024; i++) { out[i] = C->ov[i] + acc[i]; C->ov[i] = acc[1024 + i]; }
    }
    C->prev_shape = cur;
}

/* ---------- frames ---------- */
static int decode_frame(dec_t *D, const unsigned char *p, size_t n, float pcm[2][1024]) {
    br_t b = { p, n * 8, 0, 0 }; int done[2] = { 0, 0 }, nch = 0;
    for (;;) {
        int id = (int)br_get(&b, 3); if (b.err) return 0;
        if (id == 7) break;
        if (id == 0 || id == 3) {                             /* SCE, LFE */
            br_get(&b, 4); if (nch >= D->chans) return 0;
            chan_t *C = &D->ch[nch];
            if (!read_ics(&b, D, C, 0)) return 0;
            dequant(D, C); if (C->tns) tns_apply(C, D->rate_idx);
            synth(D, C, pcm[nch]); done[nch] = 1; nch++;
        } else if (id == 1) {                                 /* CPE */
            br_get(&b, 4); if (nch + 2 > D->chans) return 0;
            chan_t *L = &D->ch[nch], *R = &D->ch[nch + 1]; int common = (int)br_get(&b, 1), ms = 0; unsigned char msu[8][64]; memset(msu, 0, sizeof msu);
            if (common) {
                if (!read_ics_info(&b, D, &L->info, 1)) return 0;
                R->info = L->info;
                ms = (int)br_get(&b, 2);
                if (ms == 1) { for (int g = 0; g < L->info.ngroups; g++) for (int s = 0; s < L->info.max_sfb; s++) msu[g][s] = (unsigned char)br_get(&b, 1); }
                else if (ms == 2) memset(msu, 1, sizeof msu);
                else if (ms == 3) return 0;
            }
            if (!read_ics(&b, D, L, common) || !read_ics(&b, D, R, common)) return 0;
            dequant(D, L); dequant(D, R);
            const ics_t *I = &L->info;
            if (common && ms) {
                int gw = 0; STAT_INC(6);
                for (int g = 0; g < I->ngroups; g++) { for (int s = 0; s < I->max_sfb; s++) { if (msu[g][s] && L->cb[g][s] < 13 && R->cb[g][s] < 13)
                    for (int w = 0; w < I->glen[g]; w++) for (int k = I->swb[s]; k < I->swb[s + 1]; k++) {
                        int i = (gw + w) * 128 + k; float m = L->spec[i], sd = R->spec[i]; L->spec[i] = m + sd; R->spec[i] = m - sd; } }
                    gw += I->glen[g]; }
            }
            {   /* intensity stereo: the right channel's bands marked 14/15 are copies of the left */
                int gw = 0; const ics_t *J = &R->info;
                for (int g = 0; g < J->ngroups; g++) { for (int s = 0; s < J->max_sfb; s++) { int cb = R->cb[g][s]; if (cb < 14) continue;
                    STAT_INC(7);
                    double sc = pow(0.5, R->sf[g][s] / 4.0) * (cb == 15 ? 1 : -1) * ((common && ms && msu[g][s]) ? -1 : 1);
                    for (int w = 0; w < J->glen[g]; w++) for (int k = J->swb[s]; k < J->swb[s + 1]; k++) { int i = (gw + w) * 128 + k; R->spec[i] = (float)(L->spec[i] * sc); } }
                    gw += J->glen[g]; }
            }
            if (L->tns) tns_apply(L, D->rate_idx);
            if (R->tns) tns_apply(R, D->rate_idx);
            synth(D, L, pcm[nch]); synth(D, R, pcm[nch + 1]); done[nch] = done[nch + 1] = 1; nch += 2;
        } else if (id == 4) {                                 /* DSE: skip */
            br_get(&b, 4); int al = (int)br_get(&b, 1), cnt = (int)br_get(&b, 8); if (cnt == 255) cnt += (int)br_get(&b, 8);
            if (al) b.pos = (b.pos + 7) & ~(size_t)7;
            b.pos += (size_t)cnt * 8;
        } else if (id == 6) {                                 /* FIL: skip; note SBR */
            int cnt = (int)br_get(&b, 4); if (cnt == 15) cnt += (int)br_get(&b, 8) - 1;
            if (cnt > 0) { int ext = (int)br_get(&b, 4); if (ext == 13 || ext == 14) D->sbr_seen = 1; b.pos += (size_t)cnt * 8 - 4; }
        } else return 0;                                      /* PCE, CCE: not supported */
        if (b.pos > b.nbits) return 0;
    }
    return nch == D->chans;
}

/* ---------- MP4 ---------- */
static uint32_t be32(const unsigned char *p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static int box_find(const unsigned char *d, size_t s, size_t e, const char *t, size_t *bs, size_t *be, size_t from) {   /* child box of d[s..e) */
    size_t p = from ? from : s;
    while (p + 8 <= e) {
        uint64_t sz = be32(d + p); size_t hdr = 8;
        if (sz == 1) { if (p + 16 > e) return 0; sz = ((uint64_t)be32(d + p + 8) << 32) | be32(d + p + 12); hdr = 16; }
        else if (sz == 0) sz = e - p;
        if (sz < hdr || p + sz > e) return 0;
        if (!memcmp(d + p + 4, t, 4)) { *bs = p + hdr; *be = p + (size_t)sz; return 1; }
        p += (size_t)sz;
    }
    return 0;
}
static int esds_asc(const unsigned char *p, size_t n, const unsigned char **asc, size_t *an) {
    size_t i = 4;                                             /* version and flags */
    while (i + 2 <= n) {
        int tag = p[i++]; size_t len = 0; int c = 0;
        do { len = (len << 7) | (p[i] & 0x7f); c = p[i++] & 0x80; } while (c && i < n);
        if (tag == 0x03) i += 3;                              /* ES_ID and flags; descend */
        else if (tag == 0x04) i += 13;                        /* object type, stream type, buffer size, bit rates; descend */
        else if (tag == 0x05) { if (i + len > n) return 0; *asc = p + i; *an = len; return 1; }
        else i += len;
    }
    return 0;
}

unsigned char *pnj_aac_decode(const unsigned char *f, size_t size, size_t *out_size) {
    static const int RATES[12] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000 };
    size_t a, b, ms, me, ts, te, ns, ne, bs, be2, ss, se, tbs, tbe;
    unsigned char *res = 0; dec_t *D = 0; uint32_t *sz = 0; uint64_t *off = 0;
    if (!box_find(f, 0, size, "moov", &ms, &me, 0)) return 0;
    /* first audio track that has an mp4a sample entry */
    size_t from = 0; int found = 0;
    while (box_find(f, ms, me, "trak", &ts, &te, from)) {
        from = te;
        if (box_find(f, ts, te, "mdia", &a, &b, 0) && box_find(f, a, b, "minf", &a, &b, 0) && box_find(f, a, b, "stbl", &ns, &ne, 0) &&
            box_find(f, ns, ne, "stsd", &bs, &be2, 0) && be2 - bs > 16 && !memcmp(f + bs + 12, "mp4a", 4)) { found = 1; break; }
    }
    if (!found) return 0;
    /* sample entry: 8 byte header, 20 byte audio header, then child boxes */
    const unsigned char *asc = 0; size_t an = 0, es, ee;
    { size_t e0 = bs + 8; uint32_t esz = be32(f + e0); if (esz < 36 || e0 + esz > be2) return 0;
      if (!box_find(f, e0 + 8 + 28, e0 + esz, "esds", &es, &ee, 0) || !esds_asc(f + es, ee - es, &asc, &an) || an < 2) return 0; }
    int obj = asc[0] >> 3, fi = ((asc[0] & 7) << 1) | (asc[1] >> 7), cc = (asc[1] >> 3) & 15;
    if (obj != 2 || fi > 11 || (cc != 1 && cc != 2) || (asc[1] & 4)) return 0;     /* AAC-LC only, 1024-sample frames */
    if (!box_find(f, ns, ne, "stsz", &ss, &se, 0)) return 0;
    uint32_t fixed = be32(f + ss + 4), cnt = be32(f + ss + 8);
    if (cnt == 0 || cnt > 100000 || (!fixed && ss + 12 + (size_t)cnt * 4 > se)) return 0;
    sz = (uint32_t *)malloc(cnt * sizeof *sz); off = (uint64_t *)malloc(cnt * sizeof *off);
    D = (dec_t *)calloc(1, sizeof *D); if (!sz || !off || !D) goto done;
    for (uint32_t i = 0; i < cnt; i++) sz[i] = fixed ? fixed : be32(f + ss + 12 + (size_t)i * 4);
    {   /* chunk offsets and samples per chunk */
        size_t cs, ce, sts, ste; int co64 = 0;
        if (!box_find(f, ns, ne, "stco", &cs, &ce, 0)) { if (!box_find(f, ns, ne, "co64", &cs, &ce, 0)) goto done; co64 = 1; }
        if (!box_find(f, ns, ne, "stsc", &sts, &ste, 0)) goto done;
        uint32_t nch = be32(f + cs + 4), nsc = be32(f + sts + 4), s = 0;
        if (cs + 8 + (size_t)nch * (co64 ? 8 : 4) > ce || sts + 8 + (size_t)nsc * 12 > ste || nsc == 0) goto done;
        for (uint32_t c = 0; c < nch && s < cnt; c++) {
            uint32_t spc = 0;
            for (uint32_t e = 0; e < nsc; e++) if (be32(f + sts + 8 + e * 12) <= c + 1) spc = be32(f + sts + 8 + e * 12 + 4);
            uint64_t o = co64 ? (((uint64_t)be32(f + cs + 8 + c * 8) << 32) | be32(f + cs + 12 + c * 8)) : be32(f + cs + 8 + c * 4);
            for (uint32_t k = 0; k < spc && s < cnt; k++) { off[s] = o; o += sz[s]; s++; }
        }
        if (s != cnt) goto done;
    }
    (void)tbs; (void)tbe;
    D->rate_idx = fi; D->chans = cc; D->rng = 0x1F2E3D4C; dec_init(D);
    size_t nsmp = (size_t)cnt * 1024, bytes = nsmp * (size_t)cc * 2;
    res = (unsigned char *)malloc(44 + bytes); if (!res) goto done;
    {
        short *pcm = (short *)(res + 44); float fr[2][1024];
        for (uint32_t i = 0; i < cnt; i++) {
            if (off[i] + sz[i] > size) { free(res); res = 0; goto done; }
            if (!decode_frame(D, f + off[i], sz[i], fr) || D->sbr_seen) { free(res); res = 0; goto done; }
            for (int n = 0; n < 1024; n++) for (int c = 0; c < cc; c++) {
                float v = fr[c][n]; long q = lrintf(v); if (q > 32767) q = 32767; if (q < -32768) q = -32768;
                pcm[((size_t)i * 1024 + n) * cc + c] = (short)q;
            }
        }
        /* little-endian host assumed for the samples below, like the rest of the importer */
        unsigned rate = (unsigned)RATES[fi], ch = (unsigned)cc, br = rate * ch * 2, ba = ch * 2; unsigned v;
        #define P32(o, x) (v = (unsigned)(x), res[o] = v & 255, res[(o) + 1] = (v >> 8) & 255, res[(o) + 2] = (v >> 16) & 255, res[(o) + 3] = (v >> 24) & 255)
        #define P16(o, x) (v = (unsigned)(x), res[o] = v & 255, res[(o) + 1] = (v >> 8) & 255)
        memcpy(res, "RIFF", 4); P32(4, 36 + bytes); memcpy(res + 8, "WAVEfmt ", 8); P32(16, 16); P16(20, 1); P16(22, ch); P32(24, rate); P32(28, br); P16(32, ba); P16(34, 16);
        memcpy(res + 36, "data", 4); P32(40, bytes);
        #undef P32
        #undef P16
        *out_size = 44 + bytes;
    }
done:
    free(sz); free(off); free(D);
    return res;
}

#ifdef AAC_TEST
#include <stdio.h>
int main(int argc, char **argv) {
    (void)argc; FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *d = (unsigned char *)malloc((size_t)n); if (fread(d, 1, (size_t)n, f) != (size_t)n) return 2; fclose(f);
    size_t on = 0; unsigned char *w = pnj_aac_decode(d, (size_t)n, &on);
    if (!w) { fprintf(stderr, "decode failed\n"); return 1; }
    FILE *o = fopen(argv[2], "wb"); fwrite(w + 44, 1, on - 44, o); fclose(o);
    fprintf(stderr, "ok %zu bytes pcm\nlong %d start %d short %d stop %d | tns %d pulse %d ms %d intensity %d noise %d\n", on - 44, STAT[0], STAT[1], STAT[2], STAT[3], STAT[4], STAT[5], STAT[6], STAT[7], STAT[8]); return 0;
}
#endif
