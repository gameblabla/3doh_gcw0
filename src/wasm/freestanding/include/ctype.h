#ifndef THREEDOH_WASM_CTYPE_H
#define THREEDOH_WASM_CTYPE_H
static inline int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
#endif
