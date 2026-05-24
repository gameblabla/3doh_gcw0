#include <stdint.h>
#include <string.h>
#include "input.h"

static inputState internal_input_state[6];
static unsigned char data[16];
int isexit = 0;

enum {
    W3DO_BUTTON_UP = 0,
    W3DO_BUTTON_DOWN,
    W3DO_BUTTON_LEFT,
    W3DO_BUTTON_RIGHT,
    W3DO_BUTTON_A,
    W3DO_BUTTON_B,
    W3DO_BUTTON_C,
    W3DO_BUTTON_X,
    W3DO_BUTTON_L,
    W3DO_BUTTON_R,
    W3DO_BUTTON_P,
    W3DO_BUTTON_EXIT,
    W3DO_BUTTON_COUNT
};

static int mask_for_button(int button)
{
    switch (button) {
    case W3DO_BUTTON_UP: return INPUTBUTTONUP;
    case W3DO_BUTTON_DOWN: return INPUTBUTTONDOWN;
    case W3DO_BUTTON_LEFT: return INPUTBUTTONLEFT;
    case W3DO_BUTTON_RIGHT: return INPUTBUTTONRIGHT;
    case W3DO_BUTTON_A: return INPUTBUTTONA;
    case W3DO_BUTTON_B: return INPUTBUTTONB;
    case W3DO_BUTTON_C: return INPUTBUTTONC;
    case W3DO_BUTTON_X: return INPUTBUTTONX;
    case W3DO_BUTTON_L: return INPUTBUTTONL;
    case W3DO_BUTTON_R: return INPUTBUTTONR;
    case W3DO_BUTTON_P: return INPUTBUTTONP;
    default: return 0;
    }
}

void threedoh_input_button_event(int button, int pressed)
{
    if (button == W3DO_BUTTON_EXIT && pressed) {
        isexit = 1;
        return;
    }

    int mask = mask_for_button(button);
    if (!mask)
        return;

    if (pressed) {
        if (button == W3DO_BUTTON_LEFT)
            internal_input_state[0].buttons &= ~INPUTBUTTONRIGHT;
        if (button == W3DO_BUTTON_RIGHT)
            internal_input_state[0].buttons &= ~INPUTBUTTONLEFT;
        if (button == W3DO_BUTTON_UP)
            internal_input_state[0].buttons &= ~INPUTBUTTONDOWN;
        if (button == W3DO_BUTTON_DOWN)
            internal_input_state[0].buttons &= ~INPUTBUTTONUP;
        internal_input_state[0].buttons |= mask;
    } else {
        internal_input_state[0].buttons &= ~mask;
    }
}

int inputInit(void)
{
    memset(internal_input_state, 0, sizeof(internal_input_state));
    memset(data, 0, sizeof(data));
    isexit = 0;
    return 1;
}

int inputClose(void) { return 0; }
int inputEnum(void) { return 0; }
SDL_Joystick *inputOpen(int joyid) { (void)joyid; return 0; }
int inputFullscreen(void) { return 0; }

int inputLength(void)
{
    return 16;
}

static int CheckDownButton(int deviceNumber, int button)
{
    return (internal_input_state[deviceNumber].buttons & button) ? 1 : 0;
}

static unsigned char CalculateDeviceLowByte(int deviceNumber)
{
    unsigned char returnValue = 0;
    returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONL) ? (char)0x04 : (char)0;
    returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONR) ? (char)0x08 : (char)0;
    returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONX) ? (char)0x10 : (char)0;
    returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONP) ? (char)0x20 : (char)0;
    returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONC) ? (char)0x40 : (char)0;
    returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONB) ? (char)0x80 : (char)0;
    return returnValue;
}

static unsigned char CalculateDeviceHighByte(int deviceNumber)
{
    unsigned char returnValue = 0;
    returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONA)     ? (char)0x01 : (char)0;
    returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONLEFT)  ? (char)0x02 : (char)0;
    returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONRIGHT) ? (char)0x04 : (char)0;
    returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONUP)    ? (char)0x08 : (char)0;
    returnValue |= CheckDownButton(deviceNumber, INPUTBUTTONDOWN)  ? (char)0x10 : (char)0;
    returnValue |= 0x80;
    return returnValue;
}

unsigned char *inputRead(void)
{
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
