#ifndef FREEDO_PRNG16_H_INCLUDED
#define FREEDO_PRNG16_H_INCLUDED

#include <stdint.h>

void prng16_seed(uint32_t seed);
uint32_t prng16(void);

#endif
