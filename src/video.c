#include "freedocore.h"
#include "video.h"
#include "frame.h"
#include "timer.h"
#include "tinyfps.h"

SDL_Surface *screen;
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
	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 16, flags);

	return 0;
}

void toggleFullscreen()
{
	SDL_WM_ToggleFullScreen(screen);
}

int videoClose()
{
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return 0;
}

void videoFlip()
{
	SDL_LockSurface( screen );
	Get_Frame_Bitmap((struct VDLFrame*)frame, screen->pixels, SCREEN_WIDTH, SCREEN_HEIGHT);
	#ifdef FRAMECONTER
	drawDecimal(getFps(), 0, SCREEN_HEIGHT - FPS_FONT_HEIGHT, (unsigned short*)screen->pixels);
	#endif
	SDL_UnlockSurface( screen );
	SDL_Flip(screen);
}

