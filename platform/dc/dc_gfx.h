/* Sega Dreamcast drawing layer for the SDL shell (platform/sdl/main.c), on KallistiOS and the PowerVR2 (PVR).
 *
 * Same idea as platform/n3ds/n3ds_gfx.h: the shell keeps calling SDL_RenderTexture / SDL_RenderFillRect and friends, and these macros turn them into
 * PVR primitives. Every sprite is one textured quad in the translucent list, sent in drawing order (auto-sorting is off in pvr_init), so the layering
 * is exactly what the shell draws.
 *
 * Screen: 640x480 output. The game field is 320x480 and is drawn 1:1 in the middle (x 160..479). The sprite sheets are 1024x1024 at twice the size they
 * are drawn at, so they are halved on the way in (512x512, ARGB4444, 512 KB each in video memory); with 1:1 drawing that is exactly one texel per pixel.
 * Anything that sticks out of the field is covered by two black bars at the end of the frame.
 *
 * Included by dc_sdl.h, which has already defined SDL_FRect and the blend / scale mode constants. */
#ifndef PNJ_DC_GFX_H
#define PNJ_DC_GFX_H

#define PNJ_DC_FIELD_X 160.0f

typedef struct pnj_tex { pvr_ptr_t vram; int w, h; int add, alpha, cr, cg, cb, nearest; unsigned last; } pnj_tex;
typedef struct pnj_win { int unused; } pnj_win;
typedef struct { uint32_t clr; int draw_add; float ox, oy, sc; int hkind, hadd, hnear; const pnj_tex *htex; } pnj_gfx;
#define SDL_Renderer pnj_gfx
#define SDL_Texture pnj_tex
#define SDL_Window pnj_win

static unsigned dc_frame;                        /* counts frames; textures remember the last frame that drew them */
static pnj_gfx pnj_gfx_one;

static pnj_gfx *pnj_gfx_open(void) {
    pnj_gfx *g = &pnj_gfx_one;
    /* only the translucent list is used. 32 words per tile is generous for a 2D game with overlapping text and glow sprites. */
    pvr_init_params_t pp = { { PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_0 }, 512 * 1024 };
    pp.autosort_disabled = 1; pp.opb_overflow_count = 3;
    pvr_init(&pp); pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    g->clr = 0xff000000u; g->draw_add = 0; g->ox = PNJ_DC_FIELD_X; g->oy = 0.0f; g->sc = 1.0f; g->hkind = 0;
    return g;
}
static void pnj_gfx_close(pnj_gfx *g) { (void)g; pvr_shutdown(); }

/* ---------- poly headers ----------
 * A header sets the texture and blend state for the quads that follow it. It is only sent when the state changes. */
static void dc_hdr_col(pnj_gfx *g, int add) {
    if (g->hkind == 1 && g->hadd == add) return;
    pvr_poly_cxt_t cx; pvr_poly_hdr_t h;
    pvr_poly_cxt_col(&cx, PVR_LIST_TR_POLY);
    cx.gen.alpha = true; cx.gen.culling = PVR_CULLING_NONE;
    cx.blend.src = PVR_BLEND_SRCALPHA; cx.blend.dst = add ? PVR_BLEND_ONE : PVR_BLEND_INVSRCALPHA;
    cx.depth.comparison = PVR_DEPTHCMP_ALWAYS; cx.depth.write = false;
    pvr_poly_compile(&h, &cx); pvr_prim(&h, sizeof h);
    g->hkind = 1; g->hadd = add;
}
static void dc_hdr_tex(pnj_gfx *g, const pnj_tex *t) {
    if (g->hkind == 2 && g->htex == t && g->hadd == t->add && g->hnear == t->nearest) return;
    pvr_poly_cxt_t cx; pvr_poly_hdr_t h;
    pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_TWIDDLED, t->w, t->h, t->vram, t->nearest ? PVR_FILTER_NEAREST : PVR_FILTER_BILINEAR);
    cx.gen.alpha = true; cx.gen.culling = PVR_CULLING_NONE;
    cx.blend.src = PVR_BLEND_SRCALPHA; cx.blend.dst = t->add ? PVR_BLEND_ONE : PVR_BLEND_INVSRCALPHA;
    cx.txr.env = PVR_TXRENV_MODULATEALPHA; cx.txr.uv_clamp = PVR_UVCLAMP_UV;
    cx.depth.comparison = PVR_DEPTHCMP_ALWAYS; cx.depth.write = false;
    pvr_poly_compile(&h, &cx); pvr_prim(&h, sizeof h);
    g->hkind = 2; g->htex = t; g->hadd = t->add; g->hnear = t->nearest;
}
static inline void dc_vtx(float x, float y, float u, float v, uint32_t argb, int eol) {
    pvr_vertex_t vt;
    vt.flags = eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    vt.x = x; vt.y = y; vt.z = 1.0f; vt.u = u; vt.v = v; vt.argb = argb; vt.oargb = 0;
    pvr_prim(&vt, sizeof vt);
}
/* corners: a top-left, b top-right, c bottom-left, d bottom-right; sent as a strip c a d b */
static inline void dc_quad(const float *p, float u0, float v0, float u1, float v1, uint32_t argb) {
    dc_vtx(p[4], p[5], u0, v1, argb, 0);
    dc_vtx(p[0], p[1], u0, v0, argb, 0);
    dc_vtx(p[6], p[7], u1, v1, argb, 0);
    dc_vtx(p[2], p[3], u1, v0, argb, 1);
}

/* ---------- frame ---------- */
static void pnj_gfx_begin(pnj_gfx *g) {
    pvr_wait_ready(); pvr_scene_begin(); pvr_list_begin(PVR_LIST_TR_POLY);
    dc_frame++; g->hkind = 0;
}
static void pnj_gfx_end(pnj_gfx *g) {
    float a[8], b[8];                                                    /* black bars over whatever stuck out of the 320 wide field */
    a[0] = 0; a[1] = 0; a[2] = PNJ_DC_FIELD_X; a[3] = 0; a[4] = 0; a[5] = 480; a[6] = PNJ_DC_FIELD_X; a[7] = 480;
    b[0] = PNJ_DC_FIELD_X + 320; b[1] = 0; b[2] = 640; b[3] = 0; b[4] = b[0]; b[5] = 480; b[6] = 640; b[7] = 480;
    dc_hdr_col(g, 0); dc_quad(a, 0, 0, 0, 0, 0xff000000u); dc_quad(b, 0, 0, 0, 0, 0xff000000u);
    pvr_list_finish(); pvr_scene_finish();
}
static void pnj_gfx_screen(pnj_gfx *g) { g->ox = PNJ_DC_FIELD_X; g->oy = 0.0f; g->sc = 1.0f; }

/* ---------- SDL drawing calls the shell makes ---------- */
static inline int pnj_f2b(float v) { return v <= 0 ? 0 : v >= 1 ? 255 : (int)(v * 255.0f + 0.5f); }
static void pnj_set_color(pnj_gfx *g, int r, int gr, int b, int a) {
    g->clr = ((uint32_t)(a & 255) << 24) | ((uint32_t)(r & 255) << 16) | ((uint32_t)(gr & 255) << 8) | (uint32_t)(b & 255);
}
#define SDL_SetRenderDrawColor(r, R, G, B, A) pnj_set_color((r), (R), (G), (B), (A))
#define SDL_SetRenderDrawColorFloat(r, R, G, B, A) pnj_set_color((r), pnj_f2b(R), pnj_f2b(G), pnj_f2b(B), pnj_f2b(A))
#define SDL_SetRenderDrawBlendMode(r, m) ((r)->draw_add = ((m) == SDL_BLENDMODE_ADD))
static void pnj_clear(pnj_gfx *g) {                                      /* the video background colour; the shell clears to black every frame */
    static uint32_t cur = 0xff000000u;
    if (g->clr != cur) { cur = g->clr; pvr_set_bg_color(((cur >> 16) & 255) / 255.0f, ((cur >> 8) & 255) / 255.0f, (cur & 255) / 255.0f); }
}
#define SDL_RenderClear(r) pnj_clear(r)
#define PNJ_DEPTH(r, z) ((void)0)
static void pnj_fill(pnj_gfx *g, const SDL_FRect *q) {
    if ((g->clr >> 24) == 0) return;
    float x0 = q->x * g->sc + g->ox, y0 = q->y * g->sc + g->oy, x1 = x0 + q->w * g->sc, y1 = y0 + q->h * g->sc;
    float p[8] = { x0, y0, x1, y0, x0, y1, x1, y1 };
    dc_hdr_col(g, g->draw_add); dc_quad(p, 0, 0, 0, 0, g->clr);
}
#define SDL_RenderFillRect(r, q) pnj_fill((r), (q))
static void pnj_rect_outline(pnj_gfx *g, const SDL_FRect *q) {
    SDL_FRect a = { q->x, q->y, q->w, 1 }, b = { q->x, q->y + q->h - 1, q->w, 1 }, c = { q->x, q->y, 1, q->h }, d = { q->x + q->w - 1, q->y, 1, q->h };
    pnj_fill(g, &a); pnj_fill(g, &b); pnj_fill(g, &c); pnj_fill(g, &d);
}
#define SDL_RenderRect(r, q) pnj_rect_outline((r), (q))
static void pnj_line(pnj_gfx *g, float x0, float y0, float x1, float y1) {      /* a one pixel wide quad along the line */
    if ((g->clr >> 24) == 0) return;
    x0 = x0 * g->sc + g->ox; y0 = y0 * g->sc + g->oy; x1 = x1 * g->sc + g->ox; y1 = y1 * g->sc + g->oy;
    float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy); if (l < 0.001f) return;
    float nx = -dy / l * 0.5f, ny = dx / l * 0.5f;
    float p[8] = { x0 + nx, y0 + ny, x1 + nx, y1 + ny, x0 - nx, y0 - ny, x1 - nx, y1 - ny };
    dc_hdr_col(g, g->draw_add); dc_quad(p, 0, 0, 0, 0, g->clr);
}
#define SDL_RenderLine(r, a, b, c, d) pnj_line((r), (a), (b), (c), (d))

/* ---------- textures ----------
 * ARGB4444 twiddled into video memory. pnj_tex_from_4444 takes a ready 16-bit image (a .dtx file from the pack: nothing to decode);
 * pnj_tex_make takes a decoded RGBA image (the PNG fallback), halves it when shrink > 1 and goes through the same upload.
 * Both return NULL when video memory is full; the caller then frees the least recently drawn texture (dc_evict_lru) and tries again. */
#include "../sdl/dtx.h"
static pnj_tex *pnj_tex_from_4444(const uint16_t *lin, int dw, int dh, int sharp) {
    if (dw < 8 || dh < 8 || (dw & (dw - 1)) || (dh & (dh - 1)) || dw > 1024 || dh > 1024) return NULL;     /* the PVR wants powers of two */
    pnj_tex *t = (pnj_tex *)calloc(1, sizeof *t); if (!t) return NULL;
    pvr_ptr_t vram = pvr_mem_malloc((size_t)dw * dh * 2);
    if (!vram) { free(t); return NULL; }
    pvr_txr_load_ex(lin, vram, dw, dh, PVR_TXRLOAD_16BPP);
    t->vram = vram; t->w = dw; t->h = dh; t->add = 0; t->alpha = 255; t->cr = t->cg = t->cb = 255; t->nearest = sharp; t->last = dc_frame;
    return t;
}
static pnj_tex *pnj_tex_make(const unsigned char *px, int w, int h, int shrink, int sharp) {
    if (shrink < 1) shrink = 1;
    int dw = w / shrink, dh = h / shrink;
    uint16_t *lin = (uint16_t *)memalign(32, (size_t)dw * dh * 2); if (!lin) return NULL;
    pnj_pack4444(px, w, h, shrink, lin);
    pnj_tex *t = pnj_tex_from_4444(lin, dw, dh, sharp);
    free(lin); return t;
}
static void pnj_tex_free(pnj_tex *t) { if (!t) return; pvr_mem_free(t->vram); free(t); }
/* free the texture that has gone longest without being drawn (not one drawn in the last two frames: the PVR may still be reading it). 1 if one was freed. */
static int dc_evict_lru(pnj_tex **tex, int *texw, int n) {
    int best = -1;
    for (int i = 0; i < n; i++) if (tex[i] && tex[i]->last + 2 <= dc_frame && (best < 0 || tex[i]->last < tex[best]->last)) best = i;
    if (best < 0) return 0;
    pnj_tex_free(tex[best]); tex[best] = NULL; texw[best] = 0; return 1;
}
#define SDL_DestroyTexture(t) pnj_tex_free(t)
#define SDL_SetTextureBlendMode(t, m) ((t)->add = ((m) == SDL_BLENDMODE_ADD))
#define SDL_SetTextureAlphaMod(t, a) ((t)->alpha = (a))
#define SDL_SetTextureColorMod(t, R, G, B) ((t)->cr = (R), (t)->cg = (G), (t)->cb = (B))
#define SDL_SetTextureColorModFloat(t, R, G, B) ((t)->cr = pnj_f2b(R), (t)->cg = pnj_f2b(G), (t)->cb = pnj_f2b(B))
#define SDL_SetTextureScaleMode(t, m) ((t)->nearest = ((m) == SDL_SCALEMODE_NEAREST))

static void pnj_draw_tex(pnj_gfx *g, pnj_tex *t, const SDL_FRect *src, const SDL_FRect *dst, double angle) {
    SDL_FRect full = { 0, 0, (float)t->w, (float)t->h };
    if (!src) src = &full;
    int al = t->alpha < 0 ? 0 : t->alpha > 255 ? 255 : t->alpha; if (!al) return;
    float w = dst->w * g->sc, h = dst->h * g->sc, hw = w * 0.5f, hh = h * 0.5f;
    float cx = dst->x * g->sc + g->ox + hw, cy = dst->y * g->sc + g->oy + hh;
    float rad = (hw > hh ? hw : hh) * (angle != 0.0 ? 1.5f : 1.0f);
    if (cx + rad < g->ox || cx - rad > g->ox + 320.0f * g->sc || cy + rad < 0.0f || cy - rad > 480.0f) return;     /* nothing of it lands on the field */
    t->last = dc_frame;
    dc_hdr_tex(g, t);
    float c = 1.0f, s = 0.0f;
    if (angle != 0.0) { float a = (float)(angle * 3.14159265358979 / 180.0); c = cosf(a); s = sinf(a); }
    float p[8] = { -hw, -hh, hw, -hh, -hw, hh, hw, hh };
    for (int i = 0; i < 8; i += 2) { float dx = p[i], dy = p[i + 1]; p[i] = cx + dx * c - dy * s; p[i + 1] = cy + dx * s + dy * c; }
    uint32_t col = ((uint32_t)al << 24) | ((uint32_t)(t->cr & 255) << 16) | ((uint32_t)(t->cg & 255) << 8) | (uint32_t)(t->cb & 255);
    dc_quad(p, src->x / t->w, src->y / t->h, (src->x + src->w) / t->w, (src->y + src->h) / t->h, col);
}
#define SDL_RenderTextureRotated(r, t, s, d, ang, c, f) pnj_draw_tex((r), (t), (s), (d), (ang))
#define SDL_RenderTexture(r, t, s, d) pnj_draw_tex((r), (t), (s), (d), 0.0)

/* ---------- window and renderer calls that mean nothing here ---------- */
static pnj_win pnj_win_one;
#define SDL_CreateWindow(...) (&pnj_win_one)
#define SDL_DestroyWindow(w) ((void)(w))
#define SDL_DestroyRenderer(r) pnj_gfx_close(r)
#define SDL_GetWindowFlags(w) ((SDL_WindowFlags)0)
#define SDL_SetWindowFullscreen(w, f) ((void)0)
#define SDL_ShowCursor() ((void)0)
#define SDL_HideCursor() ((void)0)
#define SDL_SetRenderLogicalPresentation(r, w, h, m) ((void)0)

#endif
