/* Reads "seed stage n" then n tilt values on stdin; prints one JSON line per step. */
#include <stdio.h>
#include <stdlib.h>
#include "../core/pnj_core.h"
static pnj_game G;
int main(void) {
    unsigned seed; int stage, n, level;
    if (scanf("%u %d %d %d", &seed, &stage, &n, &level) != 4) return 1;
    pnj_new(&G, seed, stage); G.start_level = level; G.zero_screen_tilt = 1;
    for (int i = 0; i < n; i++) {
        double t; if (scanf("%lf", &t) != 1) return 1;
        pnj_step(&G, (fx)t);
        if (G.overflow) { fprintf(stderr, "array overflow at step %d\n", i + 1); break; }
        double sx = 0, sy = 0; int na = 0;
        for (int k = 0; k < G.nplat; k++) if (G.plats[k].alive) { sx += G.plats[k].x; sy += G.plats[k].y; na++; }
        double gx = 0, gy = 0, ky = 0; int ga = 0, ka = 0;
        for (int k = 0; k < G.nghost; k++) if (G.ghosts[k].alive) { gx += G.ghosts[k].x; gy += G.ghosts[k].y; ga++; }
        for (int k = 0; k < G.npick; k++) if (G.picks[k].alive) { ky += G.picks[k].y; ka++; }
        printf("[%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%u,%d,%d,%.17g,%.17g,%d,%d,%d,%d,%d,%.17g,%.17g,%d,%.17g]\n",
               G.L, (double)G.p.x, (double)G.p.y, (double)G.p.vx, (double)G.p.vy,
               (double)G.cam593, (double)G.cam594, (double)G.cam595, (double)G.peak,
               G.seed, G.p.dead, na, sx, sy, G.npick, G.nghost, G.p.power, G.p.boost, ga, gx, gy, ka, ky);
    }
    return 0;
}
