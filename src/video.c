#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freedocore.h"
#include "video.h"
#include "frame.h"
#include "timer.h"
#include "tinyfps.h"
#include "freedo/vdlp.h"
#include "freedo/_3do_sys.h"

SDL_Surface *screen;
#ifdef SCALING
SDL_Surface *rl_screen;
#endif
struct VDLFrame *frame;

#if defined(__EMSCRIPTEN__)
#define flags SDL_SWSURFACE
#elif defined(SDL_TRIPLEBUF)
#define flags SDL_HWSURFACE | SDL_TRIPLEBUF
#else
#define flags SDL_HWSURFACE
#endif

#if BPP_TYPE == 32
#define BPP 32
#else
#define BPP 16
#endif

int videoInit(void)
{
	frame = (struct VDLFrame*)malloc(sizeof(struct VDLFrame));

	#ifndef __EMSCRIPTEN__
	if ((SDL_Init( SDL_INIT_VIDEO )) < 0 ) {
		printf("ERROR: can't init video\n");
		return 0;
	}
	SDL_ShowCursor(0);
	#else
	SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	#endif
	
	printf("Bitdepth is %d\n", BPP);
	
	#ifdef SCALING
	rl_screen = SDL_SetVideoMode(0, 0, BPP, flags);
	screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 320, 240, BPP, 0,0,0,0);
	#else
	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, BPP, flags);
	#endif
	return 0;
}

void toggleFullscreen(void)
{
	#ifndef __EMSCRIPTEN__
	SDL_WM_ToggleFullScreen(screen);
	#endif
}

int videoClose(void)
{
	if (frame)
	{
		free(frame);
		frame = NULL;
	}
	
	if (screen)
	{
		SDL_FreeSurface(screen);
		screen = NULL;
	}
	#ifdef SCALING
	if (rl_screen)
	{
		SDL_FreeSurface(rl_screen);
		rl_screen = NULL;
	}
	#endif
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return 0;
}

void videoFlip(void)
{
	uint_fast16_t line;
	_3do_Frame((struct VDLFrame*)frame, true);
	
	#if defined(__EMSCRIPTEN__)
	for(line=0;line<256;line++)
	{
		_vdl_DoLineNew(line, (struct VDLFrame*)frame);
	}
	#else
	line = 0;
	while (line < 256) _vdl_DoLineNew(line++, (struct VDLFrame*)frame);
	#endif
	
	SDL_LockSurface( screen );
	Get_Frame_Bitmap((struct VDLFrame*)frame, screen->pixels, SCREEN_WIDTH, SCREEN_HEIGHT);
	#if defined(FRAMECONTER) && !defined(SCALING)
	drawDecimal(getFps(), 0, SCREEN_HEIGHT - FPS_FONT_HEIGHT, (void*)screen->pixels);
	#endif
	SDL_UnlockSurface( screen );
	
	#ifdef SCALING
	SDL_SoftStretch(screen, NULL, rl_screen, NULL);
	#if defined(FRAMECONTER) && defined(SCALING)
	drawDecimal(getFps(), 0, rl_screen->h - FPS_FONT_HEIGHT, (void*)rl_screen->pixels);
	#endif
	SDL_Flip(rl_screen);
	#else
	SDL_Flip(screen);
	#endif
}

