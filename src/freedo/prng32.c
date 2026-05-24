#include "prng32.h"
#include "retro_inline.h"

static uint32_t g_PRNG32_STATE = 0xDEADBEEFu;

static INLINE uint32_t splitmix32(uint32_t *v)
{
    uint32_t z = (*v += 0x9e3779b9u);
    z = (z ^ (z >> 16)) * 0x85ebca6bu;
    z = (z ^ (z >> 13)) * 0xc2b2ae35u;
    return z ^ (z >> 16);
}

void prng32_seed(uint32_t seed)
{
    g_PRNG32_STATE = seed;
}

uint32_t prng32(void)
{
    return splitmix32(&g_PRNG32_STATE);
}
