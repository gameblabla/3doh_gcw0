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

#include <string.h>
#include "Clio.h"
#include "Madam.h"
#include "XBUS.h"
#include "arm.h"
#include "DSP.h"
#include "prng32.h"

#define DECREMENT    0x1
#define RELOAD       0x2
#define CASCADE      0x4
#define FLABLODE     0x8

#define RELOAD_VAL   0x10

extern int jw;

void HandleDMA(uint32_t val);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#ifndef DONTPACK
#pragma pack(push,1)
#endif
struct FIFOt {

	uint32_t StartAdr;
	int StartLen;
	uint32_t NextAdr;
	int NextLen;
};
struct CLIODatum {
	uint32_t cregs[65536];
	int DSPW1;
	int DSPW2;
	int DSPA;
	int PTRI[13];
	int PTRO[4];
	struct FIFOt FIFOI[13];
	struct FIFOt FIFOO[4];
};
#ifndef DONTPACK
#pragma pack(pop)
#endif

static uint32_t * Mregs;

static uint32_t clio_fiq_generate_count;
static uint32_t clio_fiq_need_count;
static uint32_t clio_last_fiq_reason1;
static uint32_t clio_last_fiq_reason2;
static uint32_t clio_eififo_read_count;
static uint32_t clio_eififo_empty_read_count;
static uint32_t clio_eififo_reload_count;
static uint32_t clio_eififo_empty_by_channel[13];
static uint32_t clio_eififo_reload_by_channel[13];
static uint16_t clio_eififo_last_value[13];
static uint32_t clio_last_eififo_empty_channel;
static uint32_t clio_last_eififo_reload_channel;
static uint32_t clio_eofifo_write_count;
static uint32_t clio_eofifo_disabled_write_count;
static uint32_t clio_eofifo_full_count;
static uint32_t clio_last_fifo_event;

#define CLIO_FIFO_EVENT_EI_EMPTY(ch)       (0x10000u | ((uint32_t)(ch) & 0xffffu))
#define CLIO_FIFO_EVENT_EI_RELOAD(ch)      (0x20000u | ((uint32_t)(ch) & 0xffffu))
#define CLIO_FIFO_EVENT_EO_DISABLED(ch)    (0x30000u | ((uint32_t)(ch) & 0xffffu))
#define CLIO_FIFO_EVENT_EO_FULL(ch)        (0x40000u | ((uint32_t)(ch) & 0xffffu))

static uint32_t clio_fifo_level_reassert_count;
static uint32_t clio_fifo_level_reassert_mask;
static uint32_t clio_dspp_control_write_count;
static uint32_t clio_dspp_control_last_value;
static uint32_t clio_dspp_control_non_gw_count;
static uint32_t clio_dspp_reset_write_count;
static uint32_t clio_dspp_reset_last_value;
static uint32_t clio_fifo_reload_dma_block_count;
static uint32_t clio_fifo_last_reload_dma_block_channel;
static uint32_t clio_dspp_nmem_read_count;
static uint32_t clio_dspp_nmem_last_read_address;
static uint32_t clio_xbus_dma_pulse_count;
static uint32_t clio_xbus_dma_last_len;
static uint32_t clio_xbus_dma_last_addr;
static uint32_t clio_xbus_dma_timer_accum;
static uint32_t clio_xbus_dma_timer_window;
static uint32_t clio_xbus_timer120_adjust_count;
static uint32_t clio_xbus_timer120_last_in;
static uint32_t clio_xbus_timer120_last_out;

static struct CLIODatum clio;
static int clio_video_standard_pal = 0;

/* Bit 0 of CLIO ADBIOBits was reserved/commented in the 3DO Portfolio
 * headers as the NTSC/PAL selector. Some software reaches this through
 * the ROM graphics folio via GetDisplayType()/QueryGraphics(), so the
 * host video-standard override must be visible here as well as in the
 * scheduler/render path. */
#define CLIO_ADBIO_NTSC_PAL_BIT 0x00000001U

void _clio_SetVideoStandard(int pal)
{
	clio_video_standard_pal = pal ? 1 : 0;
	if (clio_video_standard_pal)
		clio.cregs[0x84] |= CLIO_ADBIO_NTSC_PAL_BIT;
	else
		clio.cregs[0x84] &= ~CLIO_ADBIO_NTSC_PAL_BIT;
}

int _clio_GetVideoStandard(void)
{
	return clio_video_standard_pal;
}


#define CLIO_MAIN_RAM_BYTES (3u * 1024u * 1024u)

static bool clio_ram_range_valid(uint32_t addr, uint32_t len)
{
	if (len == 0)
		return true;
	if (addr >= CLIO_MAIN_RAM_BYTES)
		return false;
	if (len > CLIO_MAIN_RAM_BYTES - addr)
		return false;
	return true;
}

static bool clio_fifo_span_valid(uint32_t addr, int len)
{
	if (addr == 0 || len <= 0)
		return true;
	return clio_ram_range_valid(addr, (uint32_t)len);
}

static void clio_fifo_bounds_fault(uint32_t addr)
{
	_arm_DataAbort(addr, 5);
}

#define cregs clio.cregs
#define DSPW1 clio.DSPW1
#define DSPW2 clio.DSPW2
#define DSPA clio.DSPA
#define PTRI clio.PTRI
#define PTRO clio.PTRO
#define FIFOI clio.FIFOI
#define FIFOO clio.FIFOO

static bool clio_ei_dma_channel_enabled(unsigned channel)
{
	return channel < 13 && (cregs[0x304] & (1u << channel)) != 0;
}

static bool clio_eo_dma_channel_enabled(unsigned channel)
{
	/* Portfolio clio.h defines DSPP->DMA enable bits at 0x000F0000.
	 * Opera's 2023 fix used the same DMA enable gate for EO reloads,
	 * but the hardware register map says EO channels are bits 16..19. */
	return channel < 4 && (cregs[0x304] & (1u << (channel + 16))) != 0;
}

static void clio_reassert_level_fifo_irqs(void)
{
	/* Do not synthesize persistent EI-empty level interrupts here.
	 * Commercial streaming code clears and masks FIFO interrupts as part of
	 * its scheduling; reasserting them from generic CLIO acknowledge/FIFO
	 * programming paths causes Doom's streamed intro audio to stall/silence. */
}


uint32_t _clio_GetFifoLevelReassertCount(void) { return clio_fifo_level_reassert_count; }
uint32_t _clio_GetFifoLevelReassertMask(void) { return clio_fifo_level_reassert_mask; }
uint32_t _clio_GetDSPPControlWriteCount(void) { return clio_dspp_control_write_count; }
uint32_t _clio_GetDSPPControlLastValue(void) { return clio_dspp_control_last_value; }
uint32_t _clio_GetDSPPControlNonGWCount(void) { return clio_dspp_control_non_gw_count; }
uint32_t _clio_GetDSPPResetWriteCount(void) { return clio_dspp_reset_write_count; }
uint32_t _clio_GetDSPPResetLastValue(void) { return clio_dspp_reset_last_value; }
uint32_t _clio_GetFifoReloadDMABlockCount(void) { return clio_fifo_reload_dma_block_count; }
uint32_t _clio_GetFifoLastReloadDMABlockChannel(void) { return clio_fifo_last_reload_dma_block_channel; }
uint32_t _clio_GetDSPPNMemReadCount(void) { return clio_dspp_nmem_read_count; }
uint32_t _clio_GetDSPPNMemLastReadAddress(void) { return clio_dspp_nmem_last_read_address; }
uint32_t _clio_GetXbusDmaPulseCount(void) { return clio_xbus_dma_pulse_count; }
uint32_t _clio_GetXbusDmaLastLen(void) { return clio_xbus_dma_last_len; }
uint32_t _clio_GetXbusDmaLastAddr(void) { return clio_xbus_dma_last_addr; }
uint32_t _clio_GetXbusDmaTimerAccum(void) { return clio_xbus_dma_timer_accum; }
uint32_t _clio_GetXbusDmaTimerWindow(void) { return clio_xbus_dma_timer_window; }
uint32_t _clio_GetXbusTimer120AdjustCount(void) { return clio_xbus_timer120_adjust_count; }
uint32_t _clio_GetXbusTimer120LastIn(void) { return clio_xbus_timer120_last_in; }
uint32_t _clio_GetXbusTimer120LastOut(void) { return clio_xbus_timer120_last_out; }

void _clio_FieldTick(void)
{
	if (clio_xbus_dma_timer_window > 0) {
		clio_xbus_dma_timer_window--;
		if (clio_xbus_dma_timer_window == 0)
			clio_xbus_dma_timer_accum = 0;
	}
}

uint32_t _clio_SaveSize(void)
{
	return sizeof(struct CLIODatum);
}

void _clio_Save(void *buff)
{
	memcpy(buff, &clio, sizeof(struct CLIODatum));
}

void _clio_Load(void *buff)
{
	memcpy(&clio, buff, sizeof(struct CLIODatum));
	_clio_SetVideoStandard(clio_video_standard_pal);
}

#define CURADR Mregs[base]
#define CURLEN Mregs[base + 4]
#define RLDADR Mregs[base + 8]
#define RLDLEN Mregs[base + 0xc]

static bool clio_addr_is_timer(uint32_t addr)
{
	return addr >= 0x100 && addr <= 0x17c && ((addr & 3) == 0);
}

static bool clio_addr_is_regular(uint32_t addr)
{
	/* Keep strict mode strict about out-of-range DSP memory, but do not
	 * data-abort valid CLIO MMIO registers.  Portfolio's Clio struct maps
	 * the basic control block at 0x0000..0x003f, interrupt/mode registers
	 * at 0x0040..0x006f, ADB/Video timing at 0x0080..0x008b, timers, FIFO
	 * control/status, expansion bus, DSPP control, and a few late ASIC
	 * identification windows.  v12 only whitelisted a small subset and
	 * incorrectly aborted writes to CSysBits at 0x0004, which real software
	 * such as Invades legitimately performs. */
	if (addr & 3)
		return false;
	if (addr < 0x40)
		return true;
	if (addr >= 0x40 && addr < 0x70)
		return true;
	if (addr == 0x80 || addr == 0x84 || addr == 0x88)
		return true;
	if (clio_addr_is_timer(addr))
		return true;
	if ((addr >= 0x200 && addr <= 0x20c) || addr == 0x220)
		return true;
	if (addr >= 0x300 && addr < 0x400)
		return true;
	if (addr >= 0x400 && addr < 0xc00)
		return true;
	if (addr == 0x17d0 || addr == 0x17d4 || addr == 0x17e0 || addr == 0x17e4 ||
	    addr == 0x17e8 || addr == 0x17f0 || addr == 0x17f4 || addr == 0x17f8 ||
	    addr == 0x17fc)
		return true;
	if (addr == 0x4000 || addr == 0x4004 || addr == 0x8000 || addr == 0x8004 ||
	    addr == 0xc000 || addr == 0xc004 || addr == 0xc008 || addr == 0xc00c)
		return true;
	return false;
}

static bool clio_addr_is_dsp_nmem_pair(uint32_t addr)
{
	return (addr >= 0x1800 && addr <= 0x1bff);
}

static bool clio_addr_is_dsp_nmem_single(uint32_t addr)
{
	return (addr >= 0x2000 && addr <= 0x27ff);
}

static bool clio_addr_is_dsp_imem_write_pair(uint32_t addr)
{
	return (addr >= 0x3000 && addr <= 0x33ff);
}

static bool clio_addr_is_dsp_imem_write_single(uint32_t addr)
{
	return (addr >= 0x3400 && addr <= 0x37ff);
}

static bool clio_addr_is_dsp_imem_read_pair(uint32_t addr)
{
	return (addr >= 0x3800 && addr <= 0x3bff);
}

static bool clio_addr_is_dsp_imem_read_single(uint32_t addr)
{
	return (addr >= 0x3c00 && addr <= 0x3fff);
}

static bool clio_addr_is_dsp_ei_write_pair_primary(uint32_t addr)
{
	/* Portfolio DSPP EI memory is 0x00..0x7f.  The legacy FreeDO path
	 * accepted the whole 0x3000..0x33ff CLIO window and wrapped it with
	 * &0xff, which can hide bad audio/DSP setup by aliasing back into
	 * valid EI memory.  One ARM word writes two DSP words. */
	return (addr >= 0x3000 && addr <= 0x30fc && ((addr & 3) == 0));
}

static bool clio_addr_is_dsp_ei_write_single_primary(uint32_t addr)
{
	return (addr >= 0x3400 && addr <= 0x35fc && ((addr & 3) == 0));
}

static bool clio_addr_is_dsp_eo_read_pair_primary(uint32_t addr)
{
	uint32_t dspa;
	if ((addr & 3) != 0)
		return false;
	/* EO memory exposed to the ARM is 0x300..0x30f, but the DSP status
	 * register aliases at 0x3eb..0x3ef are also legitimate CPU-visible
	 * reads used by real software. */
	if (addr >= 0x3800 && addr <= 0x381c)
		return true;
	dspa = (((addr - 0x3800) >> 1) & 0xff) + 0x300;
	return (dspa >= 0x3eb && dspa <= 0x3ef);
}

static bool clio_addr_is_dsp_eo_read_single_primary(uint32_t addr)
{
	uint32_t dspa;
	if ((addr & 3) != 0)
		return false;
	if (addr >= 0x3c00 && addr <= 0x3c3c)
		return true;
	dspa = (((addr - 0x3c00) >> 2) & 0xff) + 0x300;
	return (dspa >= 0x3eb && dspa <= 0x3ef);
}

static int clio_dsp_resource_fault(uint32_t addr, uint32_t detail)
{
	if (_dsp_EffectiveStrictResourceFaults()) {
		_dsp_StrictResourceAbort(0x03400000u + addr, detail);
		return 1;
	}
	return 0;
}

static bool clio_addr_is_known_write(uint32_t addr)
{
	return clio_addr_is_regular(addr) ||
	       clio_addr_is_dsp_nmem_pair(addr) ||
	       clio_addr_is_dsp_nmem_single(addr) ||
	       clio_addr_is_dsp_imem_write_pair(addr) ||
	       clio_addr_is_dsp_imem_write_single(addr);
}

static bool clio_addr_is_known_read(uint32_t addr)
{
	return clio_addr_is_regular(addr) ||
	       clio_addr_is_dsp_nmem_pair(addr) ||
	       clio_addr_is_dsp_nmem_single(addr) ||
	       clio_addr_is_dsp_imem_read_pair(addr) ||
	       clio_addr_is_dsp_imem_read_single(addr);
}

int _clio_v0line(void)
{
	return cregs[8] & 0x7ff;
}

int _clio_v1line(void)
{
	return cregs[12] & 0x7ff;
}

bool _clio_NeedFIQ(void)
{
	bool need = ((cregs[0x40] & cregs[0x48]) || (cregs[0x60] & cregs[0x68])) ? true : false;
	if (need)
		clio_fiq_need_count++;
	return need;
}

void _clio_GenerateFiq(uint32_t reason1, uint32_t reason2)
{
	clio_fiq_generate_count++;
	clio_last_fiq_reason1 = reason1;
	clio_last_fiq_reason2 = reason2;
	cregs[0x40] |= reason1;
	cregs[0x60] |= reason2;
	if (cregs[0x60])
		cregs[0x40] |= 0x80000000; // irq31 if exist irq32 and high
}

uint32_t _clio_GetFiqGenerateCount(void) { return clio_fiq_generate_count; }
uint32_t _clio_GetFiqNeedCount(void) { return clio_fiq_need_count; }
uint32_t _clio_GetLastFiqReason1(void) { return clio_last_fiq_reason1; }
uint32_t _clio_GetLastFiqReason2(void) { return clio_last_fiq_reason2; }
uint32_t _clio_GetIrq0Pending(void) { return cregs[0x40]; }
uint32_t _clio_GetIrq0Mask(void) { return cregs[0x48]; }
uint32_t _clio_GetIrq1Pending(void) { return cregs[0x60]; }
uint32_t _clio_GetIrq1Mask(void) { return cregs[0x68]; }
uint32_t _clio_GetEififoReadCount(void) { return clio_eififo_read_count; }
uint32_t _clio_GetEififoEmptyReadCount(void) { return clio_eififo_empty_read_count; }
uint32_t _clio_GetEififoReloadCount(void) { return clio_eififo_reload_count; }
uint32_t _clio_GetEofifoWriteCount(void) { return clio_eofifo_write_count; }
uint32_t _clio_GetEofifoDisabledWriteCount(void) { return clio_eofifo_disabled_write_count; }
uint32_t _clio_GetEofifoFullCount(void) { return clio_eofifo_full_count; }
uint32_t _clio_GetLastFifoEvent(void) { return clio_last_fifo_event; }
uint32_t _clio_GetLastEififoEmptyChannel(void) { return clio_last_eififo_empty_channel; }
uint32_t _clio_GetLastEififoReloadChannel(void) { return clio_last_eififo_reload_channel; }
uint32_t _clio_GetEififoEmptyChannelCount(uint32_t channel) { return (channel < 13) ? clio_eififo_empty_by_channel[channel] : 0; }
uint32_t _clio_GetEififoReloadChannelCount(uint32_t channel) { return (channel < 13) ? clio_eififo_reload_by_channel[channel] : 0; }

#include "freedocore.h"
void _clio_SetTimers(uint32_t v200, uint32_t v208);
void _clio_ClearTimers(uint32_t v204, uint32_t v20c);

int _clio_Poke(uint32_t addr, uint32_t val)
{
	int base;
	int i;

	if (clio_xbus_dma_timer_window == 0)
		clio_xbus_dma_timer_accum = 0;

	if ( (addr & ~0x2C) == 0x40 ) { // 0x40..0x4C, 0x60..0x6C case
		if (addr == 0x40) {
			cregs[0x40] |= val;
			if (cregs[0x60])
				cregs[0x40] |= 0x80000000;
			//if(cregs[0x40]&cregs[0x48]) _arm_SetFIQ();
			return 0;
		} else if (addr == 0x44) {
			cregs[0x40] &= ~val;
			if (!cregs[0x60]) cregs[0x40] &= ~0x80000000;
			clio_reassert_level_fifo_irqs();
			return 0;
		} else if (addr == 0x48) {
			cregs[0x48] |= val;
			//if(cregs[0x40]&cregs[0x48]) _arm_SetFIQ();
			return 0;
		} else if (addr == 0x4c) {
			cregs[0x48] &= ~val;
			cregs[0x48] |= 0x80000000; // always one for irq31
			return 0;
		}
#if 0
		else if (addr == 0x50) {
			cregs[0x50] |= val & 0x3fff0000;
			return 0;
		} else if (addr == 0x54) {
			cregs[0x50] &= ~val;
			return 0;
		}
#endif
		else if (addr == 0x60) {
			cregs[0x60] |= val;
			if (cregs[0x60]) cregs[0x40] |= 0x80000000;
			//if(cregs[0x60]&cregs[0x68])	_arm_SetFIQ();
			return 0;
		} else if (addr == 0x64) {
			cregs[0x60] &= ~val;
			if (!cregs[0x60]) cregs[0x40] &= ~0x80000000;
			clio_reassert_level_fifo_irqs();
			return 0;
		} else if (addr == 0x68) {
			cregs[0x68] |= val;
			//if(cregs[0x60]&cregs[0x68]) _arm_SetFIQ();
			return 0;
		} else if (addr == 0x6c) {
			cregs[0x68] &= ~val;
			return 0;
		}
	} else if (addr == 0x84) {
		cregs[0x84] = val & 0xf;
		_clio_SetVideoStandard(clio_video_standard_pal);
		SelectROM((val & 4) ? 1 : 0 );
		return 0;
	} else if (addr == 0x300) {
		//clear down the fifos and stop them
		base = 0;
		cregs[0x304] &= ~val;

		for (i = 0; i < 13; i++) {
			if (val & (1 << i)) {
				base = 0x400 + (i << 4);
				RLDADR = CURADR = 0;
				RLDLEN = CURLEN = 0;
				_clio_SetFIFO(base, 0);
				_clio_SetFIFO(base + 4, 0);
				_clio_SetFIFO(base + 8, 0);
				_clio_SetFIFO(base + 0xc, 0);
				val &= ~(1 << i);
				PTRI[i] = 0;
			}

		}

		for (i = 0; i < 4; i++) {
			if (val & (1 << (i + 16))) {
				base = 0x500 + (i << 4);
				RLDADR = CURADR = 0;
				RLDLEN = CURLEN = 0;
				_clio_SetFIFO(base, 0);
				_clio_SetFIFO(base + 4, 0);
				_clio_SetFIFO(base + 8, 0);
				_clio_SetFIFO(base + 0xc, 0);

				val &= ~(1 << (i + 16));
				PTRO[i] = 0;

			}

		}


		return 0;
	} else if (addr == 0x304) { // Dma Starter!!!!! P/A !!!! need to create Handler.
		HandleDMA(val);
		return 0;
	} else if (addr == 0x308) { //Dma Stopper!!!!
		cregs[0x304] &= ~val;
		return 0;
	} else if (addr == 0x400) { //XBUS direction
		if (val & 0x800)
			return 0;

		cregs[0x400] = val;
		return 0;
	} else if ((addr >= 0x500) && (addr < 0x540)) {
		_xbus_SetSEL(val);

		return 0;
	} else if ((addr >= 0x540) && (addr < 0x580)) {
		_xbus_SetPoll(val);
		return 0;
	} else if ((addr >= 0x580) && (addr < 0x5c0)) {
		_xbus_SetCommandFIFO(val); // on FIFO Filled execute the command
		return 0;
	} else if ((addr >= 0x5c0) && (addr < 0x600)) {
		_xbus_SetDataFIFO(val); // on FIFO Filled execute the command
		return 0;
	} else if (addr == 0x28) {
		cregs[addr] = val;
		if (val == 0x30)
			return 1;
		else
			return 0;
	} else if ((addr >= 0x1800) && (addr <= 0x1fff)) {//0x0340 1800 .. 0x0340 1BFF, old compat mirrors 0x1C00 .. 0x1FFF
		if (_arm_GetStrictBusFaults() && !clio_addr_is_dsp_nmem_pair(addr))
			return -1;
		addr &= ~0x400; //compat mirror
		DSPW1 = val >> 16;
		DSPW2 = val & 0xffff;
		DSPA = (addr - 0x1800) >> 1;
		_dsp_WriteMemory(DSPA, DSPW1);
		_dsp_WriteMemory(DSPA + 1, DSPW2);
		return 0;
		//DSPNRAMWrite 2 DSPW per 1ARMW
	} else if ((addr >= 0x2000) && (addr <= 0x2fff)) {
		if (_arm_GetStrictBusFaults() && !clio_addr_is_dsp_nmem_single(addr))
			return -1;
		addr &= ~0x800;//compat mirror
		DSPW1 = val & 0xffff;
		DSPA = (addr - 0x2000) >> 2;
		_dsp_WriteMemory(DSPA, DSPW1);
		return 0;
	} else if ((addr >= 0x3000) && (addr <= 0x33ff)) { //0x0340 3000 .. 0x0340 33FF
		if (!clio_addr_is_dsp_ei_write_pair_primary(addr) && clio_dsp_resource_fault(addr, addr))
			return -1;
		DSPA = (addr - 0x3000) >> 1;
		DSPA &= 0xff;
		DSPW1 = val >> 16;
		DSPW2 = val & 0xffff;
		_dsp_WriteIMem(DSPA, DSPW1);
		_dsp_WriteIMem(DSPA + 1, DSPW2);
		return 0;
	} else if ((addr >= 0x3400) && (addr <= 0x37ff)) {//0x0340 3400 .. 0x0340 37FF
		if (!clio_addr_is_dsp_ei_write_single_primary(addr) && clio_dsp_resource_fault(addr, addr))
			return -1;
		DSPA = (addr - 0x3400) >> 2;
		DSPA &= 0xff;
		DSPW1 = val & 0xffff;
		_dsp_WriteIMem(DSPA, DSPW1);
		return 0;
	} else if (addr == 0x17E8) {//Reset
		clio_dspp_reset_write_count++;
		clio_dspp_reset_last_value = val;
		_dsp_Reset();
		return 0;
	} else if (addr == 0x17D0) {//Write DSP/ARM Semaphore
		_dsp_ARMwrite2sema4(val);
		return 0;
	} else if (addr == 0x17FC) {//start/stop
		clio_dspp_control_write_count++;
		clio_dspp_control_last_value = val;
		if (val & ~1u)
			clio_dspp_control_non_gw_count++;
		_dsp_SetRunning((val & 1u) != 0);
		return 0;
	} else if (addr == 0x200) {
		cregs[0x200] |= val;
		_clio_SetTimers(val, 0);
		return 0;
	} else if (addr == 0x204) {
		cregs[0x200] &= ~val;
		_clio_ClearTimers(val, 0);
		return 0;
	} else if (addr == 0x208) {
		cregs[0x208] |= val;
		_clio_SetTimers(0, val);
		return 0;
	} else if (addr == 0x20c) {
		cregs[0x208] &= ~val;
		_clio_ClearTimers(0, val);
		return 0;
	} else if (addr == 0x220) {
		//if(val<64)val=64;
		cregs[addr] = val & 0x3ff;
		return 0;
	} else if (addr >= 0x100 && addr <= 0x17c) {
		cregs[addr] = val & 0xffff;
		return 0;
	}

	if (_arm_GetStrictBusFaults() && !clio_addr_is_known_write(addr))
		return -1;

	if (addr == 0x128 && val == 0x0)
		jw = 17000000; //val=1;

	cregs[addr] = val;
	return 0;
}



uint32_t _clio_Peek(uint32_t addr)
{
	if ( (addr & ~0x2C) == 0x40 ) { // 0x40..0x4C, 0x60..0x6C case
		addr &= ~4; // By read 40 and 44, 48 and 4c, 60 and 64, 68 and 6c same
		if (addr == 0x40)
			return cregs[0x40];
		else if (addr == 0x48)
			return cregs[0x48] | 0x80000000;
		else if (addr == 0x60)
			return cregs[0x60];
		else if (addr == 0x68)
			return cregs[0x68];
		return 0; // for skip warning C4715
	} else if (addr == 0x204)
		return cregs[0x200];
	else if (addr == 0x20c)
		return cregs[0x208];
	else if (addr == 0x308)
		return cregs[0x304];
	else if (addr == 0x414)
		return 0x4000; //TO CHECK!!! requested by CDROMDIPIR
	else if ((addr >= 0x500) && (addr < 0x540))
		return _xbus_GetRes();
	else if ((addr >= 0x540) && (addr < 0x580)) {
		return _xbus_GetPoll();
	} else if ((addr >= 0x580) && (addr < 0x5c0))
		return _xbus_GetStatusFIFO();
	else if ((addr >= 0x5c0) && (addr < 0x600))
		return _xbus_GetDataFIFO();
	else if (addr == 0x0)
		return 0x02020000;
	else if ((addr >= 0x1800) && (addr <= 0x1fff)) {
		/* The DSPP N32/N16 windows are used as CLIO-visible MMIO windows by
		 * BIOS/Portfolio code.  Opera falls through to the CLIO register array
		 * for reads here, and MAME only models host writes.  Do not data-abort
		 * legal aligned reads such as Alone in the Dark PAL reading 0x034025e0;
		 * return the latched CLIO word rather than DSP code RAM. */
		clio_dspp_nmem_read_count++;
		clio_dspp_nmem_last_read_address = 0x03400000u + addr;
		return cregs[addr];
	} else if ((addr >= 0x2000) && (addr <= 0x2fff)) {
		clio_dspp_nmem_read_count++;
		clio_dspp_nmem_last_read_address = 0x03400000u + addr;
		return cregs[addr];
	}
	else if ((addr >= 0x3800) && (addr <= 0x3bff)) {//0x0340 3800 .. 0x0340 3BFF
		if (!clio_addr_is_dsp_eo_read_pair_primary(addr) && clio_dsp_resource_fault(addr, addr))
			return 0xBADACCE5;
		//2DSPW per 1ARMW
		DSPA = (addr - 0x3800) >> 1;
		DSPA &= 0xff;
		DSPA += 0x300;
		DSPW1 = _dsp_ReadIMem(DSPA);
		DSPW2 = _dsp_ReadIMem(DSPA + 1);
		return ((DSPW1 << 16) | DSPW2);
	} else if ((addr >= 0x3c00) && (addr <= 0x3fff)) {//0x0340 3C00 .. 0x0340 3FFF
		if (!clio_addr_is_dsp_eo_read_single_primary(addr) && clio_dsp_resource_fault(addr, addr))
			return 0xBADACCE5;
		DSPA = (addr - 0x3c00) >> 2;
		DSPA &= 0xff;
		DSPA += 0x300;
		return (_dsp_ReadIMem(DSPA));
	} else if (addr == 0x17F0)
		return prng32();
	else if (addr == 0x3c)
		return prng32();
	else if (addr == 0x17D0) //Read DSP/ARM Semaphore
		return _dsp_ARMread2sema4();
	else if (addr >= 0x100 && addr <= 0x17c)
		return cregs[addr] & 0xffff;

	if (_arm_GetStrictBusFaults() && !clio_addr_is_known_read(addr)) {
		_arm_DataAbort(0x03400000u + addr, 5);
		return 0xBADACCE5;
	}

	return cregs[addr];
}

void _clio_UpdateVCNT(int line, int halfframe)
{
	//	Poke(0x34,Peek(0x34)+1);
	cregs[0x34] = (halfframe << 11) + line;
}

void _clio_SetTimers(uint32_t v200, uint32_t v208)
{
	(void)v200;
	(void)v208;
}

void _clio_ClearTimers(uint32_t v204, uint32_t v20c)
{
	(void)v204;
	(void)v20c;
}

void _clio_DoTimers(void)
{
	uint32_t timer;
	uint16_t counter;
	bool NeedDecrementNextTimer = true; // Need decrement for next timer

	for (timer = 0; timer < 16; timer++) {
		unsigned flag = cregs[(timer < 8) ? 0x200 : 0x208] >> ((timer * 4) & 31);

		if ( !(flag & CASCADE) ) NeedDecrementNextTimer = true;

		if ( NeedDecrementNextTimer && (flag & DECREMENT) ) {
			counter = cregs[0x100 + timer * 8];
			if ((NeedDecrementNextTimer = (counter-- == 0))) {
				if ((timer & 1)) { // Only odd timers can generate
					// generate the interrupts because be overflow
					_clio_GenerateFiq(1 << (10 - timer / 2), 0);
				}
				if (flag & RELOAD) { // reload timer by reload value
					counter = cregs[0x100 + timer * 8 + 4];
					//return;
				} else {// timer stopped -> reset it's flag DECREMENT
					cregs[(timer < 8) ? 0x200 : 0x208] &= ~( DECREMENT << ((timer * 4) & 31) );
				}
			}
			cregs[0x100 + timer * 8] = counter;
		}else
			NeedDecrementNextTimer = false;
	}
}

uint32_t _clio_GetTimerDelay(void)
{
	return cregs[0x220];
}


void HandleDMA(uint32_t val)
{
	cregs[0x304] |= val;

	if (val & 0x00100000) {
		unsigned src;
		unsigned trg;
		int len;
		uint8_t b0, b1, b2, b3;

		cregs[0x304] &= ~0x00100000;
		src = _madam_Peek(0x540);
		trg = src;
		len = _madam_Peek(0x544);
		clio_xbus_dma_pulse_count++;
		clio_xbus_dma_last_addr = trg;
		clio_xbus_dma_last_len = (uint32_t)len;
		cregs[0x400] &= ~0x80;

		if ((cregs[0x404]) & 0x200) {
			while (len >= 0) {
				b3 = _xbus_GetDataFIFO();
				b2 = _xbus_GetDataFIFO();
				b1 = _xbus_GetDataFIFO();
				b0 = _xbus_GetDataFIFO();

#ifdef MSB_FIRST
				_mem_write8(trg, b3);
				_mem_write8(trg + 1, b2);
				_mem_write8(trg + 2, b1);
				_mem_write8(trg + 3, b0);
#else
				_mem_write8(trg, b0);
				_mem_write8(trg + 1, b1);
				_mem_write8(trg + 2, b2);
				_mem_write8(trg + 3, b3);
#endif

				trg += 4;
				len -= 4;
			}
			cregs[0x400] |= 0x80;

		} else {
			while (len >= 0) {
				b3 = _xbus_GetDataFIFO();
				b2 = _xbus_GetDataFIFO();
				b1 = _xbus_GetDataFIFO();
				b0 = _xbus_GetDataFIFO();

#ifdef MSB_FIRST
				_mem_write8(trg, b3);
				_mem_write8(trg + 1, b2);
				_mem_write8(trg + 2, b1);
				_mem_write8(trg + 3, b0);
#else
				_mem_write8(trg, b0);
				_mem_write8(trg + 1, b1);
				_mem_write8(trg + 2, b2);
				_mem_write8(trg + 3, b3);
#endif

				trg += 4;
				len -= 4;
			}
			cregs[0x400] |= 0x80;

		}
		len = 0xFFFFFFFC;
		_madam_Poke(0x544, len);
		_clio_GenerateFiq(1 << 29, 0);

		return;
	}//XBDMA transfer
}

void _clio_Init(int ResetReson)
{
	unsigned i;

	clio_fiq_generate_count = 0;
	clio_fiq_need_count = 0;
	clio_last_fiq_reason1 = 0;
	clio_last_fiq_reason2 = 0;
	clio_eififo_read_count = 0;
	clio_eififo_empty_read_count = 0;
	clio_eififo_reload_count = 0;
	memset(clio_eififo_empty_by_channel, 0, sizeof(clio_eififo_empty_by_channel));
	memset(clio_eififo_reload_by_channel, 0, sizeof(clio_eififo_reload_by_channel));
	memset(clio_eififo_last_value, 0, sizeof(clio_eififo_last_value));
	clio_last_eififo_empty_channel = 0xffffffffu;
	clio_last_eififo_reload_channel = 0xffffffffu;
	clio_eofifo_write_count = 0;
	clio_eofifo_disabled_write_count = 0;
	clio_eofifo_full_count = 0;
	clio_last_fifo_event = 0;
	clio_fifo_level_reassert_count = 0;
	clio_fifo_level_reassert_mask = 0;
	clio_dspp_control_write_count = 0;
	clio_dspp_control_last_value = 0;
	clio_dspp_control_non_gw_count = 0;
	clio_dspp_reset_write_count = 0;
	clio_dspp_reset_last_value = 0;
	clio_fifo_reload_dma_block_count = 0;
	clio_fifo_last_reload_dma_block_channel = 0xffffffffu;

	for (i = 0; i < 32768; i++)
		cregs[i] = 0;

	//cregs[8]=240;

	cregs[0x0028] = ResetReson;
	cregs[0x0400] = 0x80;
	cregs[0x220] = 64;
	_clio_SetVideoStandard(clio_video_standard_pal);
	Mregs = _madam_GetRegs();

}
uint16_t  _clio_EIFIFO(uint16_t channel)
{
	unsigned base = 0x400 + (channel * 16);
	unsigned mask = 1 << channel;

	clio_eififo_read_count++;

	(void)base;
	(void)mask;

	if (FIFOI[channel].StartAdr != 0) {//channel enabled
		uint32_t val;

		if ( (FIFOI[channel].StartLen - PTRI[channel]) > 0 ) {
			uint32_t dma_addr = FIFOI[channel].StartAdr + (uint32_t)PTRI[channel];
			if (_arm_GetStrictBusFaults() && !clio_ram_range_valid(dma_addr, 2)) {
				clio_fifo_bounds_fault(dma_addr);
				return 0;
			}
#ifdef MSB_FIRST
			val = _mem_read16( dma_addr );
#else
			val = _mem_read16( dma_addr ^ 2 );
#endif
			if (channel < 13) clio_eififo_last_value[channel] = (uint16_t)val;
			PTRI[channel] += 2;
		} else {
			PTRI[channel] = 0;
			_clio_GenerateFiq(1 << (channel + 16), 0);//generate fiq
			if (FIFOI[channel].NextAdr != 0 && clio_ei_dma_channel_enabled(channel)) {// reload enabled see patent WO09410641A1, 49.16 and Opera 7cef4a7
				uint32_t reload_addr = FIFOI[channel].NextAdr;
				int reload_len = FIFOI[channel].NextLen;
				clio_eififo_reload_count++;
				if (channel < 13)
					clio_eififo_reload_by_channel[channel]++;
				clio_last_eififo_reload_channel = channel;
				clio_last_fifo_event = CLIO_FIFO_EVENT_EI_RELOAD(channel);
				FIFOI[channel].StartAdr = reload_addr;
				FIFOI[channel].StartLen = reload_len;
				/* Do not clear NextAdr/NextLen on consumption. The patented/Opera
				 * behavior treats the next descriptor as reusable while the DMA
				 * channel remains enabled; disabling DMA or zeroing start stops it. */
				{
					uint32_t dma_addr = FIFOI[channel].StartAdr + (uint32_t)PTRI[channel];
					if (_arm_GetStrictBusFaults() && !clio_ram_range_valid(dma_addr, 2)) {
						clio_fifo_bounds_fault(dma_addr);
						return 0;
					}
#ifdef MSB_FIRST
					val = _mem_read16(dma_addr);         //get the value!!!
#else
					val = _mem_read16(dma_addr ^ 2);     //get the value!!!
#endif
					if (channel < 13) clio_eififo_last_value[channel] = (uint16_t)val;
				}
				PTRI[channel] += 2;
			} else {
				if (FIFOI[channel].NextAdr != 0 && !clio_ei_dma_channel_enabled(channel)) {
					clio_fifo_reload_dma_block_count++;
					clio_fifo_last_reload_dma_block_channel = channel;
				}
				clio_eififo_empty_read_count++;
				if (channel < 13)
					clio_eififo_empty_by_channel[channel]++;
				clio_last_eififo_empty_channel = channel;
				clio_last_fifo_event = CLIO_FIFO_EVENT_EI_EMPTY(channel);
				FIFOI[channel].StartAdr = 0;
				val = 0;
			}
		}

		return val;
	}

	// JMK SEZ: What is this? It was commented out along with this whole "else"
	//          block, but I had to bring this else block back from the dead
	//          in order to initialize val appropriately.

	// _clio_GenerateFiq(1<<(channel+16),0);
	clio_eififo_empty_read_count++;
	if (channel < 13)
		clio_eififo_empty_by_channel[channel]++;
	clio_last_eififo_empty_channel = channel;
	clio_last_fifo_event = CLIO_FIFO_EVENT_EI_EMPTY(channel);
	return (channel < 13) ? clio_eififo_last_value[channel] : 0;
}

void  _clio_EOFIFO(uint16_t channel, uint16_t val)
{
	clio_eofifo_write_count++;
	/* Channel disabled? */
	if (FIFOO[channel].StartAdr == 0) {
		clio_eofifo_disabled_write_count++;
		clio_last_fifo_event = CLIO_FIFO_EVENT_EO_DISABLED(channel);
		return;
	}

	if ( (FIFOO[channel].StartLen - PTRO[channel]) > 0 ) {
		uint32_t dma_addr = FIFOO[channel].StartAdr + (uint32_t)PTRO[channel];
		if (_arm_GetStrictBusFaults() && !clio_ram_range_valid(dma_addr, 2)) {
			clio_fifo_bounds_fault(dma_addr);
			return;
		}
#ifdef MSB_FIRST
		_mem_write16(dma_addr, val);
#else
		_mem_write16(dma_addr ^ 2, val);
#endif
		PTRO[channel] += 2;
	} else {
		PTRO[channel] = 0;
		clio_eofifo_full_count++;
		clio_last_fifo_event = CLIO_FIFO_EVENT_EO_FULL(channel);
		_clio_GenerateFiq(1 << (channel + 12), 0);//generate fiq

		if (FIFOO[channel].NextAdr != 0 && clio_eo_dma_channel_enabled(channel)) { //reload enabled?
			uint32_t reload_addr = FIFOO[channel].NextAdr;
			int reload_len = FIFOO[channel].NextLen;
			FIFOO[channel].StartAdr = reload_addr;
			FIFOO[channel].StartLen = reload_len;
		} else {
			if (FIFOO[channel].NextAdr != 0 && !clio_eo_dma_channel_enabled(channel)) {
				clio_fifo_reload_dma_block_count++;
				clio_fifo_last_reload_dma_block_channel = 0x100u | channel;
			}
			FIFOO[channel].StartAdr = 0;
		}
	}
}

uint16_t  _clio_EIFIFONI(uint16_t channel)
{
	uint32_t dma_addr;
	clio_eififo_read_count++;
	if (FIFOI[channel].StartAdr == 0 || (FIFOI[channel].StartLen - PTRI[channel]) <= 0) {
		if (FIFOI[channel].StartAdr != 0)
			_clio_GenerateFiq(1 << (channel + 16), 0);
		clio_eififo_empty_read_count++;
		if (channel < 13)
			clio_eififo_empty_by_channel[channel]++;
		clio_last_eififo_empty_channel = channel;
		clio_last_fifo_event = CLIO_FIFO_EVENT_EI_EMPTY(channel);
		clio_reassert_level_fifo_irqs();
		return (channel < 13) ? clio_eififo_last_value[channel] : 0;
	}
	dma_addr = FIFOI[channel].StartAdr + (uint32_t)PTRI[channel];
	if (_arm_GetStrictBusFaults() && !clio_ram_range_valid(dma_addr, 2)) {
		clio_fifo_bounds_fault(dma_addr);
		return 0;
	}
#ifdef MSB_FIRST
	return _mem_read16(dma_addr);
#else
	return _mem_read16(dma_addr ^ 2);
#endif
}

uint16_t   _clio_GetEIFIFOStat(uint8_t channel)
{
	if (FIFOI[channel].StartAdr != 0 && (FIFOI[channel].StartLen - PTRI[channel]) > 0)
		return 2;

	if (FIFOI[channel].StartAdr != 0 && FIFOI[channel].NextAdr != 0 && FIFOI[channel].NextLen > 0 && clio_ei_dma_channel_enabled(channel))
		return 2;

	if (FIFOI[channel].StartAdr != 0)
		return 1;

	return 0;
}

uint16_t   _clio_GetEOFIFOStat(uint8_t channel)
{
	if ( FIFOO[channel].StartAdr != 0 )
		return 1;
	return 0;
}

static void clio_validate_fifo_programming(uint32_t adr)
{
	unsigned channel = (adr >> 4) & 0xf;
	if (!_arm_GetStrictBusFaults())
		return;
	if ((adr & 0x500) == 0x400) {
		if (channel < 13) {
			if (!clio_fifo_span_valid(FIFOI[channel].StartAdr, FIFOI[channel].StartLen))
				clio_fifo_bounds_fault(FIFOI[channel].StartAdr);
			if (!clio_fifo_span_valid(FIFOI[channel].NextAdr, FIFOI[channel].NextLen))
				clio_fifo_bounds_fault(FIFOI[channel].NextAdr);
		}
	} else {
		if (channel < 4) {
			if (!clio_fifo_span_valid(FIFOO[channel].StartAdr, FIFOO[channel].StartLen))
				clio_fifo_bounds_fault(FIFOO[channel].StartAdr);
			if (!clio_fifo_span_valid(FIFOO[channel].NextAdr, FIFOO[channel].NextLen))
				clio_fifo_bounds_fault(FIFOO[channel].NextAdr);
		}
	}
}

void _clio_SetFIFO(uint32_t adr, uint32_t val)
{
	if ( (adr & 0x500) == 0x400) {

		switch (adr & 0xf) {
		case 0:
			FIFOI[(adr >> 4) & 0xf].StartAdr = val;
			FIFOI[(adr >> 4) & 0xf].NextAdr = 0;//see patent WO09410641A1, 46.25
			break;
		case 4:
			FIFOI[(adr >> 4) & 0xf].StartLen = val + 4;
			if (val == 0)
				FIFOI[(adr >> 4) & 0xf].StartLen = 0;

			FIFOI[(adr >> 4) & 0xf].NextLen = 0;//see patent WO09410641A1, 46.25
			break;
		case 8:
			FIFOI[(adr >> 4) & 0xf].NextAdr = val;
			break;
		case 0xc:
			if (val != 0)
				FIFOI[(adr >> 4) & 0xf].NextLen = val + 4;
			else
				FIFOI[(adr >> 4) & 0xf].NextLen = 0;
			break;
		}
	} else {
		switch (adr & 0xf) {
		case 0:
			FIFOO[(adr >> 4) & 0xf].StartAdr = val;
			break;
		case 4:
			FIFOO[(adr >> 4) & 0xf].StartLen = val + 4;
			break;
		case 8:
			FIFOO[(adr >> 4) & 0xf].NextAdr = val;
			break;
		case 0xc:
			FIFOO[(adr >> 4) & 0xf].NextLen = val + 4;
			break;
		}
	}
	clio_validate_fifo_programming(adr);
	clio_reassert_level_fifo_irqs();
}

void _clio_Reset(void)
{
	int i;

	for (i = 0; i < 65536; i++)
		cregs[i] = 0;
}

uint32_t _clio_FIFOStruct(uint32_t addr)
{
	if ((addr & 0x500) == 0x400) {
		switch (addr & 0xf) {
		case 0:
			return FIFOI[(addr >> 4) & 0xf].StartAdr + PTRI[(addr >> 4) & 0xf];
		case 4:
			return FIFOI[(addr >> 4) & 0xf].StartLen - PTRI[(addr >> 4) & 0xf];
		case 8:
			return FIFOI[(addr >> 4) & 0xf].NextAdr;
		case 0xc:
			return FIFOI[(addr >> 4) & 0xf].NextLen;
		}
	}

	switch (addr & 0xf) {
	case 0:
		return FIFOO[(addr >> 4) & 0xf].StartAdr + PTRO[(addr >> 4) & 0xf];
	case 4:
		return FIFOO[(addr >> 4) & 0xf].StartLen - PTRO[(addr >> 4) & 0xf];
	case 8:
		return FIFOO[(addr >> 4) & 0xf].NextAdr;
	case 0xc:
		return FIFOO[(addr >> 4) & 0xf].NextLen;
	}

	return 0;

}
