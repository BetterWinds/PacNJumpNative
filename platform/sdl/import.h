/* In-app importer: reads the user's own Pac'N-Jump Android APK and writes pnj.pack (same result as tools/make_pack.py, including the sprite table, read by
 * dex_atlas below). The wakawaka jingle is AAC: imp_jingle decodes it with aac_dec.c.
 * Keep the file mapping in sync with make_pack.py. Included by main.c after pack.h; needs stb_image (for its raw inflate).
 *
 * The APK is a zip: end-of-central-directory record -> central directory -> local headers. Entries are stored or deflated. */
#include <ctype.h>
#include <stdio.h>

typedef struct { char name[160]; Uint32 method, csize, usize, lho; } zip_ent;
typedef struct { unsigned char *d; size_t n; zip_ent *e; int ne; } zip_t;
typedef struct { char name[64]; unsigned char *d; size_t n; } imp_file;

static Uint32 rd32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((Uint32)p[3] << 24); }
static Uint32 rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }

static int zip_open(zip_t *z, const char *path) {
    memset(z, 0, sizeof *z);
    SDL_IOStream *f = SDL_IOFromFile(path, "rb"); if (!f) return 0;
    Sint64 sz = SDL_GetIOSize(f);
    if (sz < 100) { SDL_CloseIO(f); return 0; }
    z->d = (unsigned char *)malloc((size_t)sz); z->n = (size_t)sz;
    if (!z->d || SDL_ReadIO(f, z->d, (size_t)sz) != (size_t)sz) { SDL_CloseIO(f); free(z->d); z->d = 0; return 0; }
    SDL_CloseIO(f);
    size_t eo = 0; for (size_t i = z->n - 22; ; i--) { if (rd32(z->d + i) == 0x06054b50) { eo = i; break; } if (i == 0 || z->n - i > 70000) return 0; }
    int cnt = (int)rd16(z->d + eo + 10); size_t cd = rd32(z->d + eo + 16);
    z->e = (zip_ent *)calloc((size_t)cnt, sizeof *z->e);
    size_t p = cd;
    for (int i = 0; i < cnt; i++) {
        if (p + 46 > z->n || rd32(z->d + p) != 0x02014b50) return 0;
        zip_ent *e = &z->e[z->ne]; Uint32 nl = rd16(z->d + p + 28), xl = rd16(z->d + p + 30), cl = rd16(z->d + p + 32);
        e->method = rd16(z->d + p + 10); e->csize = rd32(z->d + p + 20); e->usize = rd32(z->d + p + 24); e->lho = rd32(z->d + p + 42);
        if (nl < sizeof e->name && p + 46 + nl <= z->n) { memcpy(e->name, z->d + p + 46, nl); e->name[nl] = 0; z->ne++; }
        p += 46 + nl + xl + cl;
    }
    return z->ne > 0;
}
static void zip_close(zip_t *z) { free(z->d); free(z->e); memset(z, 0, sizeof *z); }
static int zip_find(const zip_t *z, const char *name) { for (int i = 0; i < z->ne; i++) if (!strcmp(z->e[i].name, name)) return i; return -1; }
static unsigned char *zip_read(const zip_t *z, int i, size_t *size) {
    const zip_ent *e = &z->e[i]; size_t p = e->lho;
    if (p + 30 > z->n || rd32(z->d + p) != 0x04034b50) return 0;
    size_t start = p + 30 + rd16(z->d + p + 26) + rd16(z->d + p + 28);
    if (start + e->csize > z->n) return 0;
    if (e->method == 0) { unsigned char *b = (unsigned char *)malloc(e->usize ? e->usize : 1); memcpy(b, z->d + start, e->usize); *size = e->usize; return b; }
    if (e->method != 8) return 0;
    int out = 0; char *b = stbi_zlib_decode_noheader_malloc((const char *)z->d + start, (int)e->csize, &out);
    if (!b || (Uint32)out != e->usize) { free(b); return 0; }
    *size = (size_t)out; return (unsigned char *)b;
}

/* ---- sprite table: compiled into classes.dex, not stored as data. Same walk as tools/dexatlas.py (read that for the details). ---- */
static const unsigned char DEX_LEN_RANGES[][3] = { {0x02,0x02,2},{0x03,0x03,3},{0x05,0x05,2},{0x06,0x06,3},{0x08,0x08,2},{0x09,0x09,3},{0x13,0x13,2},{0x14,0x14,3},
    {0x15,0x16,2},{0x17,0x17,3},{0x18,0x18,5},{0x19,0x19,2},{0x1a,0x1a,2},{0x1b,0x1b,3},{0x1c,0x1c,2},{0x1f,0x20,2},{0x22,0x23,2},{0x24,0x26,3},{0x29,0x29,2},
    {0x2a,0x2c,3},{0x2d,0x3d,2},{0x44,0x6d,2},{0x6e,0x72,3},{0x74,0x78,3},{0x90,0xaf,2},{0xd0,0xe2,2} };
static int dex_uleb(const unsigned char *d, size_t n, size_t *p, Uint32 *v) {
    Uint32 r = 0; int s = 0;
    for (;;) { if (*p >= n || s > 28) return 0; unsigned c = d[(*p)++]; r |= (Uint32)(c & 0x7f) << s; s += 7; if (!(c & 0x80)) break; }
    *v = r; return 1;
}
static const char *dex_str(const unsigned char *d, size_t n, Uint32 sid_off, Uint32 sid_n, Uint32 i) {   /* NUL-terminated string i, or NULL */
    if (i >= sid_n || (size_t)sid_off + 4 * (size_t)i + 4 > n) return 0;
    size_t p = rd32(d + sid_off + 4 * (size_t)i); Uint32 skip; if (!dex_uleb(d, n, &p, &skip)) return 0;
    for (size_t q = p; q < n; q++) if (!d[q]) return (const char *)d + p;
    return 0;
}
/* returns malloc'd text, one "tex x y w h tw" line per sprite, or NULL */
static char *dex_atlas(const unsigned char *d, size_t n, size_t *outn) {
    if (n < 0x70 || memcmp(d, "dex\n", 4)) return 0;
    Uint32 str_n = rd32(d + 0x38), str_off = rd32(d + 0x3c), type_off = rd32(d + 0x44), type_n = rd32(d + 0x40), meth_n = rd32(d + 0x58), meth_off = rd32(d + 0x5c), cls_n = rd32(d + 0x60), cls_off = rd32(d + 0x64);
    static unsigned char len[256]; if (!len[0]) { for (int i = 0; i < 256; i++) len[i] = 1; for (size_t r = 0; r < sizeof DEX_LEN_RANGES / sizeof *DEX_LEN_RANGES; r++) for (int i = DEX_LEN_RANGES[r][0]; i <= DEX_LEN_RANGES[r][1]; i++) len[i] = DEX_LEN_RANGES[r][2]; }
    size_t code = 0;
    for (Uint32 c = 0; c < cls_n && !code; c++) {
        size_t co = (size_t)cls_off + 32 * (size_t)c; if (co + 32 > n) return 0;
        Uint32 tidx = rd32(d + co), data = rd32(d + co + 24);
        if (tidx >= type_n || (size_t)type_off + 4 * (size_t)tidx + 4 > n || !data) continue;
        const char *tn = dex_str(d, n, str_off, str_n, rd32(d + type_off + 4 * (size_t)tidx));
        if (!tn || strcmp(tn, "Ljp/co/zoo/pacnjump/b/i;")) continue;
        size_t p = data; Uint32 sf, inf, dm, vm, t;
        if (!dex_uleb(d, n, &p, &sf) || !dex_uleb(d, n, &p, &inf) || !dex_uleb(d, n, &p, &dm) || !dex_uleb(d, n, &p, &vm)) return 0;
        for (Uint32 i = 0; i < (sf + inf) * 2; i++) if (!dex_uleb(d, n, &p, &t)) return 0;
        Uint32 mi = 0;
        for (Uint32 i = 0; i < dm + vm; i++) {
            Uint32 diff, acc, cd; if (!dex_uleb(d, n, &p, &diff) || !dex_uleb(d, n, &p, &acc) || !dex_uleb(d, n, &p, &cd)) return 0;
            mi += diff; if (mi >= meth_n || (size_t)meth_off + 8 * (size_t)mi + 8 > n) return 0;
            const char *mn = dex_str(d, n, str_off, str_n, rd32(d + meth_off + 8 * (size_t)mi + 4));
            if (mn && !strcmp(mn, "<clinit>") && cd) { code = cd; break; }
        }
    }
    if (!code || code + 16 > n) return 0;
    Uint32 size = rd32(d + code + 12); if ((size_t)code + 16 + 2 * (size_t)size > n) return 0;
    const unsigned char *u = d + code + 16;
    #define U(i) ((Uint32)rd16(u + 2 * (size_t)(i)))
    int reg[512]; memset(reg, 0, sizeof reg); int pend[6], have = 0, cnt = 0; int (*rows)[6] = (int (*)[6])malloc(sizeof(int[6]) * 256); if (!rows) return 0;
    #define R(i) reg[(i) & 511]
    for (Uint32 p = 0; p < size; ) {
        Uint32 w = U(p), op = w & 0xff, a4 = (w >> 8) & 15, b4 = w >> 12, aa = w >> 8; Uint32 nx = p + len[op];
        if (nx > size) break;
        if (op == 0x01 || op == 0x07) R(a4) = R(b4);
        else if (op == 0x02 || op == 0x08) R(aa) = R(U(p + 1));
        else if (op == 0x03 || op == 0x09) R(U(p + 1)) = R(U(p + 2));
        else if (op == 0x12) R(a4) = b4 > 7 ? (int)b4 - 16 : (int)b4;
        else if (op == 0x13) R(aa) = (int16_t)U(p + 1);
        else if (op == 0x14) R(aa) = (int)(U(p + 1) | U(p + 2) << 16);
        else if (op == 0x15) R(aa) = (int)(U(p + 1) << 16);
        else if (op == 0x77 && aa == 6) { Uint32 c0 = U(p + 2); for (int i = 0; i < 6; i++) pend[i] = R(c0 + (Uint32)i); have = 1; }
        else if (op == 0x70 && (w >> 12) == 3 && have) {
            if (cnt >= 256) { free(rows); return 0; }
            rows[cnt][0] = R((U(p + 2) >> 4) & 15); for (int i = 0; i < 5; i++) rows[cnt][1 + i] = pend[i]; cnt++; have = 0;
        } else if (op >= 0x0e && op <= 0x11) break;
        else if (op >= 0x28 && op <= 0x3d) { free(rows); return 0; }
        p = nx;
    }
    #undef U
    #undef R
    char *out = 0;
    if (cnt == 224) {
        out = (char *)malloc((size_t)cnt * 40); size_t o = 0;
        if (out) { for (int i = 0; i < cnt; i++) o += (size_t)sprintf(out + o, "%d %d %d %d %d %d\n", rows[i][0], rows[i][1], rows[i][2], rows[i][3], rows[i][4], rows[i][5]); *outn = o; }
    }
    free(rows); return out;
}

/* Progress for the loader screen. import_apk may run on its own thread; the UI only reads these two ints (permille done, and which step). */
enum { IS_OPEN, IS_UNPACK, IS_SPRITES, IS_AUDIO, IS_WRITE, IS_DONE };
static volatile int imp_pm = 0, imp_stage = IS_OPEN;
static void imp_prog(int pm, int st) { imp_pm = pm; imp_stage = st; }

static void imp_add(imp_file **fl, int *n, int *cap, const char *name, unsigned char *d, size_t sz) {
    if (*n == *cap) { *cap = *cap ? *cap * 2 : 128; *fl = (imp_file *)realloc(*fl, (size_t)*cap * sizeof **fl); }
    imp_file *f = &(*fl)[(*n)++]; snprintf(f->name, sizeof f->name, "%s", name); f->d = d; f->n = sz;
}
static int imp_cmp(const void *a, const void *b) { return strcmp(((const imp_file *)a)->name, ((const imp_file *)b)->name); }
static void wr32(FILE *f, Uint32 v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc((int)(v >> 24), f); }
static void wr16(FILE *f, Uint32 v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

/* The power-up jingle (res/raw/wakawaka.3gp) is AAC-LC. aac_dec.c decodes it itself, so every desktop platform gets the same result with nothing else installed.
 * If that ever fails on some other file, the OS decoder is tried (Windows Media Foundation, macOS AudioToolbox, jingle_native.c), then ffmpeg if it is installed.
 * Without any of them the stage music just keeps playing while powered up. (The web builder decodes it in the browser. Android's in-app import uses aac_dec.c only.) */
unsigned char *pnj_aac_decode(const unsigned char *file, size_t size, size_t *out_size);   /* aac_dec.c */
unsigned char *pnj_native_decode(const char *path, size_t *out_size);      /* jingle_native.c: the OS's own AAC decoder (Windows, macOS) */
static void imp_jingle(const zip_t *z, const char *out_pack, imp_file **fl, int *nf, int *cap) {
    int k = zip_find(z, "res/raw/wakawaka.3gp"); if (k < 0) return;
    size_t sz; unsigned char *b = zip_read(z, k, &sz); if (!b) return;
    { size_t an = 0; unsigned char *ad = pnj_aac_decode(b, sz, &an); if (ad) { imp_add(fl, nf, cap, "audio/wakawaka.wav", ad, an); free(b); return; } }
#if defined(__ANDROID__) || defined(__IPHONEOS__)
    (void)out_pack; free(b);
#else
    char in[640], wav[640], cmd[1500];
    snprintf(in, sizeof in, "%s.jingle.3gp", out_pack); snprintf(wav, sizeof wav, "%s.jingle.wav", out_pack);
    FILE *f = fopen(in, "wb"); if (!f) { free(b); return; }
    size_t w = fwrite(b, 1, sz, f); fclose(f); free(b);
    if (w == sz) {
        size_t wn = 0; unsigned char *wd = pnj_native_decode(in, &wn);                 /* Windows Media Foundation / macOS AudioToolbox */
        if (wd) { imp_add(fl, nf, cap, "audio/wakawaka.wav", wd, wn); remove(in); return; }
        snprintf(cmd, sizeof cmd, "ffmpeg -v error -y -i \"%s\" -ar 44100 -ac 2 -c:a pcm_s16le \"%s\"", in, wav);
        if (system(cmd) == 0) {
            FILE *g = fopen(wav, "rb");
            if (g) {
                fseek(g, 0, SEEK_END); long n = ftell(g); fseek(g, 0, SEEK_SET);
                unsigned char *d = n > 44 && n < (32L << 20) ? (unsigned char *)malloc((size_t)n) : 0;
                if (d && fread(d, 1, (size_t)n, g) == (size_t)n) imp_add(fl, nf, cap, "audio/wakawaka.wav", d, (size_t)n); else free(d);
                fclose(g);
            }
        }
    }
    remove(in); remove(wav);
#endif
}

/* returns 1 on success; on failure err holds a short reason */
static int import_apk(const char *apk, const char *out_pack, char *err, size_t errn) {
    static const char *TEX[] = { "bnglogo", "bndr_logo", "sysparts", "stages", "charaparts", "stage_1_map", "stage_1_cha", "stage_2_map", "stage_2_cha", "stage_3_map", "stage_3_cha",
        "stage_4_map", "stage_4_cha", "stage_5_map", "stage_5_cha", "stage_6_map", "stage_6_cha", "stage_7_map", "stage_7_cha", "stage_8_map", "stage_8_cha", 0 };
    static const char *SFX[] = { "balloon", "bound", "cancel", "candle", "decide", "eat01", "eat02", "eat03", "eat04", "eat05", "eat06", "eat07", "eat08", "eat_enemy",
        "eat_item", "eat_pow", "gameover", "hat", "hiscore", "jump", "mappy_jump", "miss", "select", "st8_warning", 0 };
    zip_t z; imp_file *fl = 0; int nf = 0, cap = 0, ok = 0, nmaps = 0; err[0] = 0;
    imp_prog(0, IS_OPEN);
    if (!zip_open(&z, apk)) { snprintf(err, errn, "could not read %s as a zip/APK", apk); return 0; }
    if (zip_find(&z, "AndroidManifest.xml") < 0 || zip_find(&z, "res/drawable-hdpi/sysparts.png") < 0) { snprintf(err, errn, "this is not the Pac'N-Jump Android APK"); goto done; }
    char nm[200], out[260]; size_t sz;
    imp_prog(40, IS_UNPACK);
    for (int i = 0; TEX[i]; i++) {
        imp_prog(40 + 160 * i / 21, IS_UNPACK);
        snprintf(nm, sizeof nm, "res/drawable-hdpi/%s.png", TEX[i]); int k = zip_find(&z, nm);
        if (k < 0) { if (i < 2) continue; snprintf(err, errn, "missing %s", nm); goto done; }
        unsigned char *b = zip_read(&z, k, &sz); if (!b) { snprintf(err, errn, "could not unpack %s", nm); goto done; }
        if (i >= 2 && (sz < 24 || ((b[16] << 24 | b[17] << 16 | b[18] << 8 | b[19]) != 1024))) { free(b); snprintf(err, errn, "%s is not the expected 1024 px sheet", nm); goto done; }
        snprintf(out, sizeof out, "tex/%s.png", TEX[i]); imp_add(&fl, &nf, &cap, out, b, sz);
    }
    for (int i = 0; i < z.ne; i++) {
        if ((i & 15) == 0) imp_prog(200 + 400 * i / (z.ne ? z.ne : 1), IS_UNPACK);
        const char *n = z.e[i].name; size_t l = strlen(n);
        if (!strncmp(n, "res/raw/", 8) && l > 12 && !strcmp(n + l - 4, ".map")) {
            unsigned char *b = zip_read(&z, i, &sz); if (!b) continue;
            snprintf(out, sizeof out, "maps/%.*s.map", (int)(l - 12), n + 8); imp_add(&fl, &nf, &cap, out, b, sz); nmaps++;
        } else if (!strncmp(n, "res/raw/", 8) && l > 12 && !strcmp(n + l - 4, ".ogg")) {
            char base[64]; snprintf(base, sizeof base, "%.*s", (int)(l - 12), n + 8);
            const char *b2 = !strncmp(base, "pack_action_bgm", 15) ? base + 12 : !strcmp(base, "breeak") ? "break" : base;
            unsigned char *b = zip_read(&z, i, &sz); if (!b) continue;
            snprintf(out, sizeof out, "audio/%s.ogg", b2); imp_add(&fl, &nf, &cap, out, b, sz);
        } else if (!strncmp(n, "assets/language.", 16)) {
            unsigned char *b = zip_read(&z, i, &sz); if (!b) continue;
            snprintf(out, sizeof out, "lang/%s", n + 7); imp_add(&fl, &nf, &cap, out, b, sz);
        }
    }
    {
        imp_prog(600, IS_SPRITES);
        int k = zip_find(&z, "classes.dex"); size_t dn = 0; unsigned char *dx = k >= 0 ? zip_read(&z, k, &dn) : 0;
        size_t an = 0; char *at = dx ? dex_atlas(dx, dn, &an) : 0; free(dx);
        if (!at) { snprintf(err, errn, "could not read the sprite table from classes.dex (is this the 1.1.0 APK?)"); goto done; }
        imp_add(&fl, &nf, &cap, "meta/atlas.txt", (unsigned char *)at, an);
    }
    imp_prog(700, IS_AUDIO);
    imp_jingle(&z, out_pack, &fl, &nf, &cap);
    if (nmaps != 42) { snprintf(err, errn, "expected 42 level maps, found %d", nmaps); goto done; }
    for (int i = 0; SFX[i]; i++) {
        int have = 0; snprintf(out, sizeof out, "audio/%s.ogg", SFX[i]);
        for (int k = 0; k < nf; k++) if (!strcmp(fl[k].name, out)) have = 1;
        if (!have) { snprintf(err, errn, "missing sound %s", out); goto done; }
    }
    {
        char meta[160]; int ml = snprintf(meta, sizeof meta, "{\"importer\":\"pnj-import\",\"apk_size\":%u,\"files\":%d}", (unsigned)z.n, nf + 1);
        unsigned char *mb = (unsigned char *)malloc((size_t)ml); memcpy(mb, meta, (size_t)ml); imp_add(&fl, &nf, &cap, "meta.json", mb, (size_t)ml);
    }
    qsort(fl, (size_t)nf, sizeof *fl, imp_cmp);
    {
        imp_prog(800, IS_WRITE);
        char tmp[620]; snprintf(tmp, sizeof tmp, "%s.tmp", out_pack);
        FILE *f = fopen(tmp, "wb"); if (!f) { snprintf(err, errn, "cannot write %s", tmp); goto done; }
        fwrite("PNJPACK1", 1, 8, f); wr32(f, (Uint32)nf);
        Uint32 off = 12; for (int i = 0; i < nf; i++) off += 2 + (Uint32)strlen(fl[i].name) + 8;
        for (int i = 0; i < nf; i++) { Uint32 l = (Uint32)strlen(fl[i].name); wr16(f, l); fwrite(fl[i].name, 1, l, f); wr32(f, off); wr32(f, (Uint32)fl[i].n); off += (Uint32)fl[i].n; }
        for (int i = 0; i < nf; i++) { fwrite(fl[i].d, 1, fl[i].n, f); if ((i & 7) == 0) imp_prog(800 + 190 * i / nf, IS_WRITE); }
        int bad = ferror(f); fclose(f);
        if (bad) { snprintf(err, errn, "write failed (disk full?)"); remove(tmp); goto done; }
        remove(out_pack);
        if (rename(tmp, out_pack) != 0) { snprintf(err, errn, "could not move the pack into place"); goto done; }
    }
    ok = 1; imp_prog(1000, IS_DONE);
done:
    for (int i = 0; i < nf; i++) free(fl[i].d);
    free(fl); zip_close(&z);
    return ok;
}
