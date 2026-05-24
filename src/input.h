/*
    Platform-neutral 3DO controller interface.
    Each host backend owns the real keyboard/gamepad event source and exposes
    the compact 3DO daisy-chain packet through inputRead().
 */
#ifndef THREEDOH_INPUT_H
#define THREEDOH_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int buttons;
} inputState;

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
} inputMapping;

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

enum threedoh_button {
    THREEDOH_BUTTON_UP = 0,
    THREEDOH_BUTTON_DOWN,
    THREEDOH_BUTTON_LEFT,
    THREEDOH_BUTTON_RIGHT,
    THREEDOH_BUTTON_A,
    THREEDOH_BUTTON_B,
    THREEDOH_BUTTON_C,
    THREEDOH_BUTTON_X,
    THREEDOH_BUTTON_L,
    THREEDOH_BUTTON_R,
    THREEDOH_BUTTON_P,
    THREEDOH_BUTTON_EXIT,
    THREEDOH_BUTTON_COUNT
};

unsigned char *inputRead(void);
int inputLength(void);
int inputEnum(void);
int inputInit(void);
int inputClose(void);
void *inputOpen(int joyid);
void inputPoll(void *joy);
int inputFullscreen(void);
void threedoh_input_button_event(int button, int pressed);
extern int isexit;

#ifdef __cplusplus
}
#endif

#endif
