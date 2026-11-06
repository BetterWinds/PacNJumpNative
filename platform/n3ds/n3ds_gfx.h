/* Nintendo 3DS drawing and input layer for the SDL shell (platform/sdl/main.c), built with devkitARM + libctru + citro2d.
 *
 * SDL3 has a 3DS backend, but it only does software rendering. This header swaps the handful of SDL window and renderer calls the shell
 * makes for citro2d (GPU) versions, so menus, the HUD and the pack/import code stay the same files. Audio, file access, timers and threads
 * still come from SDL3's own 3DS backend; input is read straight from libctru and handed to the shell as ordinary SDL events.
 *
 * Screens: the game field is 320x480 (portrait). The top screen (400x240) shows the upper half, centred; the touch screen (320x240) shows the
 * lower half. Every frame is drawn twice, once per screen, with a different offset.
 *
 * Included by main.c right after the SDL headers (and only when PNJ_N3DS is defined). The macros below replace SDL's names, so everything
 * after this point that says SDL_Renderer / SDL_Texture / SDL_RenderTextureRotated ... is talking to this file. */
#ifndef PNJ_N3DS_GFX_H
#define PNJ_N3DS_GFX_H

#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* citro2d rotates with a different sign than SDL on some versions; flip this if rotated sprites (the title bird, spinning pickups) turn the wrong way */
#ifndef PNJ_N3DS_ROT_SIGN
#define PNJ_N3DS_ROT_SIGN 1.0f
#endif

typedef struct { C3D_RenderTarget *scr[2]; u32 clr; int blend; int draw_add; float ox, oy, sc; int cur; } pnj_gfx;
static int pnj_dbg_tex = -1, pnj_dbg_sub;    /* texture viewer: hold L and press R to step through the sheets, hold L and press X to move the close-up */
static int pnj_top_pass;                      /* 1 while the top screen is drawn in menu mode: the game scene without the pause overlay */
typedef struct { C3D_Tex tex; int w, h; int add; int alpha; int cr, cg, cb; } pnj_tex;
typedef struct pnj_win { int unused; } pnj_win;
#define SDL_Renderer pnj_gfx
#define SDL_Texture pnj_tex
#define SDL_Window pnj_win

#define PNJ_N3DS_FIELD_W 320
#define PNJ_N3DS_FIELD_H 480

/* ---------- renderer ---------- */
static pnj_gfx pnj_gfx_one;
static pnj_gfx *pnj_gfx_open(void) {
    pnj_gfx *g = &pnj_gfx_one;
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    g->scr[0] = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g->scr[1] = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g->clr = C2D_Color32(0, 0, 0, 255); g->blend = -1; g->draw_add = 0; g->cur = 0; g->sc = 1.0f;
    return g;
}
static void pnj_gfx_close(pnj_gfx *g) { (void)g; C2D_Fini(); C3D_Fini(); gfxExit(); }
static void pnj_gfx_blend(pnj_gfx *g, int add) {
    if (g->blend == add) return;
    C2D_Flush(); g->blend = add;
    if (add) C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);
    else C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
}
static void pnj_gfx_begin(pnj_gfx *g) { (void)g; C3D_FrameBegin(C3D_FRAME_SYNCDRAW); }
static void pnj_gfx_end(pnj_gfx *g) { (void)g; C3D_FrameEnd(0); }
/* Playing: screen 0 (top, centred in 400 wide) shows field rows 0..239, screen 1 (touch) rows 240..479, both 1:1.
 * Menu mode: the whole 320x480 menu is drawn on the touch screen at half size, and the top screen shows the background or the paused game. */
static void pnj_gfx_screen(pnj_gfx *g, int s, int menu) {
    g->cur = s; g->sc = 1.0f;
    if (!menu) { g->ox = s == 0 ? 40.0f : 0.0f; g->oy = s == 0 ? 0.0f : -240.0f; }
    else if (s == 0) { g->ox = 40.0f; g->oy = 0.0f; }
    else { g->sc = 0.5f; g->ox = 80.0f; g->oy = 0.0f; }
    C2D_TargetClear(g->scr[s], C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(g->scr[s]);
    g->blend = -1; pnj_gfx_blend(g, 0);
}

/* ---------- SDL drawing calls the shell makes ---------- */
static void pnj_set_color(pnj_gfx *g, int r, int gr, int b, int a) { g->clr = C2D_Color32((u8)r, (u8)gr, (u8)b, (u8)a); }
static inline int pnj_f2b(float v) { return v <= 0 ? 0 : v >= 1 ? 255 : (int)(v * 255.0f + 0.5f); }
#define SDL_SetRenderDrawColor(r, R, G, B, A) pnj_set_color((r), (R), (G), (B), (A))
#define SDL_SetRenderDrawColorFloat(r, R, G, B, A) pnj_set_color((r), pnj_f2b(R), pnj_f2b(G), pnj_f2b(B), pnj_f2b(A))
#define SDL_SetRenderDrawBlendMode(r, m) ((r)->draw_add = ((m) == SDL_BLENDMODE_ADD))
static void pnj_clear(pnj_gfx *g) { C2D_TargetClear(g->scr[g->cur], g->clr); }
#define SDL_RenderClear(r) pnj_clear(r)
static void pnj_fill(pnj_gfx *g, const SDL_FRect *q) {
    pnj_gfx_blend(g, g->draw_add);
    C2D_DrawRectSolid(q->x * g->sc + g->ox, q->y * g->sc + g->oy, 0.0f, q->w * g->sc, q->h * g->sc, g->clr);
}
#define SDL_RenderFillRect(r, q) pnj_fill((r), (q))
static void pnj_rect_outline(pnj_gfx *g, const SDL_FRect *q) {
    SDL_FRect a = { q->x, q->y, q->w, 1 }, b = { q->x, q->y + q->h - 1, q->w, 1 }, c = { q->x, q->y, 1, q->h }, d = { q->x + q->w - 1, q->y, 1, q->h };
    pnj_fill(g, &a); pnj_fill(g, &b); pnj_fill(g, &c); pnj_fill(g, &d);
}
#define SDL_RenderRect(r, q) pnj_rect_outline((r), (q))
static void pnj_line(pnj_gfx *g, float x0, float y0, float x1, float y1) {
    pnj_gfx_blend(g, g->draw_add);
    C2D_DrawLine(x0 * g->sc + g->ox, y0 * g->sc + g->oy, g->clr, x1 * g->sc + g->ox, y1 * g->sc + g->oy, g->clr, 1.0f, 0.0f);
}
#define SDL_RenderLine(r, a, b, c, d) pnj_line((r), (a), (b), (c), (d))

/* ---------- textures ----------
 * Made from a decoded RGBA image. The sheets in the pack are 1024x1024 at 2x the size they are drawn at, so they are halved on the way in
 * (alpha-weighted 2x2 average): a quarter of the memory, and the 3DS screen is only 400 wide anyway. The GPU wants its own tiled layout, so
 * the pixels go into a plain linear buffer first and the console's own copy engine tiles and flips them into the texture. */
#ifndef PNJ_N3DS_TEX_FLIP
#define PNJ_N3DS_TEX_FLIP 1          /* the image's top row ends up at v = 1; set to 0 if every sprite comes out upside down */
#endif
static pnj_tex *pnj_tex_make(const unsigned char *px, int w, int h, int half, int sharp) {
    int dw = half ? w / 2 : w, dh = half ? h / 2 : h;
    if (dw < 8 || dh < 8 || (dw & (dw - 1)) || (dh & (dh - 1)) || dw > 1024 || dh > 1024) return NULL;     /* the GPU needs powers of two */
    pnj_tex *t = (pnj_tex *)calloc(1, sizeof *t); if (!t) return NULL;
    if (!C3D_TexInit(&t->tex, (u16)dw, (u16)dh, GPU_RGBA8)) { free(t); return NULL; }
    size_t bytes = (size_t)dw * dh * 4;
    u32 *lin = (u32 *)linearAlloc(bytes);
    if (!lin) { C3D_TexDelete(&t->tex); free(t); return NULL; }
    for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
        unsigned r, g, b, a;
        if (half) {
            const unsigned char *p0 = px + ((size_t)(y * 2) * w + x * 2) * 4, *p1 = p0 + 4, *p2 = p0 + (size_t)w * 4, *p3 = p2 + 4;
            const unsigned char *q[4] = { p0, p1, p2, p3 }; unsigned sa = 0, sr = 0, sg = 0, sb = 0;
            for (int k = 0; k < 4; k++) { sa += q[k][3]; sr += q[k][0] * q[k][3]; sg += q[k][1] * q[k][3]; sb += q[k][2] * q[k][3]; }
            if (sa) { r = sr / sa; g = sg / sa; b = sb / sa; } else r = g = b = 0;
            a = sa / 4;
        } else { const unsigned char *p = px + ((size_t)y * w + x) * 4; r = p[0]; g = p[1]; b = p[2]; a = p[3]; }
        lin[(size_t)y * dw + x] = (r << 24) | (g << 16) | (b << 8) | a;
    }
    GSPGPU_FlushDataCache(lin, bytes);
    C3D_SyncDisplayTransfer(lin, GX_BUFFER_DIM(dw, dh), (u32 *)t->tex.data, GX_BUFFER_DIM(dw, dh),
        GX_TRANSFER_FLIP_VERT(PNJ_N3DS_TEX_FLIP) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    linearFree(lin);
    C3D_TexFlush(&t->tex);
    C3D_TexSetFilter(&t->tex, sharp ? GPU_NEAREST : GPU_LINEAR, sharp ? GPU_NEAREST : GPU_LINEAR);
    C3D_TexSetWrap(&t->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    t->w = dw; t->h = dh; t->add = 0; t->alpha = 255; t->cr = t->cg = t->cb = 255;
    return t;
}
static void pnj_tex_free(pnj_tex *t) { if (!t) return; C3D_TexDelete(&t->tex); free(t); }
#define SDL_DestroyTexture(t) pnj_tex_free(t)
#define SDL_SetTextureBlendMode(t, m) ((t)->add = ((m) == SDL_BLENDMODE_ADD))
#define SDL_SetTextureAlphaMod(t, a) ((t)->alpha = (a))
#define SDL_SetTextureColorMod(t, R, G, B) ((t)->cr = (R), (t)->cg = (G), (t)->cb = (B))
#define SDL_SetTextureColorModFloat(t, R, G, B) ((t)->cr = pnj_f2b(R), (t)->cg = pnj_f2b(G), (t)->cb = pnj_f2b(B))
#define SDL_SetTextureScaleMode(t, m) pnj_tex_filter((t), (m))
static void pnj_tex_filter(pnj_tex *t, int mode) { GPU_TEXTURE_FILTER_PARAM f = mode == SDL_SCALEMODE_NEAREST ? GPU_NEAREST : GPU_LINEAR; C3D_TexSetFilter(&t->tex, f, f); }

static void pnj_draw_tex(pnj_gfx *g, pnj_tex *t, const SDL_FRect *src, const SDL_FRect *dst, double angle) {
    SDL_FRect full = { 0, 0, (float)t->w, (float)t->h };
    if (!src) src = &full;
    Tex3DS_SubTexture sub;
    sub.width = (u16)src->w; sub.height = (u16)src->h;
    sub.left = src->x / t->w; sub.right = (src->x + src->w) / t->w;
    sub.top = 1.0f - src->y / t->h; sub.bottom = 1.0f - (src->y + src->h) / t->h;     /* texture v runs bottom to top */
    C2D_Image img = { &t->tex, &sub };
    pnj_gfx_blend(g, t->add);
    /* the colour tint is a multiply on desktop; citro2d only mixes towards a colour, so additive glow (the only place it matters) is dimmed instead */
    float a = t->alpha / 255.0f;
    if (t->add) { int m = t->cr > t->cg ? t->cr : t->cg; if (t->cb > m) m = t->cb; a *= m / 255.0f; if (a > 1.0f) a = 1.0f; }
    C2D_ImageTint tint; C2D_AlphaImageTint(&tint, a);
    C2D_DrawParams p;
    /* citro2d puts the centre point at pos, not the top-left corner, so pos is the middle of the destination (spin about that point) */
    p.pos.w = dst->w * g->sc; p.pos.h = dst->h * g->sc;
    p.pos.x = dst->x * g->sc + g->ox + p.pos.w * 0.5f; p.pos.y = dst->y * g->sc + g->oy + p.pos.h * 0.5f;
    p.center.x = p.pos.w * 0.5f; p.center.y = p.pos.h * 0.5f;
    p.depth = 0.0f; p.angle = (float)(angle * 3.14159265358979 / 180.0) * PNJ_N3DS_ROT_SIGN;
    C2D_DrawImage(img, &p, &tint);
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
#define SDL_ShowSimpleMessageBox(f, t, m, w) (SDL_Log("%s: %s", (t), (m)), 1)

/* ---------- input: libctru -> SDL events ----------
 * Buttons become gamepad events (A confirms, B goes back, like every other Nintendo handheld), the circle pad becomes the left stick, and the
 * touch screen becomes one finger. Touch positions are sent as 0..1 over the touch screen; main.c (finger_field) turns them into field positions. */
static void pnj_push_button(int b, int down) {
    SDL_Event e; SDL_zero(e); e.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
    e.gbutton.which = 1; e.gbutton.button = (Uint8)b; e.gbutton.down = down ? true : false; SDL_PushEvent(&e);
}
static void pnj_push_axis(int axis, int v) {
    SDL_Event e; SDL_zero(e); e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION; e.gaxis.which = 1; e.gaxis.axis = (Uint8)axis; e.gaxis.value = (Sint16)v; SDL_PushEvent(&e);
}
static void pnj_push_finger(int type, float x, float y) {
    SDL_Event e; SDL_zero(e); e.type = type; e.tfinger.touchID = 1; e.tfinger.fingerID = 1; e.tfinger.x = x; e.tfinger.y = y; e.tfinger.pressure = type == SDL_EVENT_FINGER_UP ? 0.0f : 1.0f;
    SDL_PushEvent(&e);
}
static void pnj_n3ds_pump(void) {
    if (!aptMainLoop()) { SDL_Event q; SDL_zero(q); q.type = SDL_EVENT_QUIT; SDL_PushEvent(&q); return; }
    hidScanInput();
    static const struct { u32 k; int b; } map[] = {
        { KEY_DUP, SDL_GAMEPAD_BUTTON_DPAD_UP }, { KEY_DDOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN }, { KEY_DLEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT }, { KEY_DRIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT },
        { KEY_A, SDL_GAMEPAD_BUTTON_SOUTH }, { KEY_B, SDL_GAMEPAD_BUTTON_EAST }, { KEY_X, SDL_GAMEPAD_BUTTON_NORTH }, { KEY_Y, SDL_GAMEPAD_BUTTON_WEST },
        { KEY_START, SDL_GAMEPAD_BUTTON_START }, { KEY_SELECT, SDL_GAMEPAD_BUTTON_BACK }, { KEY_L, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER }, { KEY_R, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER } };
    u32 kd = hidKeysDown(), ku = hidKeysUp(), kh = hidKeysHeld();
    if ((kh & KEY_L) && (kd & KEY_R)) { if (++pnj_dbg_tex >= 21) pnj_dbg_tex = -1; SDL_Log("texview %d", pnj_dbg_tex); }
    if ((kh & KEY_L) && (kd & KEY_X)) pnj_dbg_sub++;
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++) { if (kd & map[i].k) pnj_push_button(map[i].b, 1); if (ku & map[i].k) pnj_push_button(map[i].b, 0); }
    { circlePosition cp; hidCircleRead(&cp);
      static int lx, ly; int nx = abs(cp.dx) > 30 ? cp.dx * 32767 / 156 : 0, ny = abs(cp.dy) > 30 ? -cp.dy * 32767 / 156 : 0;   /* the circle pad's y is up-positive */
      if (nx != lx) { lx = nx; pnj_push_axis(SDL_GAMEPAD_AXIS_LEFTX, nx); }
      if (ny != ly) { ly = ny; pnj_push_axis(SDL_GAMEPAD_AXIS_LEFTY, ny); } }
    { static float tx, ty; static int down;
      if (kh & KEY_TOUCH) {
          touchPosition tp; hidTouchRead(&tp);
          float x = tp.px / 320.0f, y = tp.py / 240.0f;
          if (!down) { down = 1; tx = x; ty = y; pnj_push_finger(SDL_EVENT_FINGER_DOWN, x, y); }
          else if (x != tx || y != ty) { tx = x; ty = y; pnj_push_finger(SDL_EVENT_FINGER_MOTION, x, y); }
      } else if (down) { down = 0; pnj_push_finger(SDL_EVENT_FINGER_UP, tx, ty); } }
}

#endif
