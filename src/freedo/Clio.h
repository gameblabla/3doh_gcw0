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

// Clio.h: interface for the CClio class.
//
//////////////////////////////////////////////////////////////////////

#ifndef CLIO_3DO_HEADER
#define CLIO_3DO_HEADER

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int _clio_v0line(void);
int _clio_v1line(void);
bool _clio_NeedFIQ(void);

uint32_t _clio_FIFOStruct(uint32_t addr);
void _clio_Reset(void);
void _clio_SetFIFO(uint32_t adr, uint32_t val);
uint16_t  _clio_GetEOFIFOStat(uint8_t channel);
uint16_t  _clio_GetEIFIFOStat(uint8_t channel);
uint16_t  _clio_EIFIFONI(uint16_t channel);
void  _clio_EOFIFO(uint16_t channel, uint16_t val);
uint16_t  _clio_EIFIFO(uint16_t channel);

void _clio_Init(int ResetReson);
void _clio_SetVideoStandard(int pal);
int  _clio_GetVideoStandard(void);

void _clio_DoTimers(void);
uint32_t _clio_Peek(uint32_t addr);
int _clio_Poke(uint32_t addr, uint32_t val);
void _clio_UpdateVCNT(int line, int halfframe);
void _clio_GenerateFiq(uint32_t reason1, uint32_t reason2);

uint32_t _clio_GetFiqGenerateCount(void);
uint32_t _clio_GetFiqNeedCount(void);
uint32_t _clio_GetLastFiqReason1(void);
uint32_t _clio_GetLastFiqReason2(void);
uint32_t _clio_GetIrq0Pending(void);
uint32_t _clio_GetIrq0Mask(void);
uint32_t _clio_GetIrq1Pending(void);
uint32_t _clio_GetIrq1Mask(void);
uint32_t _clio_GetEififoReadCount(void);
uint32_t _clio_GetEififoEmptyReadCount(void);
uint32_t _clio_GetEififoReloadCount(void);
uint32_t _clio_GetEofifoWriteCount(void);
uint32_t _clio_GetEofifoDisabledWriteCount(void);
uint32_t _clio_GetEofifoFullCount(void);
uint32_t _clio_GetLastFifoEvent(void);
uint32_t _clio_GetLastEififoEmptyChannel(void);
uint32_t _clio_GetLastEififoReloadChannel(void);
uint32_t _clio_GetEififoEmptyChannelCount(uint32_t channel);
uint32_t _clio_GetEififoReloadChannelCount(uint32_t channel);
uint32_t _clio_GetFifoLevelReassertCount(void);
uint32_t _clio_GetFifoLevelReassertMask(void);
uint32_t _clio_GetDSPPControlWriteCount(void);
uint32_t _clio_GetDSPPControlLastValue(void);
uint32_t _clio_GetDSPPControlNonGWCount(void);
uint32_t _clio_GetDSPPResetWriteCount(void);
uint32_t _clio_GetDSPPResetLastValue(void);
uint32_t _clio_GetFifoReloadDMABlockCount(void);
uint32_t _clio_GetFifoLastReloadDMABlockChannel(void);
uint32_t _clio_GetDSPPNMemReadCount(void);
uint32_t _clio_GetDSPPNMemLastReadAddress(void);
uint32_t _clio_GetXbusDmaPulseCount(void);
uint32_t _clio_GetXbusDmaLastLen(void);
uint32_t _clio_GetXbusDmaLastAddr(void);
uint32_t _clio_GetXbusDmaTimerAccum(void);
uint32_t _clio_GetXbusDmaTimerWindow(void);
uint32_t _clio_GetXbusTimer120AdjustCount(void);
uint32_t _clio_GetXbusTimer120LastIn(void);
uint32_t _clio_GetXbusTimer120LastOut(void);
void _clio_FieldTick(void);

uint32_t _clio_GetTimerDelay(void);

uint32_t _clio_SaveSize(void);
void _clio_Save(void *buff);
void _clio_Load(void *buff);

#ifdef __cplusplus
}
#endif

#endif
