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

#ifndef DSP_3DO_HEADER
#define DSP_3DO_HEADER

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t _dsp_Loop(void);

uint16_t  _dsp_ReadIMem(uint16_t addr);
void  _dsp_WriteIMem(uint16_t addr, uint16_t val);
void  _dsp_WriteMemory(uint16_t addr, uint16_t val);
void  _dsp_SetRunning(bool val);
void  _dsp_ARMwrite2sema4(unsigned int val);
unsigned int _dsp_ARMread2sema4(void);

void _dsp_Init(void);
void _dsp_Reset(void);

unsigned int _dsp_SaveSize(void);
void _dsp_Save(void *buff);
void _dsp_Load(void *buff);

void _dsp_SetStrictResourceFaults(bool enabled);
bool _dsp_GetStrictResourceFaults(void);
bool _dsp_EffectiveStrictResourceFaults(void);
void _dsp_StrictResourceAbort(uint32_t bus_addr, uint32_t detail);
uint32_t _dsp_GetResourceFaultCount(void);
uint32_t _dsp_GetResourceMirrorFaultCount(void);
uint32_t _dsp_GetLastResourceFaultAddress(void);
uint32_t _dsp_GetLastResourceFaultDetail(void);
uint32_t _dsp_GetRunStartCount(void);
uint32_t _dsp_GetRunStopCount(void);
uint32_t _dsp_GetResetCount(void);
uint32_t _dsp_GetIntWriteCount(void);
uint32_t _dsp_GetLastIntValue(void);
uint32_t _dsp_GetArmSemaWriteCount(void);
uint32_t _dsp_GetArmSemaReadCount(void);
uint32_t _dsp_GetDspSemaWriteCount(void);
uint32_t _dsp_GetDspSemaAckCount(void);
uint32_t _dsp_GetCpuSupplyWriteCount(void);
uint32_t _dsp_GetCpuSupplyReadCount(void);
uint32_t _dsp_GetCpuSupplyRandomReadCount(void);
uint32_t _dsp_GetLastCpuSupplyChannel(void);
uint32_t _dsp_GetCurrentPC(void);
uint32_t _dsp_GetCounterValue(void);
uint32_t _dsp_GetReloadValue(void);
uint32_t _dsp_GetCurrentStatus(void);
uint32_t _dsp_GetAudioTickCount(void);
uint32_t _dsp_GetCounterReloadCount(void);
uint32_t _dsp_GetProgramFrameCount(void);
uint32_t _dsp_GetSleepCount(void);
uint32_t _dsp_GetDeferredTickCount(void);
uint32_t _dsp_GetMultiReloadCount(void);
uint32_t _dsp_GetAudlockWriteCount(void);
uint32_t _dsp_GetAudlockResetCount(void);
uint32_t _dsp_GetLastAudioStatusValue(void);

#ifdef __cplusplus
}
#endif

#endif
