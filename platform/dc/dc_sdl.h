/* A small stand-in for the parts of SDL3 that the shell (platform/sdl/main.c, menus.h, audio.h, pack.h) uses, for the Sega Dreamcast on KallistiOS.
 * KallistiOS has no SDL3, so this file supplies the same names: types, a ring buffer of events, file streams, the audio stream, timers. Drawing is
 * in dc_gfx.h. The controller is read in pnj_dc_pump() and handed to the shell as ordinary gamepad events.
 *
 * Included by main.c instead of <SDL3/SDL.h> when PNJ_DC is defined. Only what the shell calls is here; it is not a general SDL. */
#ifndef PNJ_DC_SDL_H
#define PNJ_DC_SDL_H

#define PNJ_INTERP 1        /* the screen runs at 60 Hz but the game steps at 40: blend the last two steps when drawing (platform/sdl/interp.h) */

#include <kos.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <malloc.h>
#include <math.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

#define SDLCALL
typedef uint8_t Uint8; typedef int8_t Sint8; typedef uint16_t Uint16; typedef int16_t Sint16;
typedef uint32_t Uint32; typedef int32_t Sint32; typedef uint64_t Uint64; typedef int64_t Sint64;
typedef Uint32 SDL_Keycode, SDL_PropertiesID, SDL_JoystickID, SDL_AudioDeviceID, SDL_WindowFlags, SDL_InitFlags;
typedef Uint64 SDL_FingerID, SDL_TouchID;
typedef int SDL_Scancode;
#define SDL_zero(x) memset(&(x), 0, sizeof (x))
#define SDL_free free
#define SDL_strcasecmp strcasecmp
#define SDL_Log(...) (printf(__VA_ARGS__), putchar('\n'))
#define SDL_GetError() ""
#define SDL_ShowSimpleMessageBox(f, t, m, w) (SDL_Log("%s: %s", (t), (m)), 1)
#define SDL_MESSAGEBOX_INFORMATION 0
#define SDL_MESSAGEBOX_ERROR 1
#define SDL_SetHint(a, b) ((void)0)
#define SDL_HINT_ORIENTATIONS ""
#define SDL_HINT_ANDROID_TRAP_BACK_BUTTON ""
#define SDL_HINT_MOUSE_TOUCH_EVENTS ""
#define SDL_HINT_TOUCH_MOUSE_EVENTS ""
#define SDL_INIT_VIDEO 1
#define SDL_INIT_EVENTS 2
#define SDL_INIT_AUDIO 4
#define SDL_INIT_GAMEPAD 8
#define SDL_Init(f) true
#define SDL_InitSubSystem(f) true
#define SDL_Quit() ((void)0)
#define SDL_WINDOW_FULLSCREEN 1

typedef struct { float x, y, w, h; } SDL_FRect;
typedef struct { int x, y, w, h; } SDL_Rect;
enum { SDL_BLENDMODE_BLEND = 1, SDL_BLENDMODE_ADD = 2 };
enum { SDL_SCALEMODE_NEAREST = 0, SDL_SCALEMODE_LINEAR = 1 };

/* ---------- time ---------- */
static Uint64 SDL_GetTicks(void) { return timer_ms_gettime64(); }
static Uint64 SDL_GetPerformanceCounter(void) { return timer_ns_gettime64(); }
static Uint64 SDL_GetPerformanceFrequency(void) { return 1000000000ULL; }
static void dc_audio_poll(void);
static void SDL_Delay(Uint32 ms) { dc_audio_poll(); thd_sleep(ms); }

/* ---------- threads: the importer is not on this system ---------- */
typedef struct SDL_Thread SDL_Thread;
static void SDL_WaitThread(SDL_Thread *t, int *status) { (void)t; (void)status; }

/* ---------- events ---------- */
enum {
    SDL_EVENT_QUIT = 0x100, SDL_EVENT_WILL_ENTER_BACKGROUND = 0x102, SDL_EVENT_DID_ENTER_FOREGROUND = 0x104,
    SDL_EVENT_WINDOW_FIRST = 0x202, SDL_EVENT_WINDOW_MINIMIZED = 0x205, SDL_EVENT_WINDOW_RESTORED = 0x207,
    SDL_EVENT_WINDOW_FOCUS_GAINED = 0x20e, SDL_EVENT_WINDOW_FOCUS_LOST = 0x20f, SDL_EVENT_WINDOW_LAST = 0x21f,
    SDL_EVENT_KEY_DOWN = 0x300, SDL_EVENT_KEY_UP = 0x301,
    SDL_EVENT_MOUSE_MOTION = 0x400, SDL_EVENT_MOUSE_BUTTON_DOWN = 0x401, SDL_EVENT_MOUSE_BUTTON_UP = 0x402,
    SDL_EVENT_GAMEPAD_AXIS_MOTION = 0x650, SDL_EVENT_GAMEPAD_BUTTON_DOWN = 0x651, SDL_EVENT_GAMEPAD_BUTTON_UP = 0x652, SDL_EVENT_GAMEPAD_ADDED = 0x653,
    SDL_EVENT_FINGER_DOWN = 0x700, SDL_EVENT_FINGER_UP = 0x701, SDL_EVENT_FINGER_MOTION = 0x702,
    SDL_EVENT_DROP_FILE = 0x1000
};
typedef struct { Uint32 type; SDL_Keycode key; Uint16 mod; bool down, repeat; } SDL_KeyboardEvent;
typedef struct { Uint32 type; SDL_JoystickID which; } SDL_GamepadDeviceEvent;
typedef struct { Uint32 type; SDL_JoystickID which; Uint8 button; bool down; } SDL_GamepadButtonEvent;
typedef struct { Uint32 type; SDL_JoystickID which; Uint8 axis; Sint16 value; } SDL_GamepadAxisEvent;
typedef struct { Uint32 type; SDL_TouchID touchID; SDL_FingerID fingerID; float x, y, dx, dy, pressure; } SDL_TouchFingerEvent;
typedef struct { Uint32 type; Uint32 which; float x, y; } SDL_MouseMotionEvent;
typedef struct { Uint32 type; Uint32 which; Uint8 button; bool down; float x, y; } SDL_MouseButtonEvent;
typedef struct { Uint32 type; const char *data; } SDL_DropEvent;
typedef union SDL_Event {
    Uint32 type; SDL_KeyboardEvent key; SDL_GamepadDeviceEvent gdevice; SDL_GamepadButtonEvent gbutton; SDL_GamepadAxisEvent gaxis;
    SDL_TouchFingerEvent tfinger; SDL_MouseMotionEvent motion; SDL_MouseButtonEvent button; SDL_DropEvent drop; Uint8 pad[64];
} SDL_Event;
#define SDL_TOUCH_MOUSEID ((Uint32)-1)
typedef enum { SDL_TOUCH_DEVICE_INVALID = -1, SDL_TOUCH_DEVICE_DIRECT, SDL_TOUCH_DEVICE_INDIRECT_ABSOLUTE, SDL_TOUCH_DEVICE_INDIRECT_RELATIVE } SDL_TouchDeviceType;
#define SDL_GetTouchDeviceType(id) SDL_TOUCH_DEVICE_DIRECT
#define SDL_OpenGamepad(id) ((void)0)

#define DC_NEVENTS 64
static SDL_Event dc_evq[DC_NEVENTS]; static unsigned dc_evh, dc_evt;
static bool SDL_PushEvent(SDL_Event *e) { if (dc_evt - dc_evh >= DC_NEVENTS) return false; dc_evq[dc_evt++ % DC_NEVENTS] = *e; return true; }
static bool SDL_PollEvent(SDL_Event *e) { if (dc_evh == dc_evt) return false; *e = dc_evq[dc_evh++ % DC_NEVENTS]; return true; }

/* ---------- keyboard: the menus ask about keys; there is no keyboard handling here, so every answer is "not pressed" ---------- */
enum {
    SDLK_RETURN = '\r', SDLK_ESCAPE = 0x1b, SDLK_SPACE = ' ', SDLK_1 = '1', SDLK_8 = '8',
    SDLK_A = 'a', SDLK_D = 'd', SDLK_P = 'p', SDLK_R = 'r', SDLK_S = 's', SDLK_W = 'w',
    SDLK_LEFT = 0x40000050, SDLK_RIGHT = 0x4000004f, SDLK_UP = 0x40000052, SDLK_DOWN = 0x40000051,
    SDLK_F11 = 0x40000044, SDLK_KP_ENTER = 0x40000058, SDLK_CAPSLOCK = 0x40000039, SDLK_SCROLLLOCK = 0x40000047, SDLK_NUMLOCKCLEAR = 0x40000053,
    SDLK_LCTRL = 0x400000e0, SDLK_RGUI = 0x400000e7, SDLK_AC_BACK = 0x40000118, SDLK_VOLUMEUP = 0x40000080, SDLK_VOLUMEDOWN = 0x40000081,
    SDLK_MUTE = 0x4000007f, SDLK_POWER = 0x40000066
};
#define SDL_KMOD_CTRL 0x00c0
#define SDL_KMOD_ALT 0x0300
#define SDL_KMOD_GUI 0xc000
#define SDL_SCANCODE_UNKNOWN 0
static const bool *SDL_GetKeyboardState(int *n) { static const bool none[512]; if (n) *n = 512; return none; }
#define SDL_GetScancodeFromKey(k, m) SDL_SCANCODE_UNKNOWN
#define SDL_GetKeyName(k) "KEY"

/* ---------- gamepad (SDL3 numbering, so saved bindings mean the same thing everywhere) ---------- */
enum {
    SDL_GAMEPAD_BUTTON_SOUTH = 0, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE,
    SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_COUNT = 26
};
enum { SDL_GAMEPAD_AXIS_LEFTX = 0, SDL_GAMEPAD_AXIS_LEFTY };

/* ---------- file streams (stdio underneath, or a block of memory) ---------- */
enum { SDL_IO_SEEK_SET = 0 };
typedef struct SDL_IOStream { FILE *f; const unsigned char *mem; Sint64 n, pos; } SDL_IOStream;
static SDL_IOStream *SDL_IOFromFile(const char *path, const char *mode) {
    FILE *f = fopen(path, mode); if (!f) return NULL;
    SDL_IOStream *s = (SDL_IOStream *)calloc(1, sizeof *s); if (!s) { fclose(f); return NULL; }
    s->f = f; return s;
}
static SDL_IOStream *SDL_IOFromConstMem(const void *mem, size_t n) {
    SDL_IOStream *s = (SDL_IOStream *)calloc(1, sizeof *s); if (!s) return NULL;
    s->mem = (const unsigned char *)mem; s->n = (Sint64)n; return s;
}
static Sint64 SDL_GetIOSize(SDL_IOStream *s) {
    if (!s->f) return s->n;
    long cur = ftell(s->f); fseek(s->f, 0, SEEK_END); long n = ftell(s->f); fseek(s->f, cur, SEEK_SET); return n;
}
static size_t SDL_ReadIO(SDL_IOStream *s, void *buf, size_t n) {
    if (s->f) return fread(buf, 1, n, s->f);
    if (s->pos + (Sint64)n > s->n) n = (size_t)(s->n - s->pos);
    memcpy(buf, s->mem + s->pos, n); s->pos += (Sint64)n; return n;
}
static Sint64 SDL_SeekIO(SDL_IOStream *s, Sint64 off, int whence) {
    (void)whence;
    if (s->f) return fseek(s->f, (long)off, SEEK_SET) == 0 ? off : -1;
    if (off < 0 || off > s->n) return -1;
    s->pos = off; return off;
}
static bool SDL_CloseIO(SDL_IOStream *s) { if (s->f) fclose(s->f); free(s); return true; }
static bool SDL_ReadU16LE(SDL_IOStream *s, Uint16 *v) { unsigned char b[2]; if (SDL_ReadIO(s, b, 2) != 2) return false; *v = (Uint16)(b[0] | (b[1] << 8)); return true; }
static bool SDL_ReadU32LE(SDL_IOStream *s, Uint32 *v) { unsigned char b[4]; if (SDL_ReadIO(s, b, 4) != 4) return false; *v = (Uint32)b[0] | ((Uint32)b[1] << 8) | ((Uint32)b[2] << 16) | ((Uint32)b[3] << 24); return true; }

/* ---------- audio ---------- */
#define SDL_AUDIO_S16LE 0x8010
#define SDL_AUDIO_S16 SDL_AUDIO_S16LE
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK 0xffffffffu
typedef struct { int format; int channels; int freq; } SDL_AudioSpec;

static void dc_skip(SDL_IOStream *io, Uint32 n) { unsigned char t[64]; while (n) { Uint32 k = n < sizeof t ? n : (Uint32)sizeof t; if (SDL_ReadIO(io, t, k) != k) return; n -= k; } }
/* A WAV file (16-bit PCM only) as raw samples. closeio: close the stream afterwards. */
static bool SDL_LoadWAV_IO(SDL_IOStream *io, bool closeio, SDL_AudioSpec *spec, Uint8 **buf, Uint32 *len) {
    bool ok = false, fmt_ok = false; unsigned char h[12];
    *buf = NULL; *len = 0;
    if (!io) return false;
    if (SDL_ReadIO(io, h, 12) == 12 && !memcmp(h, "RIFF", 4) && !memcmp(h + 8, "WAVE", 4)) {
        for (;;) {
            unsigned char ch[8]; Uint32 sz;
            if (SDL_ReadIO(io, ch, 8) != 8) break;
            sz = (Uint32)ch[4] | ((Uint32)ch[5] << 8) | ((Uint32)ch[6] << 16) | ((Uint32)ch[7] << 24);
            if (!memcmp(ch, "fmt ", 4) && sz >= 16) {
                unsigned char f[16]; if (SDL_ReadIO(io, f, 16) != 16) break;
                int tag = f[0] | (f[1] << 8), bits = f[14] | (f[15] << 8);
                spec->channels = f[2] | (f[3] << 8); spec->freq = f[4] | (f[5] << 8) | (f[6] << 16) | (f[7] << 24); spec->format = SDL_AUDIO_S16LE;
                fmt_ok = (tag == 1 && bits == 16 && spec->channels >= 1 && spec->channels <= 2);
                dc_skip(io, sz - 16 + (sz & 1));
            } else if (!memcmp(ch, "data", 4)) {
                if (!fmt_ok) break;
                Uint8 *b = (Uint8 *)malloc(sz ? sz : 1);
                if (b && SDL_ReadIO(io, b, sz) == sz) { *buf = b; *len = sz; ok = true; } else free(b);
                break;
            } else dc_skip(io, sz + (sz & 1));
        }
    }
    if (closeio) SDL_CloseIO(io);
    return ok;
}
/* 16-bit PCM to 16-bit stereo at another rate (linear). */
static bool SDL_ConvertAudioSamples(const SDL_AudioSpec *sp, const Uint8 *src, int slen, const SDL_AudioSpec *dp, Uint8 **dst, int *dlen) {
    *dst = NULL; *dlen = 0;
    if (sp->format != SDL_AUDIO_S16LE || dp->format != SDL_AUDIO_S16LE || sp->channels < 1 || sp->channels > 2 || dp->channels != 2 || sp->freq < 1) return false;
    int sch = sp->channels, frames = slen / (2 * sch);
    if (frames < 1) return false;
    int of = (int)((double)frames * dp->freq / sp->freq); if (of < 1) of = 1;
    short *o = (short *)malloc((size_t)of * 4); if (!o) return false;
    const short *in = (const short *)src;
    for (int i = 0; i < of; i++) {
        double pos = (double)i * sp->freq / dp->freq; int i0 = (int)pos, i1 = i0 + 1 < frames ? i0 + 1 : i0; double f = pos - i0;
        for (int c = 0; c < 2; c++) {
            int cc = sch == 1 ? 0 : c; double s0 = in[i0 * sch + cc], s1 = in[i1 * sch + cc];
            o[i * 2 + c] = (short)(s0 + (s1 - s0) * f);
        }
    }
    *dst = (Uint8 *)o; *dlen = of * 4; return true;
}

/* The mixer in audio.h writes into an "audio stream" from a callback. Here the callback is run from the main loop (dc_audio_poll), which is also
 * when the KallistiOS stream asks for more data, so the mixer never runs on another thread and the lock calls do nothing.
 * The buffer size is the delay before a sound effect is heard: 16 KB of 44.1 kHz stereo is about 93 ms for the whole buffer, refilled in halves. */
#define DC_AUD_BUF (16 * 1024)
typedef struct SDL_AudioStream SDL_AudioStream;
typedef void (*SDL_AudioStreamCallback)(void *ud, SDL_AudioStream *st, int additional, int total);
struct SDL_AudioStream {
    SDL_AudioStreamCallback cb; void *ud; snd_stream_hnd_t h; int run, fill;
    unsigned char buf[DC_AUD_BUF] __attribute__((aligned(32)));
};
static SDL_AudioStream dc_aud;
static void *dc_snd_get(snd_stream_hnd_t h, int req, int *got) {          /* req and *got are in bytes */
    SDL_AudioStream *s = &dc_aud; (void)h;
    if (req > (int)sizeof s->buf) req = (int)sizeof s->buf;
    req &= ~3; s->fill = 0;
    if (s->run && s->cb) s->cb(s->ud, s, req, req);
    if (s->fill < req) { memset(s->buf + s->fill, 0, (size_t)(req - s->fill)); s->fill = req; }
    *got = s->fill; return s->buf;
}
static bool SDL_PutAudioStreamData(SDL_AudioStream *s, const void *data, int n) {
    if (s->fill + n > (int)sizeof s->buf) n = (int)sizeof s->buf - s->fill;
    if (n > 0) { memcpy(s->buf + s->fill, data, (size_t)n); s->fill += n; }
    return true;
}
static SDL_AudioStream *SDL_OpenAudioDeviceStream(SDL_AudioDeviceID dev, const SDL_AudioSpec *spec, SDL_AudioStreamCallback cb, void *ud) {
    (void)dev; (void)spec;
    SDL_AudioStream *s = &dc_aud;
    if (snd_stream_init() < 0) return NULL;
    s->cb = cb; s->ud = ud; s->run = 0; s->fill = 0;
    s->h = snd_stream_alloc(dc_snd_get, DC_AUD_BUF);
    if (s->h == SND_STREAM_INVALID) return NULL;
    snd_stream_start(s->h, 44100, 1);                    /* the stream runs from now on; "paused" just means it is fed silence */
    return s;
}
static bool SDL_ResumeAudioStreamDevice(SDL_AudioStream *s) { s->run = 1; return true; }
static bool SDL_PauseAudioStreamDevice(SDL_AudioStream *s) { s->run = 0; return true; }
#define SDL_LockAudioStream(s) true
#define SDL_UnlockAudioStream(s) true
#define SDL_GetAudioStreamDevice(s) 1u
#define SDL_GetCurrentAudioDriver() "kos"
static void dc_audio_poll(void) { if (dc_aud.cb) snd_stream_poll(dc_aud.h); }
/* A loading screen blocks the main loop for a while, and nothing refills the stream meanwhile: it would play the last 93 ms over and over. Stop it for the
 * wait and start it again afterwards (it asks the mixer for fresh data when it starts). */
static void dc_audio_hush(void) { if (dc_aud.cb) snd_stream_stop(dc_aud.h); }
static void dc_audio_unhush(void) { if (dc_aud.cb) snd_stream_start(dc_aud.h, 44100, 1); }

#include "dc_gfx.h"

/* ---------- controller -> gamepad events ---------- */
static void pnj_push_button(int b, int down) {
    SDL_Event e; SDL_zero(e); e.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
    e.gbutton.which = 1; e.gbutton.button = (Uint8)b; e.gbutton.down = down ? true : false; SDL_PushEvent(&e);
}
static void pnj_push_axis(int axis, int v) {
    SDL_Event e; SDL_zero(e); e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION; e.gaxis.which = 1; e.gaxis.axis = (Uint8)axis; e.gaxis.value = (Sint16)v; SDL_PushEvent(&e);
}
/* Once per loop: keep the audio fed, then read the first controller. A, B, X, Y and Start together quit (the usual Dreamcast reset). */
static void pnj_dc_pump(void) {
    static const struct { uint32_t k; int b; } map[] = {
        { CONT_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_UP }, { CONT_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN }, { CONT_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT },
        { CONT_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT }, { CONT_A, SDL_GAMEPAD_BUTTON_SOUTH }, { CONT_B, SDL_GAMEPAD_BUTTON_EAST },
        { CONT_X, SDL_GAMEPAD_BUTTON_WEST }, { CONT_Y, SDL_GAMEPAD_BUTTON_NORTH }, { CONT_START, SDL_GAMEPAD_BUTTON_START } };
    static uint32_t prev; static int pl, pr, lx, ly;
    dc_audio_poll();
    maple_device_t *d = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = d ? (cont_state_t *)maple_dev_status(d) : NULL;
    uint32_t b = st ? st->buttons : 0;
    if ((b & CONT_RESET_BUTTONS) == CONT_RESET_BUTTONS) { SDL_Event q; SDL_zero(q); q.type = SDL_EVENT_QUIT; SDL_PushEvent(&q); return; }
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++) { uint32_t m = map[i].k; if ((b & m) != (prev & m)) pnj_push_button(map[i].b, (b & m) != 0); }
    prev = b;
    int l = st && st->ltrig > 128, r = st && st->rtrig > 128;
    if (l != pl) { pl = l; pnj_push_button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, l); }
    if (r != pr) { pr = r; pnj_push_button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, r); }
    int jx = st ? st->joyx : 0, jy = st ? st->joyy : 0;                  /* -128..127, y is down-positive like SDL */
    int nx = abs(jx) > 24 ? jx * 256 : 0, ny = abs(jy) > 24 ? jy * 256 : 0;
    if (nx != lx) { lx = nx; pnj_push_axis(SDL_GAMEPAD_AXIS_LEFTX, nx); }
    if (ny != ly) { ly = ny; pnj_push_axis(SDL_GAMEPAD_AXIS_LEFTY, ny); }
}

#endif
