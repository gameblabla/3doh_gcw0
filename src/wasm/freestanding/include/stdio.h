#ifndef THREEDOH_WASM_STDIO_H
#define THREEDOH_WASM_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct THREEDOH_WASM_FILE FILE;
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
int printf(const char *fmt, ...);
int sprintf(char *str, const char *fmt, ...);
int snprintf(char *str, size_t size, const char *fmt, ...);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int fscanf(FILE *stream, const char *fmt, ...);
int feof(FILE *stream);
#endif
