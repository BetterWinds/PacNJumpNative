/* Render-time interpolation. The simulation runs at a fixed 40 Hz; a 60 Hz screen would show some steps twice and the motion judders.
 * After every simulation step the draw list is built once and kept; the last two are blended by how far the clock is between them
 * (the leftover time in the step accumulator). Only positions, rotation and the camera are blended; frames, tints and sprites come from
 * the newer list. The simulation itself is untouched, so input, physics and replays behave exactly as at 40 Hz. */
#ifndef PNJ_INTERP_H
#define PNJ_INTERP_H
static pnj_draw ID_p[PNJ_MAX_DRAW], ID_c[PNJ_MAX_DRAW];
static int ID_np, ID_nc, ID_ok;
static fx ID_camp, ID_camc;
#define ID_NEAR 48.0f           /* a sprite further than this from its old place is a new one or a wrap-around: no blending */
#define ID_WIN 4                /* how far from the same index in the older list to look for the matching sprite */

static void interp_snap(pnj_game *g) {
    memcpy(ID_p, ID_c, (size_t)ID_nc * sizeof *ID_c); ID_np = ID_nc; ID_camp = ID_camc;
    pnj_build_draw(g);
    memcpy(ID_c, g->draw, (size_t)g->ndraw * sizeof *ID_c); ID_nc = g->ndraw; ID_camc = g->cam594;
    if (!ID_ok) { memcpy(ID_p, ID_c, (size_t)ID_nc * sizeof *ID_c); ID_np = ID_nc; ID_camp = ID_camc; ID_ok = 1; }
}
static void interp_reset(pnj_game *g) { ID_ok = 0; interp_snap(g); }
static float interp_cam(float t) {
    float d = (float)(ID_camc - ID_camp);
    return (float)ID_camp + (d > 150.0f || d < -150.0f ? d : d * t);
}
/* fills g->draw with the list as it looks t (0..1) of the way from the older step to the newer one */
static void interp_blend(pnj_game *g, float t) {
    static uint8_t used[PNJ_MAX_DRAW];
    int n = ID_nc; memset(used, 0, (size_t)ID_np); memcpy(g->draw, ID_c, (size_t)n * sizeof *ID_c); g->ndraw = n;
    if (t >= 0.999f) return;
    for (int i = 0; i < n; i++) {
        pnj_draw *d = &g->draw[i]; const pnj_draw *best = 0; int bj = -1; float bd = ID_NEAR * ID_NEAR;
        for (int k = -ID_WIN; k <= ID_WIN; k++) {
            int j = i + k; if (j < 0 || j >= ID_np) continue;
            const pnj_draw *o = &ID_p[j];
            if (used[j] || o->sprite != d->sprite || o->layer != d->layer) continue;
            float dx = (float)(d->x - o->x), dy = (float)(d->y - o->y), dd = dx * dx + dy * dy;
            if (dd < bd) { bd = dd; best = o; bj = j; }
        }
        if (!best) continue;
        used[bj] = 1;
        d->x = (fx)((float)best->x + (float)(d->x - best->x) * t);
        d->y = (fx)((float)best->y + (float)(d->y - best->y) * t);
        { float dr = (float)(d->rot - best->rot); dr -= 360.0f * floorf(dr / 360.0f + 0.5f);           /* the short way round */
          if (dr > -90.0f && dr < 90.0f) d->rot = (fx)((float)best->rot + dr * t); }
    }
}
#endif
