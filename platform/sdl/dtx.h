/* Dreamcast texture files ("dc/<sheet>.dtx" in the pack). The console decodes nothing at run time: the file is the sheet already halved to the size it is
 * drawn at and packed to 16 bits, and the loader only has to copy it into video memory.
 *   bytes 0..3  "DTX1"      4..5  width (u16 LE)      6..7  height (u16 LE)      then width * height ARGB4444 values (u16 LE), top row first
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
#endif
