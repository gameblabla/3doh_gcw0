#ifndef THREEDOH_INPUT_PACKET_H
#define THREEDOH_INPUT_PACKET_H

#include <string.h>
#include "input.h"

static inline int threedoh_input_mask_for_button(int button)
{
    switch (button) {
    case THREEDOH_BUTTON_UP: return INPUTBUTTONUP;
    case THREEDOH_BUTTON_DOWN: return INPUTBUTTONDOWN;
    case THREEDOH_BUTTON_LEFT: return INPUTBUTTONLEFT;
    case THREEDOH_BUTTON_RIGHT: return INPUTBUTTONRIGHT;
    case THREEDOH_BUTTON_A: return INPUTBUTTONA;
    case THREEDOH_BUTTON_B: return INPUTBUTTONB;
    case THREEDOH_BUTTON_C: return INPUTBUTTONC;
    case THREEDOH_BUTTON_X: return INPUTBUTTONX;
    case THREEDOH_BUTTON_L: return INPUTBUTTONL;
    case THREEDOH_BUTTON_R: return INPUTBUTTONR;
    case THREEDOH_BUTTON_P: return INPUTBUTTONP;
    default: return 0;
    }
}

static inline void threedoh_input_set_button(inputState state[6], int device, int button, int pressed)
{
    int mask;
    if (!state || device < 0 || device >= 6)
        return;
    if (button == THREEDOH_BUTTON_EXIT)
        return;
    mask = threedoh_input_mask_for_button(button);
    if (!mask)
        return;
    if (pressed) {
        if (button == THREEDOH_BUTTON_LEFT)
            state[device].buttons &= ~INPUTBUTTONRIGHT;
        if (button == THREEDOH_BUTTON_RIGHT)
            state[device].buttons &= ~INPUTBUTTONLEFT;
        if (button == THREEDOH_BUTTON_UP)
            state[device].buttons &= ~INPUTBUTTONDOWN;
        if (button == THREEDOH_BUTTON_DOWN)
            state[device].buttons &= ~INPUTBUTTONUP;
        state[device].buttons |= mask;
    } else {
        state[device].buttons &= ~mask;
    }
}

static inline int threedoh_input_down(const inputState state[6], int device, int button)
{
    if (!state || device < 0 || device >= 6)
        return 0;
    return (state[device].buttons & button) ? 1 : 0;
}

static inline unsigned char threedoh_input_low_byte(const inputState state[6], int device)
{
    unsigned char v = 0;
    v |= threedoh_input_down(state, device, INPUTBUTTONL) ? (char)0x04 : (char)0;
    v |= threedoh_input_down(state, device, INPUTBUTTONR) ? (char)0x08 : (char)0;
    v |= threedoh_input_down(state, device, INPUTBUTTONX) ? (char)0x10 : (char)0;
    v |= threedoh_input_down(state, device, INPUTBUTTONP) ? (char)0x20 : (char)0;
    v |= threedoh_input_down(state, device, INPUTBUTTONC) ? (char)0x40 : (char)0;
    v |= threedoh_input_down(state, device, INPUTBUTTONB) ? (char)0x80 : (char)0;
    return v;
}

static inline unsigned char threedoh_input_high_byte(const inputState state[6], int device)
{
    unsigned char v = 0;
    v |= threedoh_input_down(state, device, INPUTBUTTONA)     ? (char)0x01 : (char)0;
    v |= threedoh_input_down(state, device, INPUTBUTTONLEFT)  ? (char)0x02 : (char)0;
    v |= threedoh_input_down(state, device, INPUTBUTTONRIGHT) ? (char)0x04 : (char)0;
    v |= threedoh_input_down(state, device, INPUTBUTTONUP)    ? (char)0x08 : (char)0;
    v |= threedoh_input_down(state, device, INPUTBUTTONDOWN)  ? (char)0x10 : (char)0;
    v |= 0x80;
    return v;
}

static inline void threedoh_input_build_packet(const inputState state[6], unsigned char data[16])
{
    memset(data, 0, 16);
    data[0x0] = 0x00;
    data[0x1] = 0x48;
    data[0x2] = threedoh_input_low_byte(state, 0);
    data[0x3] = threedoh_input_high_byte(state, 0);
    data[0x4] = threedoh_input_low_byte(state, 2);
    data[0x5] = threedoh_input_high_byte(state, 2);
    data[0x6] = threedoh_input_low_byte(state, 1);
    data[0x7] = threedoh_input_high_byte(state, 1);
    data[0x8] = threedoh_input_low_byte(state, 4);
    data[0x9] = threedoh_input_high_byte(state, 4);
    data[0xA] = threedoh_input_low_byte(state, 3);
    data[0xB] = threedoh_input_high_byte(state, 3);
    data[0xC] = 0x00;
    data[0xD] = 0x80;
    data[0xE] = threedoh_input_low_byte(state, 5);
    data[0xF] = threedoh_input_high_byte(state, 5);
}

#endif
