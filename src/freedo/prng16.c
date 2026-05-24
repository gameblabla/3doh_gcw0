#include "prng16.h"
#include "retro_inline.h"

static uint32_t g_PRNG16_STATE = 0xDEADBEEFu;

static INLINE uint32_t hash16(uint32_t input, uint32_t key)
{
    uint32_t hash = input * key;
    return ((hash >> 16) ^ hash) & 0xFFFFu;
}

void prng16_seed(uint32_t seed)
{
    g_PRNG16_STATE = seed;
}

uint32_t prng16(void)
{
    g_PRNG16_STATE += 0xFC15u;
    return hash16(g_PRNG16_STATE, 0x02ABu);
}
