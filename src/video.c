#include "freedocore.h"
#include "video.h"
#include "frame.h"
#include "timer.h"
#include "tinyfps.h"

SDL_Surface *screen;
#ifdef SCALING
SDL_Surface *rl_screen;
#endif
struct VDLFrame *frame;

#ifdef SDL_TRIPLEBUF
#define flags SDL_HWSURFACE | SDL_TRIPLEBUF
#else
#define flags SDL_HWSURFACE
#endif

int videoInit(void)
{
	frame = (struct VDLFrame*)malloc(sizeof(struct VDLFrame));

	if ((SDL_Init( SDL_INIT_VIDEO )) < 0 ) {
		printf("ERROR: can't init video\n");
		return 0;
	}

	SDL_ShowCursor(0);
	#ifdef SCALING
	rl_screen = SDL_SetVideoMode(0, 0, 16, flags);
	screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 320, 240, 16, 0,0,0,0);
	#else
	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 16, flags);
	#endif
	return 0;
}

void toggleFullscreen()
{
	SDL_WM_ToggleFullScreen(screen);
}

int videoClose()
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

void videoFlip()
{
	SDL_LockSurface( screen );
	Get_Frame_Bitmap((struct VDLFrame*)frame, screen->pixels, SCREEN_WIDTH, SCREEN_HEIGHT);
	#if defined(FRAMECONTER) && !defined(SCALING)
	drawDecimal(getFps(), 0, SCREEN_HEIGHT - FPS_FONT_HEIGHT, (unsigned short*)screen->pixels);
	#endif
	SDL_UnlockSurface( screen );
	
	#ifdef SCALING
	SDL_SoftStretch(screen, NULL, rl_screen, NULL);
	#if defined(FRAMECONTER) && defined(SCALING)
	drawDecimal(getFps(), 0, rl_screen->h - FPS_FONT_HEIGHT, (unsigned short*)rl_screen->pixels);
	#endif
	SDL_Flip(rl_screen);
	#else
	SDL_Flip(screen);
	#endif
}

