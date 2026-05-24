#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "freedocore.h"
#include "frame.h"
#include "fs.h"
#include "input.h"
#include "sound.h"
#include "vdlp.h"
#include "_3do_sys.h"

#define THREEDOH_SCREEN_WIDTH 320
#define THREEDOH_SCREEN_HEIGHT 240

typedef struct {
    char recordType;
    char syncBytes[5];
    char recordVersion;
    char flags;
    char comment[32];
    char label[32];
    uint32_t id;
    uint32_t blockSize;
    uint32_t blockCount;
    uint32_t rootDirId;
    uint32_t rootDirBlocks;
    uint32_t rootDirBlockSize;
    uint32_t lastRootDirCopy;
    uint32_t rootDirCopies[8];
} NvRamStr;

char biosFile[128] = "browser:bios.bin";
static char imageFile[128] = "browser:game.iso";
static struct VDLFrame wasm_frame;
static uint8_t wasm_framebuffer[THREEDOH_SCREEN_WIDTH * THREEDOH_SCREEN_HEIGHT * 4];
static int wasm_started = 0;
static int wasm_last_error = 0;

extern int isexit;
void soundBeginFrame(void);

static uint32_t ReverseBytes(uint32_t value)
{
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 |
           (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24;
}

void readNvRam(void *pnvram)
{
    uint_fast8_t x;
    NvRamStr *nvramStruct = (NvRamStr *)pnvram;

    nvramStruct->recordType = 0x01;
    for (x = 0; x < 5; x++)
        nvramStruct->syncBytes[x] = (char)'Z';
    nvramStruct->recordVersion = 0x02;
    nvramStruct->flags = 0x00;
    for (x = 0; x < 32; x++)
        nvramStruct->comment[x] = 0;

    nvramStruct->label[0] = (char)'n';
    nvramStruct->label[1] = (char)'v';
    nvramStruct->label[2] = (char)'r';
    nvramStruct->label[3] = (char)'a';
    nvramStruct->label[4] = (char)'m';
    for (x = 5; x < 32; x++)
        nvramStruct->label[x] = 0;

    nvramStruct->id = ReverseBytes(0xFFFFFFFF);
    nvramStruct->blockSize = ReverseBytes(0x00000001);
    nvramStruct->blockCount = ReverseBytes(0x00008000);
    nvramStruct->rootDirId = ReverseBytes(0xFFFFFFFE);
    nvramStruct->rootDirBlocks = ReverseBytes(0x00000000);
    nvramStruct->rootDirBlockSize = ReverseBytes(0x00000001);
    nvramStruct->lastRootDirCopy = ReverseBytes(0x00000000);
    nvramStruct->rootDirCopies[0] = ReverseBytes(0x00000084);
    for (x = 1; x < 8; x++)
        nvramStruct->rootDirCopies[x] = 0;
}

void writeNvRam(void)
{
}

int threedoh_width(void) { return THREEDOH_SCREEN_WIDTH; }
int threedoh_height(void) { return THREEDOH_SCREEN_HEIGHT; }
uint8_t *threedoh_framebuffer_ptr(void) { return wasm_framebuffer; }
int threedoh_last_error(void) { return wasm_last_error; }
int threedoh_is_started(void) { return wasm_started; }

int threedoh_start(void)
{
    if (wasm_started)
        return 0;

    memset(&wasm_frame, 0, sizeof(wasm_frame));
    memset(wasm_framebuffer, 0, sizeof(wasm_framebuffer));

    if (!fsInit()) {
        wasm_last_error = 1;
        return wasm_last_error;
    }
    if (!fsOpenIso(imageFile)) {
        wasm_last_error = 2;
        return wasm_last_error;
    }
    if (!soundInit()) {
        wasm_last_error = 3;
        return wasm_last_error;
    }
    if (!inputInit()) {
        wasm_last_error = 4;
        return wasm_last_error;
    }
    if (!_3do_Init()) {
        wasm_last_error = 5;
        return wasm_last_error;
    }

    wasm_started = 1;
    wasm_last_error = 0;
    return 0;
}

int threedoh_frame(void)
{
    if (!wasm_started || isexit)
        return 0;

    soundBeginFrame();
    _3do_Frame(&wasm_frame, true);
    for (uint_fast16_t line = 0; line < 256; line++)
        _vdl_DoLineNew(line, &wasm_frame);
    Get_Frame_Bitmap(&wasm_frame, wasm_framebuffer, THREEDOH_SCREEN_WIDTH, THREEDOH_SCREEN_HEIGHT);
    return 1;
}

int threedoh_soft_reset(void)
{
    if (!wasm_started) {
        wasm_last_error = 6;
        return wasm_last_error;
    }

    _3do_Destroy();
    isexit = 0;
    memset(&wasm_frame, 0, sizeof(wasm_frame));
    memset(wasm_framebuffer, 0, sizeof(wasm_framebuffer));
    soundBeginFrame();

    if (!_3do_Init()) {
        wasm_started = 0;
        wasm_last_error = 5;
        return wasm_last_error;
    }

    wasm_last_error = 0;
    return 0;
}

void threedoh_shutdown(void)
{
    if (!wasm_started)
        return;
    _3do_Destroy();
    inputClose();
    soundClose();
    fsCloseIso();
    fsClose();
    wasm_started = 0;
    wasm_last_error = 0;
    isexit = 0;
}
