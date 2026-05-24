/*
   www.freedo.org
   The first and only working 3DO multiplayer emulator.

   The FreeDO licensed under modified GNU LGPL, with following notes:

 *   The owners and original authors of the FreeDO have full right to develop closed source derivative work.
 *   Any non-commercial uses of the FreeDO sources or any knowledge obtained by studying or reverse engineering
   of the sources, or any other material published by FreeDO have to be accompanied with full credits.
 *   Any commercial uses of FreeDO sources or any knowledge obtained by studying or reverse engineering of the sources,
   or any other material published by FreeDO is strictly forbidden without owners approval.

   The above notes are taking precedence over GNU LGPL in conflicting situations.

   Project authors:

   Alexander Troosh
   Maxim Grishin
   Allen Wright
   John Sammons
   Felix Lazarev
 */


#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdbool.h>
#include "retro_inline.h"

#include "arm.h"
#include "Madam.h"
#include "Clio.h"
#include "DiagPort.h"
#include "SPORT.h"

#include "freedocore.h"

#ifdef HLE_SWI
#include "freedo_swi_hle_0x5XXXX.h"
#include "freedo_fixedpoint_math.h"
#endif

extern int fixmode;
extern int cnbfix;

//extern _ext_Interface io_interface;

#define ARM_ALU_MASK    0x0c000000
#define ARM_ALU_SIGN    0x00000000
#define ARM_MUL_MASK    0x0fc000f0
#define ARM_MUL_SIGN    0x00000090
#define ARM_SDS_MASK    0x0fb00ff0
#define ARM_SDS_SIGN    0x01000090
#define ARM_SDT_MASK    0x0c000000
#define ARM_SDT_SIGN    0x04000000
#define ARM_BDT_MASK    0x0e000000
#define ARM_BDT_SIGN    0x08000000
#define ARM_BRA_MASK    0x0e000000
#define ARM_BRA_SIGN    0x0a000000
#define ARM_COP_MASK    0x0f000000
#define ARM_COP_SIGN    0x0e000000
#define ARM_SWI_MASK    0x0f000000
#define ARM_SWI_SIGN    0x0f000000

//режимы процессора----------------------------------------------------------
#define ARM_MODE_USER   0
#define ARM_MODE_FIQ    1
#define ARM_MODE_IRQ    2
#define ARM_MODE_SVC    3
#define ARM_MODE_ABT    4
#define ARM_MODE_UND    5
#define ARM_MODE_UNK    0xff

static const uint8_t arm_mode_table[] =
{
	ARM_MODE_UNK,  ARM_MODE_UNK,	 ARM_MODE_UNK,	   ARM_MODE_UNK,
	ARM_MODE_UNK,  ARM_MODE_UNK,	 ARM_MODE_UNK,	   ARM_MODE_UNK,
	ARM_MODE_UNK,  ARM_MODE_UNK,	 ARM_MODE_UNK,	   ARM_MODE_UNK,
	ARM_MODE_UNK,  ARM_MODE_UNK,	 ARM_MODE_UNK,	   ARM_MODE_UNK,
	ARM_MODE_USER, ARM_MODE_FIQ,	 ARM_MODE_IRQ,	   ARM_MODE_SVC,
	ARM_MODE_UNK,  ARM_MODE_UNK,	 ARM_MODE_UNK,	   ARM_MODE_ABT,
	ARM_MODE_UNK,  ARM_MODE_UNK,	 ARM_MODE_UNK,	   ARM_MODE_UND,
	ARM_MODE_UNK,  ARM_MODE_UNK,	 ARM_MODE_UNK,	   ARM_MODE_UNK
};

//для посчета тактов------------------------------------------------------------
#define NCYCLE 4
#define SCYCLE 1
#define ICYCLE 1

//--------------------------Conditions-------------------------------------------
//flags - N Z C V  -  31...28
static const uint16_t cond_flags_cross[] = {    //((cond_flags_cross[cond_feald]>>flags)&1)  -- пример проверки
	0xf0f0,                                 //EQ - Z set (equal)
	0x0f0f,                                 //NE - Z clear (not equal)
	0xcccc,                                 //CS - C set (unsigned higher or same)
	0x3333,                                 //CC - C clear (unsigned lower)
	0xff00,                                 //N set (negative)
	0x00ff,                                 //N clear (positive or zero)
	0xaaaa,                                 //V set (overflow)
	0x5555,                                 //V clear (no overflow)
	0x0c0c,                                 //C set and Z clear (unsigned higher)
	0xf3f3,                                 //C clear or Z set (unsigned lower or same)
	0xaa55,                                 //N set and V set, or N clear and V clear (greater or equal)
	0x55aa,                                 //N set and V clear, or N clear and V set (less than)
	0x0a05,                                 //Z clear, and either N set and V set, or N clear and V clear (greater than)
	0xf5fa,                                 //Z set, or N set and V clear, or N clear and V set (less than or equal)
	0xffff,                                 //always
	0x0000                                  //never
};


///////////////////////////////////////////////////////////////
// Global variables;
///////////////////////////////////////////////////////////////
#define RAMSIZE     3 * 1024 * 1024     //dram1+dram2+vram
#define REAL_MAIN_RAM_BYTES (2u * 1024u * 1024u)
static uint32_t arm_highram_read_count;
static uint32_t arm_highram_write_count;
static uint32_t arm_highram_last_read;
static uint32_t arm_highram_last_write;
static uint32_t arm_highram_first_read;
static uint32_t arm_highram_first_write;
#define ROMSIZE     1 * 1024 * 1024     //rom
#define NVRAMSIZE   (65536 >> 1)        //nvram at 0x03140000...0x317FFFF
#define REG_PC  RON_USER[15]
#define UNDEFVAL 0xBAD12345

enum {
	ARM_FAULT_NONE = 0,
	ARM_FAULT_UNMAPPED_READ = 1,
	ARM_FAULT_UNMAPPED_WRITE = 2,
	ARM_FAULT_DEVICE_UNALIGNED_READ = 3,
	ARM_FAULT_DEVICE_UNALIGNED_WRITE = 4,
	ARM_FAULT_DSP_BOUNDS = 5,
	ARM_FAULT_UNALIGNED_BLOCK = 6,
	ARM_FAULT_PREFETCH = 7,
	ARM_FAULT_DSP_RUNAWAY = 8,
	ARM_FAULT_MADAM_RUNAWAY = 9,
	ARM_FAULT_CPU_RUNAWAY = 10,
	ARM_FAULT_MMU_TRANSLATION = 11,
	ARM_FAULT_MMU_PERMISSION = 12,
	ARM_FAULT_CP15_UNDEFINED = 13
};

static uint32_t current_instr_pc;
static uint32_t arm_fiq_entry_count;
static uint32_t arm_unaligned_prefetch_count;
static uint32_t arm_unaligned_prefetch_last;
static uint32_t arm_unaligned_prefetch_fetch;
static uint32_t arm_mirrored_prefetch_count;
static uint32_t arm_mirrored_prefetch_last;
static uint32_t arm_mirrored_prefetch_fetch;
static bool strict_bus_configured;
static INLINE void SETM(uint32_t a);
static INLINE void SETI(bool a);


struct ARM_CoreState arm;
static int CYCLES;      //cycle counter

//forward decls
uint32_t rreadusr(uint32_t rn);
void loadusr(uint32_t rn, uint32_t val);
uint32_t mreadb(uint32_t addr);
void mwriteb(uint32_t addr, uint32_t val);
uint32_t mreadw(uint32_t addr);
void mwritew(uint32_t addr, uint32_t val);
void _arm_SetCPSR(uint32_t a);

#define MAS_Access_Exept	arm.MAS_Access_Exept
#define pRam			arm.Ram
#define pRom			arm.Rom
#define pNVRam			arm.NVRam
#define RON_USER		arm.USER
#define RON_CASH		arm.CASH
#define RON_SVC			arm.SVC
#define RON_ABT			arm.ABT
#define RON_FIQ			arm.FIQ
#define RON_IRQ			arm.IRQ
#define RON_UND			arm.UND
#define SPSR			arm.SPSR
#define CPSR			arm.CPSR
#define gFIQ			arm.nFIQ
#define gSecondROM		arm.SecondROM
#define STRICT_BUS_FAULTS	arm.StrictBusFaults
#define BUS_FAULTED		arm.BusFaulted
#define LAST_FAULT_ADDR		arm.LastFaultAddr
#define LAST_FAULT_PC		arm.LastFaultPC
#define LAST_FAULT_TYPE		arm.LastFaultType
#define CP15_ID			arm.CP15_ID
#define CP15_CONTROL		arm.CP15_Control
#define CP15_TTB		arm.CP15_TranslationBase
#define CP15_DACR		arm.CP15_DomainAccessControl
#define CP15_FSR		arm.CP15_FaultStatus
#define CP15_FAR		arm.CP15_FaultAddress
#define CP15_LAST_OP		arm.CP15_LastOp
#define CP15_OPS		arm.CP15_CoprocessorOps
#define CP15_CACHE_FLUSHES	arm.CP15_CacheFlushes
#define CP15_WB_FLUSHES	arm.CP15_WriteBufferFlushes

void* Getp_NVRAM(void)
{
	return pNVRam;
}

void* Getp_ROMS(void)
{
	return pRom;
}

void* Getp_RAMS(void)
{
	return pRam;
}

uint32_t _arm_SaveSize(void)
{
	return sizeof(struct ARM_CoreState) + RAMSIZE + (ROMSIZE * 2) + NVRAMSIZE;
}

void _arm_Save(void *buff)
{
	_arm_FlushWriteBuffer();
	memcpy(buff, &arm, sizeof(struct ARM_CoreState));
	memcpy(((uint8_t*)buff) + sizeof(struct ARM_CoreState), pRam, RAMSIZE);
	memcpy(((uint8_t*)buff) + sizeof(struct ARM_CoreState) + RAMSIZE, pRom, ROMSIZE * 2);
	memcpy(((uint8_t*)buff) + sizeof(struct ARM_CoreState) + RAMSIZE + ROMSIZE * 2, pNVRam, NVRAMSIZE);
}

void _arm_Load(void *buff)
{
	uint8_t *tRam = pRam;
	uint8_t *tRom = pRom;
	uint8_t *tNVRam = pNVRam;

	memcpy(&arm, buff, sizeof(struct ARM_CoreState));
	memcpy(tRam, ((uint8_t*)buff) + sizeof(struct ARM_CoreState), RAMSIZE);
	memcpy(tRom, ((uint8_t*)buff) + sizeof(struct ARM_CoreState) + RAMSIZE, ROMSIZE * 2);
	memcpy(tNVRam, ((uint8_t*)buff) + sizeof(struct ARM_CoreState) + RAMSIZE + ROMSIZE * 2, NVRAMSIZE);

	pRom = tRom;
	pRam = tRam;
	pNVRam = tNVRam;
	if (!CP15_ID)
		CP15_ID = 0x41560610u;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static INLINE void load(uint32_t rn, uint32_t val)
{
	/* ARM26 R15 writes carry NZCVIF and mode bits around the 26-bit PC. */
	if (rn == 15 && (val & 0xfc000003u)) {
		uint32_t psr = (val & 0xf0000000u) |
		               ((val & 0x08000000u) ? 0x80u : 0u) |
		               ((val & 0x04000000u) ? 0x40u : 0u) |
		               0x10u | (val & 3u);
		_arm_SetCPSR(psr);
		RON_USER[15] = val & 0x03fffffcu;
		return;
	}
	RON_USER[rn] = val;
}

void ARM_RestUserRONS(void)
{
	switch (arm_mode_table[(CPSR & 0x1f)]) {
	case ARM_MODE_USER:
		break;
	case ARM_MODE_FIQ:
		memcpy(RON_FIQ, &RON_USER[8], 7 << 2);
		memcpy(&RON_USER[8], RON_CASH, 7 << 2);
		break;
	case ARM_MODE_IRQ:
		RON_IRQ[0] = RON_USER[13];
		RON_IRQ[1] = RON_USER[14];
		RON_USER[13] = RON_CASH[5];
		RON_USER[14] = RON_CASH[6];
		break;
	case ARM_MODE_SVC:
		RON_SVC[0] = RON_USER[13];
		RON_SVC[1] = RON_USER[14];
		RON_USER[13] = RON_CASH[5];
		RON_USER[14] = RON_CASH[6];
		break;
	case ARM_MODE_ABT:
		RON_ABT[0] = RON_USER[13];
		RON_ABT[1] = RON_USER[14];
		RON_USER[13] = RON_CASH[5];
		RON_USER[14] = RON_CASH[6];
		break;
	case ARM_MODE_UND:
		RON_UND[0] = RON_USER[13];
		RON_UND[1] = RON_USER[14];
		RON_USER[13] = RON_CASH[5];
		RON_USER[14] = RON_CASH[6];
		break;
	}
}

void ARM_RestFiqRONS(void)
{
	switch (arm_mode_table[(CPSR & 0x1f)]) {
	case ARM_MODE_USER:
		memcpy(RON_CASH, &RON_USER[8], 7 << 2);
		memcpy(&RON_USER[8], RON_FIQ, 7 << 2);
		break;
	case ARM_MODE_FIQ:
		break;
	case ARM_MODE_IRQ:
		memcpy(RON_CASH, &RON_USER[8], 5 << 2);
		RON_IRQ[0] = RON_USER[13];
		RON_IRQ[1] = RON_USER[14];
		memcpy(&RON_USER[8], RON_FIQ, 7 << 2);
		break;
	case ARM_MODE_SVC:
		memcpy(RON_CASH, &RON_USER[8], 5 << 2);
		RON_SVC[0] = RON_USER[13];
		RON_SVC[1] = RON_USER[14];
		memcpy(&RON_USER[8], RON_FIQ, 7 << 2);
		break;
	case ARM_MODE_ABT:
		memcpy(RON_CASH, &RON_USER[8], 5 << 2);
		RON_ABT[0] = RON_USER[13];
		RON_ABT[1] = RON_USER[14];
		memcpy(&RON_USER[8], RON_FIQ, 7 << 2);
		break;
	case ARM_MODE_UND:
		memcpy(RON_CASH, &RON_USER[8], 5 << 2);
		RON_UND[0] = RON_USER[13];
		RON_UND[1] = RON_USER[14];
		memcpy(&RON_USER[8], RON_FIQ, 7 << 2);
		break;
	}
}

void ARM_RestIrqRONS(void)
{
	switch (arm_mode_table[(CPSR & 0x1f)]) {
	case ARM_MODE_USER:
		RON_CASH[5] = RON_USER[13];
		RON_CASH[6] = RON_USER[14];
		RON_USER[13] = RON_IRQ[0];
		RON_USER[14] = RON_IRQ[1];
		break;
	case ARM_MODE_FIQ:
		memcpy(RON_FIQ, &RON_USER[8], 7 << 2);
		memcpy(&RON_USER[8], RON_CASH, 5 << 2);
		RON_USER[13] = RON_IRQ[0];
		RON_USER[14] = RON_IRQ[1];
		break;
	case ARM_MODE_IRQ:
		break;
	case ARM_MODE_SVC:
		RON_SVC[0] = RON_USER[13];
		RON_SVC[1] = RON_USER[14];
		RON_USER[13] = RON_IRQ[0];
		RON_USER[14] = RON_IRQ[1];
		break;
	case ARM_MODE_ABT:
		RON_ABT[0] = RON_USER[13];
		RON_ABT[1] = RON_USER[14];
		RON_USER[13] = RON_IRQ[0];
		RON_USER[14] = RON_IRQ[1];
		break;
	case ARM_MODE_UND:
		RON_UND[0] = RON_USER[13];
		RON_UND[1] = RON_USER[14];
		RON_USER[13] = RON_IRQ[0];
		RON_USER[14] = RON_IRQ[1];
		break;
	}
}

void ARM_RestSvcRONS(void)
{
	switch (arm_mode_table[(CPSR & 0x1f)]) {
	case ARM_MODE_USER:
		RON_CASH[5] = RON_USER[13];
		RON_CASH[6] = RON_USER[14];
		RON_USER[13] = RON_SVC[0];
		RON_USER[14] = RON_SVC[1];
		break;
	case ARM_MODE_FIQ:
		memcpy(RON_FIQ, &RON_USER[8], 7 << 2);
		memcpy(&RON_USER[8], RON_CASH, 5 << 2);
		RON_USER[13] = RON_SVC[0];
		RON_USER[14] = RON_SVC[1];
		break;
	case ARM_MODE_IRQ:
		RON_IRQ[0] = RON_USER[13];
		RON_IRQ[1] = RON_USER[14];
		RON_USER[13] = RON_SVC[0];
		RON_USER[14] = RON_SVC[1];
		break;
	case ARM_MODE_SVC:
		break;
	case ARM_MODE_ABT:
		RON_ABT[0] = RON_USER[13];
		RON_ABT[1] = RON_USER[14];
		RON_USER[13] = RON_SVC[0];
		RON_USER[14] = RON_SVC[1];
		break;
	case ARM_MODE_UND:
		RON_UND[0] = RON_USER[13];
		RON_UND[1] = RON_USER[14];
		RON_USER[13] = RON_SVC[0];
		RON_USER[14] = RON_SVC[1];
		break;
	}
}

void ARM_RestAbtRONS(void)
{
	switch (arm_mode_table[(CPSR & 0x1f)]) {
	case ARM_MODE_USER:
		RON_CASH[5] = RON_USER[13];
		RON_CASH[6] = RON_USER[14];
		RON_USER[13] = RON_ABT[0];
		RON_USER[14] = RON_ABT[1];
		break;
	case ARM_MODE_FIQ:
		memcpy(RON_FIQ, &RON_USER[8], 7 << 2);
		memcpy(&RON_USER[8], RON_CASH, 5 << 2);
		RON_USER[13] = RON_ABT[0];
		RON_USER[14] = RON_ABT[1];
		break;
	case ARM_MODE_IRQ:
		RON_IRQ[0] = RON_USER[13];
		RON_IRQ[1] = RON_USER[14];
		RON_USER[13] = RON_ABT[0];
		RON_USER[14] = RON_ABT[1];
		break;
	case ARM_MODE_SVC:
		RON_SVC[0] = RON_USER[13];
		RON_SVC[1] = RON_USER[14];
		RON_USER[13] = RON_ABT[0];
		RON_USER[14] = RON_ABT[1];
		break;
	case ARM_MODE_ABT:
		break;
	case ARM_MODE_UND:
		RON_UND[0] = RON_USER[13];
		RON_UND[1] = RON_USER[14];
		RON_USER[13] = RON_ABT[0];
		RON_USER[14] = RON_ABT[1];
		break;
	}
}

void ARM_RestUndRONS(void)
{
	switch (arm_mode_table[(CPSR & 0x1f)]) {
	case ARM_MODE_USER:
		RON_CASH[5] = RON_USER[13];
		RON_CASH[6] = RON_USER[14];
		RON_USER[13] = RON_UND[0];
		RON_USER[14] = RON_UND[1];
		break;
	case ARM_MODE_FIQ:
		memcpy(RON_FIQ, &RON_USER[8], 7 << 2);
		memcpy(&RON_USER[8], RON_CASH, 5 << 2);
		RON_USER[13] = RON_UND[0];
		RON_USER[14] = RON_UND[1];
		break;
	case ARM_MODE_IRQ:
		RON_IRQ[0] = RON_USER[13];
		RON_IRQ[1] = RON_USER[14];
		RON_USER[13] = RON_UND[0];
		RON_USER[14] = RON_UND[1];
		break;
	case ARM_MODE_SVC:
		RON_SVC[0] = RON_USER[13];
		RON_SVC[1] = RON_USER[14];
		RON_USER[13] = RON_UND[0];
		RON_USER[14] = RON_UND[1];
		break;
	case ARM_MODE_ABT:
		RON_ABT[0] = RON_USER[13];
		RON_ABT[1] = RON_USER[14];
		RON_USER[13] = RON_UND[0];
		RON_USER[14] = RON_UND[1];
		break;
	case ARM_MODE_UND:
		break;
	}
}

void ARM_Change_ModeSafe(uint32_t mode)
{
	switch (arm_mode_table[mode & 0x1f]) {
	case ARM_MODE_USER:
		ARM_RestUserRONS();
		break;
	case ARM_MODE_FIQ:
		ARM_RestFiqRONS();
		break;
	case ARM_MODE_IRQ:
		ARM_RestIrqRONS();
		break;
	case ARM_MODE_SVC:
		ARM_RestSvcRONS();
		break;
	case ARM_MODE_ABT:
		ARM_RestAbtRONS();
		break;
	case ARM_MODE_UND:
		ARM_RestUndRONS();
		break;
	}
}

void SelectROM(int n)
{
	gSecondROM = (n > 0) ? true : false;
}

void _arm_SetStrictBusFaults(bool enabled)
{
	STRICT_BUS_FAULTS = enabled;
	strict_bus_configured = true;
}

bool _arm_GetStrictBusFaults(void)
{
	return STRICT_BUS_FAULTS;
}

bool _arm_BusFaulted(void)
{
	return BUS_FAULTED;
}

uint32_t _arm_LastFaultAddress(void)
{
	return LAST_FAULT_ADDR;
}

uint32_t _arm_LastFaultPC(void)
{
	return LAST_FAULT_PC;
}

uint32_t _arm_LastFaultType(void)
{
	return LAST_FAULT_TYPE;
}

uint32_t _arm_CurrentPC(void)
{
	return REG_PC;
}

uint32_t _arm_CurrentCPSR(void)
{
	return CPSR;
}

uint32_t _arm_FiqEntryCount(void)
{
	return arm_fiq_entry_count;
}

uint32_t _arm_UnalignedPrefetchCount(void)
{
	return arm_unaligned_prefetch_count;
}

uint32_t _arm_UnalignedPrefetchLast(void)
{
	return arm_unaligned_prefetch_last;
}

uint32_t _arm_UnalignedPrefetchFetch(void)
{
	return arm_unaligned_prefetch_fetch;
}

uint32_t _arm_MirroredPrefetchCount(void)
{
	return arm_mirrored_prefetch_count;
}

uint32_t _arm_MirroredPrefetchLast(void)
{
	return arm_mirrored_prefetch_last;
}

uint32_t _arm_MirroredPrefetchFetch(void)
{
	return arm_mirrored_prefetch_fetch;
}

void _arm_ClearFault(void)
{
	BUS_FAULTED = false;
	MAS_Access_Exept = false;
	LAST_FAULT_ADDR = 0;
	LAST_FAULT_PC = 0;
	LAST_FAULT_TYPE = ARM_FAULT_NONE;
}


#define DIPIR_QSI_ADDR                  0x0000020cu
#define DIPIR_QSI_HLE_INSN              0xeafffffeu /* B . */
#define SYSINFO_BADTAG                  0xffffffffu
#define SYSINFO_TAG_FIELDFREQ           0x00010001u
#define SYSINFO_TAG_GRAPHDISPSUPP       0x00020004u
#define SYSINFO_PAL_SUPPORTED           0x00000002u
#define SYSINFO_PAL_DFLT                0x00000200u
#define SYSINFO_PAL_CURDISP             0x00020000u
#define SYSINFO_FREQ_50HZ               50u

static int arm_romqsi_tag_supported(uint32_t tag)
{
	switch (tag) {
	case SYSINFO_TAG_FIELDFREQ:
	case SYSINFO_TAG_GRAPHDISPSUPP:
		return 1;
	default:
		return 0;
	}
}

static int arm_romqsi_hook_installed(void)
{
	/* The CD-ROM Portfolio fallback already describes an NTSC Opera system.
	 * Install the ROMQSI HLE only when the emulated machine is PAL, and only
	 * for the tags whose fallback value is wrong for PAL.  The hook is exposed
	 * virtually through reads from DIPIR_QSI_ADDR so low RAM is not patched.
	 */
	return pRam != NULL && _clio_GetVideoStandard() && arm_romqsi_tag_supported(RON_USER[0]);
}

static int arm_execute_romqsi_hle(void)
{
	uint32_t ret;

	switch (RON_USER[0]) {
	case SYSINFO_TAG_FIELDFREQ:
		ret = SYSINFO_FREQ_50HZ;
		break;
	case SYSINFO_TAG_GRAPHDISPSUPP:
		ret = SYSINFO_PAL_SUPPORTED | SYSINFO_PAL_DFLT | SYSINFO_PAL_CURDISP;
		break;
	default:
		ret = SYSINFO_BADTAG;
		break;
	}

	RON_USER[0] = ret;
	REG_PC = RON_USER[14];
	return SCYCLE + NCYCLE;
}

static void arm_enter_data_abort(uint32_t addr, uint32_t type)
{
	if (BUS_FAULTED)
		return;

	BUS_FAULTED = true;
	MAS_Access_Exept = true;
	LAST_FAULT_ADDR = addr;
	LAST_FAULT_PC = current_instr_pc;
	LAST_FAULT_TYPE = type;

	SPSR[arm_mode_table[0x17]] = CPSR;
	SETI(1);
	SETM(0x17);
	load(14, current_instr_pc + 8);
	REG_PC = 0x00000010;
}

void _arm_DataAbort(uint32_t addr, uint32_t type)
{
	if (STRICT_BUS_FAULTS)
		arm_enter_data_abort(addr, type);
	else {
		LAST_FAULT_ADDR = addr;
		LAST_FAULT_PC = current_instr_pc;
		LAST_FAULT_TYPE = type;
	}
}

#define CP15_CTRL_MMU          (1u << 0)
#define CP15_CTRL_IDC          (1u << 2)
#define CP15_CTRL_WRITE_BUFFER (1u << 3)
#define CP15_CACHE_LINE_SIZE   16u
#define CP15_CACHE_LINES       256u

static INLINE bool cp15_mmu_enabled(void) { return (CP15_CONTROL & CP15_CTRL_MMU) != 0; }
static INLINE bool cp15_cache_enabled(void) { return (CP15_CONTROL & (CP15_CTRL_MMU | CP15_CTRL_IDC)) == (CP15_CTRL_MMU | CP15_CTRL_IDC); }
static INLINE bool cp15_write_buffer_enabled(void) { return (CP15_CONTROL & (CP15_CTRL_MMU | CP15_CTRL_WRITE_BUFFER)) == (CP15_CTRL_MMU | CP15_CTRL_WRITE_BUFFER); }

static void cp15_cache_flush(void)
{
	memset(arm.CacheValid, 0, sizeof(arm.CacheValid));
	CP15_CACHE_FLUSHES++;
}

static bool cp15_ram_span(uint32_t addr, uint32_t len)
{
	return addr < RAMSIZE && len <= RAMSIZE && addr + len <= RAMSIZE;
}

static void arm_note_highram_read(uint32_t addr)
{
	if (addr >= REAL_MAIN_RAM_BYTES && addr < RAMSIZE) {
		if (!arm_highram_read_count) arm_highram_first_read = addr;
		arm_highram_read_count++;
		arm_highram_last_read = addr;
	}
}

static void arm_note_highram_write(uint32_t addr)
{
	if (addr >= REAL_MAIN_RAM_BYTES && addr < RAMSIZE) {
		if (!arm_highram_write_count) arm_highram_first_write = addr;
		arm_highram_write_count++;
		arm_highram_last_write = addr;
	}
}

uint32_t _arm_HighRamReadCount(void) { return arm_highram_read_count; }
uint32_t _arm_HighRamWriteCount(void) { return arm_highram_write_count; }
uint32_t _arm_HighRamFirstRead(void) { return arm_highram_first_read; }
uint32_t _arm_HighRamFirstWrite(void) { return arm_highram_first_write; }
uint32_t _arm_HighRamLastRead(void) { return arm_highram_last_read; }
uint32_t _arm_HighRamLastWrite(void) { return arm_highram_last_write; }

static void arm_raw_ram_write_masked(uint32_t addr, uint32_t data, uint8_t mask)
{
	addr &= ~3u;
	if (!cp15_ram_span(addr, 4))
		return;
#ifdef MSB_FIRST
	if (mask & 0x8) pRam[addr + 0] = (uint8_t)(data >> 24);
	if (mask & 0x4) pRam[addr + 1] = (uint8_t)(data >> 16);
	if (mask & 0x2) pRam[addr + 2] = (uint8_t)(data >> 8);
	if (mask & 0x1) pRam[addr + 3] = (uint8_t)data;
#else
	if (mask & 0x1) pRam[addr + 0] = (uint8_t)data;
	if (mask & 0x2) pRam[addr + 1] = (uint8_t)(data >> 8);
	if (mask & 0x4) pRam[addr + 2] = (uint8_t)(data >> 16);
	if (mask & 0x8) pRam[addr + 3] = (uint8_t)(data >> 24);
#endif
}

void _arm_FlushWriteBuffer(void)
{
	uint8_t i;
	for (i = 0; i < arm.WriteBufferCount; i++)
		arm_raw_ram_write_masked(arm.WriteBufferAddr[i], arm.WriteBufferData[i], arm.WriteBufferMask[i]);
	if (arm.WriteBufferCount)
		CP15_WB_FLUSHES++;
	arm.WriteBufferCount = 0;
}

static void cp15_write_buffer_drain_one(void)
{
	uint8_t i;
	if (!arm.WriteBufferCount)
		return;
	arm_raw_ram_write_masked(arm.WriteBufferAddr[0], arm.WriteBufferData[0], arm.WriteBufferMask[0]);
	for (i = 1; i < arm.WriteBufferCount; i++) {
		arm.WriteBufferAddr[i - 1] = arm.WriteBufferAddr[i];
		arm.WriteBufferData[i - 1] = arm.WriteBufferData[i];
		arm.WriteBufferMask[i - 1] = arm.WriteBufferMask[i];
	}
	arm.WriteBufferCount--;
}

static void cp15_write_buffer_enqueue(uint32_t addr, uint32_t data, uint8_t mask)
{
	uint8_t i;
	addr &= ~3u;
	for (i = 0; i < arm.WriteBufferCount; i++) {
		if ((arm.WriteBufferAddr[i] & ~3u) == addr) {
			uint32_t old = arm.WriteBufferData[i];
			if (mask & 0x1) old = (old & ~0x000000ffu) | (data & 0x000000ffu);
			if (mask & 0x2) old = (old & ~0x0000ff00u) | (data & 0x0000ff00u);
			if (mask & 0x4) old = (old & ~0x00ff0000u) | (data & 0x00ff0000u);
			if (mask & 0x8) old = (old & ~0xff000000u) | (data & 0xff000000u);
			arm.WriteBufferData[i] = old;
			arm.WriteBufferMask[i] |= mask;
			return;
		}
	}
	if (arm.WriteBufferCount >= 8)
		cp15_write_buffer_drain_one();
	arm.WriteBufferAddr[arm.WriteBufferCount] = addr;
	arm.WriteBufferData[arm.WriteBufferCount] = data;
	arm.WriteBufferMask[arm.WriteBufferCount] = mask;
	arm.WriteBufferCount++;
}

static bool cp15_write_buffer_forward(uint32_t addr, uint32_t *data)
{
	int i;
	uint32_t out;
	uint8_t have = 0;
	if (!data)
		return false;
	addr &= ~3u;
	out = _mem_read32(addr);
	for (i = (int)arm.WriteBufferCount - 1; i >= 0; i--) {
		if ((arm.WriteBufferAddr[i] & ~3u) == addr) {
			uint32_t d = arm.WriteBufferData[i];
			uint8_t m = arm.WriteBufferMask[i];
			if (m & 0x1) { out = (out & ~0x000000ffu) | (d & 0x000000ffu); have = 1; }
			if (m & 0x2) { out = (out & ~0x0000ff00u) | (d & 0x0000ff00u); have = 1; }
			if (m & 0x4) { out = (out & ~0x00ff0000u) | (d & 0x00ff0000u); have = 1; }
			if (m & 0x8) { out = (out & ~0xff000000u) | (d & 0xff000000u); have = 1; }
		}
	}
	*data = out;
	return have != 0;
}

static uint32_t cp15_cache_read32(uint32_t pa)
{
	uint32_t line_addr = pa & ~(CP15_CACHE_LINE_SIZE - 1u);
	uint32_t index = (line_addr >> 4) & (CP15_CACHE_LINES - 1u);
	uint32_t tag = line_addr >> 12;
	uint32_t off = pa & (CP15_CACHE_LINE_SIZE - 1u);
	uint32_t value;
	if (!arm.CacheValid[index] || arm.CacheTag[index] != tag) {
		if (!cp15_ram_span(line_addr, CP15_CACHE_LINE_SIZE))
			return _mem_read32(pa & ~3u);
		memcpy(arm.CacheData[index], pRam + line_addr, CP15_CACHE_LINE_SIZE);
		arm.CacheTag[index] = tag;
		arm.CacheValid[index] = 1;
	}
#ifdef MSB_FIRST
	value = ((uint32_t)arm.CacheData[index][off + 0] << 24) |
	        ((uint32_t)arm.CacheData[index][off + 1] << 16) |
	        ((uint32_t)arm.CacheData[index][off + 2] << 8) |
	        ((uint32_t)arm.CacheData[index][off + 3]);
#else
	value = ((uint32_t)arm.CacheData[index][off + 3] << 24) |
	        ((uint32_t)arm.CacheData[index][off + 2] << 16) |
	        ((uint32_t)arm.CacheData[index][off + 1] << 8) |
	        ((uint32_t)arm.CacheData[index][off + 0]);
#endif
	return value;
}

static void cp15_cache_write32_if_hit(uint32_t pa, uint32_t value, uint8_t mask)
{
	uint32_t line_addr = pa & ~(CP15_CACHE_LINE_SIZE - 1u);
	uint32_t index = (line_addr >> 4) & (CP15_CACHE_LINES - 1u);
	uint32_t tag = line_addr >> 12;
	uint32_t off = pa & (CP15_CACHE_LINE_SIZE - 1u);
	if (!arm.CacheValid[index] || arm.CacheTag[index] != tag)
		return;
#ifdef MSB_FIRST
	if (mask & 0x8) arm.CacheData[index][off + 0] = (uint8_t)(value >> 24);
	if (mask & 0x4) arm.CacheData[index][off + 1] = (uint8_t)(value >> 16);
	if (mask & 0x2) arm.CacheData[index][off + 2] = (uint8_t)(value >> 8);
	if (mask & 0x1) arm.CacheData[index][off + 3] = (uint8_t)value;
#else
	if (mask & 0x1) arm.CacheData[index][off + 0] = (uint8_t)value;
	if (mask & 0x2) arm.CacheData[index][off + 1] = (uint8_t)(value >> 8);
	if (mask & 0x4) arm.CacheData[index][off + 2] = (uint8_t)(value >> 16);
	if (mask & 0x8) arm.CacheData[index][off + 3] = (uint8_t)(value >> 24);
#endif
}

static int cp15_domain_ok(uint32_t desc, int write)
{
	uint32_t domain = (desc >> 5) & 0xf;
	uint32_t access = (CP15_DACR >> (domain * 2)) & 3;
	uint32_t ap = (desc >> 10) & 3;
	(void)write;
	if (access == 0)
		return 0;
	if (access == 3)
		return 1;
	return ap != 0;
}

static int cp15_translate(uint32_t va, int write, int fetch, uint32_t *pa, int *cacheable, int *bufferable)
{
	uint32_t l1addr, l1, type;
	(void)fetch;
	if (pa) *pa = va;
	if (cacheable) *cacheable = 0;
	if (bufferable) *bufferable = 0;
	if (!cp15_mmu_enabled())
		return 1;
	l1addr = (CP15_TTB & 0xffffc000u) + ((va >> 18) & 0x3ffcu);
	if (!cp15_ram_span(l1addr, 4)) {
		CP15_FAR = va; CP15_FSR = 0x5;
		_arm_DataAbort(va, ARM_FAULT_MMU_TRANSLATION);
		return 0;
	}
	l1 = _mem_read32(l1addr);
	type = l1 & 3u;
	if (type == 2) {
		if (!cp15_domain_ok(l1, write)) {
			CP15_FAR = va; CP15_FSR = 0xd;
			_arm_DataAbort(va, ARM_FAULT_MMU_PERMISSION);
			return 0;
		}
		if (pa) *pa = (l1 & 0xfff00000u) | (va & 0x000fffffu);
		if (cacheable) *cacheable = (l1 & 0x8u) != 0;
		if (bufferable) *bufferable = (l1 & 0x4u) != 0;
		return 1;
	} else if (type == 1) {
		uint32_t l2base = l1 & 0xfffffc00u;
		uint32_t l2addr = l2base + ((va >> 10) & 0x3fcu);
		uint32_t l2, l2type;
		if (!cp15_domain_ok(l1, write) || !cp15_ram_span(l2addr, 4)) {
			CP15_FAR = va; CP15_FSR = 0x7;
			_arm_DataAbort(va, ARM_FAULT_MMU_TRANSLATION);
			return 0;
		}
		l2 = _mem_read32(l2addr);
		l2type = l2 & 3u;
		if (l2type == 1) {
			if (pa) *pa = (l2 & 0xffff0000u) | (va & 0x0000ffffu);
		} else if (l2type == 2) {
			if (pa) *pa = (l2 & 0xfffff000u) | (va & 0x00000fffu);
		} else {
			CP15_FAR = va; CP15_FSR = 0x7;
			_arm_DataAbort(va, ARM_FAULT_MMU_TRANSLATION);
			return 0;
		}
		if (cacheable) *cacheable = (l2 & 0x8u) != 0;
		if (bufferable) *bufferable = (l2 & 0x4u) != 0;
		return 1;
	}
	CP15_FAR = va; CP15_FSR = 0x5;
	_arm_DataAbort(va, ARM_FAULT_MMU_TRANSLATION);
	return 0;
}

uint32_t _arm_CP15Control(void) { return CP15_CONTROL; }
uint32_t _arm_CP15Ops(void) { return CP15_OPS; }
uint32_t _arm_CP15CacheFlushes(void) { return CP15_CACHE_FLUSHES; }
uint32_t _arm_CP15WriteBufferFlushes(void) { return CP15_WB_FLUSHES; }

void _arm_SetCPSR(uint32_t a)
{
#if 0
	if (arm_mode_table[a & 0x1f] == ARM_MODE_UNK) {
		//!!Exeption!!
	}
#endif
	a |= 0x10;
	ARM_Change_ModeSafe(a);
	CPSR = a & 0xf00000df;
}


static INLINE void SETM(uint32_t a)
{
#if 0
	if (arm_mode_table[a & 0x1f] == ARM_MODE_UNK) {
		//!!Exeption!!
	}
#endif
	a |= 0x10;
	ARM_Change_ModeSafe(a);
	CPSR = (CPSR & 0xffffffe0) | (a & 0x1F);
}

// This functions d'nt change mode bits, then need no update regcur
static INLINE void SETN(bool a)
{
	CPSR = (CPSR & 0x7fffffff) | ((a ? 1 << 31 : 0));
}
static INLINE void SETZ(bool a)
{
	CPSR = (CPSR & 0xbfffffff) | ((a ? 1 << 30 : 0));
}
static INLINE void SETC(bool a)
{
	CPSR = (CPSR & 0xdfffffff) | ((a ? 1 << 29 : 0));
}
static INLINE void SETV(bool a)
{
	CPSR = (CPSR & 0xefffffff) | ((a ? 1 << 28 : 0));
}
static INLINE void SETI(bool a)
{
	CPSR = (CPSR & 0xffffff7f) | ((a ? 1 << 7 : 0));
}
static INLINE void SETF(bool a)
{
	CPSR = (CPSR & 0xffffffbf) | ((a ? 1 << 6 : 0));
}


///////////////////////////////////////////////////////////////
// Macros
///////////////////////////////////////////////////////////////
#define ISN  ((CPSR >> 31) & 1)
#define ISZ  ((CPSR >> 30) & 1)
#define ISC  ((CPSR >> 29) & 1)
#define ISV  ((CPSR >> 28) & 1)

#define MODE ((CPSR & 0x1f))
#define ISI  ((CPSR >> 7) & 1)
#define ISF  ((CPSR >> 6) & 1)

#define ROTR(val, shift) ((shift)) ? (((val) >> (shift)) | ((val) << (32 - (shift)))) : (val)

static inline unsigned long __rotr(unsigned long val, unsigned long shift)
{
	if (!shift)
		return val;
	return (val >> shift) | (val << (32 - shift));
}


uint8_t *_arm_Init(void)
{
	int i;

#ifdef MIPSREC
	cpu = &cpuRec;
#else
	cpu = &cpuInt;
#endif

	cpu->Init();

	MAS_Access_Exept = false;
	BUS_FAULTED = false;
	LAST_FAULT_ADDR = 0;
	LAST_FAULT_PC = 0;
	LAST_FAULT_TYPE = ARM_FAULT_NONE;
	arm_fiq_entry_count = 0;
	arm_unaligned_prefetch_count = 0;
	arm_unaligned_prefetch_last = 0;
	arm_unaligned_prefetch_fetch = 0;
	arm_mirrored_prefetch_count = 0;
	arm_mirrored_prefetch_last = 0;
	arm_mirrored_prefetch_fetch = 0;
	if (!strict_bus_configured)
		STRICT_BUS_FAULTS = true;
	CP15_ID = 0x41560610u;
	CP15_CONTROL = 0;
	CP15_TTB = 0;
	CP15_DACR = 0;
	CP15_FSR = 0;
	CP15_FAR = 0;
	CP15_LAST_OP = 0;
	CP15_OPS = 0;
	CP15_CACHE_FLUSHES = 0;
	CP15_WB_FLUSHES = 0;
	memset(arm.CacheValid, 0, sizeof(arm.CacheValid));
	arm.WriteBufferCount = 0;

	CYCLES = 0;
	for (i = 0; i < 16; i++)
		RON_USER[i] = 0;

	for (i = 0; i < 2; i++) {
		RON_SVC[i] = 0;
		RON_ABT[i] = 0;
		RON_IRQ[i] = 0;
		RON_UND[i] = 0;
	}

	for (i = 0; i < 7; i++)
		RON_CASH[i] = RON_FIQ[i] = 0;

	gSecondROM = 0;
	pRam   = malloc(RAMSIZE * sizeof(uint8_t));
	pRom   = malloc(ROMSIZE * 2 * sizeof(uint8_t));
	pNVRam = malloc(NVRAMSIZE * sizeof(uint8_t));

	memset( pRam, 0, RAMSIZE);
	memset( pRom, 0, ROMSIZE * 2);
	memset( pNVRam, 0, NVRAMSIZE);
	arm_highram_read_count = arm_highram_write_count = 0;
	arm_highram_last_read = arm_highram_last_write = 0;
	arm_highram_first_read = arm_highram_first_write = 0;
	gFIQ = false;

	readNvRam(pNVRam);
	// Endian swap for loaded ROM image

	REG_PC = 0x03000000;
	_arm_SetCPSR(0x13); //set svc mode

	return (uint8_t*)pRam;
}

void _arm_Destroy(void)
{
	cpu->Destroy();

	/* Does nothing right now */
	writeNvRam();
	//io_interface(EXT_WRITE_NVRAM, pNVRam);//_3do_SaveNVRAM(pNVRam);

	free(pNVRam);
	free(pRom);
	free(pRam);
}

void _arm_Reset(void)
{
	uint_fast8_t i;

	cpu->Reset();

	gSecondROM = 0;
	CYCLES = 0;
	for (i = 0; i < 16; i++)
		RON_USER[i] = 0;

	for (i = 0; i < 2; i++) {
		RON_SVC[i] = 0;
		RON_ABT[i] = 0;
		RON_IRQ[i] = 0;
		RON_UND[i] = 0;
	}

	for (i = 0; i < 7; i++)
		RON_CASH[i] = RON_FIQ[i] = 0;

	MAS_Access_Exept = false;
	BUS_FAULTED = false;
	LAST_FAULT_ADDR = 0;
	LAST_FAULT_PC = 0;
	LAST_FAULT_TYPE = ARM_FAULT_NONE;
	arm_fiq_entry_count = 0;
	arm_unaligned_prefetch_count = 0;
	arm_unaligned_prefetch_last = 0;
	arm_unaligned_prefetch_fetch = 0;
	arm_mirrored_prefetch_count = 0;
	arm_mirrored_prefetch_last = 0;
	arm_mirrored_prefetch_fetch = 0;
	CP15_CONTROL = 0;
	CP15_TTB = 0;
	CP15_DACR = 0;
	CP15_FSR = 0;
	CP15_FAR = 0;
	CP15_LAST_OP = 0;
	CP15_OPS = 0;
	cp15_cache_flush();
	_arm_FlushWriteBuffer();

	REG_PC = 0x03000000;
	_arm_SetCPSR(0x13);     //set svc mode
	gFIQ = false;           //no FIQ!!!
	gSecondROM = 0;

	_clio_Reset();
	_madam_Reset();
}

uint32_t vall = 0, addrr = 0;
int inuse = 0;

void ldm_accur(uint32_t opc, uint32_t base, uint32_t rn_ind)
{
	uint16_t x = opc & 0xffff;
	uint16_t list = opc & 0xffff;
	uint32_t base_comp, i = 0, tmp;

	x = (x & 0x5555) + ((x >> 1) & 0x5555);
	x = (x & 0x3333) + ((x >> 2) & 0x3333);
	x = (x & 0xff) + (x >> 8);
	x = (x & 0xf) + (x >> 4);

	switch ((opc >> 23) & 3) {
	case 0:
		base -= (x << 2);
		base_comp = base + 4;
		break;
	case 1:
		base_comp = base;
		base += (x << 2);
		break;
	case 2:
		base_comp = base = base - (x << 2);
		break;
	case 3:
		base_comp = base + 4;
		base += (x << 2);
		break;
	}

	if (STRICT_BUS_FAULTS && (base_comp & 3)) {
		_arm_DataAbort(base_comp, ARM_FAULT_UNALIGNED_BLOCK);
		return;
	}

	//if(opc&(1<<21))RON_USER[rn_ind]=base;

	if ((opc & (1 << 22)) && !(opc & 0x8000)) {
		if (opc & (1 << 21)) loadusr(rn_ind, base);
		while (list) {
			if (list & 1) {
				tmp = mreadw(base_comp);
				if (MAS_Access_Exept)
					return;
				/*if(MAS_Access_Exept)
				   {
				   if(opc&(1<<21))RON_USER[rn_ind]=base;
				   break;
				   } */
				loadusr(i, tmp);
				base_comp += 4;
			}
			i++;
			list >>= 1;
		}
	} else {
		if (opc & (1 << 21)) RON_USER[rn_ind] = base;
		while (list) {
			if (list & 1) {
				tmp = mreadw(base_comp);
				if (MAS_Access_Exept)
					return;
				if (tmp == 0xF1000 && i == 0x1 && RON_USER[2] != 0xF0000 && cnbfix == 0 && (fixmode & FIX_BIT_TIMING_1)) {
					tmp += 0x1000;
				}
				//if(i==0x1&&tmp==0xF1000&&RON_USER[0]==RON_USER[i]){tmp+=0x1000;cnbfix=1;}
				if (inuse == 1 && base_comp & 0x1FFFFF) {
					if (base_comp == addrr)
						inuse = 0;
					if (tmp != vall) {
						if (tmp == 0xEFE54 && i == 0x4 && cnbfix == 0 && (fixmode & FIX_BIT_TIMING_1))
							tmp -= 0xF;
						//if(tmp==0xF1014)tmp=0x25000;
					}
				}
				RON_USER[i] = tmp;
				base_comp += 4;
			}
			i++;
			list >>= 1;
		}
		if ((opc & (1 << 22)) && arm_mode_table[MODE] /*&& !MAS_Access_Exept*/)
			_arm_SetCPSR(SPSR[arm_mode_table[MODE]]);
	}

	CYCLES -= (x - 1) * SCYCLE + NCYCLE + ICYCLE;

}


void stm_accur(uint32_t opc, uint32_t base, uint32_t rn_ind)
{
	uint16_t x = opc & 0xffff;
	uint16_t list = opc & 0x7fff;
	uint32_t base_comp, //по ней шагаем
		 i = 0;

	x = (x & 0x5555) + ((x >> 1) & 0x5555);
	x = (x & 0x3333) + ((x >> 2) & 0x3333);
	x = (x & 0xff) + (x >> 8);
	x = (x & 0xf) + (x >> 4);

	switch ((opc >> 23) & 3) {
	case 0:
		base -= (x << 2);
		base_comp = base + 4;
		break;
	case 1:
		base_comp = base;
		base += (x << 2);
		break;
	case 2:
		base_comp = base = base - (x << 2);
		break;
	case 3:
		base_comp = base + 4;
		base += (x << 2);
		break;
	}

	if (STRICT_BUS_FAULTS && (base_comp & 3)) {
		_arm_DataAbort(base_comp, ARM_FAULT_UNALIGNED_BLOCK);
		return;
	}

	if ((opc & (1 << 22))) {
		if ((opc & (1 << 21)) && (opc & ((1 << rn_ind) - 1)) ) loadusr(rn_ind, base);
		while (list) {
			if (list & 1) {
				mwritew(base_comp, rreadusr(i));
				if (MAS_Access_Exept)
					return;
				base_comp += 4;
			}
			i++;
			list >>= 1;
		}
		if (opc & (1 << 21)) loadusr(rn_ind, base);
	} else {
		if ((opc & (1 << 21)) && (opc & ((1 << rn_ind) - 1)) ) RON_USER[rn_ind] = base;
		while (list) {
			if (list & 1) {
				int aac = RON_USER[i];
				mwritew(base_comp, aac);
				if (MAS_Access_Exept)
					return;
				if (base_comp & 0x1FFFFF) {
					addrr = base_comp; vall = aac; inuse = 1;
				}
				base_comp += 4;
			}
			i++;
			list >>= 1;
		}
		if (opc & (1 << 21)) RON_USER[rn_ind] = base;
	}

	if ((opc & 0x8000) /*&& !MAS_Access_Exept*/) {
		mwritew(base_comp, RON_USER[15] + 8);
		if (MAS_Access_Exept)
			return;
	}

	CYCLES -= (x - 2) * SCYCLE + NCYCLE + NCYCLE;
}



void arm60_BDT(uint32_t opc)
{
	uint32_t base;
	uint32_t rn_ind = (opc >> 16) & 0xf;

	if (rn_ind == 0xf)
		base = RON_USER[rn_ind] + 8;
	else
		base = RON_USER[rn_ind];

	if (opc & (1 << 20)) { //memory or register?
		if (opc & 0x8000)
			CYCLES -= SCYCLE + NCYCLE;

		ldm_accur(opc, base, rn_ind);

	} else  //из регистра в память
		stm_accur(opc, base, rn_ind);
}

//------------------------------math SWI------------------------------------------------
typedef struct TagArg {
	uint32_t Type;
	uint32_t Arg;
} TagItem;

#ifdef HLE_SWI
static void decode_swi_lle(void)
{
	SPSR[arm_mode_table[0x13]] = CPSR;
	
	SETI(1);
	SETM(0x13);
	
	RON_USER[14] = RON_USER[15];
	RON_USER[15] = 0x00000008;
}

static void decode_swi(const uint32_t op_)
{
	CYCLES -= (SCYCLE + NCYCLE);  // +2S+1N
	switch(op_ & 0x000FFFFF)
    {
    case 0x50000:
      freedo_swi_hle_0x50000(pRam,RON_USER[0],RON_USER[1],RON_USER[2]);
      return;
    case 0x50001:
      freedo_swi_hle_0x50001(pRam,RON_USER[0],RON_USER[1],RON_USER[2]);
      return;
    case 0x50002:
      freedo_swi_hle_0x50002(pRam,RON_USER[0],RON_USER[1],RON_USER[2],RON_USER[3]);
      return;
    case 0x50003:
      break;
    case 0x50004:
      break;
    case 0x50005:
      freedo_swi_hle_0x50005(pRam,RON_USER[0],RON_USER[1],RON_USER[2],RON_USER[3]);
      return;
    case 0x50006:
      freedo_swi_hle_0x50006(pRam,RON_USER[0],RON_USER[1],RON_USER[2],RON_USER[3]);
      return;
    case 0x50007:
      freedo_swi_hle_0x50007(pRam,RON_USER[0],RON_USER[1],RON_USER[2]);
      return;
    case 0x50008:
      freedo_swi_hle_0x50008(pRam,RON_USER[0],RON_USER[1],RON_USER[2]);
      return;
    case 0x50009:
      freedo_swi_hle_0x50009(pRam,RON_USER[0],RON_USER[1],RON_USER[2],RON_USER[3]);
      return;
    case 0x5000A:
      break;
    case 0x5000B:
      break;
    case 0x5000C:
      RON_USER[0] = freedo_swi_hle_0x5000C(pRam,RON_USER[0],RON_USER[1]);
      return;
    case 0x5000E:
      freedo_swi_hle_0x5000E(pRam,RON_USER[0],RON_USER[1],RON_USER[2]);
      return;
    case 0x5000F:
      RON_USER[0] = freedo_swi_hle_0x5000F(pRam,RON_USER[0]);
      return;
    case 0x50010:
      RON_USER[0] = freedo_swi_hle_0x50010(pRam,RON_USER[0]);
      return;
    case 0x50011:
      freedo_swi_hle_0x50011(pRam,RON_USER[0],RON_USER[1],RON_USER[2],RON_USER[3]);
      return;
    case 0x50012:
      freedo_swi_hle_0x50012(pRam,RON_USER[0]);
      return;
    }

	return decode_swi_lle();
}
#else
static void decode_swi(void)
{

	SPSR[arm_mode_table[0x13]] = CPSR;

	SETI(1);
	SETM(0x13);

	load(14, REG_PC);

	REG_PC = 0x00000008;
	CYCLES -= SCYCLE + NCYCLE; // +2S+1N
}
#endif


uint32_t carry_out = 0;

void ARM_SET_C(uint32_t x)
{
	//old_C=(CPSR>>29)&1;

	CPSR = ((CPSR & 0xdfffffff) | (((x) & 1) << 29));
}

#define ARM_SET_Z(x)    (CPSR = ((CPSR & 0xbfffffff) | ((x) == 0 ? 0x40000000 : 0)))
#define ARM_SET_N(x)    (CPSR = ((CPSR & 0x7fffffff) | ((x) & 0x80000000)))
#define ARM_GET_C       ((CPSR >> 29) & 1)

static INLINE void ARM_SET_ZN(uint32_t val)
{
	if (val)
		CPSR = ((CPSR & 0x3fffffff) | (val & 0x80000000));
	else
		CPSR = ((CPSR & 0x3fffffff) | 0x40000000);
}

static INLINE void ARM_SET_CV(uint32_t rd, uint32_t op1, uint32_t op2)
{
	//old_C=(CPSR>>29)&1;

	CPSR = (CPSR & 0xcfffffff) |
	       ((((op1 & op2) | ((~rd) & (op1 | op2))) & 0x80000000) >> 2) |
	       (((((op1 & (op2 & (~rd))) | ((~op1) & (~op2) & rd))) & 0x80000000) >> 3);

}

static INLINE void ARM_SET_CV_sub(uint32_t rd, uint32_t op1, uint32_t op2)
{
	//old_C=(CPSR>>29)&1;

	CPSR = (CPSR & 0xcfffffff) |
	       //(( ( ~( ((~op1) & op2) | (rd&((~op1)|op2))) )&0x80000000)>>2) |
	       ((((op1 & (~op2)) | ((~rd) & (op1 | (~op2)))) & 0x80000000) >> 2) |
	       (((((op1 & ((~op2) & (~rd))) | ((~op1) & op2 & rd))) & 0x80000000) >> 3);
}

uint32_t ARM_SHIFT_NSC(uint32_t value, uint8_t shift, uint8_t type)
{
	switch (type) {
	case 0:
		if (shift) {
			if (shift > 32) carry_out = (0);
			else carry_out = (((value << (shift - 1)) & 0x80000000) >> 31);
		}else carry_out = ARM_GET_C;

		if (shift == 0) return value;
		if (shift > 31) return 0;
		return value << shift;
	case 1:

		if (shift) {
			if (shift > 32) carry_out = (0);
			else carry_out = ((value >> (shift - 1)) & 1);
		}else carry_out = ARM_GET_C;

		if (shift == 0) return value;
		if (shift > 31) return 0;
		return value >> shift;
	case 2:

		if (shift) {
			if (shift > 32) carry_out = ((((signed int)value) >> 31) & 1);
			else carry_out = ((((signed int)value) >> (shift - 1)) & 1);
		}else carry_out = ARM_GET_C;

		if (shift == 0) return value;
		if (shift > 31) return (((signed int)value) >> 31);
		return (((signed int)value) >> shift);
	case 3:

		if (shift) {
			if (shift & 31) carry_out = ((value >> (shift - 1)) & 1);
			else carry_out = ((value >> 31) & 1);
		}else carry_out = ARM_GET_C;

		shift &= 31;
		if (shift == 0) return value;
		return ROTR(value, shift);
	case 4:
		carry_out = value & 1;
		return (value >> 1) | (ARM_GET_C << 31);
	}
	return 0;
}

uint32_t  ARM_SHIFT_SC(uint32_t value, uint8_t shift, uint8_t type)
{
	uint32_t tmp;

	switch (type) {
	case 0:
		if (shift) {
			if (shift > 32) ARM_SET_C(0);
			else ARM_SET_C(((value << (shift - 1)) & 0x80000000) >> 31);
		}else
			return value;
		if (shift > 31)
			return 0;
		return value << shift;
	case 1:
		if (shift) {
			if (shift > 32)
				ARM_SET_C(0);
			else
				ARM_SET_C((value >> (shift - 1)) & 1);
		}else
			return value;
		if (shift > 31)
			return 0;
		return value >> shift;
	case 2:
		if (shift) {
			if (shift > 32) ARM_SET_C((((signed int)value) >> 31) & 1);
			else ARM_SET_C((((signed int)value) >> (shift - 1)) & 1);
		}else
			return value;
		if (shift > 31)
			return (((signed int)value) >> 31);
		return ((signed int)value) >> shift;
	case 3:
		if (shift) {
			shift = ((shift) & 31);
			if (shift) {
				ARM_SET_C((value >> (shift - 1)) & 1);
			} else {
				ARM_SET_C((value >> 31) & 1);
			}
		}else
			return value;
		return ROTR(value, shift);
	case 4:
		tmp = ARM_GET_C << 31;
		ARM_SET_C(value & 1);
		return (value >> 1) | (tmp);
	}

	return 0;
}



void arm60_SWAP(uint32_t cmd)
{
	uint32_t tmp, addr;

	REG_PC += 4;
	addr = RON_USER[(cmd >> 16) & 0xf];
	REG_PC += 4;

	if (cmd & (1 << 22)) {
		tmp = mreadb(addr);
		if (MAS_Access_Exept) return;
		mwriteb(addr, RON_USER[cmd & 0xf]);
		if (MAS_Access_Exept) return;
		REG_PC -= 8;
		RON_USER[(cmd >> 12) & 0xf] = tmp;
	} else {
		if (STRICT_BUS_FAULTS && (addr & 3)) { _arm_DataAbort(addr, ARM_FAULT_DEVICE_UNALIGNED_READ); return; }
		tmp = mreadw(addr);
		if (MAS_Access_Exept) return;
		mwritew(addr, RON_USER[cmd & 0xf]);
		if (MAS_Access_Exept) return;
		REG_PC -= 8;
		if (addr & 3)
			tmp = (tmp >> ((addr & 3) << 3)) |
			      (tmp << (32 - ((addr & 3) << 3)));
		RON_USER[(cmd >> 12) & 0xf] = tmp;
	}
}

static INLINE uint32_t calcbits(uint32_t num)
{
	if ((num & 0xFFFF0000) && (num & 0x0000FFFF))
		return 32; //3doh fix
	return 0;
}

static const bool is_logic[] = {
	true,  true,  false, false,
	false, false, false, false,
	true,  true,  false, false,
	true,  true,  true,  true
};

void arm60_BRANCH(unsigned long cmd)
{

	if (cmd & (1 << 24)) {
		RON_USER[14] = REG_PC;
	}

	REG_PC += (((cmd & 0xffffff) | ((cmd & 0x800000) ? 0xff000000 : 0)) << 2) + 4;

	CYCLES -= SCYCLE + NCYCLE;                             //2S+1N

}

/*
	MUL{cond}{S} Rd,Rm,Rs
	MLA{cond}{S} Rd,Rm,Rs,Rn
	Rd = Rm * Rs [+ Rn]
*/
void arm60_MULT(unsigned long cmd)
{
	unsigned int Rd, Rn, Rs, Rm, A, S;
	unsigned int res;

	A = cmd & (1 << 21);
	S = cmd & (1 << 20);
	Rd = (cmd >> 16) & 0xf;
	Rn = (cmd >> 12) & 0xf;
	Rs = (cmd >> 8) & 0xf;
	Rm = cmd & 0xf;

	res = ((calcbits(RON_USER[Rs]) + 5) >> 1) - 1;
	if (res > 16)
		CYCLES -= 16;
	else
		CYCLES -= res;

	// According to arm60.pdf if Rd == Rm the multiplication returns 0
	// because of hardware algo which uses Rd to store intermediate values.
	// R15 should not be used as operand or destination register, because
	// the result is unpredicted
	if (Rd == Rm) {
		// TODO: Find which game really uses it
		res = (A ? RON_USER[Rn] : 0);
	} else {
		res = RON_USER[Rm] * RON_USER[Rs];
		if (A) {
			res += RON_USER[Rn];
		}
	}

	if (S) {
		ARM_SET_ZN(res);
	}

	RON_USER[Rd] = res;
}

void arm60_SDT(unsigned long cmd)
{
	unsigned char shift, shtype;
	uint32_t pc_tmp;
	uint32_t W; // writeback flag
	uint32_t P; // pre/post indexing
	uint32_t L; // load/store
	uint32_t B; // byte/word
	uint32_t I; // reg+shift/immediate
	uint32_t Rn, Rd; // base register, dest register

	unsigned int base, tbas;
	unsigned int oper2;
	unsigned int val, rora;

	L = !!(cmd & (1 << 20));
	W = !!(cmd & (1 << 21));
	B = !!(cmd & (1 << 22));
	P = !!(cmd & (1 << 24));
	I = !!(cmd & (1 << 25));
	Rn = (cmd >> 16) & 0xf;
	Rd = (cmd >> 12) & 0xf;

	pc_tmp = REG_PC;
	REG_PC += 4;

	if (I) { // reg+shift
		shtype = (cmd >> 5) & 0x3;
		if ((cmd >> 4) & 1) {	// shift by reg
			shift = (RON_USER[(cmd >> 8) & 0xf]) & 0xff;
			REG_PC += 4;
		} else {		// shift by imm
			shift = (cmd >> 7) & 0x1f;
			if (!shift) {          //revisar
				switch (shtype) {
				case 0:
					break;
				case 3:
					shtype++;
					break;
				default:
					shift = 32;
					break;
				}
			}
		}

		oper2 = ARM_SHIFT_NSC(RON_USER[cmd & 0xf], shift, shtype);
	} else { // immediate
		oper2 = (cmd & 0xfff);
	}

	tbas = base = RON_USER[(Rn)];

	if (!(cmd & (1 << 23)))
		oper2 = 0 - oper2;

	if (P)
		tbas = base = base + oper2;
	else
		base = base + oper2;


	if (L) { //load
		if (B) {				//bytes
			val = mreadb(tbas) & 0xff;
			if (MAS_Access_Exept) return;
		} else {				//words/halfwords
			val = mreadw(tbas);
			if (MAS_Access_Exept) return;
			rora = tbas & 3;
			if ((rora))
				val = __rotr(val, rora * 8);
		}

		if (Rd == 15) {
			CYCLES -= SCYCLE + NCYCLE;	// +1S+1N if R15 load
		}

		CYCLES -= NCYCLE + ICYCLE;		// +1N+1I
		REG_PC = pc_tmp;

		if (W || !P)
			load(Rn, base);

		if (W && !P)
			loadusr(Rd, val);		//privil mode
		else
			load(Rd, val);

	} else { // store
		if (W && !P)
			val = rreadusr(Rd);		// privil mode
		else
			val = RON_USER[Rd];

		REG_PC = pc_tmp;
		CYCLES -= -SCYCLE + 2 * NCYCLE;		// 2N

		if (B)					//bytes
			mwriteb(tbas, val);
		else					//words
			mwritew(tbas, val);
		if (MAS_Access_Exept) return;

		if (W || !P)
			load(Rn, base);
	}
}

static void arm60_undefined_instruction(void)
{
	SPSR[arm_mode_table[0x1b]] = CPSR;
	SETI(1);
	SETM(0x1b);
	load(14, REG_PC);
	REG_PC = 0x00000004;
	CYCLES -= SCYCLE + NCYCLE;
}

void arm60_COPRO(unsigned long cmd)
{
	uint32_t cpnum = (cmd >> 8) & 0xf;
	uint32_t crn = (cmd >> 16) & 0xf;
	uint32_t rd = (cmd >> 12) & 0xf;
	uint32_t l = (cmd >> 20) & 1;

	if (cpnum != 15 || (cmd & 0x10) == 0 || crn > 7) {
		if (STRICT_BUS_FAULTS) {
			LAST_FAULT_ADDR = REG_PC - 4;
			LAST_FAULT_PC = current_instr_pc;
			LAST_FAULT_TYPE = ARM_FAULT_CP15_UNDEFINED;
		}
		arm60_undefined_instruction();
		return;
	}

	CP15_LAST_OP = cmd;
	CP15_OPS++;

	if (l) {
		uint32_t value = 0;
		switch (crn) {
		case 0: value = CP15_ID ? CP15_ID : 0x41560610u; break;
		case 1: value = CP15_CONTROL; break;
		case 2: value = CP15_TTB; break;
		case 3: value = CP15_DACR; break;
		case 5: value = CP15_FSR; break;
		case 6: value = CP15_FAR; break;
		case 7: value = 0; break;
		default: value = 0; break;
		}
		if (rd == 15)
			CPSR = (CPSR & 0x0fffffffu) | (value & 0xf0000000u);
		else
			RON_USER[rd] = value;
	} else {
		uint32_t value = RON_USER[rd];
		switch (crn) {
		case 1: {
			uint32_t old = CP15_CONTROL;
			CP15_CONTROL = value & 0x00003fffu;
			if ((old & CP15_CTRL_IDC) && !(CP15_CONTROL & CP15_CTRL_IDC))
				cp15_cache_flush();
			if ((old & CP15_CTRL_WRITE_BUFFER) && !(CP15_CONTROL & CP15_CTRL_WRITE_BUFFER))
				_arm_FlushWriteBuffer();
			break;
		}
		case 2: CP15_TTB = value & 0xffffc000u; break;
		case 3: CP15_DACR = value; break;
		case 5: CP15_FSR = value; break;
		case 6: CP15_FAR = value; break;
		case 7:
			cp15_cache_flush();
			_arm_FlushWriteBuffer();
			break;
		default: break;
		}
	}

	CYCLES -= SCYCLE + NCYCLE;
}

enum {
	OPCODE_AND,	/* 0000 */
	OPCODE_EOR,	/* 0001 */
	OPCODE_SUB,	/* 0010 */
	OPCODE_RSB,	/* 0011 */
	OPCODE_ADD,	/* 0100 */
	OPCODE_ADC,	/* 0101 */
	OPCODE_SBC,	/* 0110 */
	OPCODE_RSC,	/* 0111 */
	OPCODE_TST,	/* 1000 */
	OPCODE_TEQ,	/* 1001 */
	OPCODE_CMP,	/* 1010 */
	OPCODE_CMN,	/* 1011 */
	OPCODE_ORR,	/* 1100 */
	OPCODE_MOV,	/* 1101 */
	OPCODE_BIC,	/* 1110 */
	OPCODE_MVN	/* 1111 */
};

/* Rd = op1 operation op2 [operation2 op3] */
void arm60_ALU(unsigned long cmd)
{
	uint32_t Rn, Rd, S, I;
	unsigned char shift, shtype;
	unsigned long op2, op1, pc_tmp;

	I = cmd & (1 << 25); // 0 - op2 is shift+reg, 1 - op2 is rotate+imm8
	S = cmd & (1 << 20);
	Rn = (cmd >> 16) & 0xf;
	Rd = (cmd >> 12) & 0xf;

	/////////////////////////////////////////////SHIFT
	pc_tmp = REG_PC;
	REG_PC += 4;
	if (I) {
		op2 = cmd & 0xff;
		if (((cmd >> 7) & 0x1e)) {
			op2 = __rotr(op2, (cmd >> 7) & 0x1e);
		}
		op1 = RON_USER[Rn];
	} else {
		shtype = (cmd >> 5) & 0x3;
		if (cmd & (1 << 4)) {
			shift = ((cmd >> 8) & 0xf);
			shift = (RON_USER[shift]) & 0xff;
			REG_PC += 4;
			op2 = RON_USER[cmd & 0xf];
			op1 = RON_USER[Rn];
			CYCLES -= ICYCLE;
		} else {
			shift = (cmd >> 7) & 0x1f;

			if (!shift) {
				if (shtype) {
					if (shtype == 3) shtype++;
					else shift = 32;
				}
			}
			op2 = RON_USER[cmd & 0xf];
			op1 = RON_USER[Rn];
		}
		op2 = ARM_SHIFT_NSC(op2, shift, shtype);
	}

	REG_PC = pc_tmp;

	if (S && is_logic[((cmd >> 21) & 0xf)])
		ARM_SET_C(carry_out);

	switch ((cmd >> 20) & 0x1f) {
	case 0: // OPCODE_AND
		RON_USER[Rd] = op1 & op2;
		break;
	case 2: // OPCODE_EOR
		RON_USER[Rd] = op1 ^ op2;
		break;
	case 4: // OPCODE_SUB
		RON_USER[Rd] = op1 - op2;
		break;
	case 6: // OPCODE_RSB
		RON_USER[Rd] = op2 - op1;
		break;
	case 8: // OPCODE_ADD
		RON_USER[Rd] = op1 + op2;
		break;
	case 10: // OPCODE_ADC
		RON_USER[Rd] = op1 + op2 + ARM_GET_C;
		break;
	case 12: // OPCODE_SBC
		RON_USER[Rd] = op1 - op2 - (ARM_GET_C ^ 1);
		break;
	case 14: // OPCODE_RSC
		RON_USER[Rd] = op2 - op1 - (ARM_GET_C ^ 1);
		break;
	case 16: // OPCODE_TST
	case 20: // OPCODE_CMP
		if ((cmd >> 22) & 1)
			RON_USER[Rd] = SPSR[arm_mode_table[CPSR & 0x1f]];
		else
			RON_USER[Rd] = CPSR;

		return;
	case 18: // OPCODE_TEQ
	case 22: // OPCODE_CMN
		if (!((cmd >> 16) & 0x1) || !(arm_mode_table[MODE])) {
			if ((cmd >> 22) & 1)
				SPSR[arm_mode_table[MODE]] = (SPSR[arm_mode_table[MODE]] & 0x0fffffff) | (op2 & 0xf0000000);
			else
				CPSR = (CPSR & 0x0fffffff) | (op2 & 0xf0000000);
		} else {
			if ((cmd >> 22) & 1)
				SPSR[arm_mode_table[MODE]] = op2 & 0xf00000df;
			else
				_arm_SetCPSR(op2);
		}
		return;
	case 24: // OPCODE_ORR
		RON_USER[Rd] = op1 | op2;
		break;
	case 26: // OPCODE_MOV
		RON_USER[Rd] = op2;
		break;
	case 28: // OPCODE_BIC
		RON_USER[Rd] = op1 & (~op2);
		break;
	case 30: // OPCODE_MVN
		RON_USER[Rd] = ~op2;
		break;
	case 1:
		RON_USER[Rd] = op1 & op2;
		ARM_SET_ZN(RON_USER[Rd]);
		break;
	case 3:
		RON_USER[Rd] = op1 ^ op2;
		ARM_SET_ZN(RON_USER[Rd]);
		break;
	case 5:
		RON_USER[Rd] = op1 - op2;
		ARM_SET_ZN(RON_USER[Rd]);
		ARM_SET_CV_sub(RON_USER[Rd], op1, op2);
		break;
	case 7:
		RON_USER[Rd] = op2 - op1;
		ARM_SET_ZN(RON_USER[Rd]);
		ARM_SET_CV_sub(RON_USER[Rd], op2, op1);
		break;
	case 9:
		RON_USER[Rd] = op1 + op2;
		ARM_SET_ZN(RON_USER[Rd]);
		ARM_SET_CV(RON_USER[Rd], op1, op2);
		break;
	case 11:
		RON_USER[Rd] = op1 + op2 + ARM_GET_C;
		ARM_SET_ZN(RON_USER[Rd]);
		ARM_SET_CV(RON_USER[Rd], op1, op2);
		break;
	case 13:
		RON_USER[Rd] = op1 - op2 - (ARM_GET_C ^ 1);
		ARM_SET_ZN(RON_USER[Rd]);
		ARM_SET_CV_sub(RON_USER[Rd], op1, op2);
		break;
	case 15:
		RON_USER[Rd] = op2 - op1 - (ARM_GET_C ^ 1);
		ARM_SET_ZN(RON_USER[Rd]);
		ARM_SET_CV_sub(RON_USER[Rd], op2, op1);
		break;
	case 17:
		op1 &= op2;
		ARM_SET_ZN(op1);
		return;
	case 19:
		op1 ^= op2;
		ARM_SET_ZN(op1);
		return;
	case 21:
		ARM_SET_CV_sub(op1 - op2, op1, op2);
		ARM_SET_ZN(op1 - op2);
		return;
	case 23:
		ARM_SET_CV(op1 + op2, op1, op2);
		ARM_SET_ZN(op1 + op2);
		return;
	case 25:
		RON_USER[Rd] = op1 | op2;
		ARM_SET_ZN(RON_USER[Rd]);
		break;
	case 27:
		RON_USER[Rd] = op2;
		ARM_SET_ZN(RON_USER[Rd]);
		break;
	case 29:
		RON_USER[Rd] = op1 & (~op2);
		ARM_SET_ZN(RON_USER[Rd]);
		break;
	case 31:
		RON_USER[Rd] = ~op2;
		ARM_SET_ZN(RON_USER[Rd]);
		break;
	}

	if (Rd == 0xf) { //destination = pc, take care of cpsr
		if (S) {
			_arm_SetCPSR(SPSR[arm_mode_table[MODE]]);
		}
		CYCLES -= ICYCLE + NCYCLE;
	}
}

static bool arm_prefetch_window_mapped(uint32_t addr)
{
	if (addr < RAMSIZE)
		return true;
	if (addr >= 0x03000000u && addr < 0x03500000u)
		return true;
	return false;
}

static uint32_t arm_normalize_prefetch_address(uint32_t addr)
{
	uint32_t mirrored;
	if (arm_prefetch_window_mapped(addr))
		return addr;

	/* Several ARM60/Portfolio paths carry old R15/status residue into the
	 * branch target.  On the 3DO bus the low DRAM decode is mirrored rather
	 * than producing a useful high-memory instruction stream.  Alone in the
	 * Dark PAL reaches 0x10411882 after the Krisalis logo; the executable code
	 * is present at the decoded DRAM mirror 0x00011880.  Mirror only the low
	 * 2 MB DRAM decode and only for instruction prefetches, so normal CLIO,
	 * MADAM, ROM and SPORT device accesses remain strict. */
	mirrored = addr & (REAL_MAIN_RAM_BYTES - 1u);
	if (mirrored < REAL_MAIN_RAM_BYTES) {
		arm_mirrored_prefetch_count++;
		arm_mirrored_prefetch_last = addr;
		arm_mirrored_prefetch_fetch = mirrored;
		return mirrored;
	}
	return addr;
}

int _arm_Execute(void)
{
	uint32_t cmd;
	uint32_t fetch_pc;

	BUS_FAULTED = false;
	MAS_Access_Exept = false;
	fetch_pc = REG_PC;
	if (fetch_pc & 3u) {
		/* ARM instruction fetches are word-granular.  Some 3DO titles briefly
		 * return through R15 values that still contain status/low-bit residue;
		 * treating that residue itself as a strict prefetch abort is less
		 * hardware-like than letting the aligned bus access decide whether the
		 * target is valid.  Keep the event visible for diagnostics, normalize
		 * the PC to the word boundary, and still allow mreadw()/MMU/device
		 * checks below to raise real unmapped or permission faults. */
		arm_unaligned_prefetch_count++;
		arm_unaligned_prefetch_last = fetch_pc;
		fetch_pc &= ~3u;
		arm_unaligned_prefetch_fetch = fetch_pc;
		REG_PC = fetch_pc;
	}
	fetch_pc = arm_normalize_prefetch_address(fetch_pc);
	if (fetch_pc != REG_PC)
		REG_PC = fetch_pc;
	current_instr_pc = fetch_pc;
	if (fetch_pc == DIPIR_QSI_ADDR && arm_romqsi_hook_installed())
		return arm_execute_romqsi_hle();
	cmd = mreadw(fetch_pc);
	if (MAS_Access_Exept)
		return SCYCLE + NCYCLE;

#ifdef DEBUG_CORE
	if (REG_PC < 0x00300000) {
		profiling[REG_PC >> 2]++;
	}
#endif

	REG_PC += 4;
	CYCLES = -SCYCLE;

	if (((cond_flags_cross[((cmd) >> 28)] >> ((CPSR) >> 28)) & 1)) {
		if ((cmd & ARM_MUL_MASK) == ARM_MUL_SIGN) {		/* Multiplication */
			arm60_MULT(cmd);
		} else if ((cmd & ARM_SDS_MASK) == ARM_SDS_SIGN) {	/* Single data swap */
			arm60_SWAP(cmd);
			CYCLES -= 2 * NCYCLE + ICYCLE;
		} else if (((cmd & ARM_ALU_MASK) == ARM_ALU_SIGN)) {	/* Data processing */
			arm60_ALU(cmd);
		} else if ((cmd & ARM_SDT_MASK) == ARM_SDT_SIGN) {	/* Single data transfer */
			arm60_SDT(cmd);
		} else if ((cmd & ARM_BDT_MASK) == ARM_BDT_SIGN) {	/* Block data transfer */
			arm60_BDT(cmd);
		} else if ((cmd & ARM_BRA_MASK) == ARM_BRA_SIGN) {	/* Branch */
			arm60_BRANCH(cmd);
		} else if ((cmd & ARM_COP_MASK) == ARM_COP_SIGN) {	/* Coprocessor */
			arm60_COPRO(cmd);
		} else if ((cmd & ARM_SWI_MASK) == ARM_SWI_SIGN) {	/* Software interrupt */
			#ifdef HLE_SWI
			decode_swi(cmd);
			#else
			decode_swi(); /* Gameblabla */
			#endif
		} else {						/* Undefined */
			SPSR[arm_mode_table[0x1b]] = CPSR;
			SETI(1);
			SETM(0x1b);
			load(14, REG_PC);
			REG_PC = 0x00000004;                    // (-4) fetch!!!
			CYCLES -= SCYCLE + NCYCLE;              // +2S+1N
		}
	}

	if (MAS_Access_Exept)
		return -CYCLES;

	if (!ISF && _clio_NeedFIQ() /*gFIQ*/) {
		//Set_madam_FSM(FSM_SUSPENDED);
		arm_fiq_entry_count++;
		gFIQ = 0;

		SPSR[arm_mode_table[0x11]] = CPSR;
		SETF(1);
		SETI(1);
		SETM(0x11);
		load(14, REG_PC + 4);
		REG_PC = 0x0000001c; //1c
	}

	return -CYCLES;
}

void _mem_write8(uint32_t addr, uint8_t val)
{
	pRam[addr] = val;
}

void _mem_write16(uint32_t addr, uint16_t val)
{
#ifdef MSB_FIRST
	pRam[addr + 0] = (uint8_t)(val >> 8);
	pRam[addr + 1] = (uint8_t)val;
#else
	pRam[addr + 0] = (uint8_t)val;
	pRam[addr + 1] = (uint8_t)(val >> 8);
#endif
}

void _mem_write32(uint32_t addr, uint32_t val)
{
#ifdef MSB_FIRST
	pRam[addr + 0] = (uint8_t)(val >> 24);
	pRam[addr + 1] = (uint8_t)(val >> 16);
	pRam[addr + 2] = (uint8_t)(val >> 8);
	pRam[addr + 3] = (uint8_t)val;
#else
	pRam[addr + 0] = (uint8_t)val;
	pRam[addr + 1] = (uint8_t)(val >> 8);
	pRam[addr + 2] = (uint8_t)(val >> 16);
	pRam[addr + 3] = (uint8_t)(val >> 24);
#endif
}

uint16_t _mem_read16(uint32_t addr)
{
#ifdef MSB_FIRST
	return ((uint16_t)pRam[addr] << 8) | pRam[addr + 1];
#else
	return ((uint16_t)pRam[addr + 1] << 8) | pRam[addr];
#endif
}

uint32_t _mem_read32(uint32_t addr)
{
#ifdef MSB_FIRST
	return ((uint32_t)pRam[addr] << 24) | ((uint32_t)pRam[addr + 1] << 16) | ((uint32_t)pRam[addr + 2] << 8) | pRam[addr + 3];
#else
	return ((uint32_t)pRam[addr + 3] << 24) | ((uint32_t)pRam[addr + 2] << 16) | ((uint32_t)pRam[addr + 1] << 8) | pRam[addr];
#endif
}

uint8_t _mem_read8(uint32_t addr)
{
	return pRam[addr];
}

static void mwrite_ram32(uint32_t pa, uint32_t val, int cacheable, int bufferable)
{
	pa &= ~3u;
	if (cacheable && cp15_cache_enabled())
		cp15_cache_write32_if_hit(pa, val, 0xf);
	if (bufferable && cp15_write_buffer_enabled())
		cp15_write_buffer_enqueue(pa, val, 0xf);
	else
		_mem_write32(pa, val);
}

static void mwrite_ram8(uint32_t pa, uint32_t val, int cacheable, int bufferable)
{
	uint32_t wa = pa & ~3u;
	uint32_t lane = (pa ^ 3u) & 3u;
	uint32_t word = (val & 0xffu) << (lane * 8u);
	uint8_t mask = (uint8_t)(1u << lane);
	if (cacheable && cp15_cache_enabled())
		cp15_cache_write32_if_hit(wa, word, mask);
	if (bufferable && cp15_write_buffer_enabled())
		cp15_write_buffer_enqueue(wa, word, mask);
	else
		_mem_write8(pa ^ 3u, val & 0xffu);
}

static uint32_t mread_ram32(uint32_t pa, int cacheable)
{
	uint32_t forwarded;
	pa &= ~3u;
	if (cp15_write_buffer_forward(pa, &forwarded))
		return forwarded;
	if (cacheable && cp15_cache_enabled())
		return cp15_cache_read32(pa);
	return _mem_read32(pa);
}

static uint32_t mread_ram8(uint32_t pa, int cacheable)
{
	uint32_t word = mread_ram32(pa & ~3u, cacheable);
	uint32_t lane = (pa ^ 3u) & 3u;
	return (word >> (lane * 8u)) & 0xffu;
}

void mwritew(uint32_t addr, uint32_t val)
{
	uint32_t index;
	const uint32_t raw_addr = addr;
	uint32_t pa = addr;
	int cacheable = 0;
	int bufferable = 0;

	if (!cp15_translate(addr, 1, 0, &pa, &cacheable, &bufferable))
		return;
	pa &= ~3u;

	if (pa < 0x00300000) {
		arm_note_highram_write(pa);
		mwrite_ram32(pa, val, cacheable, bufferable);
		return;
	}

	if (!((index = (pa ^ 0x03300000)) & ~0x7FF)) {
		if (STRICT_BUS_FAULTS && (raw_addr & 3)) { _arm_DataAbort(raw_addr, ARM_FAULT_DEVICE_UNALIGNED_WRITE); return; }
		_arm_FlushWriteBuffer();
		_madam_Poke(index, val);
		return;
	}

	if (!((index = (pa ^ 0x03400000)) & ~0xFFFF)) {
		int poke;
		if (STRICT_BUS_FAULTS && (raw_addr & 3)) { _arm_DataAbort(raw_addr, ARM_FAULT_DEVICE_UNALIGNED_WRITE); return; }
		_arm_FlushWriteBuffer();
		poke = _clio_Poke(index, val);
		if (poke < 0) { _arm_DataAbort(raw_addr, ARM_FAULT_DSP_BOUNDS); return; }
		if (poke)
			REG_PC += 4;
		return;
	}

	if (!((index = (pa ^ 0x03200000)) & ~0xFFFFF)) {
		if (STRICT_BUS_FAULTS && (raw_addr & 3)) { _arm_DataAbort(raw_addr, ARM_FAULT_DEVICE_UNALIGNED_WRITE); return; }
		_arm_FlushWriteBuffer();
		_sport_WriteAccess(index, val);
		return;
	}

	if (!((index = (pa ^ 0x03100000)) & ~0xFFFFF)) {
		if (STRICT_BUS_FAULTS && (raw_addr & 3)) { _arm_DataAbort(raw_addr, ARM_FAULT_DEVICE_UNALIGNED_WRITE); return; }
		_arm_FlushWriteBuffer();
		if (index & 0x80000) {
			_diag_Send(val);
			return;
		} else if (index & 0x40000) {
			pNVRam[(index >> 2) & 32767] = (uint8_t)val;
			writeNvRam();
			return;
		}
	}

	_arm_DataAbort(raw_addr, ARM_FAULT_UNMAPPED_WRITE);
}

uint32_t mreadw(uint32_t addr)
{
	uint32_t index;
	const uint32_t raw_addr = addr;
	uint32_t pa = addr;
	int cacheable = 0;
	int bufferable = 0;
	(void)bufferable;

	if (!cp15_translate(addr, 0, 0, &pa, &cacheable, &bufferable))
		return 0xBADACCE5;
	pa &= ~3u;

	if (pa < 0x00300000) {
		arm_note_highram_read(pa);
		if ((pa & ~3u) == DIPIR_QSI_ADDR && arm_romqsi_hook_installed())
			return DIPIR_QSI_HLE_INSN;
		return mread_ram32(pa, cacheable);
	}

	if (!((index = (pa ^ 0x03300000)) & ~0xFFFFF)) {
		if (STRICT_BUS_FAULTS && (raw_addr & 3)) { _arm_DataAbort(raw_addr, ARM_FAULT_DEVICE_UNALIGNED_READ); return 0xBADACCE5; }
		return _madam_Peek(index);
	}

	if (!((index = (pa ^ 0x03400000)) & ~0xFFFFF)) {
		uint32_t out;
		if (STRICT_BUS_FAULTS && (raw_addr & 3)) { _arm_DataAbort(raw_addr, ARM_FAULT_DEVICE_UNALIGNED_READ); return 0xBADACCE5; }
		out = _clio_Peek(index);
		if (STRICT_BUS_FAULTS && _arm_BusFaulted()) return 0xBADACCE5;
		return out;
	}

	if (!((index = (pa ^ 0x03200000)) & ~0xFFFFF)) {
		if (STRICT_BUS_FAULTS && (raw_addr & 3)) { _arm_DataAbort(raw_addr, ARM_FAULT_DEVICE_UNALIGNED_READ); return 0xBADACCE5; }
		if (!((index = (pa ^ 0x03200000)) & ~0x1FFF))
			return _sport_SetSource(index);
		return 0xBADACCE5;
	}

	if (!((index = (pa ^ 0x03000000)) & ~0xFFFFF)) {
		uint8_t *romp = pRom + index + (gSecondROM ? 1024 * 1024 : 0);
#ifdef MSB_FIRST
		return ((uint32_t)romp[0] << 24) | ((uint32_t)romp[1] << 16) | ((uint32_t)romp[2] << 8) | romp[3];
#else
		return ((uint32_t)romp[3] << 24) | ((uint32_t)romp[2] << 16) | ((uint32_t)romp[1] << 8) | romp[0];
#endif
	}

	if (!((index = (pa ^ 0x03100000)) & ~0xFFFFF)) {
		if (STRICT_BUS_FAULTS && (raw_addr & 3)) { _arm_DataAbort(raw_addr, ARM_FAULT_DEVICE_UNALIGNED_READ); return 0xBADACCE5; }
		if (index & 0x80000)
			return _diag_Get();
		else if (index & 0x40000)
			return (uint32_t)pNVRam[(index >> 2) & 32767];
	}

	_arm_DataAbort(raw_addr, ARM_FAULT_UNMAPPED_READ);
	return 0xBADACCE5;
}

void mwriteb(uint32_t addr, uint32_t val)
{
	uint32_t index;
	const uint32_t raw_addr = addr;
	uint32_t pa = addr;
	int cacheable = 0;
	int bufferable = 0;

	val &= 0xff;
	if (!cp15_translate(addr, 1, 0, &pa, &cacheable, &bufferable))
		return;

	if (pa < 0x00300000) {
		arm_note_highram_write(pa);
		mwrite_ram8(pa, val, cacheable, bufferable);
		return;
	} else if (!((index = (pa ^ 0x03100003)) & ~0xFFFFF)) {
		_arm_FlushWriteBuffer();
		if ((index & 0x40000) == 0x40000) {
			pNVRam[(index >> 2) & 32767] = (uint8_t)val;
			writeNvRam();
			return;
		}
	}
	_arm_DataAbort(raw_addr, ARM_FAULT_UNMAPPED_WRITE);
}

uint32_t mreadb(uint32_t addr)
{
	uint32_t index;
	const uint32_t raw_addr = addr;
	uint32_t pa = addr;
	int cacheable = 0;
	int bufferable = 0;
	(void)bufferable;

	if (!cp15_translate(addr, 0, 0, &pa, &cacheable, &bufferable))
		return 0xBADACCE5;

	if (pa < 0x00300000) {
		arm_note_highram_read(pa);
		return mread_ram8(pa, cacheable);
	}
	else if (!((index = (pa ^ 0x03000003)) & ~0xFFFFF)) {
		if (gSecondROM)
			return pRom[index + 1024 * 1024];
		return pRom[index];
	} else if (!((index = (pa ^ 0x03100003)) & ~0xFFFFF)) {
		if ((index & 0x40000) == 0x40000)
			return pNVRam[(index >> 2) & 32767];
	}

	_arm_DataAbort(raw_addr, ARM_FAULT_UNMAPPED_READ);
	return 0xBADACCE5;
}


void  loadusr(uint32_t n, uint32_t val)
{
	if (n == 15) {
		load(15, val);
		return;
	}

	switch (arm_mode_table[(CPSR & 0x1f) | 0x10]) {
	case ARM_MODE_USER:
		RON_USER[n] = val;
		break;
	case ARM_MODE_FIQ:
		if (n > 7)
			RON_CASH[n - 8] = val;
		else
			RON_USER[n] = val;
		break;
	case ARM_MODE_IRQ:
	case ARM_MODE_ABT:
	case ARM_MODE_UND:
	case ARM_MODE_SVC:
		if (n > 12)
			RON_CASH[n - 8] = val;
		else
			RON_USER[n] = val;
		break;
	}
}


uint32_t rreadusr(uint32_t n)
{
	if (n == 15)
		return RON_USER[15];

	switch (arm_mode_table[(CPSR & 0x1f)]) {
	case ARM_MODE_USER:
		return RON_USER[n];
	case ARM_MODE_FIQ:
		if (n > 7)
			return RON_CASH[n - 8];
		return RON_USER[n];
	case ARM_MODE_IRQ:
	case ARM_MODE_ABT:
	case ARM_MODE_UND:
	case ARM_MODE_SVC:
		if (n > 12)
			return RON_CASH[n - 8];
		return RON_USER[n];
	}
	return 0;
}

uint32_t ReadIO(uint32_t addr)
{
	return mreadw(addr);
}

void WriteIO(uint32_t addr, uint32_t val)
{
	mwritew(addr, val);
}

static void intInit(void)
{
}

static void intReset(void)
{
}

static int intExec(int cycles)
{
	int cnt = 0;
	unsigned int steps = 0;
	const unsigned int max_steps = STRICT_BUS_FAULTS ? 1024u : 0u;
	do {
		int c = _arm_Execute();
		cnt += c;
		steps++;
		if (STRICT_BUS_FAULTS && BUS_FAULTED)
			break;
		if (STRICT_BUS_FAULTS && steps > max_steps) {
			_arm_DataAbort(REG_PC, ARM_FAULT_CPU_RUNAWAY);
			break;
		}
	} while (cycles > cnt);

	return cnt > 0 ? cnt : cycles;
}

static void intDestroy(void)
{
}

ARM60cpu cpuInt = {
	intInit,
	intReset,
	intExec,
	intDestroy
};

ARM60cpu *cpu = &cpuInt;
