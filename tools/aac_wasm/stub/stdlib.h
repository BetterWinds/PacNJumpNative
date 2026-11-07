#include <stddef.h>
void *malloc(size_t); void *calloc(size_t, size_t); void free(void *);
long lrintf(float);
static inline int abs(int x) { return x < 0 ? -x : x; }
