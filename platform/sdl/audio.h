/* Small mixer for the SDL shell: 44.1 kHz stereo s16, one streamed ogg music track + up to 16 sfx voices.
 * Included by main.c (single translation unit). Files live in assets/audio/: sfx as 44.1k stereo wav, music as ogg. */
#define STB_VORBIS_NO_STDIO
#include "stb_vorbis.c"
#ifdef L
#undef L
#endif
#ifdef R
#undef R
#endif
#ifdef C
#undef C
#endif

typedef struct { Uint8 *buf; Uint32 len; } sfx_buf;
typedef struct { const sfx_buf *s; Uint32 pos; } voice;

static SDL_AudioStream *aud_stream; static SDL_AudioDeviceID aud_dev;
static sfx_buf SFXB[32];
static voice voices[16];
static stb_vorbis *mus_vb; static char mus_name[32], mus_want[32];
static short *mus_pcm; static Uint32 mus_frames, mus_pos;      /* a track stored as WAV (the web builder decodes the AAC jingle to this) */
static int sfx_on = 1, mus_on = 1;
static float mus_vol = 0.55f, sfx_vol = 0.8f;

enum { A_SELECT = 24, A_DECIDE, A_CANCEL, A_GAMEOVER, A_HISCORE, A_EAT_ITEM, A_WARN };

/* decode one of the APK's own ogg effects and convert it to the mixer format (44.1 kHz stereo s16, linear resample) */
static int audio_load_sfx_ogg(int slot, const char *file) {
    char name[64]; snprintf(name, sizeof name, "audio/%s.ogg", file);
    size_t n; unsigned char *d = read_asset(name, &n); if (!d) return 0;
    int ch = 0, rate = 0; short *pcm = 0;
    int frames = stb_vorbis_decode_memory(d, (int)n, &ch, &rate, &pcm); free(d);
    if (frames <= 0 || !pcm || ch < 1 || ch > 2 || rate < 4000) { free(pcm); return 0; }
    Uint32 outf = (Uint32)((double)frames * 44100.0 / rate);
    short *o = (short *)malloc((size_t)outf * 4); if (!o) { free(pcm); return 0; }
    for (Uint32 i = 0; i < outf; i++) {
        double pos = (double)i * rate / 44100.0; int i0 = (int)pos, i1 = i0 + 1 < frames ? i0 + 1 : i0; double f = pos - i0;
        for (int c = 0; c < 2; c++) {
            int cc = ch == 1 ? 0 : c; double s0 = pcm[i0 * ch + cc], s1 = pcm[i1 * ch + cc];
            o[i * 2 + c] = (short)(s0 + (s1 - s0) * f);
        }
    }
    free(pcm); SFXB[slot].buf = (Uint8 *)o; SFXB[slot].len = outf * 4; return 1;
}
static void audio_load_sfx(int slot, const char *file) {
    if (PKN && audio_load_sfx_ogg(slot, file)) return;
    char name[64]; snprintf(name, sizeof name, "audio/%s.wav", file);
    size_t n; unsigned char *d = read_asset(name, &n);
    if (!d) return;
    SDL_AudioSpec spec; Uint8 *buf = 0; Uint32 len = 0;
    SDL_IOStream *rw = SDL_IOFromConstMem(d, n);
    if (SDL_LoadWAV_IO(rw, true, &spec, &buf, &len) && spec.freq == 44100 && spec.channels == 2 && spec.format == SDL_AUDIO_S16LE) {
        SFXB[slot].buf = buf; SFXB[slot].len = len;
    } else if (buf) SDL_free(buf);
    free(d);
}

/* Music tracks are cached (raw ogg bytes, or decoded PCM for a wav track) so switching to the power-up jingle and back
 * does not hit the pack file or the decoder mid-game. Reading and decoding happen with the audio NOT locked; only the
 * pointer swap is under the lock, so the audio callback never waits on slow SD reads (the 3DS power-up stutter). */
typedef struct { char name[32]; unsigned char *ogg; size_t n; short *pcm; Uint32 frames; int pin; } mus_ent;
static void (*boot_cb)(const char *what);                    /* set while a loading screen is up (Dreamcast): called after each piece of audio is loaded */
static int mus_pinned(const char *t) { return !strcmp(t, "bgm1") || !strcmp(t, "wakawaka"); }    /* menu music and the power-up jingle stay in memory, so the menus never read the disc */
static mus_ent MC[4]; static mus_ent *mus_cur; static int mc_next;
static void mus_ent_free(mus_ent *e) { free(e->ogg); SDL_free(e->pcm); memset(e, 0, sizeof *e); }
static mus_ent *mus_find(const char *track) {
    for (int i = 0; i < 4; i++) if (MC[i].name[0] && !strcmp(MC[i].name, track)) return &MC[i];
    return 0;
}
static int mus_fetch(const char *track, mus_ent *o) {        /* slow part: file read + decode, no lock held */
    char name[64]; size_t n; memset(o, 0, sizeof *o);
    snprintf(name, sizeof name, "audio/%s.ogg", track);
    unsigned char *d = read_asset(name, &n);
    if (d) { o->ogg = d; o->n = n; snprintf(o->name, sizeof o->name, "%s", track); return 1; }
    snprintf(name, sizeof name, "audio/%s.wav", track);
    d = read_asset(name, &n); if (!d) return 0;
    SDL_AudioSpec sp, dst = { SDL_AUDIO_S16, 2, 44100 }; Uint8 *buf = 0, *cb = 0; Uint32 len = 0; int cl = 0, ok = 0;
    if (SDL_LoadWAV_IO(SDL_IOFromConstMem(d, n), true, &sp, &buf, &len) && len > 0 && SDL_ConvertAudioSamples(&sp, buf, (int)len, &dst, &cb, &cl) && cl >= 4) {
        o->pcm = (short *)cb; o->frames = (Uint32)cl / 4; snprintf(o->name, sizeof o->name, "%s", track); ok = 1;
    } else SDL_free(cb);
    if (buf) SDL_free(buf);
    free(d); return ok;
}
static mus_ent *mus_insert(const mus_ent *src) {             /* audio locked by the caller; never evicts the entry that is playing */
    int i = mc_next; for (int k = 0; k < 4; k++, i = (i + 1) & 3) if (&MC[i] != mus_cur && !MC[i].pin) break;
    mc_next = (i + 1) & 3; mus_ent_free(&MC[i]); MC[i] = *src; MC[i].pin = mus_pinned(src->name); return &MC[i];
}
static void audio_music_stop(void) {                         /* audio locked by the caller */
    if (mus_vb) { stb_vorbis_close(mus_vb); mus_vb = 0; }
    mus_cur = 0; mus_pcm = 0; mus_frames = mus_pos = 0; mus_name[0] = 0;
}
static void audio_music_flush(void) {                        /* a new pack arrived: forget everything cached */
    if (aud_stream) SDL_LockAudioStream(aud_stream);
    audio_music_stop(); for (int i = 0; i < 4; i++) mus_ent_free(&MC[i]);
    if (aud_stream) SDL_UnlockAudioStream(aud_stream);
}
/* a track is audio/<name>.ogg, or audio/<name>.wav. If it is not in the pack the current music keeps playing. */
static void audio_music_open(const char *track) {            /* main thread only; call with the audio NOT locked */
    if (!track[0]) { SDL_LockAudioStream(aud_stream); audio_music_stop(); SDL_UnlockAudioStream(aud_stream); return; }
    mus_ent *e = mus_find(track), tmp; stb_vorbis *vb = 0;
    if (!e) { if (!mus_fetch(track, &tmp)) return; }
    const mus_ent *src = e ? e : &tmp;
    if (src->ogg) { int err = 0; vb = stb_vorbis_open_memory(src->ogg, (int)src->n, &err, 0); if (!vb) { if (!e) free(tmp.ogg); return; } }
    SDL_LockAudioStream(aud_stream);
    if (!e) e = mus_insert(&tmp);
    if (mus_vb) { stb_vorbis_close(mus_vb); mus_vb = 0; }
    mus_cur = e; mus_pcm = e->pcm; mus_frames = e->frames; mus_pos = 0; mus_vb = vb;
    snprintf(mus_name, sizeof mus_name, "%s", track);
    SDL_UnlockAudioStream(aud_stream);
}
static void audio_music_prefetch(const char *track) {        /* warm the cache so the first power-up does not hitch */
    if (mus_find(track)) return;
    mus_ent tmp; if (!mus_fetch(track, &tmp)) return;
    SDL_LockAudioStream(aud_stream); mus_insert(&tmp); SDL_UnlockAudioStream(aud_stream);
}

static void audio_cb(void *ud, Uint8 *out, int bytes) {
    (void)ud;
    short *o = (short *)out; int frames = bytes / 4;
    memset(out, 0, (size_t)bytes);
    if (mus_on && mus_pcm && mus_frames) {
        for (int i = 0; i < frames; i++) {
            if (mus_pos >= mus_frames) mus_pos = 0;
            o[i * 2] = (short)(mus_pcm[mus_pos * 2] * mus_vol); o[i * 2 + 1] = (short)(mus_pcm[mus_pos * 2 + 1] * mus_vol); mus_pos++;
        }
    } else if (mus_on && mus_vb) {
        int got = 0;
        while (got < frames) {
            short tmp[2048]; int want = frames - got; if (want > 1024) want = 1024;
            int n = stb_vorbis_get_samples_short_interleaved(mus_vb, 2, tmp, want * 2);
            if (n <= 0) { stb_vorbis_seek_start(mus_vb); n = stb_vorbis_get_samples_short_interleaved(mus_vb, 2, tmp, want * 2); if (n <= 0) break; }
            for (int i = 0; i < n * 2; i++) o[got * 2 + i] = (short)(tmp[i] * mus_vol);
            got += n;
        }
    }
    if (!sfx_on) return;
    for (int v = 0; v < 16; v++) {
        voice *vo = &voices[v];
        if (!vo->s) continue;
        const short *src = (const short *)(vo->s->buf + vo->pos);
        int left = (int)((vo->s->len - vo->pos) / 2), n = left < frames * 2 ? left : frames * 2;
        for (int i = 0; i < n; i++) { int m = o[i] + (int)(src[i] * sfx_vol); o[i] = (short)(m > 32767 ? 32767 : m < -32768 ? -32768 : m); }
        vo->pos += (Uint32)n * 2;
        if (vo->pos >= vo->s->len) vo->s = 0;
    }
}

static void audio_load_all(void) {      /* (re)loads every effect; used at start-up and after a pack import */
    static const char *names[] = { "jump", "break", "mappy_jump", "eat01", "eat02", "eat03", "eat04", "eat05", "eat06", "eat07", "eat08",
        "eat_enemy", "eat_pow", "miss", "bound", "balloon", "candle", "hat" };       /* slots 1.. match PNJ_SFX_* */
    if (aud_stream) SDL_LockAudioStream(aud_stream);
    for (int v = 0; v < 16; v++) voices[v].s = 0;
    for (int i = 0; i < 32; i++) { SDL_free(SFXB[i].buf); SFXB[i].buf = 0; SFXB[i].len = 0; }
    for (int i = 0; i < 18; i++) { audio_load_sfx(i + 1, names[i]); if (boot_cb) boot_cb("Sound effects"); }
    static const struct { int slot; const char *f; } more[] = { { A_SELECT, "select" }, { A_DECIDE, "decide" }, { A_CANCEL, "cancel" },
        { A_GAMEOVER, "gameover" }, { A_HISCORE, "hiscore" }, { A_EAT_ITEM, "eat_item" }, { A_WARN, "st8_warning" } };
    for (int i = 0; i < 7; i++) { audio_load_sfx(more[i].slot, more[i].f); if (boot_cb) boot_cb("Sound effects"); }
    if (aud_stream) SDL_UnlockAudioStream(aud_stream);
    if (aud_stream) { audio_music_prefetch("wakawaka"); if (boot_cb) boot_cb("Music"); audio_music_prefetch("bgm1"); if (boot_cb) boot_cb("Music"); }
}
/* SDL3 audio streams: SDL asks for more data and we feed it from the same mixer callback the SDL2 build used */
static void SDLCALL audio_sdl_cb(void *ud, SDL_AudioStream *st, int additional, int total) {
    (void)ud; (void)total;
    while (additional > 0) {
        Uint8 tmp[4096]; int n = additional < (int)sizeof tmp ? additional : (int)sizeof tmp; n &= ~3; if (!n) break;
        audio_cb(NULL, tmp, n); SDL_PutAudioStreamData(st, tmp, n); additional -= n;
    }
}
static void audio_init(void) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) { SDL_Log("audio: init failed: %s", SDL_GetError()); return; }
    SDL_AudioSpec want = { SDL_AUDIO_S16, 2, 44100 };
    aud_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want, audio_sdl_cb, NULL);
    if (!aud_stream) { SDL_Log("audio: no output device: %s (driver %s)", SDL_GetError(), SDL_GetCurrentAudioDriver()); return; }
    SDL_Log("audio: open, driver %s", SDL_GetCurrentAudioDriver());
    aud_dev = SDL_GetAudioStreamDevice(aud_stream);
    audio_load_all();
    SDL_ResumeAudioStreamDevice(aud_stream);
}

static void audio_play(int slot) {
    if (!aud_stream || slot <= 0 || slot >= 32 || !SFXB[slot].buf) return;
    SDL_LockAudioStream(aud_stream);
    for (int v = 0; v < 16; v++) if (!voices[v].s) { voices[v].s = &SFXB[slot]; voices[v].pos = 0; break; }
    SDL_UnlockAudioStream(aud_stream);
}

static void audio_music(const char *track) {            /* "" = stop; no-op if already playing */
    if (!aud_stream || !strcmp(track, mus_want)) return;
    snprintf(mus_want, sizeof mus_want, "%s", track);
    if (!strcmp(track, mus_name)) return;                 /* already playing it (e.g. the jingle was missing and the stage music carried on) */
    audio_music_open(track);
}
static void audio_pause(int p) { if (aud_stream) { if (p) SDL_PauseAudioStreamDevice(aud_stream); else SDL_ResumeAudioStreamDevice(aud_stream); } }
