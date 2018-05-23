/*
   ARM60 to mips recompiler
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdbool.h>
#include "retro_inline.h"

#include "arm.h"
#include "freedocore.h"

// Memory map:
// 0x0000 0000 - 0x002f ffff - dram1+dram2+vram
// 0x0300 0000 - 0x030f ffff - rom (2 switchable)
// 0x0310 0000 - 0x031f ffff - nvram + diag port
// 0x0320 0000 - 0x0321 ffff - sport
// 0x0330 0000 - 0x033f ffff - madam
// 0x0340 0000 - 0x034f ffff - clio

// Address look up table for recompiled blocks
// 2Mb RAM + 1Mb VRAM + 1Mb ROM (in fact 2 switchable copies)
// NOTE: ROM must be totally invalidated when switching SelectROM()
static uint8_t recLUT[0x400000];

static void recReset(void);

static inline uint32_t getLUT(uint32_t addr)
{
	if (!((addr ^ 0x03000000) & ~0xFFFFF))
		addr -= 0x2d00000;
		// place rom at 0x00300000

	if (addr < 0x00400000)
		return *(uint32_t *)(recLUT + addr);

	return 0;
}

static inline void setLUT(uint32_t addr, uint32_t val)
{
	if (!((addr ^ 0x03000000) & ~0xFFFFF))
		addr -= 0x2d00000;
		// place rom at 0x00300000

	if (addr < 0x00400000)
		*(uint32_t *)(recLUT + addr) = val;
}

static void recInit(void)
{
	recReset();
}

static void recReset(void)
{
	memset(recLUT, 0, sizeof(recLUT));
}

static int recExec(int cycles)
{
	int cnt = 0;
	do {
		uint32_t PC = arm.USER[15];
		uint32_t func = getLUT(PC);
		if (!func) {
			setLUT(PC, 1 /* recRecompile() */);
			//printf("Mark %08x\n", PC);
		}

		cnt += _arm_Execute();
	} while (cycles > cnt);

	return cnt;
}

static void recDestroy(void)
{
}


ARM60cpu cpuRec = {
	recInit,
	recReset,
	recExec,
	recDestroy
};
