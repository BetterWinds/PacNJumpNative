/* Level maps. The 42 hand-made pickup layouts are not part of this repository: the normal build reads them from pnj.pack at runtime.
 * For the personal/parity build, run tools/gen_maps.py on your own APK; that writes pnj_maps_data.h and this file picks it up. */
#ifndef PNJ_MAPS_H
#define PNJ_MAPS_H
#include <stdint.h>
#include "pnj_core.h"
#if defined(PNJ_BYO) || !defined(__has_include)
#define PNJ_MAPS ((const pnj_map *)0)
#define PNJ_NMAPS 0
#elif __has_include("pnj_maps_data.h")
#include "pnj_maps_data.h"
#else
#define PNJ_MAPS ((const pnj_map *)0)
#define PNJ_NMAPS 0
#endif
#endif
