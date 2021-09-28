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
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "freedocore.h"
#include "common.h"
#include "timer.h"
#include "tinyfps.h"
#include "vdlp.h"
#include "_3do_sys.h"
#ifndef __EMSCRIPTEN__
#include "font/font_drawing.h"
#endif

char* pNVRam;
/*extern _ext_Interface io_interface;
_ext_Interface fd_interface;*/
int onsector = 0;
char biosFile[128];
static char imageFile[128];
//static char configFile[128];

static unsigned char __temporalfixes;

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

void writeNvRam()
{
}


void loadRom1(void *prom)
{
	fsReadBios(biosFile, prom);
}


static int quit = 0;

static inline void mainloop(void)
{
	uint32_t line;
	extern struct VDLFrame *frame;

	_3do_Frame((struct VDLFrame*)frame, true);
	line = 0;
	#if defined(__EMSCRIPTEN__)
	for(line = 0; line < 256; line++)
	{
		_vdl_DoLineNew(line, (struct VDLFrame*)frame);
	}
	#else
	while (line < 256) _vdl_DoLineNew(line++, (struct VDLFrame*)frame);
	#endif

	videoFlip();

	quit = inputQuit();

	/* Framerate control */
	#ifndef SDL_TRIPLEBUF
	synchronize_us();
	#endif
}

int main(int argc, char *argv[])
{
	extern SDL_Surface *screen;
#ifdef SCALING
	extern SDL_Surface *rl_screen;
#endif
	SDL_Event event;
	char home[128];
	int error = 0;
	int waitfps;
	FILE* fp;

	/* Create Display before in case it crashes before that */
	videoInit();

#if defined(__EMSCRIPTEN__)

#elif !defined(_WIN32)
	snprintf(home, sizeof(home), "%s/.3doh", getenv("HOME"));
	if (access( home, F_OK ) == -1) {
		printf("Creating home directory...\n");
		printf("Put your bios there (rename it to bios.bin)\n");
		mkdir(home
#ifndef _WIN32
		, 0755
#endif
		);
		error = 2;
		goto got_error;
	}
#endif

#if defined(__EMSCRIPTEN__)
	snprintf(biosFile, sizeof(biosFile), "data/bios.bin");
	snprintf(imageFile, sizeof(imageFile), "data/game.iso");
#elif defined(_WIN32)
	strcpy(biosFile, "bios.bin");
	strcpy(imageFile, argv[1]);
#else
	snprintf(biosFile, sizeof(biosFile), "%s/.3doh/bios.bin", getenv("HOME"));
	//snprintf(configFile, sizeof(configFile), "%s/.3doh/config.ini", getenv("HOME"));
	snprintf(imageFile, sizeof(imageFile), "%s", argv[1]);
#endif
	fp = fopen(biosFile, "rb");
	if (!fp)
	{
		error = 2;
		goto got_error;
	}
	else
	{
		fclose(fp);
	}

	fsInit();
	/*readConfiguration(configFile);*/

	#ifdef FRAMECOUNTER
	initFpsFonts();
	#endif

	//io_interface = &emuinterface;
	soundInit();
	inputInit();

	if (!fsOpenIso(imageFile))
	{
		error = 1;
		goto got_error;
	}

	_3do_Init();

#ifdef __EMSCRIPTEN__
	printf("Emulation started\n");
	emscripten_set_main_loop(mainloop, 0, 1);
#else
	while (!quit) {
		mainloop();
	}
#endif

got_error:

/* That loop causes it to just shit itself */
#ifndef __EMSCRIPTEN__
	/* Jump here in case of error */
	if (error > 0)
	{
		while(!quit)
		{
			while (SDL_PollEvent(&event)) 
			{
				switch(event.type) 
				{
					case SDL_KEYDOWN:
						switch(event.key.keysym.sym) 
						{
							case SDLK_HOME:
							case SDLK_3:
							case SDLK_RCTRL:
							case SDLK_RETURN:
							case SDLK_ESCAPE:
								quit = 1;
							break;
							default:
							break;
						}
					break;
					case SDL_QUIT:
						quit = 1;
					break;
				}
			}
			
			switch(error)
			{
				case 1:
					print_string("ISO file INVALID or", 0xFFFF, 0, 0, 15, screen->pixels);
					print_string("can't be read, does not exist...", 0xFFFF, 0, 0, 30, screen->pixels);
					print_string(imageFile, 0xFFFF, 0, 0, 50, screen->pixels);	
					print_string("Make sure to use iso or", 0xFFFF, 0, 0, 80, screen->pixels);
					print_string("iso/cue dumps !", 0xFFFF, 0, 0, 100, screen->pixels);
				break;
				case 2:
					print_string("BIOS file INVALID", 0xFFFF, 0, 0, 15, screen->pixels);
					print_string("or DOES NOT EXIST !", 0xFFFF, 0, 0, 30, screen->pixels);
							
					print_string("Make sure to put the bios file in", 0xFFFF, 0, 0, 80, screen->pixels);
					print_string(home, 0xFFFF, 0, 0, 100, screen->pixels);
					print_string("as bios.bin, lowercase.", 0xFFFF, 0, 0, 120, screen->pixels);
				break;
			}
#ifdef SCALING
			SDL_SoftStretch(screen, NULL, rl_screen, NULL);
			SDL_Flip(rl_screen);
#else
			SDL_Flip(screen);
#endif
		}

	}
#else
			switch(error)
			{
				case 1:
					printf("ISO file INVALID\n");
				break;
				case 2:
					printf("BIOS file INVALID %s\n", biosFile);
				break;
			}
#endif

	/* Close everything and return */
	inputClose();
	soundClose();
	videoClose();
	SDL_Quit();
	_3do_Destroy();
	fsClose();

	return 0;
}



