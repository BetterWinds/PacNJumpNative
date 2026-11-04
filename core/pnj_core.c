#include "pnj_core.h"
#include "pnj_maps.h"
#include <math.h>
#include <string.h>

/* Mirrors proto/game.js. Stage 1 (index 0) is complete: generator, platform types 0/1/2/4/5/6/8, pickups, ghosts 0-3.
 * Other stages are still web-only; their chunks fall back to plain platforms in normal builds. */

#define DT ((fx)1.0 / (fx)PNJ_FPS)
#define R(g, n) ((int)(pnj_rnd(g) % (uint32_t)(n)))
#define RAD(d) ((d) * (fx)3.14159265358979323846 / (fx)180.0)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static const fx BOUNCE[14] = { 1.0, 1.5, 0.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 3.5 };
static const fx PICKUP_R[14] = { 20, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 };
static const fx GHOST_RANGE[4] = { 100, 80, 60, 40 };
#define GALAGA_STAGE 3
static const int MAP_CELL_TYPE[12] = { 0, 1, 0, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
static const int MAP_AT_LEVEL[3] = { 2, 5, 7 };
/* platform type weights per stage, one row per level 0..9 (copied from game.js TABLES) */
static const uint8_t TABLES[8][10][12] = {
 {{0,0,0,0,0,0,0,0,0,0,0,1},{0,0,0,0,0,0,8,0,0,0,2,1},{0,0,0,0,0,0,8,8,0,5,2,1},{0,0,0,0,0,0,0,0,0,6,2,1},{0,0,0,0,0,0,0,0,6,5,2,1},{0,0,0,0,0,1,8,8,8,8,2,1},{0,0,0,0,2,1,8,8,6,5,2,1},{0,0,8,8,2,1,8,8,6,5,2,1},{0,0,8,8,2,1,8,8,2,2,2,1},{8,8,6,5,2,1,8,8,6,5,2,1}},
 {{0,0,0,0,0,0,0,0,0,0,2,1},{0,0,0,0,2,1,0,0,0,0,2,1},{0,0,0,0,2,1,8,0,0,5,2,1},{0,0,0,0,2,1,8,0,0,6,2,1},{0,0,0,0,2,1,8,0,6,5,2,1},{0,0,0,0,2,1,8,8,6,5,2,1},{0,0,0,0,2,1,8,8,6,5,2,1},{0,0,8,8,1,1,8,8,1,1,1,1},{0,0,8,8,2,2,8,8,2,2,2,2},{8,8,6,5,2,1,8,8,6,5,2,1}},
 {{0,0,0,0,0,0,0,0,0,8,2,1},{0,0,0,0,0,0,8,8,8,2,2,1},{0,0,0,0,0,8,8,8,6,2,2,1},{0,0,0,2,2,1,8,8,8,2,2,1},{0,0,0,8,8,8,8,8,6,5,2,1},{8,8,8,8,2,1,8,8,8,8,2,1},{8,8,6,5,2,1,8,8,6,5,2,1},{8,8,8,8,2,1,8,8,6,5,2,1},{8,8,8,8,2,1,8,8,6,5,2,1},{8,8,8,8,8,2,8,8,8,6,5,2}},
 {{0}},
 {{0,0,0,0,0,0,9,3,1,1,2,2},{0,0,0,0,0,0,9,9,3,3,1,2},{0,0,0,0,0,0,9,9,3,3,1,2},{0,0,0,0,0,0,9,9,3,3,1,7},{0,0,0,0,0,7,9,9,3,3,1,7},{0,0,0,0,3,7,9,9,8,8,3,7},{0,0,8,8,1,7,9,9,3,3,2,7},{0,0,0,8,8,8,9,9,3,3,2,1},{0,0,8,8,3,3,9,9,9,9,2,1},{0,0,8,8,7,7,9,9,9,3,3,1}},
 {{0,0,0,0,0,0,0,0,0,8,2,1},{0,0,0,0,0,0,8,8,8,2,2,1},{0,0,0,0,0,8,8,8,6,2,2,1},{0,0,0,2,2,1,8,8,8,2,3,3},{0,0,3,3,8,7,8,8,6,5,2,1},{8,8,8,7,3,3,8,8,8,7,2,1},{8,7,6,5,3,3,8,8,6,5,2,1},{8,3,3,8,2,1,8,7,6,5,2,1},{8,8,7,7,3,3,8,8,6,5,2,1},{8,8,8,7,7,3,8,8,8,6,5,3}},
 {{0,0,0,0,0,0,0,0,10,8,2,1},{0,0,10,0,0,0,8,8,8,2,2,1},{0,0,10,0,0,8,8,8,7,2,2,1},{0,0,10,2,2,1,8,8,8,2,3,3},{0,0,3,3,8,7,8,8,6,5,2,1},{8,8,10,7,3,3,8,8,8,7,2,1},{8,7,6,5,3,3,8,10,6,5,2,1},{8,3,3,8,2,1,8,7,6,5,2,1},{8,10,7,7,3,3,8,10,6,5,2,1},{0,10,8,7,7,3,8,8,10,6,5,3}},
 {{0,0,0,0,0,0,0,0,0,8,2,1},{0,0,0,0,0,0,8,8,8,2,2,1},{0,0,0,0,0,0,8,8,8,8,8,8},{0,0,0,0,0,0,8,8,8,8,8,8},{0,0,0,2,2,1,8,8,8,2,3,3},{0,0,0,2,2,1,8,8,8,2,3,3},{0,0,0,2,1,3,8,8,8,2,1,3},{8,8,8,2,1,3,8,8,8,2,1,3},{8,8,8,2,1,3,8,8,8,2,1,3},{8,8,8,2,1,3,8,8,8,2,1,3}}
};

uint32_t pnj_rnd(pnj_game *g) {
    g->seed += 0x6D2B79F5u;
    uint32_t t = g->seed;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return (t ^ (t >> 14)) & 0x7fffffffu;
}

static void sfx(pnj_game *g, int e) { if (g->nev < 32) g->ev[g->nev++] = (uint8_t)e; }

static int plat_index(const pnj_game *g, const pnj_plat *p) { return p ? (int)(p - g->plats) : -1; }
static pnj_plat *add_plat(pnj_game *g, int t, fx x, fx y, fx range) {
    if (g->nplat >= PNJ_MAX_PLATS) { g->overflow = 1; return 0; }
    pnj_plat *p = &g->plats[g->nplat++];
    memset(p, 0, sizeof *p);
    p->t = t; p->id = g->next_plat_id++; p->parent = -1;
    p->x = p->bx = p->px = x; p->y = p->by = p->py = y; p->r = range; p->alive = 1;
    return p;
}
static void add_pick(pnj_game *g, int t, fx x, fx y) {
    if (g->npick >= PNJ_MAX_PICKS) { g->overflow = 1; return; }
    pnj_pick *k = &g->picks[g->npick++];
    k->t = t; k->x = x; k->y = y; k->alive = 1;
}
static int add_ghost(pnj_game *g, int t, fx x, fx y, int lvl, int phase) {
    if (g->nghost >= PNJ_MAX_GHOSTS) { g->overflow = 1; return 0; }
    int idx = g->nghost;
    pnj_ghost *h = &g->ghosts[g->nghost++];
    memset(h, 0, sizeof *h);
    h->parent = h->link = -1; h->vis = 1;
    h->t = t; h->x = h->bx = h->px = x; h->y = h->by = h->py = y; h->lvl = lvl; h->phase = phase; h->alive = 1;
    return idx;
}

/* ---- cosmetic effects ---- */
static uint32_t crand(pnj_game *g) { uint32_t x = g->crng ? g->crng : 1; x ^= x << 13; x ^= x >> 17; x ^= x << 5; g->crng = x; return x; }
static int crn(pnj_game *g, int n) { return (int)(crand(g) % (uint32_t)n); }
static const int EM_BASE[PNJ_EM_KINDS + 1] = { 0, 10, 20, 30, 40, 50, 60, 61 };
static void init_emitters(pnj_game *g) {
    for (int k = 0; k < PNJ_EM_KINDS; k++)
        for (int i = EM_BASE[k]; i < EM_BASE[k + 1]; i++) {
            pnj_emitter *e = &g->em[i]; int ii = i - EM_BASE[k];
            static const fx C[7][4] = { { 0.5, 0.5, 0, 0.5 }, { 0.5, 0.5, 0.5, 0.5 }, { 0.8, 0.7, 0, 0.2 }, { 0.4, 0.4, 0.8, 0.6 }, { 0, 0, 0, 0.7 }, { 0, 0, 0, 0.5 }, { 0.9, 0.3, 0.3, 0.5 } };
            static const int KIND[7] = { 0, 1, 1, 2, 2, 2, 1 }, N[7] = { 3, 10, 10, 3, 3, 1, 20 }, SPR[7] = { 81, 82, 82, 82, 82, 82, 82 };
            static const fx DIST[7] = { 150, 100, 150, 150, 150, 200, 200 };
            e->kind = KIND[k]; e->n = N[k]; e->dist = DIST[k]; e->life = k == PNJ_EM_LIFT ? PNJ_FPS * 2 : PNJ_FPS; e->spr = SPR[k];
            for (int c = 0; c < 4; c++) e->col[c] = C[k][c];
            if (k == PNJ_EM_B13) { static const fx A[4] = { 0.2, 0.1, 1, 0.7 }, B[4] = { 1, 0.4, 1, 0.7 }; for (int c = 0; c < 4; c++) e->col[c] = ii % 2 ? A[c] : B[c]; }
            if (k == PNJ_EM_LIFT) { static const fx A[4] = { 0.9, 0.5, 0.55, 0.5 }, B[4] = { 0.9, 0.3, 0.55, 0.5 }; for (int c = 0; c < 4; c++) e->col[c] = ii % 2 ? A[c] : B[c]; }
            e->age = e->life;
        }
}
static void emit(pnj_game *g, int k, fx x, fx y) {
    int n = EM_BASE[k + 1] - EM_BASE[k];
    pnj_emitter *e = &g->em[EM_BASE[k] + g->em_i[k]];
    g->em_i[k] = (g->em_i[k] + 1) % n;
    e->ox = x; e->oy = y; e->age = 0;
    if (e->kind == 0) { fx f = (e->dist / e->life) * 4; for (int i = 0; i < e->n; i++) e->ps[i].s0 = i * -f; return; }
    for (int i = 0; i < e->n; i++) {
        pnj_particle *q = &e->ps[i];
        fx f = (((int)(e->dist / 2)) + (fx)crn(g, (int)e->dist) / 2) / e->life;
        e->rot = (fx)crn(g, 45);
        int fan = e->kind == 2;
        fx ang = (fan ? crn(g, 120) + 30 : crn(g, 360)) * (fx)3.14159265358979323846 / 180;
        q->sc = fan ? (crn(g, 30) + 120) / (fx)2 : (crn(g, 80) + 160) / (fx)2;
        q->x = q->y = 0; q->vx = cos(ang) * f; q->vy = sin(ang) * f;
    }
}
static void spark(pnj_game *g, fx x, fx y) { if (g->nspark < 24) g->spark[g->nspark++] = (pnj_spark){ x, y, 0 }; }
static void step_effects(pnj_game *g) {
    pnj_player *p = &g->p; int L = g->L;
    if (p->power && ((L >> 1) % 10) == 0) emit(g, PNJ_EM_POWER, p->x, p->y);
    if (p->boost13 && ((L >> 1) % 2) == 0) emit(g, PNJ_EM_B13, p->x, p->y);
    if ((p->boost || p->boost13) && ((L >> 1) % 2) == 0) emit(g, PNJ_EM_BOOST, p->px, p->py);
    if (p->lift && ((L >> 1) % 3) == 0 && p->lift_cnt > 8) emit(g, PNJ_EM_LIFT, p->px + (fx)(crn(g, 60) - 30), p->py + 15);
    for (int i = 0; i < 61; i++) {
        pnj_emitter *e = &g->em[i];
        if (e->age >= e->life) continue;
        if (e->kind != 0) for (int j = 0; j < e->n; j++) { e->ps[j].x += e->ps[j].vx; e->ps[j].y += e->ps[j].vy; }
        e->age++;
    }
    int k = 0;
    for (int i = 0; i < g->nspark; i++) { g->spark[i].t++; if (g->spark[i].t < 12) g->spark[k++] = g->spark[i]; }
    g->nspark = k;
    int t = pnj_tier(g);
    if (t > g->tier_shown) { g->tier_shown = t; g->banner_on = 1; g->banner_x = 330; }
    if (g->banner_on) { g->banner_x -= 4; if (g->banner_x <= -310) { g->banner_on = 0; g->banner_x = 330; } }
}

void pnj_new(pnj_game *g, uint32_t seed, int stage) {
    memset(g, 0, sizeof *g);
    g->seed = seed;
    g->stage = stage;
    g->p.x = g->p.px = 160; g->p.y = g->p.py = PNJ_START_Y; g->p.vy = -1600;
    g->peak = PNJ_START_Y;
    g->x_phase = -1;
    g->crng = seed * 2654435761u + 12345u;
    init_emitters(g);
    for (int i = 0; i < 5; i++) add_plat(g, stage == 4 && i > 0 && i < 4 ? 9 : 0, 20 + 70 * i, PNJ_FLOOR_Y, 0);
    g->gen_top = PNJ_FLOOR_Y;
}

int pnj_height(const pnj_game *g) { int h = (int)floor((double)(PNJ_START_Y - g->peak)); return h > 0 ? h : 0; }
int pnj_tier(const pnj_game *g) { return pnj_height(g) / 10000; }

/* ---- generator ---- */
static int level_of(const pnj_game *g, fx origin) { int l = (int)floor((double)(-origin / 14400)); return MAX(g->start_level, l); }

static const pnj_map *map_tab = PNJ_MAPS; static int map_n = PNJ_NMAPS;
void pnj_set_maps(const pnj_map *maps, int n) { map_tab = maps; map_n = n; }
int pnj_map_count(void) { return map_n; }
static fx place_map(pnj_game *g, fx local_y, fx origin, const char *name) {
    const pnj_map *m = 0;
    for (int i = 0; i < map_n; i++) if (!strcmp(map_tab[i].name, name)) { m = &map_tab[i]; break; }
    if (!m) return local_y;
    fx f2 = local_y - 10; int idx = 0;
    for (int row = 0; row < 24; row++) {
        f2 -= 20;
        for (int c = 16; c > 0; c--) {
            int v = m->cells[idx];
            if (v != 0 && v < 12) add_pick(g, MAP_CELL_TYPE[v], (fx)(c * 20 - 10), origin + f2);
            idx++;
            if (idx >= m->n) return f2 - 10;
        }
    }
    return f2 - 10;
}

static void gen_galaga(pnj_game *g, fx origin, int lvl) {     /* stage 4 (Galaga) chunk: dot ladder, items 9/10, hand-made maps */
    int L = MIN(lvl, 9); fx gap = (fx)(L * 3 + 64); int chance = 5 - L / 3;
    fx y = 0, last_x = 0; int count = 0, map_done = 0;
    int lvl_idx = -1;
    for (int i = 0; i < 3; i++) if (lvl >= MAP_AT_LEVEL[i]) lvl_idx = i;
    while (count < 100) {
        if (count % 2 == 0) y -= gap;
        if (count == 0 && !map_done && y - 480 > -1440 && lvl_idx >= 0 && !g->map_placed[lvl_idx]) {
            for (int k = 0; k < 5; k++) { add_plat(g, 0, (fx)(R(g, 250) + 35), origin + y, 0); y -= 72; count++; }
            char nm[16] = "st04_m0X"; nm[7] = (char)('1' + lvl_idx);
            fx top = place_map(g, y, origin, nm);
            map_done = 1; g->map_placed[lvl_idx] = 1; y = top - gap;
        } else if (count % 2 == 0 && !map_done && y - 480 > -1440 && !g->gal_rush && R(g, 100) < 4 && L > 0) {
            char nm[16] = "st04_r0X"; nm[7] = (char)('1' + R(g, 3));
            y = place_map(g, y, origin, nm) - gap; map_done = 1;
        }
        fx x = (fx)(R(g, 280) + 20); int typ = 0;
        if (R(g, 100) < chance && !g->gal_rush) {
            typ = R(g, 2) + 9;
            if (R(g, 15) == 0) { typ = 1; if (R(g, 5) == 0) typ = 13; }
        }
        if (count % 2 != 0 && last_x - 48 <= x && last_x + 48 >= x) {
            x = last_x > x ? x - 48 : x + 48;
            if (x <= 24 || x >= 296) x = (fx)(((int)floor((double)(x + 160))) % 320);
        }
        add_pick(g, typ, x, origin + y);
        if (count % 2 == 0) last_x = x;
        if (L <= 0 && y > -1440 && count % 2 == 0 && R(g, 100) < 15) add_plat(g, 0, (fx)(R(g, 250) + 35), origin + y - 32, 0);
        if (y < -1440 && count % 2 != 0) break;
        count++;
    }
    g->gen_top = origin + y;
}

static void gen_chunk(pnj_game *g) {
    fx origin = g->gen_top;
    int lvl = level_of(g, origin);
    int idx = g->gen_index++;
    const int S = g->stage;
    fx y = 0;
    if (idx == 0 && S == 4) {          /* Mappy start: wall platforms at both edges, then plain/spring platforms every 48 px */
        y = 0;
        for (int k = 0; k < 3; k++) { y -= 96; add_plat(g, 0, 35, origin + y, 0); add_plat(g, 0, 285, origin + y, 0); }
        y -= 48;
        int count = 11;
        while (count < 100) {
            y -= 48;
            int sp = R(g, 8) <= 1 ? 9 : 0;
            fx px = (fx)(R(g, 250) + 35);
            add_plat(g, sp, px, origin + y, 0); count++;
            if (y < -1440) break;
        }
        g->gen_top = origin + y;
        return;
    }
    if (idx == 0 && S == 6) {          /* Druaga start: plain platforms every 48 px, a candle on the first one */
        int count = 0, got = 0; y = -48;
        while (count < 100) {
            y -= 48; count++;
            fx x = (fx)(R(g, 250) + 35);
            add_plat(g, 0, x, origin + y, 0);
            if (!got) { fx cx = x + (fx)(R(g, 40) - 20); add_pick(g, 3, cx, origin + y - 32); y -= 36; got = 1; }
            if (y < -1440) break;
        }
        g->gen_top = origin + y;
        return;
    }
    if (idx == 0) {
        int count = 0; y = -48;
        while (count < 100 && y > -1440) { y -= 48; count++; add_plat(g, 0, (fx)(R(g, 250) + 35), origin + y, 0); }
        g->gen_top = origin + y;
        return;
    }
    if (idx == 1 && S == 0) {
        int got_pellet = 0, ghost_done = 0, count = 0;
        fx ghost_line = (y - 432) - (fx)R(g, 360);
        y = -48;
        while (count < 100 && y > -1440) {
            y -= 48; count++;
            fx x = (fx)(R(g, 250) + 35);
            add_plat(g, 0, x, origin + y, 0);
            if (!got_pellet) {
                add_pick(g, 1, x + (fx)(R(g, 40) - 20), origin + y - 26);
                y -= 36; got_pellet = 1;
            } else if (!ghost_done && y <= ghost_line) {
                int gt = R(g, 4);                    /* explicit order: C leaves argument order unspecified */
                int gx = R(g, 40) - 20;
                int gph = R(g, 360);
                add_ghost(g, gt, x + (fx)gx, origin + y - 48, 0, gph);
                y -= 48; ghost_done = 1;
            }
        }
        g->gen_top = origin + y;
        return;
    }
    {
        int mode = S == 2 || S == 6 ? 0 : S == 3 ? 1 : S == 5 ? lvl % 2 : S == 7 ? (lvl % 3 < 2 ? 1 : 0) : R(g, 2);
        int L = MIN(lvl, 9);
        if (mode == 0) {
            /* ---- platform mode ---- */
            int spin_groups = 0, ghost_count = 0, map_done = 0;
            fx step = S == 7 ? 48 : (fx)(((L / 3) + 1) * 24 + 24);
            int prev_t = -1, count = 0;
            g->f569 = 0;
            if (S == 4) { if (L % 2 == 0) g->f566[0] = 1; else { g->f566[0] = 0; g->f566[1] = 0; } }
            int lvl_idx = -1;
            for (int i = 0; i < 3; i++) if (lvl >= MAP_AT_LEVEL[i]) lvl_idx = i;
            while (count < 100) {
                y -= step;
                fx x = (fx)(R(g, 250) + 35);
                if (count > 4 && !map_done && y - 480 > -1440 && lvl_idx >= 0 && !g->map_placed[lvl_idx]) {
                    char nm[16] = "st0S_m0X"; nm[3] = (char)('1' + S); nm[7] = (char)('1' + lvl_idx);
                    fx top = place_map(g, y, origin, nm);
                    map_done = 1; g->map_placed[lvl_idx] = 1;
                    y = top - step;
                }
                int t = TABLES[S][L][R(g, 12)];
                if (S == 1 && ghost_count < 2 && origin < -1440 && count < 97 && R(g, 100) < L / 2 + 5) t = 11;
                if (S == 2 && L > 0 && ghost_count < 2 && origin < -1440 && R(g, 100) < L / 2 + 5) t = 11;
                if (S == 6) {
                    if (ghost_count < 2 && origin < -1440 && count < 95 && count % 40 != 0) {
                        if (L % 2 != 0 && R(g, 100) < L / 2 + 5) t = 11;
                        if (L % 2 == 0 && L != 0 && fabs((double)(g->f569 - y)) > 800) t = 11;
                    }
                    if (count % 50 == 0 && R(g, 50) < L + 35) t = 13;
                }
                if (S == 4 && ghost_count < 2 && origin < -1440 && count < 98 && R(g, 100) < L / 2 + 4) t = 11;
                if (S == 5 && ghost_count < 2 && origin < -1440 && count < 95 && R(g, 100) < L / 2 + 5) t = 11;
                pnj_plat *last = 0; int last_t = t;
                if (t == 0 || t == 8 || t == 9) {
                    if ((S == 5 || S == 6) && t == 8) {   /* breakable platforms only after a couple of normal ones */
                        int n = g->nplat;
                        if (count < 3 || g->plats[n - 1].t == 4 || g->plats[n - 1].t == 8 || g->plats[n - 3].t == 4) { t = 0; last_t = 0; }
                    }
                    last = add_plat(g, t, x, origin + y, 0);
                } else if (t == 3) {
                    if (count == 0 || prev_t == 8 || prev_t == 4) {
                        last_t = 0; last = add_plat(g, 0, x, origin + y, 0);
                    } else {
                        fx rg = (fx)(L * 3 + (S >= 5 ? 60 : 50));
                        if (x <= rg) x += rg;
                        if (x >= 320 - rg) x -= rg;
                        last = add_plat(g, 3, x, origin + y - rg, x < 160 ? -rg : rg);
                        y = y - rg - rg;
                        if (count + 1 < 100) {
                            fx x2 = x < 160 ? (fx)(R(g, 55) + 230) : (fx)(R(g, 55) + 35);
                            add_plat(g, 4, x2, origin + y + rg + rg, 0);
                            x = x2; count++; last_t = 4;
                        }
                    }
                } else if (t == 7) {
                    if (count == 0 || prev_t == 8 || prev_t == 7 || prev_t == 10) {
                        last_t = 0; last = add_plat(g, 0, x, origin + y, 0);
                    } else {
                        int sp = L / 4 + 3, gap = L < 9 ? (L < 6 ? 34 : 17) : 0;
                        y -= gap;
                        int neg = R(g, 2) == 0 ? 0 : 1;
                        last = add_plat(g, 7, x, origin + y, (fx)(neg ? -sp : sp));
                        y -= gap;
                    }
                } else if (t == 10) {
                    if (count == 0 || prev_t == 8 || prev_t == 10 || prev_t == 4) { last_t = 0; last = add_plat(g, 0, x, origin + y, 0); }
                    else { last_t = 10; last = add_plat(g, 10, x, origin + y, 0); }
                } else if (t == 1) {
                    fx rg = (fx)(L * 3 + 60);
                    if (x <= rg) x += rg;
                    if (x >= 320 - rg) x -= rg;
                    last = add_plat(g, 1, x, origin + y, rg);
                } else if (t == 2) {
                    if (count == 0 || prev_t == 8 || prev_t == 4) {
                        last_t = 0; last = add_plat(g, 0, x, origin + y, 0);
                    } else {
                        fx rg = (fx)(L * 4 + 30);
                        last = add_plat(g, 2, x, origin + y - rg, rg);
                        y = y - rg - rg;
                        if (count + 1 < 100 && !(S == 2 && L >= 2 && L != 9)) {
                            fx x2 = x < 160 ? (fx)(R(g, 55) + 230) : (fx)(R(g, 55) + 35);
                            int sub = R(g, MAX(1, (int)floor((double)(rg * 2))));
                            add_plat(g, 4, x2, origin + (y + rg + rg) - (fx)sub, 0);
                            x = x2; count++; last_t = 4;
                        }
                    }
                } else if (t == 5 || t == 6) {
                    if (spin_groups > 4) {
                        last_t = 0; last = add_plat(g, 0, x, origin + y, 0);
                    } else {
                        fx cy = y - (fx)(L + 87);
                        int parent = -1;
                        for (int k = 0; k < 8; k++) {
                            pnj_plat *pl = add_plat(g, t, x, origin + cy, (fx)(L + 72));
                            if (!pl) break;
                            pl->off = (fx)(45 * k);
                            if (k > 0) pl->parent = parent; else parent = plat_index(g, pl);
                            last = pl;
                        }
                        count += 7;
                        spin_groups++;
                        y = cy - ((fx)(L + 82) - (fx)((L / 3) * 24) / 2);
                    }
                }
                if (t == 13 && S == 6) {                   /* candle sitting on a lone platform */
                    add_plat(g, 4, x, origin + y, 0); add_pick(g, 3, x, origin + y - 32); y -= 36;
                    prev_t = 4; count++;
                    if (y < -1440) break;
                    continue;
                }
                if (t == 11 && S == 6) {                   /* Druaga monsters: slime, mage + wisp, dragon + fireball, lizardman */
                    int kind = 15;
                    if (R(g, 2) == 0 && ghost_count < 4) kind = 17;
                    if (R(g, 2) == 0 && ghost_count < 4) kind = 16;
                    if (L % 2 == 0) { kind = 18; g->f569 = y; }
                    fx cy = origin + y;
                    if (kind == 15) {
                        fx cx = (fx)(R(g, 110) + 105);
                        pnj_plat *mid = add_plat(g, 4, cx, cy, 0); add_plat(g, 4, cx - 70, cy, 0); add_plat(g, 4, cx + 70, cy, 0);
                        int midi = plat_index(g, mid);
                        int ph = R(g, 360);
                        int gi = add_ghost(g, 15, cx, cy - 36, 0, ph); g->ghosts[gi].parent = midi; x = cx;
                    } else if (kind == 16) {
                        fx cx = (fx)(R(g, 70) + 125);
                        int first = plat_index(g, add_plat(g, 4, cx, cy, 0));
                        fx ox2 = R(g, 2) == 0 ? cx + 105 : cx - 105;
                        add_plat(g, 4, ox2, cy, 0);
                        int gi = add_ghost(g, 16, cx, cy - 32, 0, 0); g->ghosts[gi].parent = first; g->ghosts[gi].sub = 0; g->ghosts[gi].cnt = 0;
                        int wi = add_ghost(g, 20, cx, cy - 36, 0, 0); g->ghosts[wi].link = gi; g->ghosts[wi].vis = 0; x = cx;
                    } else if (kind == 17) {
                        fx x0; int dir;
                        if (R(g, 2) == 0) { dir = 0; x0 = (fx)(215 - R(g, 55)); } else { dir = 1; x0 = (fx)(R(g, 55) + 105); }
                        int first = plat_index(g, add_plat(g, 4, x0, cy, 0)); add_plat(g, 4, dir == 0 ? x0 + 70 : x0 - 70, cy, 0);
                        fx gx = dir == 0 ? x0 + 82 : x0 - 82;
                        int gi = add_ghost(g, 17, gx, cy - 36, 0, 0); g->ghosts[gi].parent = first; g->ghosts[gi].sub = dir; g->ghosts[gi].cnt = 0;
                        int fi = add_ghost(g, 19, gx + (dir == 0 ? -50 : 50), cy - 36, 0, 0); g->ghosts[fi].link = gi; g->ghosts[fi].vis = 0;
                        x = gx;
                    } else {
                        int first = -1;
                        for (int k = 0; k < 5; k++) { pnj_plat *pl = add_plat(g, 4, (fx)(20 + 70 * k), cy, 0); if (first < 0) first = plat_index(g, pl); }
                        int gi = add_ghost(g, 18, 160, cy - 36, 0, 0); g->ghosts[gi].parent = first; g->ghosts[gi].sub = 0; g->ghosts[gi].cnt = 0; x = 160;
                    }
                    ghost_count += kind == 15 || kind == 18 ? 1 : 2;
                    if (L < 6) { y -= 24; if (L < 3) y -= 24; }
                    prev_t = 4; count++;
                    if (y < -1440) break;
                    continue;
                }
                if (t == 11 && S == 5) {                   /* Pac-Land: trio with a swaying ghost, or a street of 5 with a sliding car */
                    int kind = R(g, 2) == 0 ? 13 : 12;
                    if (g->last_pl == kind) kind = kind == 12 ? 13 : 12;
                    g->last_pl = kind;
                    fx cy = origin + y;
                    if (kind == 12) {
                        fx cx = (fx)(R(g, 110) + 105);
                        pnj_plat *mid = add_plat(g, 4, cx, cy, 0); add_plat(g, 4, cx - 70, cy, 0); add_plat(g, 4, cx + 70, cy, 0);
                        int midi = plat_index(g, mid);
                        int ph = R(g, 360);
                        int gi = add_ghost(g, 12, cx, cy - 39, 0, ph); g->ghosts[gi].parent = midi;
                        y -= 24;
                        x = cx;
                    } else {
                        int s0 = (R(g, 2) == 0 ? 3 : 0) + (L >= 4 && L < 7 ? 1 : 0);
                        int first = -1;
                        for (int k = 0; k < 5; k++) { pnj_plat *pl = add_plat(g, 4, (fx)(20 + 70 * k), cy, 0); if (first < 0) first = plat_index(g, pl); }
                        int gi = add_ghost(g, 13, 160, cy - 36, 0, 0); g->ghosts[gi].sub = s0; g->ghosts[gi].parent = first;
                        x = 160;
                    }
                    ghost_count++;
                    if (L < 6) { y -= 24; if (L < 3) y -= 24; }
                    prev_t = 4;
                    count++;
                    if (y < -1440) break;
                    continue;
                }
                if (t == 11 && S == 4) {                   /* Mappy: two platforms with a cat pair, or a beam shooter */
                    int k = 10;
                    if (R(g, 2) == 0 && ghost_count < 4) k = 11;
                    if (R(g, 3) == 0 && g->f566[0] && !g->f566[1]) { k = 9; g->f566[1] = 1; }
                    int side = R(g, 2);
                    fx cy = origin + y, cx = 0;
                    int first;
                    if (k != 11) {
                        cx = (fx)(R(g, 120) + 100);
                        first = plat_index(g, add_plat(g, 4, cx - 60, cy, 0)); add_plat(g, 4, cx + 60, cy, 0);
                        int gi = add_ghost(g, k, cx, cy - 36, 0, 0);
                        g->ghosts[gi].sub = side; g->ghosts[gi].cx = cx; g->ghosts[gi].parent = first;
                        ghost_count += 1;
                        if (L < 6) { y -= 28; if (L < 3) y -= 28; }
                    } else {
                        fx x0; int dir;
                        if (R(g, 2) == 0) { dir = 0; x0 = (fx)(215 - R(g, 55)); first = plat_index(g, add_plat(g, 4, x0, cy, 0)); add_plat(g, 4, x0 + 70, cy, 0); }
                        else { dir = 1; x0 = (fx)(R(g, 55) + 105); first = plat_index(g, add_plat(g, 4, x0, cy, 0)); add_plat(g, 4, x0 - 70, cy, 0); }
                        fx gx = dir == 0 ? x0 + 72 : x0 - 72, bx = dir == 0 ? -50 : 50;
                        int sh = add_ghost(g, 11, gx, cy - 36, 0, 0);
                        g->ghosts[sh].sub = dir; g->ghosts[sh].cnt = 0; g->ghosts[sh].parent = first;
                        int bm = add_ghost(g, 24, gx + bx, cy - 36, 0, 0);
                        g->ghosts[bm].link = sh; g->ghosts[bm].vis = 0;
                        ghost_count += 2;
                        if (L < 6) { y -= 24; if (L < 3) y -= 24; }
                        x = gx;
                    }
                    if (R(g, 100) < 8) {
                        fx px = (k == 11 ? x : cx) + (fx)(R(g, 40) - 20);
                        if (R(g, 8) >= 5) add_pick(g, 2, px, origin + y - 36); else add_pick(g, 1, px, origin + y - 26);
                        y -= 36;
                    }
                    prev_t = 4;
                    count += 1;
                    if (y < -1440) break;
                    continue;
                }
                if (t == 11 && S == 2) {                   /* two plain platforms with a fixed hazard above */
                    fx x2 = x < 160 ? (fx)(R(g, 55) + 230) : (fx)(R(g, 55) + 35);
                    add_plat(g, 4, x, origin + y, 0);
                    add_plat(g, 4, x2, origin + y, 0);
                    int hx = R(g, 60) - 30;
                    add_ghost(g, 21, x2 + (fx)hx, origin + y - 32, 0, 0);
                    ghost_count++;
                    if (L < 6) { y -= 24; if (L < 3) y -= 24; }
                    count++;
                    if (y < -1440) break;
                    continue;
                }
                if (t == 11) {                             /* stage 2: three platforms guarded by a patrol or a fire-breather */
                    int k = 4;
                    if (R(g, 2) == 0 && ghost_count < 2) k = 5;
                    fx cx = 0; int dir = 0;
                    if (k == 4) {
                        cx = (fx)(R(g, 110) + 105);
                        if (R(g, 2) == 0) dir = 0; else { dir = 1; if (L < 6) { y -= 24; if (L < 3) y -= 24; } }
                    } else if (R(g, 2) == 0) { cx = (fx)(215 - R(g, 55)); dir = 0; }
                    else { cx = (fx)(R(g, 55) + 105); dir = 1; }
                    fx cy = origin + y;
                    pnj_plat *mid = add_plat(g, 4, cx, cy, 0);
                    add_plat(g, 4, cx - 70, cy, 0); add_plat(g, 4, cx + 70, cy, 0);
                    int midi = plat_index(g, mid);
                    if (k == 4) {
                        int ph = R(g, 360);
                        int gi = add_ghost(g, 4, cx, cy + (dir == 0 ? -36 : 36), 0, ph);
                        g->ghosts[gi].parent = midi; g->ghosts[gi].dir = dir;
                        ghost_count += 1;
                    } else {
                        fx gx = dir == 0 ? cx + 82 : cx - 82, stp = dir == 0 ? -50 : 50;
                        int sh = add_ghost(g, 5, gx, cy - 36, 0, 0);
                        g->ghosts[sh].parent = midi; g->ghosts[sh].sub = dir; g->ghosts[sh].cnt = 0;
                        for (int j = 0; j < 3; j++) {
                            int bm = add_ghost(g, 6 + j, gx + stp * (fx)(j + 1), cy - 36, 0, 0);
                            g->ghosts[bm].link = sh; g->ghosts[bm].vis = 0;
                        }
                        ghost_count += 4;
                    }
                    if (L < 6) { y -= 24; if (L < 3) y -= 24; }
                    count++;
                    if (y < -1440) break;
                    continue;
                }
                prev_t = last_t;
                /* pickups after plain platforms */
                if (S == 2) {
                    if (last && (last->t == 0 || last->t == 8) && R(g, 100) < 7 - L / 2) {
                        fx px = x + (fx)(R(g, 40) - 20); int ty = 11 + R(g, 2);
                        if (R(g, 10) == 0) add_pick(g, 1, px, origin + y - 26);
                        else {
                            if (R(g, 8) == 0) add_pick(g, 13, px, origin + y - 30); else add_pick(g, ty, px, origin + y - 36);
                            y -= 12;
                        }
                        y -= 36;
                    }
                    if (ghost_count < 2 && origin < -1440 && R(g, 100) < 20 && last && (last->t <= 1 || last->t == 8)) {
                        int gt = 22 + R(g, 2);
                        int gxx = R(g, 220) + 50;
                        int gi = add_ghost(g, gt, (fx)gxx, origin + y - 48, 0, 0);
                        g->ghosts[gi].sub = (gt - 22) * 3 + (L + 1) / 5;
                        ghost_count++;
                        if (L < 6) { y -= 24; if (L < 3) y -= 24; }
                    }
                } else if (S == 7) {
                    if (last && (last->t == 0 || last->t == 8) && R(g, 100) < 3) {
                        fx px = x + (fx)(R(g, 40) - 20);
                        if (R(g, 8) == 0) { add_pick(g, 13, px, origin + y - 30); y -= 12; } else add_pick(g, 1, px, origin + y - 26);
                        y -= 36;
                    }
                } else if (S == 6) {
                    if (last && (last->t == 0 || last->t == 8) && R(g, 100) < 6 - L / 3) {
                        int alt = R(g, 2) == 0 ? 11 : 5;
                        fx px = x + (fx)(R(g, 40) - 20);
                        if (R(g, 2) == 0) { if (R(g, 100) < 20) add_pick(g, 13, px, origin + y - 30); else add_pick(g, 1, px, origin + y - 26); }
                        else if (R(g, 100) < 20) add_pick(g, alt, px, origin + y - 32); else add_pick(g, 3, px, origin + y - 32);
                        y -= 36;
                    }
                } else if (S == 5) {
                    if (last && (last->t == 0 || last->t == 8) && R(g, 100) < 8 - L / 2) {
                        fx px = x + (fx)(R(g, 40) - 20);
                        if (R(g, 8) == 0) add_pick(g, 13, px, origin + y - 30);
                        else if (R(g, 2) == 0) add_pick(g, 1, px, origin + y - 26);
                        else add_pick(g, 4, px, origin + y - 32);
                        y -= 36;
                    }
                    if (ghost_count < 2 && origin < -1440 && R(g, 100) < 10 && last && (last->t <= 1 || last->t == 8)) {
                        const pnj_ghost *lg = g->nghost > 0 ? &g->ghosts[g->nghost - 1] : 0;
                        if (!lg || fabs((double)(origin + y - lg->by)) > 250) {
                            int gxx = R(g, 220) + 50;
                            int gi = add_ghost(g, 14, (fx)gxx, origin + y - 60, 0, 0);
                            int sa = R(g, 2) == 0 ? 3 : 0;
                            g->ghosts[gi].sub = sa + (L < 4 ? 0 : L < 7 ? 1 : 2);
                            g->ghosts[gi].jit = R(g, 50);
                            ghost_count++;
                            y -= 24; if (L < 6) { y -= 24; if (L < 3) y -= 24; }
                        }
                    }
                } else if (S == 4) {
                    if (last && (last->t == 0 || last->t == 8) && R(g, 100) < 8) {
                        fx px = x + (fx)(R(g, 40) - 20);
                        if (R(g, 8) >= 5) add_pick(g, 2, px, origin + y - 36); else add_pick(g, 1, px, origin + y - 26);
                        y -= 36;
                    }
                } else if (last && (last->t == 0 || last->t == 8) && R(g, 100) < (S == 0 ? 8 : 5)) {
                    fx px = x + (fx)(R(g, 40) - 20);
                    if (R(g, S == 0 ? 8 : 6) == 0) { add_pick(g, 13, px, origin + y - 30); y -= 12; }
                    else add_pick(g, 1, px, origin + y - 26);
                    y -= 36;
                }
                /* ghosts */
                if (S == 0 && origin < -1440) {
                    int cap = L <= 0 ? 1 : 2; if (L == 1) cap = 2;
                    if (ghost_count < cap && R(g, 100) < 10 && last && last->t <= 1) {
                        int gt = R(g, 4);
                        int gx = R(g, 220) + 50;
                        int gph = R(g, 360);
                        add_ghost(g, gt, (fx)gx, origin + y - 48, 0, gph);
                        ghost_count++;
                        if (L < 6) { y -= 24; if (L < 3) y -= 24; }
                    }
                }
                count++;
                if (y < -1440) break;
            }
        } else {
            /* ---- dot ladder mode ---- */
            if (S == 3) { gen_galaga(g, origin, lvl); return; }
            int L2 = MIN(lvl, 9);
            fx gap = (fx)(L2 * 3 + 64);
            int fruit_chance = (S == 0 || S == 4 || S == 5 || S == 7 ? 7 : 6) - L2 / 2;
            int map_done = 0, count = 0; fx last_x = 0;
            while (count < 100) {
                if (count % 2 == 0) y -= gap;
                if (count % 2 == 0 && !map_done && y - 480 > -1440 && R(g, 100) < 4 && L2 > 0) {
                    char nm[16] = "st0S_r0X"; nm[3] = (char)('1' + S); nm[7] = (char)('1' + R(g, 3));
                    fx top = place_map(g, y, origin, nm);
                    map_done = 1; y = top - gap;
                }
                fx x = (fx)(R(g, 280) + 20);
                int typ = 0;
                if (R(g, 100) < fruit_chance) typ = S == 7 ? 10 + 2 * R(g, 2) : R(g, 2) + (S == 0 ? 5 : S == 4 ? 8 : S == 5 ? 6 : 7);
                if (count % 2 != 0 && last_x - 48 <= x && last_x + 48 >= x) {
                    x = last_x > x ? x - 48 : x + 48;
                    if (x <= 24 || x >= 296) x = (fx)(((int)floor((double)(x + 160))) % 320);
                }
                add_pick(g, typ, x, origin + y);
                if (count % 2 == 0) last_x = x;
                if (L2 <= 0 && y > -1440 && count % 2 == 0 && R(g, 100) < 15) add_plat(g, 0, (fx)(R(g, 250) + 35), origin + y - 32, 0);
                count++;
                if (y < -1440 && count % 2 == 0) break;
            }
        }
        g->gen_top = origin + y;
        return;
    }
}

/* ---- platforms (orig C0044h) ---- */
static void plat_seg(const pnj_plat *pl, fx a[2], fx b[2]) {
    fx yo = (pl->ang <= 90 || pl->ang > 270) ? -12 : 12;
    if (pl->ang == 0) { a[0] = pl->x - 35; a[1] = pl->y + yo; b[0] = pl->x + 35; b[1] = pl->y + yo; return; }
    fx c = (fx)cos(RAD(pl->ang)), s = (fx)sin(RAD(pl->ang));
    a[0] = pl->x + (-35) * c - yo * s; a[1] = pl->y + (-35) * s + yo * c;
    b[0] = pl->x + (35) * c - yo * s;  b[1] = pl->y + (35) * s + yo * c;
}
static int seg_hit(const fx a[2], const fx b[2], const fx c[2], const fx d[2], fx out[2]) {
    fx f3 = b[0] - a[0], f4 = b[1] - a[1], f7 = d[0] - c[0], f8 = d[1] - c[1];
    fx den = f7 * f4 - f3 * f8;
    if (den == 0) return 0;
    fx f10 = c[0] - a[0], f11 = c[1] - a[1];
    fx t = (f7 * f11 - f8 * f10) / den, u = (f11 * f3 - f10 * f4) / den;
    if (t < 0 || t > 1 || u < 0 || u > 1) return 0;
    out[0] = a[0] + f3 * t; out[1] = a[1] + f4 * t;
    return 1;
}

static void move_plat(pnj_game *g, pnj_plat *pl) {
    pl->px = pl->x; pl->py = pl->y; pl->pang = pl->ang;
    int L = g->L;
    switch (pl->t) {
    case 1: pl->x = pl->bx + (fx)sin(RAD((fx)((L % 120) * 3))) * pl->r; break;
    case 2: pl->y = pl->by + (fx)sin(RAD((fx)((L % 120) * 3))) * pl->r; break;
    case 3:
        pl->y = pl->by + (fx)sin(RAD((fx)((L % 120) * 3))) * pl->r;
        pl->x = pl->bx + (pl->r >= 0 ? 1 : -1) * (fx)sin(RAD((fx)((L % 120) * 3))) * pl->r; break;
    case 5: case 6: {
        if (pl->parent >= 0) pl->ang = g->plats[pl->parent].ang + pl->off;
        else if (pl->parent == -2) pl->ang = pl->fz_ang + pl->off;
        else {
            pl->ang += pl->t == 5 ? 1 : -1;
            if (pl->ang > 360) pl->ang -= 360;
            if (pl->ang < 0) pl->ang += 360;
        }
        if (pl->parent >= 0) pl->alive = g->plats[pl->parent].alive;
        else if (pl->parent == -2) pl->alive = pl->fz_alive;
        fx a = RAD((fx)360 - pl->ang);
        pl->x = pl->bx + (fx)cos(a) * pl->r;
        pl->y = pl->by + (fx)sin(a) * pl->r;
        break;
    }
    case 7: pl->ang = (fx)fmod((double)(pl->ang + pl->r + 360), 360.0); break;
    case 9: if (pl->phase > 0) { pl->phase++; if (pl->phase >= 16) pl->phase = 0; } break;
    case 10: pl->phase = (pl->phase + 1) % 180; break;
    case 8:
        if (pl->phase > 0) { pl->phase += 2; if (pl->phase >= 14) { pl->alive = 0; pl->phase = 0; } }
        break;
    default: break;
    }
}

static void bounce(pnj_game *g, fx mult) {
    pnj_player *p = &g->p;
    p->vy = -(800 * mult);
    p->px = p->x; p->py = p->y;
}

static int land_on(pnj_game *g, pnj_plat *pl) {
    pnj_player *p = &g->p;
    if (!pl->alive || pl->broken || p->vy < 0 || p->dead || p->lift) return 0;
    if (pl->t == 10 && pl->phase / 30 >= 4) return 0;
    fx fp[2] = { p->px, p->py + 20 }, fc[2] = { p->x, p->y + 20 };
    fx cur0[2], cur1[2], hit[2];
    plat_seg(pl, cur0, cur1);
    int stat = !pl->moving || (pl->ang == pl->pang && pl->x == pl->px && pl->y == pl->py);
    int h = 0;
    if (!pl->moving || stat) h = seg_hit(cur0, cur1, fp, fc, hit);
    else {
        fx dx = pl->x - pl->px, dy = pl->y - pl->py;
        fx pv0[2] = { cur0[0] - dx, cur0[1] - dy }, pv1[2] = { cur1[0] - dx, cur1[1] - dy };
        h = seg_hit(cur0, cur1, fp, fc, hit) || seg_hit(pv0, pv1, fp, fc, hit)
            || seg_hit(cur0, pv0, fp, fc, hit) || seg_hit(cur1, pv1, fp, fc, hit);
    }
    pl->moving = 1;
    if (!h) return 0;
    p->px = p->x = hit[0]; p->py = p->y = hit[1] - 20;
    emit(g, PNJ_EM_LAND, hit[0], hit[1]);
    if (pl->t == 9) pl->phase = 1;
    bounce(g, pl->t == 9 ? (fx)1.5 : (fx)1.0);
    sfx(g, pl->t == 9 ? PNJ_SFX_MAPPY_JUMP : PNJ_SFX_JUMP);
    p->landed = 1;
    if (pl->t == 8 && !pl->broken) { pl->broken = 1; pl->phase += 2; sfx(g, PNJ_SFX_BREAK); }
    return 1;
}

/* ---- pickups, ghosts ---- */
static void die(pnj_game *g, int kind) {
    pnj_player *p = &g->p;
    if (p->dead) return;
    p->dead = 1; p->power = 0; p->boost = 0; p->lift = 0; p->candle = 0; p->push = 0;
    if (kind == 1) { p->vy = 0; emit(g, PNJ_EM_DEATH, p->x, p->y); }
    p->dead_t = 80;
    sfx(g, PNJ_SFX_MISS);
}

static void start_lift(pnj_game *g, int balloon) {
    pnj_player *p = &g->p;
    if (!p->lift) p->lift_h = 0;
    p->lift = 1; p->lift_cnt = 10; p->lift_balloon = balloon; p->boost = 0;
    if (p->vy > 0) p->vy = 0;
}
static void collect_pickup(pnj_game *g, pnj_pick *pk) {
    pnj_player *p = &g->p;
    int t = pk->t;
    pk->alive = 0;
    spark(g, pk->x, pk->y); emit(g, PNJ_EM_PUFF, pk->x, pk->y);
    p->chain = p->chain_t > 0 ? (p->chain + 1 > 7 ? 7 : p->chain + 1) : 0;
    p->chain_t = 20;
    fx B = BOUNCE[t] * 800;
    if (!(t == 2 || t == 4) && !(p->lift && t == 0)) p->vy = p->vy < 0 ? (p->vy > -B ? p->vy / 5 - B : p->vy) : -B;
    if (t >= 5) { p->boost = 1; if (t == 13) p->boost13 = 1; }
    p->landed = 0;
    if (t == 1) { p->power = 1; p->power_cnt = 10; g->tick578 = 0; sfx(g, PNJ_SFX_EAT_POW); }
    else if (t == 2) { sfx(g, PNJ_SFX_BALLOON); start_lift(g, 1); }
    else if (t == 3) { sfx(g, PNJ_SFX_CANDLE); p->boost = 1; p->candle = 1; p->candle_cnt = 30; }
    else if (t == 4) { sfx(g, PNJ_SFX_HAT); start_lift(g, 0); }
    else sfx(g, PNJ_SFX_EAT1 + p->chain);
}

static void bounce_off_ghost(pnj_game *g) {
    pnj_player *p = &g->p; fx B = (fx)1.5 * 800;
    p->chain_t = 20; sfx(g, PNJ_SFX_EAT_ENEMY);
    if (p->vy >= 0) p->vy = -B; else if (p->vy > -B) p->vy = p->vy / 5 - B;
}

static int ghost_gone(const pnj_game *g, const pnj_ghost *h) {
    if (h->parent == -2 || (h->parent >= 0 && !g->plats[h->parent].alive)) return 1;
    if (h->link == -2) return 1;
    if (h->link >= 0 && (!g->ghosts[h->link].alive || g->ghosts[h->link].state >= 2)) return 1;
    return 0;
}

static void update_ghost(pnj_game *g, pnj_ghost *h) {
    if (ghost_gone(g, h)) { h->alive = 0; return; }
    h->px = h->x; h->py = h->y;
    if (h->state == 0) {
        switch (h->t) {
        case 4:    /* patrol: wide horizontal swing around the platform trio (period 360 frames) */
            h->x = h->bx + (fx)sin(RAD((fx)((g->L + h->phase) % 360))) * 105;
            h->y = h->by + (fx)sin(RAD((fx)((g->L % 20) * 18))) * 2;
            break;
        case 5: {  /* fire-breather: idle 50 -> warn 30 -> beam grows in 5-frame steps (sub%2 = beam direction) */
            h->cnt++;
            int s = h->sub;
            if (s <= 1) { if (h->cnt >= 50) { h->sub += 2; h->cnt = 0; } }
            else if (s <= 3) { if (h->cnt >= 30) { h->sub += 2; h->cnt = 0; } }
            else if (h->cnt >= 5) { h->sub += 2; if (h->sub >= 10) h->sub -= 10; h->cnt = 0; }
            break;
        }
        case 21: break;   /* fixed hazard */
        case 15: h->x = h->bx + (fx)sin(RAD((fx)((g->L + h->phase) % 360))) * 82; break;   /* Druaga slime */
        case 16: {   /* mage: visible 50, warn 30, three 5-frame flashes, hidden 110 while its wisp hunts you */
            h->cnt++;
            int s2 = h->sub;
            if (s2 == 0) { if (h->cnt >= 50) { h->sub++; h->cnt = 0; } }
            else if (s2 == 1) { if (h->cnt >= 30) { h->sub++; h->cnt = 0; } }
            else if (s2 <= 4) { if (h->cnt >= 5) { h->sub++; h->cnt = 0; } }
            else if (h->cnt >= 110) { h->sub = 0; h->cnt = 0; }
            break;
        }
        case 17: {   /* dragon: wait 50, charge 30, then a fireball sweeps */
            h->cnt++;
            int s2 = h->sub;
            if (s2 <= 1) { if (h->cnt >= 50) { h->sub += 2; h->cnt = 0; } }
            else if (s2 <= 3) { if (h->cnt >= 30) { h->sub += 2; h->cnt = 0; } }
            else if (s2 <= 7) { if (h->cnt >= 5) { h->sub += 2; h->cnt = 0; } }
            else if (h->cnt >= 35) { h->sub += 2; if (h->sub >= 10) h->sub -= 10; h->cnt = 0; }
            break;
        }
        case 18:     /* lizardman: runs under you at 5 px/step, rests 100 frames */
            if (h->sub == 0) {
                h->x += h->x < g->p.x ? 5 : -5;
                if (fabs((double)(h->x - g->p.x)) < 5) { h->sub = 1; h->cnt = 0; }
            } else if (++h->cnt > 100) h->sub = 0;
            break;
        case 19: {   /* dragon fireball */
            int s2 = g->ghosts[h->link].sub;
            if (s2 >= 4) { h->vis = 1; h->x += s2 % 2 == 0 ? -5 : 5; } else { h->x = h->bx; h->vis = 0; }
            break;
        }
        case 20: {   /* wisp: homes in on you while the mage is flashing or hidden */
            int s2 = g->ghosts[h->link].sub;
            if (s2 == 2) { h->x = h->bx; h->y = h->by; h->vis = 0; }
            else if (s2 >= 3) {
                h->vis = 1;
                h->x += h->x <= g->p.x ? (fx)0.8 : (fx)-0.8;
                h->y += h->y < g->p.y ? (fx)0.8 : (fx)-0.8;
            } else h->vis = 0;
            break;
        }
        case 12:   /* Pac-Land: swaying ghost, passes in front of / behind its platform trio */
            h->x = h->bx + (fx)sin(RAD((fx)((g->L + h->phase) % 360))) * 97;
            h->y = h->by - 10 + (fx)sin(RAD((fx)((g->L % 20) * 18))) * 20;
            h->front = h->y > h->by;
            break;
        case 13: case 14: {   /* slides along a street of platforms (13) or drifts freely (14), wrapping at the edges */
            if (h->t == 14) h->y = h->by + (fx)sin(RAD((fx)((g->L % 20) * 18))) * 10;
            fx sp = h->sub <= 2 ? -(3 + h->sub * (fx)1.5) : 3 + (h->sub - 3) * (fx)1.5, j = h->t == 14 ? (fx)h->jit : 0;
            h->x += sp;
            if (sp < 0 && h->x - j < -72) h->x = 392;
            if (sp > 0 && h->x + j > 392) h->x = -72;
            break;
        }
        case 9: case 10: {   /* Mappy cats: hop back and forth across the platform gap */
            fx sp = h->t == 9 ? (fx)1.5 : (fx)2.0;
            if (h->sub == 1) { h->x += sp; if (h->x > h->cx + 70) { h->x = h->cx + 70; h->sub = 0; } }
            else { h->x -= sp; if (h->x < h->cx - 70) { h->x = h->cx - 70; h->sub = 1; } }
            fx d = (fx)fabs((double)(h->cx - h->x));
            h->y = d < 50 ? h->by - (fx)sin((double)((50 - d) / 50)) * 35 : h->by;
            break;
        }
        case 11: {   /* beam shooter: wait 80 -> charge 40 -> fire */
            if (h->cnt < 0) { h->sub -= 4; h->cnt = 0; }
            h->cnt++;
            if (h->sub <= 1) { if (h->cnt >= 80) { h->sub += 2; h->cnt = 0; } }
            else if (h->sub <= 3) { if (h->cnt >= 40) { h->sub += 2; h->cnt = 0; } }
            else if (h->cnt >= 100) { h->sub -= 4; h->cnt = 0; }
            break;
        }
        case 24: {   /* travelling beam leaves the shooter at 2 px/step while it fires */
            pnj_ghost *sh = &g->ghosts[h->link]; int s2 = sh->sub;
            if (s2 == 4) { h->vis = 1; h->x = h->bx - (fx)sh->cnt * 2; if (h->x < 80) sh->cnt = -1; }
            else if (s2 == 5) { h->vis = 1; h->x = h->bx + (fx)sh->cnt * 2; if (h->x > 240) sh->cnt = -1; }
            else h->vis = 0;
            break;
        }
        case 22: case 23: {   /* sliders: cross the screen and wrap; speed from sub (22 goes left, 23 right) */
            fx sp = h->sub <= 2 ? -(3 + h->sub * (fx)1.5) : 3 + (h->sub - 3) * (fx)1.5;
            h->x += sp;
            if (sp < 0 && h->x < -72) h->x = 392;
            if (sp > 0 && h->x > 392) h->x = -72;
            break;
        }
        case 6: case 7: case 8:   /* beam pieces show once the shooter's charge reaches their step */
            h->vis = g->ghosts[h->link].sub >= 4 + (h->t - 6) * 2;
            break;
        default:
            h->x = h->bx + (fx)sin(RAD((fx)(((g->L + h->phase) % 120) * 3))) * GHOST_RANGE[h->lvl];
            h->y = h->by + (fx)sin(RAD((fx)((g->L % 20) * 18))) * 2;
        }
    } else {
        h->vy += 1600 * DT;
        h->y += h->vy * DT;
        h->x += (h->state == 3 ? 150 : h->state == 4 ? -150 : 0) * DT;
        h->spin += 15;
    }
}

static void ghost_hit(pnj_game *g, pnj_ghost *h) {
    pnj_player *p = &g->p;
    if (!h->alive || p->dead || h->state != 0) return;
    if (hypot((double)(p->x - h->x), (double)(p->y - h->y)) >= 40) return;
    int safe = p->power || p->boost || p->boost13;
    if (h->t >= 6 && h->t <= 8) { if (h->vis && !safe) die(g, 1); return; }
    if (h->t == 16 && h->sub == 5) return;
    if (h->t == 19 || h->t == 20) { if (h->vis && !safe) die(g, 1); return; }
    if (h->t == 24) { if (h->vis && !safe) p->push = g->ghosts[h->link].sub % 2 == 0 ? -2 : 2; return; }
    if (h->t >= 22 && !safe) { p->vy = p->y > h->y ? 800 : -400; spark(g, h->x, h->y); sfx(g, PNJ_SFX_BOUND); return; }   /* solid: slams you down from below, small hop from above */
    if (p->power) { h->state = 2; h->vy = -200; spark(g, h->x, h->y); emit(g, PNJ_EM_PUFF, h->x, h->y); bounce_off_ghost(g); }
    else if (p->boost || p->boost13) { h->vy = -400; h->state = p->x < h->x ? 4 : 3; sfx(g, PNJ_SFX_EAT_ENEMY); emit(g, PNJ_EM_PUFF, h->x, h->y); }
    else die(g, 1);
}

/* ---- Mappy screen-space enemy (orig C0040d): blob (t9) or bonus (t8) dropping from the top ---- */
static const int MAP_F374[10] = { 10, 10, 8, 8, 8, 6, 6, 6, 4, 3 }, MAP_F373[4] = { 2800, 2500, 2200, 2000 };
static void update_mappy(pnj_game *g) {
    pnj_enemy *e = &g->me; pnj_player *p = &g->p;
    int tier = MIN(9, pnj_tier(g));
    fx off = 480 - g->cam594;
    if (!e->on) {
        if (tier >= 2 && R(g, MAP_F373[tier / 3]) <= 10) {
            int x = R(g, 220) + 50;
            int t = R(g, MAP_F374[tier]) == 0 ? 9 : 8;
            memset(e, 0, sizeof *e);
            e->on = 1; e->t = t; e->x = (fx)x; e->y = -130;
        }
        return;
    }
    if (e->dead > 0) { e->dead++; if (e->dead >= 14) e->on = 0; return; }
    if (e->fl) {
        if (e->fl == 2) { e->vy += 1600 * DT; e->y += DT * e->vy; }
        else {
            fx nvx = (e->fl == 3 ? -150 : 150) + e->vx * (fx)0.6;
            e->vy += 1600 * DT; e->x += DT * nvx; e->y += DT * e->vy; e->vx = nvx;
            if (e->x < -50 || e->x > 370) e->on = 0;
        }
        if (e->y >= 500) e->on = 0;
        return;
    }
    fx sp = e->t == 8 ? 3 : 5;
    if (e->st == 0) { e->y += sp; if (e->y >= 25) { e->y = 25; e->st = 1; } }
    else if (e->st == 1) {
        if (e->t == 9) e->x += e->x <= p->x ? (fx)0.8 : (fx)-0.8;
        if (e->cnt++ > (e->t == 9 ? 150 : 80)) { e->st = 2; e->cnt = 0; e->danger = 1; }
    }
    else e->y += sp;
    if (e->y >= 520) { e->on = 0; return; }
    if (e->danger && !p->dead && hypot((double)(p->x - e->x), (double)(p->y - (e->y - off))) < 30) {
        if (e->t == 8) { e->dead = 14; pnj_pick tmp = { 10, e->x, e->y - off, 1 }; collect_pickup(g, &tmp); }
        else if (p->power) { e->fl = 2; e->vy = 0; spark(g, e->x, e->y - off); bounce_off_ghost(g); }
        else if (p->boost || p->boost13) { e->fl = p->x < e->x ? 4 : 3; e->vy = -400; e->vx = 0; sfx(g, PNJ_SFX_EAT_ENEMY); }
        else die(g, 1);
    }
}

/* ---- Pac-Land screen-space enemies (orig C0040d): a cyan/purple ghost pair sweeps the top, a ghost rises from below ---- */
static const int PL_BX1[4] = { 0, 60, 160, 260 }, PL_BX2[4] = { 0, 120, 240, 0 };
static void update_pacland(pnj_game *g) {
    pnj_player *p = &g->p;
    fx off = 480 - g->cam594;
    int on = MAX(g->start_level, (int)floor((double)(MAX((fx)0, -p->y) / 14400))) % 2 == 1;
    fx px = p->x, py = p->y + off;
    for (int i = 0; i < 4; i++) {
        pnj_enemy *e = &g->pl[i];
        if (!e->on) {
            if (!on) continue;
            int it = 0; fx ix = 0, iy = 0;
            const pnj_enemy *s0 = &g->pl[0];
            if (i == 0) {
                if (R(g, 2) == 0) { g->f233d = 1; ix = (fx)(R(g, 220) + 50); it = 12; iy = 580; }
                else { g->f233d = 0; ix = R(g, 2) == 0 ? 0 : 320; it = 10; iy = -100; }
            } else if (!g->f233d && s0->on && s0->t == 10 && s0->fl == 0 && s0->st == 1) {
                if (s0->ox < 160 && s0->x == PL_BX1[i]) { it = 11; ix = s0->x; iy = 50; }
                else if (s0->ox > 160 && s0->x == PL_BX2[i]) { it = 11; ix = s0->x; iy = 50; }
            }
            if (it) { memset(e, 0, sizeof *e); e->on = 1; e->t = it; e->x = e->ox = e->pvx = ix; e->y = iy; }
            if (!e->on) continue;
        }
        int t = e->t;
        if (!on && e->fl == 0 && e->st >= 0) e->st = -1;
        e->pvx = e->x;
        if (e->fl == 0) {
            if (t == 10) {
                if (e->st == -1) { e->y -= 6; if (e->y < -20) e->on = 0; }
                else if (e->st == 0) { e->y += 3; if (e->y >= 50) { e->y = 50; e->st = 1; } }
                else if (e->st == 1) { e->x += e->ox < 160 ? 1 : -1; if (e->x <= 0 || e->x >= 320) { e->st = 2; e->cnt = 0; } }
                else if (e->st == 2) { e->y -= 3; if (e->y < -20) e->on = 0; }
            } else if (t == 11) {
                if (e->st == -1) {
                    if (e->x < 100) e->x -= 5; else if (e->x > 220) e->x += 5;
                    e->y += e->y < 240 ? -6 : 6;
                    if (e->y < 30 || e->y > 510 || e->x < -30 || e->x > 350) e->on = 0;
                } else { e->danger = 1; e->y += 3; e->x = e->ox + (fx)sin(RAD((fx)((g->L % 36) * 10))) * 5; }
            } else if (t == 12) {
                e->danger = 1;
                if (e->st == -1) {
                    if (e->y >= 100) e->x += e->x <= px ? (fx)-2.5 : (fx)2.5;
                    e->y += 6;
                    if (e->y < -30 || e->y > 530 || e->x < -30 || e->x > 370) e->on = 0;
                } else if (e->st == 0) { e->y -= 3; if (e->y <= 430) { e->y = 430; e->st = 2; } e->cnt = 0; }
                else if (e->st == 2) {
                    e->x += e->x <= px ? (fx)0.5 : (fx)-0.5;
                    e->cnt++;
                    if (e->y < py) e->y += (fx)1.5; else e->y -= (fx)0.5;
                    if (e->cnt > 300) { e->st = -1; e->cnt = 0; }
                    if (e->y < -50) e->on = 0;
                }
            }
        } else if (e->fl == 2) { e->vy += 1600 * DT; e->y += DT * e->vy; }
        else {
            fx nvx = (e->fl == 3 ? -150 : 150) + e->vx * (fx)0.6;
            e->vy += 1600 * DT; e->x += DT * nvx; e->y += DT * e->vy; e->vx = nvx;
            if (e->x < -50 || e->x > 370) e->on = 0;
        }
        if (e->y >= (t == 12 ? 581 : 500)) e->on = 0;
        if (e->on && e->danger && e->fl == 0 && !p->dead && hypot((double)(px - e->x), (double)(py - e->y)) < 30) {
            if (p->power) { e->fl = 2; e->vy = 0; spark(g, e->x, e->y - off); bounce_off_ghost(g); }
            else if (p->boost || p->boost13) { e->fl = p->x < e->x ? 4 : 3; e->vy = -400; e->vx = 0; sfx(g, PNJ_SFX_EAT_ENEMY); }
            else die(g, 1);
        }
    }
}

/* ---- Galaga screen-space enemies (orig C0041e case 3, C0040d types 0-2) ---- */
static const fx GAL_X[5] = { 160, 40, 280, 100, 220 };
static const int GAL_SPEED[3] = { 3, 4, 4 }, GAL_WAIT[10] = { 650, 650, 600, 700, 650, 1000, 900, 800, 700, 600 };
static void update_galaga(pnj_game *g, fx tilt) {
    pnj_player *p = &g->p;
    if (fabs((double)tilt) >= 0.9) { if (++g->gal_rush_cnt >= 100) { g->gal_rush_cnt = 100; g->gal_rush = 1; } } else g->gal_rush_cnt = 0;
    int t0 = pnj_tier(g), n = t0 > 0 ? 1 : 0;
    if (t0 >= 3) n += 2;
    if (t0 >= 5) n += 2;
    if (g->gal_rush) n = 5;
    int tier = MIN(9, t0);
    fx off = 480 - g->cam594;
    for (int i = 0; i < n; i++) {
        pnj_slot *e = &g->gs[i];
        if (!e->on) {
            int t = R(g, 3);
            memset(e, 0, sizeof *e);
            e->on = 1; e->t = t; e->x = e->bx = e->px = GAL_X[i]; e->y = -100;
        }
        e->px = e->x;
        if (e->dead > 0) { e->dead++; if (e->dead >= 14) e->on = 0; continue; }
        switch (e->st) {
        case 0: e->y += GAL_SPEED[e->t]; if (e->y >= 50) { e->y = 50; e->st = 1; } break;
        case 1: {
            e->x = e->bx + (fx)sin((double)(((g->L >> 1) / 16) % 2)) * 6;
            if (R(g, GAL_WAIT[tier]) != 0) { if (e->cnt++ <= 1000 && !g->gal_rush) break; }
            e->st = 2; if (R(g, 2) == 0) { e->rot = 360; e->st = 3; } e->cnt = 0;
            break;
        }
        case 2: e->rot += 2; if (e->rot >= 180) { e->rot = 180; e->danger = 1; e->st = 4; } break;
        case 3: e->rot -= 2; if (e->rot <= 180) { e->rot = 180; e->danger = 1; e->st = 4; } break;
        case 4:
            if (e->t == 1) e->x = e->bx + (fx)sin(RAD((fx)((g->L % 120) * 3))) * 20;
            else if (e->t == 2) e->x += e->x > p->x ? (fx)-0.5 : (fx)0.5;
            e->y += GAL_SPEED[e->t];
            break;
        }
        if (e->y >= 500) { e->on = 0; continue; }
        if (e->danger && !p->dead && hypot((double)(p->x - e->x), (double)(p->y - (e->y - off))) < 30) {
            if (p->power) { e->dead = 1; spark(g, e->x, e->y - off); bounce_off_ghost(g); }
            else if (p->boost || p->boost13) { e->dead = 1; sfx(g, PNJ_SFX_EAT_ENEMY); }
            else die(g, 1);
        }
    }
}

/* ---- Xevious screen-space enemies and bullets (orig C0041e case 7, C0040d types 3-7, C0043g) ---- */
static const fx XEV_X[4] = { 0, 160, 40, 280 };
static const int XEV_F377[10] = { 1, 60, 60, 50, 50, 50, 40, 30, 20, 10 };
static const fx XEV_SPD[5] = { 3, 4, 6, 6, 9 };
static void xev_aim(const pnj_game *g, fx px, fx py, fx ex, fx ey, fx *vx, fx *vy) {
    (void)g;
    fx dx = px - ex, dy = py - ey, d = (fx)hypot((double)dx, (double)dy); if (d == 0) d = 1;
    *vx = dx / d; *vy = dy / d;
}
static void xev_shoot(pnj_game *g, fx x, fx y, fx px, fx py) {
    if (g->nxbul >= 64) { g->overflow = 1; return; }
    pnj_bullet *b = &g->xbul[g->nxbul++];
    memset(b, 0, sizeof *b); b->x = x; b->y = y;
    xev_aim(g, px, py, x, y, &b->vx, &b->vy);
}
static void update_xevious(pnj_game *g) {
    pnj_player *p = &g->p;
    int tier_raw = pnj_tier(g), tier = MIN(9, tier_raw);
    fx off = 480 - g->cam594;
    int ladder = MAX(g->start_level, (int)floor((double)(MAX((fx)0, -p->y) / 14400))) % 3 < 2;
    int n = tier > 0 ? 2 : 0; if (tier >= 3) n++; if (tier >= 5) n++;
    if (g->x_phase != ladder) {
        g->x_phase = ladder;
        for (int i = 0; i < 5; i++) if (g->gs[i].on && g->gs[i].dead == 0) g->gs[i].st = -1;
    }
    fx px = p->x, py = p->y + off;
    for (int i = 0; i < (n > 5 ? n : 5); i++) {
        pnj_slot *e = &g->gs[i];
        if (!e->on) {
            if (i >= n) continue;
            if (ladder) {
                if (R(g, XEV_F377[tier]) == 0) {
                    int t = R(g, 3) + 3; fx x = XEV_X[tier >= 2 && tier <= 4 ? i + 1 : i];
                    memset(e, 0, sizeof *e); e->on = 1; e->t = t; e->x = x; e->y = -150;
                }
            } else if (i == 0) {
                sfx(g, PNJ_SFX_WARN);
                memset(e, 0, sizeof *e); e->on = 1; e->t = 7; e->x = 160; e->y = -100;
            } else if (g->gs[0].on && g->gs[0].t == 7 && g->gs[0].st == 1 && R(g, XEV_F377[tier]) == 0) {
                fx x = XEV_X[tier == 3 || tier == 4 ? i + 1 : i];
                memset(e, 0, sizeof *e); e->on = 1; e->t = 6; e->x = x; e->y = 50; e->danger = 1;
            }
            continue;
        }
        if (e->dead > 0) { e->dead++; if (e->dead >= 15) e->on = 0; continue; }
        int t = e->t;
        if (t == 3 || t == 4) {
            switch (e->st) {
            case -1: e->y -= 3; e->x += e->x < 160 ? (fx)-1.5 : (fx)1.5; if ((e->x < -30 || e->x > 350) && e->y < -100) e->on = 0; break;
            case 0: e->x += e->x < px ? 1 : -1; e->y += XEV_SPD[t]; if (e->y >= 50) { e->y = 50; e->st = 3; e->cnt = 0; } break;
            case 3: e->cnt++; e->danger = 1; e->y += (fx)0.5; e->x += e->x <= px ? (fx)0.5 : (fx)-0.5; if (e->cnt > 18) { e->st = 4; e->cnt = 0; } break;
            case 4:
                if (e->y >= 100) e->x += (e->x > px ? 1 : -1) * (t == 3 ? (fx)1.5 : (fx)2.5);
                e->y += XEV_SPD[(t - 3) + 2]; break;
            }
        } else if (t == 5) {
            e->danger = 0;
            switch (e->st) {
            case -1: e->y -= XEV_SPD[2]; e->x += e->x < 160 ? (fx)-1.5 : (fx)1.5; if (e->x < -30 || e->x > 350) e->on = 0; break;
            case 0: e->x += e->x < px ? 1 : -1; if (e->x == px && e->y > 20) e->st = 1; e->y += XEV_SPD[2]; if (e->y >= 50) { e->y = 50; e->st = 1; } break;
            case 1: e->cnt++; if (e->cnt > 15) { e->st = 2; e->cnt = 0; } break;
            default:
                e->cnt++;
                if (e->cnt > 3) {
                    if (e->st == 3) xev_shoot(g, e->x, e->y, px, py);
                    e->st = e->st == 6 ? -1 : e->st + 1; e->cnt = 0;
                }
            }
        } else if (t == 6) {
            e->danger = 1;
            if (e->st == 0) { xev_aim(g, px, py, e->x, e->y, &e->vx, &e->vy); if (++e->cnt > 18) { e->st = 1; e->cnt = 0; } }
            else if (e->st >= 1 && e->st <= 4) { if (++e->cnt > 3) { e->st++; e->cnt = 0; } }
            else if (e->st == 5) {
                e->cnt++; e->x += e->vx * 2; e->y += e->vy * 2;
                if (tier_raw >= 9 && e->cnt > R(g, 50) + 30) { e->st = 6; e->cnt = 0; }
                if (e->x < -50 || e->x > 350 || e->y < -50 || e->y > 530) e->on = 0;
            } else if (e->st == 6) { if (++e->cnt > 10) e->st = 7; }
            else if (e->st == 7) { xev_shoot(g, e->x, e->y, px, py); e->st = 8; }
            else { e->on = 0; e->danger = 0; }
        } else if (t == 7) {
            e->danger = 0;
            if (e->st == -1) { e->y -= 3; if (e->y < -20) e->on = 0; }
            else if (e->st == 0) { e->y += (fx)0.5; if (e->y >= 70) { e->y = 70; e->st = 1; } }
        }
        if (e->y >= 500) e->on = 0;
        if (e->danger && e->on && e->dead == 0 && !p->dead && hypot((double)(px - e->x), (double)(py - e->y)) < 30) {
            if (p->power) { e->dead = 1; spark(g, e->x, e->y - off); bounce_off_ghost(g); }
            else if (p->boost || p->boost13) { e->dead = 1; sfx(g, PNJ_SFX_EAT_ENEMY); }
            else die(g, 1);
        }
    }
    int keep = 0;
    for (int i = 0; i < g->nxbul; i++) {
        pnj_bullet *b = &g->xbul[i];
        if (b->dead > 0) { b->dead++; }
        else {
            b->x += b->vx * 5; b->y += b->vy * 5;
            if (b->x < -50 || b->x > 350 || b->y < -50 || b->y > 530) b->dead = 99;
            else if (!p->dead && hypot((double)(px - b->x), (double)(py - b->y)) < 9) {
                if (p->power) { b->dead = 1; bounce_off_ghost(g); }
                else if (p->boost || p->boost13) { b->dead = 1; sfx(g, PNJ_SFX_EAT_ENEMY); }
                else die(g, 1);
            }
        }
    }
    for (int i = 0; i < g->nxbul; i++) if (g->xbul[i].dead < 15) g->xbul[keep++] = g->xbul[i];
    g->nxbul = keep;
}

/* ---- player + camera ---- */
static int step_player(pnj_game *g, fx tilt) {
    pnj_player *p = &g->p;
    if (p->power || p->lift || p->candle) {
        g->tick578++;
        if (g->tick578 >= PNJ_FPS) {
            g->tick578 = 0;
            if (p->power) { p->power_cnt--; if (p->power_cnt <= 0) { p->power_cnt = 0; p->power = 0; } }
            if (p->candle) { p->candle_cnt--; if (p->candle_cnt <= 0) { p->candle_cnt = 0; p->candle = 0; } }
            if (p->lift) { p->lift_cnt--; if (p->lift_cnt <= 0) { p->lift_cnt = 0; p->lift = 0; } }
        }
    }
    if (p->boost13 && p->vy >= 0) p->boost13 = 0;
    if (p->boost && p->vy >= 0) p->boost = 0;
    if (p->chain_t > 0) p->chain_t--;
    if (p->landed && p->vy >= 0) p->landed = 0;
    if (p->dead) tilt = 0;
    p->px = p->x; p->py = p->y;
    fx ax = tilt * 500;
    fx vxd = p->vx * (fx)0.6;
    if ((ax > 0 && vxd < 0) || (ax < 0 && vxd > 0)) vxd = p->vx * (fx)0.3;
    fx vx = vxd + ax;
    if (vx > 1200) vx = 1200;
    if (vx < -1200) vx = -1200;
    fx vy = p->vy + 1600 * DT, extra = 0;
    if (p->lift) {   /* balloon / propeller hat: keeps climbing at ~220 px/s and gravity can't pull down */
        extra = (-220 + (fx)sin(RAD((fx)((g->L % 45) * 8))) * 20) * DT;
        p->lift_h = MIN(30, p->lift_h + 1);
        if (vy > 0) vy = 0;
    }
    fx x = p->x + DT * vx;
    fx y = p->y + extra + DT * vy;
    if (p->push) { x = p->px + p->push; y = p->py; vx = 0; vy = 0; p->push = 0; }   /* Mappy beam drags you sideways */
    if (x < 0) x = 320;
    if (x > 320) x = 0;
    p->x = x; p->y = y; p->vx = vx; p->vy = vy;
    if (p->dead) {
        p->dead_t--;
        if (p->dead_t == 40) p->dead_y = p->y;
        if (p->dead_t <= 0) return 1;
    }
    return 0;
}

static void step_camera(pnj_game *g) {
    pnj_player *p = &g->p;
    if (p->y < g->cam593 - 300) { g->cam593 = p->y + 300; g->cam595 = g->cam593; }
    if (p->y > g->cam593 - 100) g->cam593 = p->y + 100;
    fx f3 = g->cam595 + 240;
    if (p->y > (f3 <= 0 ? f3 : 0) && !p->dead) die(g, 0);
    if (g->cam593 != g->cam594) {
        fx d = g->cam594 - g->cam593;
        g->cam594 -= d * (fx)0.5;
        if (d < 2 && d > -2) g->cam594 = g->cam593;
    }
    if (p->y < g->peak) g->peak = p->y;
}

void pnj_step(pnj_game *g, fx tilt) {
    g->L++;
    while (g->gen_top > g->cam594 - 480 - 1800) gen_chunk(g);
    int over = step_player(g, tilt);
    if (g->stage == 3 && !g->p.dead) update_galaga(g, g->zero_screen_tilt ? 0 : tilt);
    if (g->stage == 4 && !g->p.dead) update_mappy(g);
    if (g->stage == 5 && !g->p.dead) update_pacland(g);
    if (g->stage == 7 && !g->p.dead) update_xevious(g);
    fx low = g->cam595 + 215, top = g->cam594 - 960;
    for (int i = 0; i < g->nplat; i++) {
        pnj_plat *pl = &g->plats[i];
        if (pl->y >= low && pl->alive && pl->t != 5 && pl->t != 6) pl->alive = 0;
        if (pl->alive && pl->y > top - 1) { move_plat(g, pl); land_on(g, pl); }
    }
    for (int i = 0; i < g->npick; i++) {
        pnj_pick *pk = &g->picks[i];
        if (!pk->alive) continue;
        if (pk->y >= low) { pk->alive = 0; continue; }
        if (pk->y > top && !g->p.dead && hypot((double)(g->p.x - pk->x), (double)(g->p.y - pk->y)) <= PICKUP_R[pk->t]) collect_pickup(g, pk);
    }
    for (int i = 0; i < g->nghost; i++) {
        pnj_ghost *h = &g->ghosts[i];
        if (!h->alive) continue;
        if (h->y >= low + 100) { h->alive = 0; continue; }
        if (h->y > top) { update_ghost(g, h); ghost_hit(g, h); }
    }
    step_camera(g);
    step_effects(g);
    /* drop far-below entities every 400 frames (same cadence as JS); remap spinner parents */
    if (g->L % 400 == 0) {
        static int remap[PNJ_MAX_PLATS], gremap[PNJ_MAX_GHOSTS], saved_alive[PNJ_MAX_PLATS];
        static fx saved_ang[PNJ_MAX_PLATS];
        int n = 0;
        for (int i = 0; i < g->nplat; i++) { saved_ang[i] = g->plats[i].ang; saved_alive[i] = g->plats[i].alive; }
        for (int i = 0; i < g->nplat; i++) {
            if (g->plats[i].y < low + 600) { remap[i] = n; g->plats[n++] = g->plats[i]; }
            else remap[i] = -2;
        }
        for (int i = 0; i < n; i++) {
            pnj_plat *q = &g->plats[i];
            if (q->parent < 0) continue;
            int old = q->parent;               /* old index: plats[] was compacted in place, so look the parent up via saved copy */
            q->parent = remap[old];
            if (q->parent == -2) { q->fz_ang = saved_ang[old]; q->fz_alive = saved_alive[old]; }
        }
        g->nplat = n; n = 0;
        for (int i = 0; i < g->npick; i++) if (g->picks[i].y < low + 600) g->picks[n++] = g->picks[i];
        g->npick = n; n = 0;
        for (int i = 0; i < g->nghost; i++) {
            if (g->ghosts[i].y < low + 600) { gremap[i] = n; g->ghosts[n++] = g->ghosts[i]; }
            else gremap[i] = -2;
        }
        for (int i = 0; i < n; i++) {
            pnj_ghost *h = &g->ghosts[i];
            if (h->parent >= 0) h->parent = remap[h->parent];
            if (h->link >= 0) h->link = gremap[h->link];
        }
        g->nghost = n;
    }
    if (over) g->over = 1;
}

/* ---- draw list ---- */
static void put(pnj_game *g, int sprite, int col, int row, int vrow, fx x, fx y, fx rot, int layer) {
    if (g->ndraw >= PNJ_MAX_DRAW) return;
    pnj_draw *d = &g->draw[g->ndraw++];
    d->sprite = (int16_t)sprite; d->col = (int16_t)col; d->row = (int16_t)row; d->vrow = (int16_t)vrow;
    d->x = x; d->y = y; d->rot = rot; d->scale = 1; d->r = d->g = d->b = d->a = 255; d->blend = 0; d->layer = (uint8_t)layer;
}

void pnj_build_draw(pnj_game *g) {
    g->ndraw = 0;
    fx off = PNJ_H - g->cam594;
    for (int i = g->nplat - 1; i >= 0; i--) {
        pnj_plat *pl = &g->plats[i];
        if (!pl->alive) continue;
        fx sy = pl->y + off;
        if (sy < -60 || sy > PNJ_H + 60) continue;
        int white = pl->t == 8 || pl->t == 10;
        if (pl->t == 10) { int ph = pl->phase / 30; if (ph >= 4) continue; if (ph == 3 && pl->phase % 4 < 2) continue; }
        if (pl->t == 9 && g->stage == 4) {
            static const int SPF[16] = { 0, 1, 2, 3, 4, 3, 2, 1, 0, 5, 6, 7, 8, 7, 6, 5 };
            put(g, 150, 0, SPF[pl->phase % 16], 0, pl->x, sy, 0, 1);
            continue;
        }
        put(g, 126 + 3 * g->stage + (white ? 1 : 0), 0, 0, pl->t == 8 ? pl->phase : 0, pl->x, sy, pl->ang, 1);
    }
    static const int pspr[14] = { 106, 105, 116, 117, 118, 107, 108, 109, 110, 111, 112, 113, 114, 115 };
    for (int i = g->npick - 1; i >= 0; i--) {
        pnj_pick *k = &g->picks[i];
        if (!k->alive) continue;
        fx sy = k->y + off;
        if (sy < -60 || sy > PNJ_H + 60) continue;
        put(g, pspr[k->t], 0, 0, 0, k->x, sy, 0, 2);
    }
    static const uint8_t gtint[4][3] = { {255,66,41}, {41,194,255}, {255,194,66}, {255,168,168} };
    for (int i = 0; i < g->nghost; i++) {
        pnj_ghost *h = &g->ghosts[i];
        if (!h->alive) continue;
        fx sy = h->y + off;
        if (sy < -80 || sy > PNJ_H + 80) continue;
        if (h->t >= 6 && h->t <= 8) {                                 /* fire beam pieces: row grows with the shooter's charge */
            if (!h->vis || h->state >= 2) continue;
            int sub = g->ghosts[h->link].sub;
            if (sub % 2 != 0) { int r = (sub - 5) / 2; if (r >= 0 && sub >= 5) put(g, 157 + (h->t - 6), 0, r, 0, h->x, sy, 0, 2); }
            else { int r = (sub - 4) / 2; if (r >= 0) put(g, 160 + (h->t - 6), 0, r, 0, h->x, sy, 0, 2); }
            continue;
        }
        if (h->t == 19) {
            if (!h->vis) continue;
            int s2 = g->ghosts[h->link].sub;
            put(g, s2 % 2 != 0 ? 192 : 191, (s2 - (s2 % 2 == 0 ? 4 : 5)) / 2, 0, 0, h->x, sy, 0, 2);
            continue;
        }
        if (h->t == 20) { if (h->vis) put(g, 193, ((g->L / 4) >> 1) % 2, 0, 0, h->x, sy, (fx)((g->L % 4) * 90), 2); continue; }
        if (h->t == 16 && h->sub == 5 && h->state == 0) continue;
        if (h->t == 24) {                                              /* Mappy travelling beam */
            if (!h->vis) continue;
            static const int BR[3] = { 0, 2, 1 };
            put(g, 177, 0, BR[(g->L >> 2) % 3], 0, h->x, sy, g->ghosts[h->link].sub % 2 != 0 ? 180 : 0, 2);
            continue;
        }
        const uint8_t *tc;
        static const uint8_t T15[3] = {222,15,0}, T16[3] = {222,105,20}, T17[3] = {33,186,235}, T18[3] = {0,105,0};
        static const uint8_t T12[3] = {255,0,0}, T13[3] = {230,179,0}, T14[3] = {230,77,130};
        static const uint8_t T9[3] = {255,38,38}, T10[3] = {255,64,217}, T11A[3] = {255,89,89}, T11B[3] = {77,102,255};
        static const uint8_t T4[3] = {255,140,89}, T5[3] = {66,255,66}, T21[3] = {255,255,51}, T22[3] = {255,66,41}, T23[3] = {89,89,255}, TRED[3] = {255,89,89}, TW[3] = {255,255,255};
        tc = h->t == 15 ? T15 : h->t == 16 ? T16 : h->t == 17 ? T17 : h->t == 18 ? T18 : h->t == 12 ? T12 : h->t == 13 ? T13 : h->t == 14 ? T14 : h->t == 9 ? T9 : h->t == 10 ? T10 : h->t == 11 ? ((h->sub >= 2 && (h->sub > 3 || g->L % 2 == 0)) ? T11A : T11B)
           : h->t < 4 ? gtint[h->t] : h->t == 4 ? T4 : h->t == 5 ? T5 : h->t == 21 ? T21 : h->t == 22 ? T22 : T23;
        fx a = (fx)0.5 + (fx)sin(RAD((fx)((g->L % 30) * 6))) * (fx)0.5;
        if (h->t == 5 && h->sub >= 2 && !g->p.power) { tc = TRED; a = (h->sub > 3 || g->L % 2 == 0) ? 1 : 0; }
        if (h->t >= 4 && g->p.power) tc = TW;
        if (h->state <= 1 && g->ndraw < PNJ_MAX_DRAW - 1) {           /* soft glow behind the ghost (sprite 80, additive) */
            put(g, 80, 0, 0, 0, h->x, sy, 0, 2);
            pnj_draw *d = &g->draw[g->ndraw - 1];
            d->r = tc[0]; d->g = tc[1]; d->b = tc[2]; d->a = (uint8_t)(a > 1 ? 255 : a * 255); d->blend = 1;
        }
        int frame = 0, anim = (g->L >> 1) % 2, grow = 0; fx rot = 0;
        int anim4 = ((g->L / 4) >> 1) % 2;
        if (h->t == 4) {
            if (h->state == 0) {
                if (h->dir) { rot = 180; frame = h->x < h->px ? anim4 + 2 : anim4; }
                else frame = h->x <= h->px ? anim4 : anim4 + 2;
            } else if (h->state == 4) { frame = 9; rot = (fx)((360 - (g->L % 360)) * 15); }
            else { frame = 8; rot = (fx)((g->L % 360) * 15); }
        } else if (h->t == 5) {
            if (h->state == 0) { int aa = h->sub <= 3 ? anim4 : 0; frame = h->sub % 2 == 0 ? aa : aa + 2; }
            else if (h->state == 4) { frame = 9; rot = (fx)((360 - (g->L % 360)) * 15); }
            else { frame = 8; rot = (fx)((g->L % 360) * 15); }
        } else if (h->t >= 15 && h->t <= 18) {
            if (h->state == 0) {
                if (g->p.power) frame = ((g->L / 4) >> 1) % 2 + 4;
                else if (h->t == 17) frame = (h->sub <= 3 ? ((g->L / 4) >> 1) % 2 : 0) + (h->sub % 2 == 0 ? 0 : 2);
                else if (h->t == 18) frame = ((g->L / 2) >> 1) % 2 + (h->x >= g->p.x ? 0 : 2);
                else frame = ((g->L / 8) >> 1) % 2 + (h->x <= h->px ? 0 : 2);
            } else if (h->state == 4) { frame = 9; rot = (fx)((360 - (g->L % 360)) * 15); }
            else { frame = 8; rot = (fx)((g->L % 360) * 15); }
        } else if (h->t >= 12 && h->t <= 14) {
            int blink = g->p.power && g->p.power_cnt <= 2 && ((g->L >> 1) % 4) < 2;
            if (h->state == 0) {
                if (h->t == 12) {
                    if (!g->p.power) frame = ((g->L / 7) >> 1) % 2 + (h->front ? 2 : 0);
                    else frame = (h->front ? 5 : 4) + (blink ? 2 : 0);
                } else if (!g->p.power) frame = anim4 + (h->x > h->px ? 2 : 0);
                else { frame = anim4 + 4 + (blink ? 2 : 0); if (h->t == 13 && h->x > h->px) grow = 1; }
            } else if (h->state == 4) { frame = 9; rot = (fx)((360 - (g->L % 360)) * 15); }
            else { frame = 8; rot = (fx)((g->L % 360) * 15); }
        } else if (h->t == 9 || h->t == 10) {
            if (h->state == 0) frame = h->y >= h->by - (fx)0.01 ? anim4 + (h->x > h->px ? 2 : 0) : (h->x <= h->px ? 0 : 2);
            else if (h->state == 4) { frame = 5; rot = (fx)((360 - (g->L % 360)) * 15); }
            else { frame = 4; rot = (fx)((g->L % 360) * 15); }
        } else if (h->t == 11) {
            int c = h->sub % 2 == 0 ? 4 : 0, f4 = anim4;
            if (h->state == 0) { if (h->sub == 2 || h->sub == 3) f4 = h->cnt < 31 ? 0 : (h->cnt - 31) / 3; else if (h->sub >= 4) f4 = 3; }
            else { f4 = 3; rot = (fx)((h->state == 4 ? -1 : 1) * (g->L % 360) * 15); }
            frame = f4 + c;
        } else if (h->t >= 21) {
            if (h->state == 0) frame = h->t == 21 ? 0 : anim4 + (h->sub < 3 ? 0 : 2);
            else if (h->state == 4) { frame = h->t == 21 ? 0 : 9; rot = (fx)((360 - (g->L % 360)) * 15); }
            else { frame = h->t == 21 ? 0 : 8; rot = (fx)((g->L % 360) * 15); }
        } else if (h->state == 0) {
            if (g->p.power) { frame = anim + 4; if (g->p.power_cnt <= 2 && ((g->L >> 1) % 4) < 2) frame += 2; }
            else frame = anim + (h->x > h->px ? 2 : 0);
        } else if (h->state == 2) { frame = 8; rot = h->spin; }
        else { frame = anim + 4; rot = (h->state == 3 ? 1 : -1) * h->spin; }
        put(g, h->t >= 21 ? 142 + h->t : h->t >= 15 ? 172 + h->t : h->t >= 12 ? 169 + h->t : h->t >= 9 ? 165 + h->t : 151 + h->t, frame, grow, 0, h->x, sy, rot, 2);
    }
    if (g->stage == 3) {
        for (int i = 0; i < 5; i++) {
            const pnj_slot *e = &g->gs[i];
            if (!e->on) continue;
            if (e->dead > 0) { put(g, 169 + MIN(4, e->dead / 3), 0, 0, 0, e->x, e->y, 0, 2); continue; }
            if (e->st >= 2 && g->ndraw < PNJ_MAX_DRAW - 1) {
                fx a = (fx)0.5 + (fx)sin(RAD((fx)((g->L % 30) * 6))) * (fx)0.5;
                put(g, 80, 0, 0, 0, e->x, e->y, 0, 2);
                pnj_draw *d = &g->draw[g->ndraw - 1];
                if (g->p.power) { d->r = d->g = d->b = 255; } else { d->r = 128; d->g = 128; d->b = 255; }
                d->a = (uint8_t)(a > 1 ? 255 : a * 255); d->blend = 1;
            }
            put(g, 166 + e->t, ((g->L / 4) >> 1) % 2, 0, 0, e->x, e->y, e->rot, 2);
        }
    }
    if (g->stage == 7) {
        static const int C5[7] = { 0, 0, 1, 2, 3, 4, 5 }, C6[7] = { 0, 0, 2, 3, 4, 5, 0 };
        for (int i = 0; i < 5; i++) {
            const pnj_slot *e = &g->gs[i];
            if (!e->on) continue;
            if (e->dead > 0) { put(g, 201 + MIN(4, e->dead / 3), 0, 0, 0, e->x, e->y, 0, 2); continue; }
            int t = e->t, L = g->L;
            if (t == 7) {
                put(g, 198, 0, 0, 0, e->x, e->y, 0, 2);
                fx a = (fx)sin(RAD((fx)((L % 60) * 3))); if (a < 0) a = 0;
                put(g, 199, 0, 0, 0, e->x, e->y, 0, 2);
                g->draw[g->ndraw - 1].blend = 1; g->draw[g->ndraw - 1].a = (uint8_t)(a * 255);
                continue;
            }
            if (t == 3) put(g, 194, e->st <= 1 && e->st >= 0 ? 0 : e->st == 3 ? ((L / 2) >> 1) % 8 : (L >> 1) % 8, 0, 0, e->x, e->y, 0, 2);
            else if (t == 4) put(g, 196, e->st <= 1 && e->st >= 0 ? 0 : e->st == 3 ? ((L / 2) >> 1) % 6 : (L >> 1) % 6, 0, 0, e->x, e->y, 0, 2);
            else if (t == 5) put(g, 195, e->st == -1 ? 6 : (e->st >= 0 && e->st <= 6 ? C5[e->st] : 0), 0, 0, e->x, e->y, 0, 2);
            else if (t == 6 && e->st > 0) put(g, 197, e->st <= 6 ? C6[e->st] : 0, 0, 0, e->x, e->y, 0, 2);
        }
        for (int i = 0; i < g->nxbul; i++) {
            const pnj_bullet *b = &g->xbul[i];
            if (b->dead > 0) { if (b->dead < 15) { put(g, 201 + MIN(4, b->dead / 3), 0, 0, 0, b->x, b->y, 0, 2); g->draw[g->ndraw - 1].scale = (fx)0.5; } continue; }
            put(g, 200, ((g->L / 4) >> 1) % 4, 0, 0, b->x, b->y, 0, 2);
        }
    }
    if (g->stage == 5) {
        for (int i = 0; i < 4; i++) {
            const pnj_enemy *e = &g->pl[i];
            if (!e->on || e->t < 10) continue;
            int t = e->t, col = 0, row = 0; fx rot = 0;
            if (t != 11 && e->st >= 0 && e->fl == 0 && g->ndraw < PNJ_MAX_DRAW - 1) {
                fx a = (fx)0.5 + (fx)sin(RAD((fx)((g->L % 30) * 6))) * (fx)0.5;
                put(g, 80, 0, 0, 0, e->x, e->y, 0, 2);
                pnj_draw *d = &g->draw[g->ndraw - 1];
                if (g->p.power) { d->r = d->g = d->b = 255; } else if (t == 10) { d->r = 0; d->g = 230; d->b = 230; } else { d->r = 153; d->g = 77; d->b = 230; }
                d->a = (uint8_t)(a > 1 ? 255 : a * 255); d->blend = 1;
            }
            if (e->fl == 0) {
                if (!g->p.power) col = ((g->L / 4) >> 1) % 2 + (t == 10 ? (e->ox >= 160 ? 0 : 2) : (e->x <= e->pvx ? 0 : 2));
                else {
                    col = ((g->L >> 1) % 2) + 4;
                    row = t == 10 ? (e->ox >= 160 ? 0 : 1) : t == 12 ? (e->x <= e->pvx ? 0 : 1) : 0;
                    if (g->p.power_cnt <= 2 && ((g->L >> 1) % 4) < 2) col += 2;
                }
            } else if (e->fl == 2) { col = 8; rot = (fx)((g->L % 360) * 15); }
            else { col = ((g->L >> 1) % 2) + 4; rot = (fx)((e->fl == 3 ? (g->L % 360) : (360 - g->L % 360)) * 15); }
            put(g, 184 + t - 10, col, row, 0, e->x, e->y, rot, 2);
        }
    }
    if (g->stage == 4 && g->me.on) {                              /* Mappy screen-space enemy */
        const pnj_enemy *e = &g->me;
        if (e->t == 8) { if (e->dead == 0) put(g, 179, 0, 0, 0, e->x, e->y, 0, 2); }
        else {
            if (e->dead == 0 && !e->fl && e->st > 0 && g->ndraw < PNJ_MAX_DRAW - 1) {
                fx a = (fx)0.5 + (fx)sin(RAD((fx)((g->L % 30) * 6))) * (fx)0.5;
                put(g, 80, 0, 0, 0, e->x, e->y, 0, 2);
                pnj_draw *d = &g->draw[g->ndraw - 1];
                if (g->p.power) { d->r = d->g = d->b = 255; } else { d->r = 128; d->g = 255; d->b = 128; }
                d->a = (uint8_t)(a > 1 ? 255 : a * 255); d->blend = 1;
            }
            int col = 0; fx rot = 0;
            if (e->fl == 2) rot = (fx)((g->L % 360) * 15);
            else if (e->fl == 3) { col = 4; rot = (fx)((g->L % 360) * 15); }
            else if (e->fl == 4) { col = 5; rot = (fx)((360 - g->L % 360) * 15); }
            else if (e->dead > 0) rot = (fx)((g->L % 360) * 15);
            else col = e->st > 0 ? ((g->L / 3) >> 1) % 4 : 0;
            put(g, 180, col, 0, 0, e->x, e->y, rot, 2);
        }
    }
    {   /* Dig Dug and Druaga are dark: a light disc follows the player */
        const pnj_player *pp = &g->p;
        if (g->stage == 1) { put(g, 206, 0, 0, 0, pp->x, pp->y + off, 90, 3); g->draw[g->ndraw - 1].scale = (fx)3.5; g->draw[g->ndraw - 1].a = 230; }
        if (g->stage == 6) {
            fx pct = 300;
            if (pp->candle) pct = pp->candle_cnt < 10 ? (pp->candle_cnt - 1) * 60 + ((fx)(PNJ_FPS - g->tick578) * 60 / PNJ_FPS) + 300 : 900;
            put(g, 207, 0, 0, 0, pp->x, pp->y + off, 90, 3); g->draw[g->ndraw - 1].scale = pct / 100;
        }
    }
    pnj_player *p = &g->p;
    if (!p->dead) {
        static const int frames[4] = { 0, 1, 2, 1 };
        fx dx = p->x - p->px, dy = p->y - p->py, ang = 0;
        if (fabs((double)dx) > 0.01 || fabs((double)dy) > 0.01) ang = (fx)(atan2((double)dx, (double)-dy) * 180.0 / 3.14159265358979323846);
        int fast = p->vy < 0;
        if (p->lift && p->lift_balloon) {
            int col = p->lift_cnt > 2 ? ((g->L >> 1) % 2) * 2 : frames[(g->L >> (fast ? 1 : 3)) % 4];
            put(g, 119, col, 0, 0, p->x, p->y + off - (54 - (9 - (fx)p->lift_h * (fx)0.3)), 0, 3);
            g->draw[g->ndraw - 1].scale = (fx)(70 + p->lift_h) / 100;
        }
        put(g, 92, frames[(g->L >> 1) % 4], 0, 0, p->x, p->y + off, ang, 3);
        if (p->lift && !p->lift_balloon && (p->lift_cnt > 2 || g->L % 3 == 0)) {
            int right = p->x > p->px;
            int i7 = p->lift_cnt > 2 ? (fast ? (g->L >> 1) % 2 : (g->L >> 2) % 2) : (g->L >> 1) % 2;
            put(g, 122, (right ? 0 : 2) + i7, 0, 0, p->x + (right ? 11 : -11), p->y + off - 32, 0, 3);
        }
    } else if (p->dead_t > 40) {
        put(g, 92, 2, 0, 0, p->x, p->y + off, (fx)(p->dead_t * 30), 3);
    } else {
        int i8 = 9 - p->dead_t / 4; if (i8 < 0) i8 = 0; if (i8 > 9) i8 = 9;
        put(g, 95 + i8, 0, 0, 0, p->x, p->dead_y + off, 0, 3);
    }
    /* sparkles, particle emitters (additive, tinted) and the score banner */
    for (int i = 0; i < g->nspark; i++) {
        const pnj_spark *f = &g->spark[i];
        put(g, 104, 0, 0, 0, f->x, f->y + off, 0, 3);
        pnj_draw *d = &g->draw[g->ndraw - 1]; d->scale = (fx)0.6 + (fx)f->t / 12; d->a = (uint8_t)(255 * (1 - (fx)f->t / 12));
    }
    for (int i = 0; i < 61; i++) {
        const pnj_emitter *e = &g->em[i];
        if (e->age >= e->life) continue;
        for (int j = 0; j < e->n; j++) {
            fx a, sc, x, y, rot = 0;
            if (e->kind == 0) {
                fx f2 = (e->dist / e->life) * 2, f3 = e->ps[j].s0 + e->age * f2;
                if (!(f3 > f2 && f3 < e->dist)) continue;
                a = ((e->dist - f3) / e->dist) * e->col[3]; sc = f3 / 100; x = e->ox; y = e->oy + off;
            } else {
                a = ((fx)(e->life - e->age) / e->life) * e->col[3]; sc = e->ps[j].sc / 100;
                x = e->ox + e->ps[j].x; y = e->oy + e->ps[j].y + off; rot = e->rot * e->age;
            }
            if (a <= 0) continue;
            put(g, e->spr, 0, 0, 0, x, y, rot, 3);
            pnj_draw *d = &g->draw[g->ndraw - 1];
            d->scale = sc; d->blend = 1; d->r = (uint8_t)(e->col[0] * 255); d->g = (uint8_t)(e->col[1] * 255); d->b = (uint8_t)(e->col[2] * 255);
            d->a = (uint8_t)(a > 1 ? 255 : a * 255);
        }
    }
    if (g->banner_on) {
        char num[16]; int n = 0, v = g->tier_shown * 10000; { char tmp[16]; int m = 0; do { tmp[m++] = (char)('0' + v % 10); v /= 10; } while (v); while (m) num[n++] = tmp[--m]; }
        int warm = g->stage <= 3;
        fx cs = cos(RAD((fx)((g->L % 36) * 10))) * 5;
        for (int i = 0; i <= n; i++) {
            if (i < n) put(g, 40 + (num[i] - '0'), 0, 0, 0, g->banner_x + i * 20 + 12, 28 + cs + 16, 0, 4);
            else put(g, 59, 0, 0, 0, g->banner_x + n * 20 + 2 + 26, 35 + cs + 12.5, 0, 4);
            pnj_draw *d = &g->draw[g->ndraw - 1]; d->blend = 1;
            d->r = warm ? 255 : 140; d->g = warm ? 230 : 179; d->b = warm ? 64 : 242; d->a = (uint8_t)(warm ? 178 : 230);
        }
    }
}
