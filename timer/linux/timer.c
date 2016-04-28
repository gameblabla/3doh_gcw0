/*
    Superjuego Engine
    Copyright (C) 2011 Jorge Luís Cabral y Gabriel Ernesto Cabral

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Contact: baigosoft@hotmail.com
*/

#include <stdio.h>
#include <SDL/SDL.h>
#include "timer.h"

Uint32 ini_mili,fin_mili;
int fps=17;


int timerGettime(){
	return SDL_GetTicks();
}


void timerReset()
{
	ini_mili=SDL_GetTicks();
}


int timerCurrent()
{
	fin_mili=SDL_GetTicks();
	return fin_mili - ini_mili;
}


void timerSetFramerate(int framespersecond)
{
	fps=1000/framespersecond;

}


void SE_timer_waitframerate(int millis)
{

	SDL_Delay(millis);

}

