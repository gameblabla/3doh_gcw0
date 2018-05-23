#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "video.h"
#include "config.h"

//extern char configFile[128];

SDL_Event eventjoy;
SDL_Joystick *joystick[6];
inputState internal_input_state[6];
inputMapping joystickmap[6];

unsigned char *data;
int usekb = 0;
int isexit = 0;
int fullscreen = 0;


int inputMapButton(char *button)
{
	printf("%s\n", button);
	if (strcmp(button, "JOY_BUTTON0") == 0) return 0;
	if (strcmp(button, "JOY_BUTTON1") == 0) return 1;
	if (strcmp(button, "JOY_BUTTON2") == 0) return 2;
	if (strcmp(button, "JOY_BUTTON3") == 0) return 3;
	if (strcmp(button, "JOY_BUTTON4") == 0) return 4;
	if (strcmp(button, "JOY_BUTTON5") == 0) return 5;
	if (strcmp(button, "JOY_BUTTON6") == 0) return 6;
	if (strcmp(button, "JOY_BUTTON7") == 0) return 7;
	if (strcmp(button, "JOY_BUTTON8") == 0) return 8;
	if (strcmp(button, "JOY_BUTTON9") == 0) return 9;
	if (strcmp(button, "JOY_BUTTON10") == 0) return 10;

	return 0;
}


void inputReadConfig()
{
	/*configOpen(configFile);
	   int i;
	   char js[10];
	   for(i=0;i<6;i++)
	   {
	        sprintf(js,"joystick%d",i);
	        joystickmap[i].buttonup    = inputMapButton(configReadString(js,"buttonup"));
	        joystickmap[i].buttondown  = inputMapButton(configReadString(js,"buttondown"));
	        joystickmap[i].buttonleft  = inputMapButton(configReadString(js,"buttonleft"));
	        joystickmap[i].buttonright = inputMapButton(configReadString(js,"buttonright"));
	        joystickmap[i].buttona     = inputMapButton(configReadString(js,"buttona"));
	        joystickmap[i].buttonb     = inputMapButton(configReadString(js,"buttonb"));
	        joystickmap[i].buttonc     = inputMapButton(configReadString(js,"buttonc"));
	        joystickmap[i].buttonl     = inputMapButton(configReadString(js,"buttonl"));
	        joystickmap[i].buttonr     = inputMapButton(configReadString(js,"buttonr"));
	        joystickmap[i].buttonx     = inputMapButton(configReadString(js,"buttonx"));
	        joystickmap[i].buttonp     = inputMapButton(configReadString(js,"buttonp"));
	   }

	   configClose();*/
}

int inputQuit()
{
	return isexit;
}

int inputFullscreen()
{
	return fullscreen;
}

int inputInit()
{
	int i = 0;

	printf("INFO: reading input config\n");
	/*inputReadConfig();*/

	if ((SDL_InitSubSystem( SDL_INIT_JOYSTICK )) < 0 ) {
		printf("ERROR: can't init joystick subsystem\n");
		return 0;
	}

	printf("INFO: input found %d joysticks\n", inputEnum());
	if (inputEnum() == 0) {
		usekb = 1;
	}

	for (i = 0; i < inputEnum(); i++) {
		joystick[i] = inputOpen(i);
	}

	data = (unsigned char*)malloc(sizeof(unsigned char) * 16);

	return SDL_InitSubSystem(SDL_INIT_JOYSTICK);

}

int inputClose()
{
	/* Close and clean everything related to input */
	free(data);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
	return 0;
}

SDL_Joystick *inputOpen(int joyid)
{

	SDL_Joystick *joystick;

	SDL_JoystickEventState(SDL_ENABLE);

	joystick = SDL_JoystickOpen(joyid);

	return joystick;
}

int inputEnum()
{
	if (SDL_NumJoysticks() == 0) return 1;
	else return SDL_NumJoysticks();
}


void inputPoll_internal()
{
	while (SDL_PollEvent(&eventjoy)) {
		switch (eventjoy.type) {

		/*case SDL_JOYAXISMOTION:
		        if( eventjoy.jaxis.axis == 0){
		                if(eventjoy.jaxis.value < -3200){
		                        internal_input_state[eventjoy.jaxis.which].buttons&=~INPUTBUTTONRIGHT;
		                        internal_input_state[eventjoy.jaxis.which].buttons|=INPUTBUTTONLEFT;
		                } else if(eventjoy.jaxis.value > 3200){
		                        internal_input_state[eventjoy.jaxis.which].buttons&=~INPUTBUTTONLEFT;
		                        internal_input_state[eventjoy.jaxis.which].buttons|=INPUTBUTTONRIGHT;
		                }else{
		                        internal_input_state[eventjoy.jaxis.which].buttons&=~INPUTBUTTONLEFT;
		                        internal_input_state[eventjoy.jaxis.which].buttons&=~INPUTBUTTONRIGHT;
		                }

		        }
		        if( eventjoy.jaxis.axis == 1){
		                if(eventjoy.jaxis.value < -3200){
		                        internal_input_state[eventjoy.jaxis.which].buttons&=~INPUTBUTTONDOWN;
		                        internal_input_state[eventjoy.jaxis.which].buttons|=INPUTBUTTONUP;
		                } else if(eventjoy.jaxis.value > 3200){
		                        internal_input_state[eventjoy.jaxis.which].buttons&=~INPUTBUTTONUP;
		                        internal_input_state[eventjoy.jaxis.which].buttons|=INPUTBUTTONDOWN;
		                }else{
		                        internal_input_state[eventjoy.jaxis.which].buttons&=~INPUTBUTTONUP;
		                        internal_input_state[eventjoy.jaxis.which].buttons&=~INPUTBUTTONDOWN;
		                }
		        }
		   break;

		   case SDL_JOYBUTTONDOWN:

		        if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.button].buttona)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons|=INPUTBUTTONA;
		        }
		        else if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttonb)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons|=INPUTBUTTONB;
		        }
		        else if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttonc)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons|=INPUTBUTTONC;
		        }
		        else if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttonl)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons|=INPUTBUTTONL;
		        }
		        else if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttonr)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons|=INPUTBUTTONR;
		        }
		        else if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttonx)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons|=INPUTBUTTONX;
		        }
		        else if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttonp)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons|=INPUTBUTTONP;
		        }

		   break;

		   case SDL_JOYBUTTONUP:

		        if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttona)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons&=~INPUTBUTTONA;
		        }
		        else if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttonb)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons&=~INPUTBUTTONB;
		        }
		        else if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttonc)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons&=~INPUTBUTTONC;
		        }
		        else if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttonl)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons&=~INPUTBUTTONL;
		        }
		        else if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttonr)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons&=~INPUTBUTTONR;
		        }
		        else if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttonx)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons&=~INPUTBUTTONX;
		        }
		        else if(eventjoy.jbutton.button==joystickmap[eventjoy.jbutton.which].buttonp)
		        {
		                internal_input_state[eventjoy.jbutton.which].buttons&=~INPUTBUTTONP;
		        }

		   break;*/
		case SDL_QUIT:
			isexit = 1;
			break;

		case SDL_KEYDOWN:
			switch ( eventjoy.key.keysym.sym ) {
			case SDLK_LEFT:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONRIGHT;
				internal_input_state[eventjoy.jbutton.which].buttons |= INPUTBUTTONLEFT;
				break;
			case SDLK_RIGHT:
				internal_input_state[eventjoy.jbutton.which].buttons |= INPUTBUTTONRIGHT;
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONLEFT;
				break;
			case SDLK_UP:
				internal_input_state[eventjoy.jbutton.which].buttons |= INPUTBUTTONUP;
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONDOWN;
				break;
			case SDLK_DOWN:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONUP;
				internal_input_state[eventjoy.jbutton.which].buttons |= INPUTBUTTONDOWN;
				break;
			case SDLK_LSHIFT:
				internal_input_state[eventjoy.jbutton.which].buttons |= INPUTBUTTONC;
				break;
			case SDLK_LALT:
				internal_input_state[eventjoy.jbutton.which].buttons |= INPUTBUTTONB;
				break;
			case SDLK_LCTRL:
				internal_input_state[eventjoy.jbutton.which].buttons |= INPUTBUTTONA;
				break;
			case SDLK_BACKSPACE:
				internal_input_state[eventjoy.jbutton.which].buttons |= INPUTBUTTONR;
				break;
			case SDLK_SPACE:
				internal_input_state[eventjoy.jbutton.which].buttons |= INPUTBUTTONX;
				break;
			case SDLK_RETURN:
				internal_input_state[eventjoy.jbutton.which].buttons |= INPUTBUTTONP;
				break;
			case SDLK_TAB:
				internal_input_state[eventjoy.jbutton.which].buttons |= INPUTBUTTONL;
				break;
			default:
				break;
			}
			break;

		case SDL_KEYUP:
			switch ( eventjoy.key.keysym.sym ) {
			case SDLK_LEFT:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONRIGHT;
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONLEFT;
				break;
			case SDLK_RIGHT:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONRIGHT;
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONLEFT;
				break;
			case SDLK_UP:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONUP;
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONDOWN;
				break;
			case SDLK_DOWN:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONUP;
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONDOWN;
				break;
			case SDLK_LSHIFT:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONC;
				break;
			case SDLK_LALT:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONB;
				break;
			case SDLK_LCTRL:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONA;
				break;
			case SDLK_BACKSPACE:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONR;
				break;
			case SDLK_SPACE:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONX;
				break;
			case SDLK_RETURN:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONP;
				break;
			case SDLK_TAB:
				internal_input_state[eventjoy.jbutton.which].buttons &= ~INPUTBUTTONL;
				break;
			case SDLK_ESCAPE:
				isexit = 1;
				break;
			default:
				break;
			}
			break;

		}
	}
}



int inputLength()
{
	return 16;
}

int CheckDownButton(int deviceNumber, int button)
{
	if (internal_input_state[deviceNumber].buttons & button) return 1;
	else return 0;
}

char CalculateDeviceLowByte(int deviceNumber)
{
	char returnValue = 0;

	returnValue |= 0x01 & 0;        // unknown
	returnValue |= 0x02 & 0;        // unknown
	returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONL) ? (char)0x04 : (char)0;
	returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONR) ? (char)0x08 : (char)0;
	returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONX) ? (char)0x10 : (char)0;
	returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONP) ? (char)0x20 : (char)0;
	returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONC) ? (char)0x40 : (char)0;
	returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONB) ? (char)0x80 : (char)0;

	return returnValue;
}

char CalculateDeviceHighByte(int deviceNumber)
{
	char returnValue = 0;

	returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONA)     ? (char)0x01 : (char)0;
	returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONLEFT)  ? (char)0x02 : (char)0;
	returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONRIGHT) ? (char)0x04 : (char)0;
	returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONUP)    ? (char)0x08 : (char)0;
	returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONDOWN)  ? (char)0x10 : (char)0;
	returnValue |= 0x20 & 0;        // unknown
	returnValue |= 0x40 & 0;        // unknown
	returnValue |= 0x80;            // This last bit seems to indicate power and/or connectivity.

	return returnValue;
}



unsigned char *inputRead()
{
	inputPoll_internal();

	data[0x0] = 0x00;
	data[0x1] = 0x48;
	data[0x2] = CalculateDeviceLowByte(0);
	data[0x3] = CalculateDeviceHighByte(0);
	data[0x4] = CalculateDeviceLowByte(2);
	data[0x5] = CalculateDeviceHighByte(2);
	data[0x6] = CalculateDeviceLowByte(1);
	data[0x7] = CalculateDeviceHighByte(1);
	data[0x8] = CalculateDeviceLowByte(4);
	data[0x9] = CalculateDeviceHighByte(4);
	data[0xA] = CalculateDeviceLowByte(3);
	data[0xB] = CalculateDeviceHighByte(3);
	data[0xC] = 0x00;
	data[0xD] = 0x80;
	data[0xE] = CalculateDeviceLowByte(5);
	data[0xF] = CalculateDeviceHighByte(5);


	return data;
}
