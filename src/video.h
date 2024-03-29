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


#ifndef DREAMCAST
//#include <GL/gl.h>
//#include <GL/glu.h>
	#include <SDL/SDL.h>
#else
	#include <kos.h>
#endif

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240


int videoInit(void);
int videoClose(void);
void toggleFullscreen(void);
void videoFlip(void);
void VideoError(void);
