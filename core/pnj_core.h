/* Pac'N-Jump portable core (phase 0).
 * Pure C99, no allocation after init, no I/O. Mirrors proto/game.js step by step.
 * Numeric type is selected at build time:
 *   default      : double  (bit-parity with the JS reference)
 *   -DPNJ_FLOAT  : float   (FPU targets: Vita, 3DS, DC)
 * Fixed-point builds (DS/GBA/Saturn/Jaguar) come later behind the same fx_* macros.
 */
#ifndef PNJ_CORE_H
#define PNJ_CORE_H
#include <stdint.h>

#ifdef PNJ_FLOAT
typedef float fx;
#else
typedef double fx;
#endif

#define PNJ_W 320
#define PNJ_H 480
#define PNJ_FPS 40
#define PNJ_START_Y (-100)
#define PNJ_FLOOR_Y (-40)

#define PNJ_MAX_PLATS 640
#define PNJ_MAX_PICKS 1536
#define PNJ_MAX_GHOSTS 64
#define PNJ_MAX_DRAW 1024

/* plat types: 0 plain, 1 slides sideways, 2 slides vertically, 4 plain (follow-up), 5/6 spinners (child of a hub),
 * 8 breakable. pickups: 0 dot, 1 power pellet, 5..13 fruit/bonus. ghosts (stage 1): t 0..3 sine patrol. */
typedef struct { int t, id, parent; fx x, y, bx, by, r, ang, pang, off, px, py, fz_ang; int alive, broken, phase, moving, fz_alive; } pnj_plat;   /* parent -2: parent was dropped, fz_* hold its frozen state */
typedef struct { int t; fx x, y; int alive; } pnj_pick;
typedef struct { int t, lvl, phase, state, alive, sub, cnt, dir, vis, front, jit, parent, link; fx cx, x, y, px, py, bx, by, vy, spin; } pnj_ghost;   /* parent: plat index, link: ghost index, -1 none, -2 gone */
typedef struct { const char *name; int n; const uint8_t *cells; } pnj_map;   /* hand-made pickup layout: 24 rows x 16 cells */
typedef struct {
    fx x, y, px, py, vx, vy; int dead, dead_t; fx dead_y; int landed;
    int power, power_cnt, boost, boost13;
    int chain, chain_t;      /* pickup chain (rising eat sounds) */
    int candle, candle_cnt, lift, lift_cnt, lift_balloon, lift_h, push;
} pnj_player;

/* screen-space Mappy enemy (blob t9 / bonus t8), one slot */
typedef struct { int on, t, st, cnt, danger, dead, fl; fx x, y, vx, vy, ox, pvx; } pnj_enemy;

/* Galaga / Xevious screen-space enemy slot and Xevious bullet */
typedef struct { int on, t, st, cnt, danger, dead; fx x, y, bx, rot, px, vx, vy; } pnj_slot;
typedef struct { int dead; fx x, y, vx, vy; } pnj_bullet;

/* Cosmetic effects (never read by the simulation, own RNG). Emitters: land ring, pickup puff, power/boost trails, lift, death burst. */
typedef struct { fx s0, x, y, vx, vy, sc; } pnj_particle;
typedef struct { int kind, n, life, age, spr; fx dist, ox, oy, rot, col[4]; pnj_particle ps[20]; } pnj_emitter;   /* kind 0 ring, 1 radial, 2 fan */
typedef struct { fx x, y; int t; } pnj_spark;
enum { PNJ_EM_LAND, PNJ_EM_PUFF, PNJ_EM_POWER, PNJ_EM_BOOST, PNJ_EM_B13, PNJ_EM_LIFT, PNJ_EM_DEATH, PNJ_EM_KINDS };

/* Sound events queued by the core; the backend drains g->ev[0..nev) after stepping and sets nev = 0. */
enum { PNJ_SFX_JUMP = 1, PNJ_SFX_BREAK, PNJ_SFX_MAPPY_JUMP, PNJ_SFX_EAT1, PNJ_SFX_EAT2, PNJ_SFX_EAT3, PNJ_SFX_EAT4, PNJ_SFX_EAT5,
       PNJ_SFX_EAT6, PNJ_SFX_EAT7, PNJ_SFX_EAT8, PNJ_SFX_EAT_ENEMY, PNJ_SFX_EAT_POW, PNJ_SFX_MISS, PNJ_SFX_BOUND, PNJ_SFX_BALLOON,
       PNJ_SFX_CANDLE, PNJ_SFX_HAT, PNJ_SFX_WARN };

/* Draw list: one entry per sprite, already in screen space (320x480, y down).
 * layer: 0 background, 1 platforms, 2 pickups/enemies, 3 player, 4 effects. Backends map
 * layer to hardware priority (and to stereo depth on 3DS). blend: 0 alpha, 1 additive. */
typedef struct { int16_t sprite, col, row, vrow; fx x, y, rot, scale; uint8_t r, g, b, a, blend, layer; } pnj_draw;

typedef struct {
    uint32_t seed;           /* rng state */
    int stage;               /* 0-based */
    int start_level;         /* ?level= in the web version */
    int L;                   /* global frame counter */
    int gen_index;
    fx gen_top;
    fx cam593, cam594, cam595;
    fx peak;
    int tick578;             /* shared 1 s timer counter (power pellet etc.) */
    int map_placed[3];
    int next_plat_id;
    int over;                /* 1 once the death animation has finished */
    uint8_t ev[32]; int nev;
    pnj_enemy me;            /* Mappy enemy slot */
    pnj_enemy pl[4];         /* Pac-Land screen enemies */
    int f233d, last_pl;
    fx f569;                 /* Druaga: y of the last lizardman chunk */
    pnj_slot gs[5];          /* Galaga / Xevious screen enemies */
    pnj_bullet xbul[64]; int nxbul;
    int gal_rush, gal_rush_cnt, x_phase;
    pnj_emitter em[61]; int em_i[PNJ_EM_KINDS];
    pnj_spark spark[24]; int nspark;
    int tier_shown, banner_on; fx banner_x;
    uint32_t crng;
    int zero_screen_tilt;    /* parity runs: the web test bot feeds 0 to the screen-space systems */
    int f566[2];             /* Mappy cat-pair flags */
    int overflow;            /* set if a fixed array filled up (would break parity) */
    pnj_player p;
    int nplat, npick, nghost;
    pnj_plat plats[PNJ_MAX_PLATS];
    pnj_pick picks[PNJ_MAX_PICKS];
    pnj_ghost ghosts[PNJ_MAX_GHOSTS];
    int ndraw;
    pnj_draw draw[PNJ_MAX_DRAW];
} pnj_game;

uint32_t pnj_rnd(pnj_game *g);                 /* mulberry32, matches rnd() in game.js */
void pnj_set_maps(const pnj_map *maps, int n);   /* replace the compiled-in map table (bring-your-own-APK builds load it from the pack) */
int pnj_map_count(void);
void pnj_new(pnj_game *g, uint32_t seed, int stage);
void pnj_step(pnj_game *g, fx tilt);           /* one 40 Hz tick; tilt in about [-0.5, 0.5] */
void pnj_build_draw(pnj_game *g);
int  pnj_height(const pnj_game *g);            /* score in pixels climbed */
int  pnj_tier(const pnj_game *g);

#endif
