/* stdin: lines "d x y" (down), "m x y" (move), "u" (up), "t" (one 40 Hz tick -> prints tilt) */
#include <stdio.h>
#include "../core/pnj_input.h"
int main(void) {
    pnj_input in = {0}; char c; double x, y;
    while (scanf(" %c", &c) == 1) {
        if (c == 'd') { scanf("%lf %lf", &x, &y); pnj_touch_down(&in, x, y); }
        else if (c == 'm') { scanf("%lf %lf", &x, &y); pnj_touch_move(&in, x, y); }
        else if (c == 'u') pnj_touch_up(&in);
        else if (c == 't') printf("%.17g\n", pnj_input_tilt(&in));
    }
    return 0;
}
