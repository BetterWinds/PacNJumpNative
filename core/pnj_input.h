/* Portable input model (original control mode 2 analog + mode 3 digital ramp, spec section 16).
 * Coordinates are logical game pixels (320x480). No SDL dependency, so every backend can reuse it. */
#ifndef PNJ_INPUT_H
#define PNJ_INPUT_H
#include <math.h>

/* Analog (orig mode 2): a touch on the slide bar (centre 160,432) uses the bar centre as origin; a touch anywhere
 * else with y <= 400 is a floating stick anchored at the touch point that trails the finger by (d-60)/2 per step
 * once the finger is 60+ px away (2D distance). tilt = dx/300 clamped to +-0.5, 0 on release. A touch below y 400
 * that is not on the bar is ignored. Digital keys use the original ramp. */
typedef struct { int left, right, snappy; double ramp; int active, bar; double ax, ay, x, y; } pnj_input;
#define PNJ_BAR_X 160.0
#define PNJ_BAR_Y 432.0

static inline void pnj_touch_down(pnj_input *in, double lx, double ly) {
    int on_bar = fabs(lx - PNJ_BAR_X) <= 110 && fabs(ly - PNJ_BAR_Y) <= 30;
    if (!on_bar && ly > 400) return;
    in->active = 1; in->bar = on_bar; in->x = lx; in->y = ly;
    in->ax = on_bar ? PNJ_BAR_X : lx; in->ay = on_bar ? PNJ_BAR_Y : ly;
}
static inline void pnj_touch_move(pnj_input *in, double lx, double ly) { if (in->active) { in->x = lx; in->y = ly; } }
static inline void pnj_touch_up(pnj_input *in) { in->active = 0; }

/* call once per 40 Hz step */
static inline double pnj_input_tilt(pnj_input *in) {
    if (in->active) {
        double dx = in->x - in->ax, dy = in->y - in->ay, d = hypot(dx, dy);
        double v = dx / 300.0; v = v > 0.5 ? 0.5 : (v < -0.5 ? -0.5 : v);   /* uses the pre-move anchor, like the JS */
        if (!in->bar && d >= 60) { in->ax += dx / d * ((d - 60) / 2); in->ay += dy / d * ((d - 60) / 2); }
        return v;
    }
    int dir = in->right - in->left;
    if (dir) {                /* digital: +0.0125/step up to 0.5, restart from 0 on direction change */
        if ((dir > 0 && in->ramp < 0) || (dir < 0 && in->ramp > 0)) in->ramp = 0;
        in->ramp += dir * (in->snappy ? 0.05 : 0.0125);
        if (in->ramp > 0.5) in->ramp = 0.5;
        if (in->ramp < -0.5) in->ramp = -0.5;
    } else {
        double dn = in->snappy ? 0.06 : 0.015;
        if (in->ramp > 0) { in->ramp -= dn; if (in->ramp < 0) in->ramp = 0; }
        else if (in->ramp < 0) { in->ramp += dn; if (in->ramp > 0) in->ramp = 0; }
    }
    return in->ramp;
}
#endif
