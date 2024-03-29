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
#include <stdbool.h>
#include "retro_inline.h"

#include "freedocore.h"
#include "arm.h"
#include "vdlp.h"
#include "DSP.h"
#include "Clio.h"
#include "frame.h"
#include "Madam.h"
#include "SPORT.h"
#include "XBUS.h"
#include "DiagPort.h"
#include "quarz.h"
#include "fs.h"
#include "sound.h"
#include "input.h"

extern void* Getp_NVRAM(void);
extern void* Getp_ROMS(void);
extern void* Getp_RAMS(void);

static INLINE uint32_t _bswap(uint32_t x)
{
	return (x >> 24) | ((x >> 8) & 0x0000FF00L) | ((x & 0x0000FF00L) << 8) | (x << 24);
}

extern void* _xbplug_MainDevice(int proc, void* data);

extern char biosFile[128];

int _3do_Init(void)
{
	int i;
	uint8_t *Memory;
	uint8_t *rom;

	Memory = _arm_Init();

	fsReadBios(biosFile, Getp_ROMS());
	
	rom = (uint8_t*)Getp_ROMS();
	for (i = (1024 * 1024 * 2) - 4; i >= 0; i -= 4)
		*(int*)(rom + i) = _bswap(*(int*)(rom + i));

	_vdl_Init(Memory + 0x200000);   // Visible only VRAM to it
	_sport_Init(Memory + 0x200000); // Visible only VRAM to it
	_madam_Init(Memory);

	_xbus_Init(_xbplug_MainDevice);

	_clio_Init(0x40); // 0x40 for start from  3D0-CD, 0x01/0x02 from PhotoCD ?? (NO use 0x40/0x02 for BIOS test)
	_dsp_Init();
	_frame_Init();
	_diag_Init(-1); // Select test, use -1 -- if d'nt need tests
	/*
	   00	DIAGNOSTICS TEST	(run of test: 1F, 24, 25, 32, 50, 51, 60, 61, 62, 68, 71, 75, 80, 81, 90)
	   01	AUTO-DIAG TEST		(run of test: 1F, 24, 25, 32, 50, 51, 60, 61, 62, 68,         80, 81, 90)
	   12	DRAM1 DATA TEST
	   1A	DRAM2 DATA TEST
	   1E	EARLY RAM TEST
	   1F	RAM DATA TEST
	   22	VRAM1 DATA TEST
	   24	VRAM1 FLASH TEST
	   25	VRAM1 SPORT TEST
	   32	SRAM DATA TEST
	   50	MADAM TEST
	   51	CLIO TEST
	   60	CD-ROM POLL TEST
	   61	CD-ROM PATH TEST
	   62	CD-ROM READ TEST	???
	   63	CD-ROM AutoAdjustValue TEST
	   67	CD-ROM#2 AutoAdjustValue TEST
	   68  DEV#15 POLL TEST
	   71	JOYPAD1 PRESS TEST
	   75	JOYPAD1 AUDIO TEST
	   80	SIN WAVE TEST
	   81	MUTING TEST
	   90	COLORBAR
	   F0	CHECK TESTTOOL  ???
	   F1	REVISION TEST
	   FF	TEST END (halt)
	 */
	_xbus_DevLoad(0, NULL);

	_qrz_Init();

	return 1;
}

struct VDLFrame *curr_frame;
bool skipframe;
int count_samples = 0;

static void *swapFrame(void *curr_frame)
{
	return curr_frame;
}

void _3do_InternalFrame(int cycles)
{
	int line;

	_qrz_PushARMCycles(cycles);

	if (_qrz_QueueDSP())
	{
		soundFillBuffer(_dsp_Loop());
		count_samples++;	
	}

	if (_qrz_QueueTimer())
		_clio_DoTimers();

	if (_qrz_QueueVDL()) {
		line = _qrz_VDCurrLine();
		_clio_UpdateVCNT(line, _qrz_VDHalfFrame());

		if (!skipframe)
			_vdl_DoLineNew(line, curr_frame);

		/* Does nothing right now - Gameblabla */
		/*if (line == 16 && skipframe)
		{
			io_interface(EXT_FRAMETRIGGER_MT, NULL);
		}*/

		if (line == _clio_v0line())
			_clio_GenerateFiq(1 << 0, 0);

		if (line == _clio_v1line()) {
			_clio_GenerateFiq(1 << 1, 0);
			_madam_KeyPressed(inputRead(), inputLength());
			//curr_frame->srcw=320;
			//curr_frame->srch=240;
			if (!skipframe)
			{
				curr_frame = swapFrame(curr_frame);
			}
		}
	}
}

void _3do_Frame(struct VDLFrame *frame, bool __skipframe)
{
	int i   = 0;
	int cnt = 0;

	curr_frame = frame;
	skipframe = __skipframe;

	do {
		while (Get_madam_FSM() == FSM_INPROCESS) {
			_madam_HandleCEL();
			Set_madam_FSM(FSM_IDLE);
		}

		/* anything between 16 .. 250 is good */
		cnt = cpu->Exec(64);
		_3do_InternalFrame(cnt);
		i += cnt;
	} while (i < (ARM_CLOCK / 60));
}

void _3do_Destroy()
{
	_arm_Destroy();
	_xbus_Destroy();
}

uint32_t _3do_SaveSize(void)
{
	uint32_t tmp = _arm_SaveSize();

	tmp += _vdl_SaveSize();
	tmp += _dsp_SaveSize();
	tmp += _clio_SaveSize();
	tmp += _qrz_SaveSize();
	tmp += _sport_SaveSize();
	tmp += _madam_SaveSize();
	tmp += _xbus_SaveSize();
	tmp += 16 * 4;
	return tmp;
}

void _3do_Save(void *buff)
{
	uint8_t *data = (uint8_t*)buff;
	int *indexes = (int*)buff;

	indexes[0] = 0x97970101;
	indexes[1] = 16 * 4;
	indexes[2] = indexes[1] + _arm_SaveSize();
	indexes[3] = indexes[2] + _vdl_SaveSize();
	indexes[4] = indexes[3] + _dsp_SaveSize();
	indexes[5] = indexes[4] + _clio_SaveSize();
	indexes[6] = indexes[5] + _qrz_SaveSize();
	indexes[7] = indexes[6] + _sport_SaveSize();
	indexes[8] = indexes[7] + _madam_SaveSize();
	indexes[9] = indexes[8] + _xbus_SaveSize();

	_arm_Save(&data[indexes[1]]);
	_vdl_Save(&data[indexes[2]]);
	_dsp_Save(&data[indexes[3]]);
	_clio_Save(&data[indexes[4]]);
	_qrz_Save(&data[indexes[5]]);
	_sport_Save(&data[indexes[6]]);
	_madam_Save(&data[indexes[7]]);
	_xbus_Save(&data[indexes[8]]);

}

bool _3do_Load(void *buff)
{
	uint8_t *data = (uint8_t*)buff;
	int *indexes = (int*)buff;

	if ((uint32_t)indexes[0] != 0x97970101)
		return false;

	_arm_Load(&data[indexes[1]]);
	_vdl_Load(&data[indexes[2]]);
	_dsp_Load(&data[indexes[3]]);
	_clio_Load(&data[indexes[4]]);
	_qrz_Load(&data[indexes[5]]);
	_sport_Load(&data[indexes[6]]);
	_madam_Load(&data[indexes[7]]);
	_xbus_Load(&data[indexes[8]]);

	return true;
}


int fixmode = 0;
int speedfixes = 0;
int sf = 0;
int sdf = 0;
int unknownflag11 = 0;
int jw = 0;
int cnbfix = 0;

/*
void _freedo_Interface(int procedure, void *datum)
{
	int line;

	switch (procedure) {
	case FDP_INIT:
		sf = 5000000;
		cnbfix = 0;
		io_interface = (_ext_Interface)datum;
		_3do_Init();
		break;
	case FDP_DESTROY:
		_3do_Destroy();
		break;
	case FDP_DO_EXECFRAME:
		_3do_Frame((struct VDLFrame*)datum, false);
		break;
	case FDP_DO_EXECFRAME_MT:
		_3do_Frame((struct VDLFrame*)datum, true);
		break;
	case FDP_DO_FRAME_MT:
		line = 0;
		while (line < 256) _vdl_DoLineNew(line++, (struct VDLFrame*)datum);
		break;
	case FDP_GET_SAVE_SIZE:
		_3do_SaveSize();
		break;
	case FDP_DO_SAVE:
		_3do_Save(datum);
		break;
	case FDP_DO_LOAD:
		sf = 0;
		_3do_Load(datum);
		break;
	case FDP_GETP_NVRAM:
		Getp_NVRAM();
		break;
	case FDP_GETP_RAMS:
		Getp_RAMS();
		break;
	case FDP_GETP_ROMS:
		Getp_ROMS();
		break;
	case FDP_GETP_PROFILE:
		break;
	#ifdef ADJUSTABLE_CLOCK
	case FDP_SET_ARMCLOCK:
		THE_ARM_CLOCK = 0;
		ARM_CLOCK = (intptr_t)datum;
		break;
	#endif
	case FDP_SET_TEXQUALITY:
		// Not supported
		break;
//	case FDP_SET_FIX_MODE:
//		fixmode=(intptr_t)datum;
//		break;
//	case FDP_GET_FRAME_BITMAP:
//		GetFrameBitmapParams* param = (GetFrameBitmapParams*)datum;
//		Get_Frame_Bitmap(
//			param->sourceFrame
//			, param->destinationBitmap
//			, param->destinationBitmapWidthPixels
//			, param->bitmapCrop
//			, param->copyWidthPixels
//			, param->copyHeightPixels
//			, param->addBlackBorder
//			, param->copyPointlessAlphaByte
//			, param->allowCrop
//			, (ScalingAlgorithm)param->scalingAlgorithm
//			, &param->resultingWidth
//			, &param->resultingHeight);
//		break;
	}
}
*/
