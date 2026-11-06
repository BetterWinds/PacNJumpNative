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
static unsigned char *mus_data; static stb_vorbis *mus_vb; static char mus_name[32], mus_want[32];
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

static void audio_music_close(void) {
    if (mus_vb) { stb_vorbis_close(mus_vb); mus_vb = 0; }
    free(mus_data); mus_data = 0; SDL_free(mus_pcm); mus_pcm = 0; mus_frames = mus_pos = 0; mus_name[0] = 0;
}
/* a track is audio/<name>.ogg, or audio/<name>.wav. If it is not in the pack the current music keeps playing. */
static void audio_music_open(const char *track) {     /* main thread only; audio is locked by the caller */
    if (!track[0]) { audio_music_close(); return; }
    char name[64]; size_t n; snprintf(name, sizeof name, "audio/%s.ogg", track);
    unsigned char *d = read_asset(name, &n);
    if (d) {
        int err = 0; stb_vorbis *vb = stb_vorbis_open_memory(d, (int)n, &err, 0);
        if (!vb) { free(d); return; }
        audio_music_close(); mus_data = d; mus_vb = vb; snprintf(mus_name, sizeof mus_name, "%s", track); return;
    }
    snprintf(name, sizeof name, "audio/%s.wav", track);
    d = read_asset(name, &n); if (!d) return;
    SDL_AudioSpec sp, dst = { SDL_AUDIO_S16, 2, 44100 }; Uint8 *buf = 0, *cb = 0; Uint32 len = 0; int cl = 0;
    if (SDL_LoadWAV_IO(SDL_IOFromConstMem(d, n), true, &sp, &buf, &len) && len > 0 && SDL_ConvertAudioSamples(&sp, buf, (int)len, &dst, &cb, &cl) && cl >= 4) {
        audio_music_close(); mus_pcm = (short *)cb; mus_frames = (Uint32)cl / 4; mus_pos = 0; snprintf(mus_name, sizeof mus_name, "%s", track);
    } else SDL_free(cb);
    if (buf) SDL_free(buf);
    free(d);
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
    for (int i = 0; i < 18; i++) audio_load_sfx(i + 1, names[i]);
    audio_load_sfx(A_SELECT, "select"); audio_load_sfx(A_DECIDE, "decide"); audio_load_sfx(A_CANCEL, "cancel");
    audio_load_sfx(A_GAMEOVER, "gameover"); audio_load_sfx(A_HISCORE, "hiscore"); audio_load_sfx(A_EAT_ITEM, "eat_item");
    audio_load_sfx(A_WARN, "st8_warning");
    if (aud_stream) SDL_UnlockAudioStream(aud_stream);
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
    SDL_LockAudioStream(aud_stream);
    audio_music_open(track);
    SDL_UnlockAudioStream(aud_stream);
}
static void audio_pause(int p) { if (aud_stream) { if (p) SDL_PauseAudioStreamDevice(aud_stream); else SDL_ResumeAudioStreamDevice(aud_stream); } }
