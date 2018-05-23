/*
    This file is part of 3d'oh, a multiplatform 3do emulator written by Gabriel Ernesto Cabral.

    3d'oh is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    3d'oh is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with 3d'oh.  If not, see <http://www.gnu.org/licenses/>.

 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <SDL/SDL.h>
#include "freedocore.h"
#include "common.h"
#include "timer.h"


char* pNVRam;
extern _ext_Interface io_interface;
_ext_Interface fd_interface;
int onsector = 0;
char biosFile[128];
char imageFile[128];
char configFile[128];

unsigned char __temporalfixes;

int count_samples = 0;

int initEmu();

uint32 ReverseBytes(uint32 value)
{
	return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 | (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24;
}


void readNvRam(void *pnvram)
{
	/*FILE* bios1;*/
	/*long fsize;*/
	/*char *buffer;*/
	NvRamStr *nvramStruct;

	nvramStruct = (NvRamStr*)pnvram;

	////////////////
	// Fill out the volume header.
	nvramStruct->recordType = 0x01;
	int x;
	for (x = 0; x < 5; x++)
		nvramStruct->syncBytes[x] = (char)'Z';
	nvramStruct->recordVersion = 0x02;
	nvramStruct->flags = 0x00;
	for (x = 0; x < 32; x++)
		nvramStruct->comment[x] = 0;

	nvramStruct->label[0] = (char)'n';
	nvramStruct->label[1] = (char)'v';
	nvramStruct->label[2] = (char)'r';
	nvramStruct->label[3] = (char)'a';
	nvramStruct->label[4] = (char)'m';

	for (x = 5; x < 32; x++)
		nvramStruct->label[x] = 0;

	nvramStruct->id         = ReverseBytes(0xFFFFFFFF);
	nvramStruct->blockSize  = ReverseBytes(0x00000001);     // Yep, one byte per block.
	nvramStruct->blockCount = ReverseBytes(0x00008000);     // 32K worth of NVRAM data.

	nvramStruct->rootDirId        = ReverseBytes(0xFFFFFFFE);
	nvramStruct->rootDirBlocks    = ReverseBytes(0x00000000);
	nvramStruct->rootDirBlockSize = ReverseBytes(0x00000001);
	nvramStruct->lastRootDirCopy  = ReverseBytes(0x00000000);

	nvramStruct->rootDirCopies[0] = ReverseBytes(0x00000084);
	for (x = 1; x < 8; x++)
		nvramStruct->rootDirCopies[x] = 0;

	/*int w = sizeof(NvRamStr) / 4;*/
}


void loadRom1(void *prom)
{
	fsReadBios(biosFile, prom);
}


void *swapFrame(void *curr_frame)
{
	return curr_frame;
}

void * emuinterface(int procedure, void *datum)
{
	/*typedef void *(*func_type)(void);*/

	switch (procedure) {
	case EXT_READ_ROMS:
		loadRom1(datum);
		break;
	case EXT_READ2048:
		fsReadBlock(datum, onsector);
		break;
	case EXT_GET_DISC_SIZE:
		return (void*)fsReadDiscSize();
		break;
	case EXT_ON_SECTOR:
		onsector = *((int*)&datum);
		break;
	case EXT_READ_NVRAM:
		readNvRam(datum);
		break;
	case EXT_WRITE_NVRAM:
		break;
	case EXT_PUSH_SAMPLE:
		soundFillBuffer(*((unsigned int*)&datum));
		count_samples++;
		break;
	case EXT_SWAPFRAME:
		return swapFrame(datum);
		break;
	case EXT_GETP_PBUSDATA:
		return (void*)inputRead();
		break;
	case EXT_GET_PBUSLEN:
		return (void*)inputLength();
		break;
	case EXT_FRAMETRIGGER_MT:
		break;
	default:
		//	return _freedo_Interface(procedure,datum);
		break;
	}
	;

	return (void*)readNvRam;
}

void readConfiguration(char* config)
{
	configOpen(config);
	configClose();
}

#undef main
int main(int argc, char *argv[])
{
	(void)argc;
	char home[128];

#ifndef _WIN32
	snprintf(home, sizeof(home), "%s/.3doh", getenv("HOME"));
#else
	strcpy(home, ".3doh");
#endif
	if (access( home, F_OK ) == -1) {
		printf("Creating home directory...\n");
		printf("Put your bios there (rename it to bios.bin)\n");
		mkdir(home
#ifndef _WIN32
		, 0755
#endif
		);
		return(0);
	}

#ifndef _WIN32
	snprintf(biosFile, sizeof(biosFile), "%s/.3doh/bios.bin", getenv("HOME"));
	//snprintf(configFile, sizeof(configFile), "%s/.3doh/config.ini", getenv("HOME"));
	snprintf(imageFile, sizeof(imageFile), argv[1]);
#else
	strcpy(biosFile, ".3doh/bios.bin");
	strcpy(imageFile, argv[1]);
#endif

	fsInit();
	/*readConfiguration(configFile);*/

	if (!initEmu())
		return 0;

	fd_interface(FDP_DESTROY, (void*)0);
	fsClose();

	return 0;
}




int initEmu()
{
	int quit = 0;
	int waitfps;

	io_interface = &emuinterface;
	fd_interface = (_ext_Interface)&_freedo_Interface;
	videoInit();
	soundInit();
	inputInit();

	if (!fsOpenIso(imageFile))
		return 0;

	fd_interface(FDP_SET_ARMCLOCK, (void*)12500000);
	fd_interface(FDP_SET_TEXQUALITY, (void*)0);

	_3do_Init();

	while (!quit) {
		extern struct VDLFrame *frame;

		fd_interface(FDP_DO_EXECFRAME_MT, (struct VDLFrame*)frame);
		fd_interface(FDP_DO_FRAME_MT, (struct VDLFrame*)frame);

		videoFlip();

		quit = inputQuit();

		/* Framerate control */
		synchronize_us();
	}


	/* Close everything and return */
	inputClose();
	soundClose();
	videoClose();
	SDL_Quit();

	return 1;
}
