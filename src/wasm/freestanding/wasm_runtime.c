#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

extern unsigned char __heap_base;
__attribute__((import_module("env"), import_name("threedoh_host_log")))
extern void threedoh_host_log(const char *message);

static uintptr_t heap_cur = 0;

static uintptr_t align_up(uintptr_t value, uintptr_t align)
{
    return (value + align - 1u) & ~(align - 1u);
}

void *malloc(size_t size)
{
    if (heap_cur == 0)
        heap_cur = align_up((uintptr_t)&__heap_base, 16u);
    if (size == 0)
        size = 1;
    uintptr_t p = heap_cur;
    heap_cur = align_up(heap_cur + size, 16u);
    return (void *)p;
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *p = malloc(total);
    unsigned char *b = (unsigned char *)p;
    for (size_t i = 0; i < total; i++)
        b[i] = 0;
    return p;
}

void *realloc(void *ptr, size_t size)
{
    if (!ptr)
        return malloc(size);
    void *p = malloc(size);
    (void)ptr;
    return p;
}

void free(void *ptr)
{
    (void)ptr;
}

void abort(void)
{
    threedoh_host_log("abort");
    __builtin_trap();
}

int abs(int x)
{
    return x < 0 ? -x : x;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dest;
}

void *memset(void *s, int c, size_t n)
{
    unsigned char *d = (unsigned char *)s;
    for (size_t i = 0; i < n; i++)
        d[i] = (unsigned char)c;
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (int)a[i] - (int)b[i];
    }
    return 0;
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}

char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++)) {}
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = 0;
    return dest;
}

char *strcat(char *dest, const char *src)
{
    strcpy(dest + strlen(dest), src);
    return dest;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca != cb || ca == 0 || cb == 0)
            return (int)ca - (int)cb;
    }
    return 0;
}

char *strchr(const char *s, int c)
{
    char ch = (char)c;
    while (*s) {
        if (*s == ch)
            return (char *)s;
        s++;
    }
    return ch == 0 ? (char *)s : 0;
}

char *strrchr(const char *s, int c)
{
    char ch = (char)c;
    const char *last = 0;
    do {
        if (*s == ch)
            last = s;
    } while (*s++);
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
    if (!*needle)
        return (char *)haystack;
    size_t nl = strlen(needle);
    for (const char *h = haystack; *h; h++) {
        if (*h == *needle && strncmp(h, needle, nl) == 0)
            return (char *)h;
    }
    return 0;
}

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *out = (char *)malloc(n);
    memcpy(out, s, n);
    return out;
}

static void append_char(char **out, size_t *left, char c)
{
    if (*left > 1) {
        **out = c;
        (*out)++;
        (*left)--;
    }
}

static void append_str(char **out, size_t *left, const char *s)
{
    while (*s)
        append_char(out, left, *s++);
}

static void append_uint(char **out, size_t *left, unsigned long v, unsigned base, int upper)
{
    char tmp[32];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int n = 0;
    do {
        tmp[n++] = digits[v % base];
        v /= base;
    } while (v && n < (int)sizeof(tmp));
    while (n--)
        append_char(out, left, tmp[n]);
}

static int mini_vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    char *out = str;
    size_t left = size ? size : 0;
    const char *start = str;
    while (*fmt) {
        if (*fmt != '%') {
            append_char(&out, &left, *fmt++);
            continue;
        }
        fmt++;
        while (*fmt >= '0' && *fmt <= '9') fmt++;
        if (*fmt == '.') { fmt++; while (*fmt >= '0' && *fmt <= '9') fmt++; }
        switch (*fmt) {
        case 's': append_str(&out, &left, va_arg(ap, const char *)); break;
        case 'c': append_char(&out, &left, (char)va_arg(ap, int)); break;
        case 'd':
        case 'i': {
            int v = va_arg(ap, int);
            if (v < 0) { append_char(&out, &left, '-'); v = -v; }
            append_uint(&out, &left, (unsigned)v, 10, 0);
            break;
        }
        case 'u': append_uint(&out, &left, va_arg(ap, unsigned), 10, 0); break;
        case 'x': append_uint(&out, &left, va_arg(ap, unsigned), 16, 0); break;
        case 'X': append_uint(&out, &left, va_arg(ap, unsigned), 16, 1); break;
        case '%': append_char(&out, &left, '%'); break;
        default: append_char(&out, &left, '%'); if (*fmt) append_char(&out, &left, *fmt); break;
        }
        if (*fmt) fmt++;
    }
    if (size)
        *out = 0;
    return (int)(out - start);
}

int snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = mini_vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *str, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = mini_vsnprintf(str, (size_t)-1, fmt, ap);
    va_end(ap);
    return r;
}

int printf(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int r = mini_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    threedoh_host_log(buf);
    return r;
}
