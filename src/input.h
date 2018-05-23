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


#include <SDL/SDL.h>

typedef struct {
	int buttons;                    /* buttons bitfield */

}inputState;

typedef struct {
	int buttonup;
	int buttondown;
	int buttonleft;
	int buttonright;
	int buttona;
	int buttonb;
	int buttonc;
	int buttonl;
	int buttonr;
	int buttonx;
	int buttonp;

}inputMapping;

#define INPUTBUTTONL     (1 << 4)
#define INPUTBUTTONR     (1 << 5)
#define INPUTBUTTONX     (1 << 6)
#define INPUTBUTTONP     (1 << 7)
#define INPUTBUTTONC     (1 << 8)
#define INPUTBUTTONB     (1 << 9)
#define INPUTBUTTONA     (1 << 10)
#define INPUTBUTTONLEFT  (1 << 11)
#define INPUTBUTTONRIGHT (1 << 12)
#define INPUTBUTTONUP    (1 << 13)
#define INPUTBUTTONDOWN  (1 << 14)


unsigned char *inputRead();
int inputLength();
int inputEnum();
int inputInit();
int inputClose();
SDL_Joystick *inputOpen(int joyid);
void inputPoll(SDL_Joystick *joy);
int inputQuit();
int inputFullscreen();
