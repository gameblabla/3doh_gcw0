#ifndef FREEDO_PRNG32_H_INCLUDED
#define FREEDO_PRNG32_H_INCLUDED

#include <stdint.h>

void prng32_seed(uint32_t seed);
uint32_t prng32(void);

#endif
