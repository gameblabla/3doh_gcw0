#include "freedocore.h"
#include "video.h"
#include "frame.h"

int screen_width=0;
int screen_height=0;
int screen_bpp=0;
SDL_Surface *screen;
struct VDLFrame *frame;
extern _ext_Interface  fd_interface;

#ifdef __GCW__
	#define flags SDL_HWSURFACE | SDL_DOUBLEBUF
#else
	#define flags SDL_SWSURFACE
#endif


int videoInit(void)
{
	frame = (struct VDLFrame*)malloc(sizeof(struct VDLFrame));

	if((SDL_Init( SDL_INIT_VIDEO )) < 0 )
	{
		printf("ERROR: can't init video\n");
		return 0;
	}
	
	SDL_ShowCursor(0);
	screen = SDL_SetVideoMode(320,240,32, flags);

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
	fd_interface(FDP_DO_EXECFRAME,(struct VDLFrame*)frame);
	
	SDL_LockSurface( screen );
    Get_Frame_Bitmap((struct VDLFrame*)frame, screen->pixels, 320, 240);
	SDL_UnlockSurface( screen );
	SDL_Flip(screen);
}

