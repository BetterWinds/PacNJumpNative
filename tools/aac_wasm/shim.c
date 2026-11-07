/* libc pieces aac_dec.c needs when built for WebAssembly without a libc. sin, cos and pow come from the page (Math.sin etc). */
#include <stddef.h>
extern unsigned char __heap_base;
static size_t heap_top;
static void *bump(size_t n) {
    n = (n + 15) & ~(size_t)15;
    if (!heap_top) heap_top = (size_t)&__heap_base;
    size_t end = heap_top + n, have = (size_t)__builtin_wasm_memory_size(0) * 65536;
    if (end > have) { size_t pages = (end - have + 65535) / 65536; if (__builtin_wasm_memory_grow(0, pages) == (size_t)-1) return 0; }
    void *p = (void *)heap_top; heap_top = end; return p;
}
void *malloc(size_t n) { return bump(n); }
void *calloc(size_t a, size_t b) { return bump(a * b); }            /* fresh memory from memory.grow is zero, and nothing is ever reused */
void free(void *p) { (void)p; }
void *memcpy(void *d, const void *s, size_t n) { unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; return d; }
void *memset(void *d, int c, size_t n) { unsigned char *a = d; while (n--) *a++ = (unsigned char)c; return d; }
int memcmp(const void *x, const void *y, size_t n) { const unsigned char *a = x, *b = y; for (; n; n--, a++, b++) if (*a != *b) return *a - *b; return 0; }
long lrintf(float x) { return (long)__builtin_rintf(x); }
/* JS side hands in the file at in_ptr() and reads the WAV back through out_ptr()/out_len() */
static unsigned char *res; static size_t res_n;
extern unsigned char *pnj_aac_decode(const unsigned char *, size_t, size_t *);
void *alloc(size_t n) { return bump(n); }
int decode(const unsigned char *f, size_t n) { res = pnj_aac_decode(f, n, &res_n); return res != 0; }
unsigned char *out_ptr(void) { return res; }
size_t out_len(void) { return res_n; }
