#ifndef THREEDOH_WASM_STDLIB_H
#define THREEDOH_WASM_STDLIB_H
#include <stddef.h>
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
void abort(void);
int abs(int x);
#endif
