#include <stdint.h>
#include <string.h>
#include "platform/common/input_packet.h"
#include "input.h"

static inputState g_input_state[6];
static unsigned char g_input_packet[16];
int isexit = 0;

void threedoh_input_button_event(int button, int pressed)
{
    if (button == THREEDOH_BUTTON_EXIT && pressed) {
        isexit = 1;
        return;
    }
    threedoh_input_set_button(g_input_state, 0, button, pressed);
}

int inputInit(void)
{
    memset(g_input_state, 0, sizeof(g_input_state));
    memset(g_input_packet, 0, sizeof(g_input_packet));
    isexit = 0;
    return 1;
}

int inputClose(void) { return 0; }
int inputEnum(void) { return 0; }
void *inputOpen(int joyid) { (void)joyid; return 0; }
void inputPoll(void *joy) { (void)joy; }
int inputFullscreen(void) { return 0; }
int inputLength(void) { return 16; }

unsigned char *inputRead(void)
{
    threedoh_input_build_packet(g_input_state, g_input_packet);
    return g_input_packet;
}
