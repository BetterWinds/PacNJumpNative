/* Sprite rectangles. The table itself is game data: it is read from meta/atlas.txt in the pack (tools/dexatlas.py extracts it from your APK).
 * One sprite per line: "tex x y w h tw". tex indexes PNJ_TEXTURES, tw is the width of the sheet the rectangle was authored for. */
#ifndef PNJ_ATLAS_H
#define PNJ_ATLAS_H
#include <stdint.h>
typedef struct { int16_t tex, x, y, w, h, tw; } pnj_sprite;
static const char *const PNJ_TEXTURES[] = {"bnglogo","bndr_logo","sysparts","stages","charaparts","stage_1_map","stage_1_cha","stage_2_map","stage_2_cha","stage_3_map","stage_3_cha","stage_4_map","stage_4_cha","stage_5_map","stage_5_cha","stage_6_map","stage_6_cha","stage_7_map","stage_7_cha","stage_8_map","stage_8_cha"};
#define PNJ_NTEX 21
static pnj_sprite *PNJ_SPRITES; static int PNJ_NSPRITES;
static const pnj_sprite NO_SPRITE = { 0, 0, 0, 1, 1, 512 };
static const pnj_sprite *spr_at(int id) { return id >= 0 && id < PNJ_NSPRITES ? &PNJ_SPRITES[id] : &NO_SPRITE; }
static int atlas_parse(const char *txt, size_t n) {
    int cap = 256, cnt = 0; pnj_sprite *a = (pnj_sprite *)malloc((size_t)cap * sizeof *a); if (!a) return 0;
    const char *p = txt, *e = txt + n;
    while (p < e) {
        int v[6], k = 0; char *q;
        while (k < 6) { while (p < e && (*p == ' ' || *p == '\t')) p++; if (p >= e || *p == '\n' || *p == '\r') break; v[k] = (int)strtol(p, &q, 10); if (q == p) break; p = q; k++; }
        while (p < e && *p != '\n') p++;
        if (p < e) p++;
        if (k != 6) continue;
        if (v[0] < 0 || v[0] >= PNJ_NTEX || v[5] <= 0) continue;
        if (cnt == cap) { cap *= 2; pnj_sprite *b = (pnj_sprite *)realloc(a, (size_t)cap * sizeof *a); if (!b) { free(a); return 0; } a = b; }
        a[cnt].tex = (int16_t)v[0]; a[cnt].x = (int16_t)v[1]; a[cnt].y = (int16_t)v[2]; a[cnt].w = (int16_t)v[3]; a[cnt].h = (int16_t)v[4]; a[cnt].tw = (int16_t)v[5]; cnt++;
    }
    free(PNJ_SPRITES); PNJ_SPRITES = a; PNJ_NSPRITES = cnt;
    return cnt;
}
#endif
