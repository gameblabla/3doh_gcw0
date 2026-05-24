#include <stdint.h>
#include <string.h>
#include "sound.h"

#define AUDIO_FRAME_CAPACITY 8192
static uint32_t audio_frame[AUDIO_FRAME_CAPACITY];
static int audio_frame_count = 0;
static int audio_overflow = 0;

int soundInit(void)
{
    audio_frame_count = 0;
    audio_overflow = 0;
    return 1;
}

void soundBeginFrame(void)
{
    audio_frame_count = 0;
    audio_overflow = 0;
}

void soundFillBuffer(unsigned int dspLoop)
{
    if (audio_frame_count < AUDIO_FRAME_CAPACITY)
        audio_frame[audio_frame_count++] = (uint32_t)dspLoop;
    else
        audio_overflow = 1;
}

void soundRun(void) {}
void soundClose(void) {}

int threedoh_audio_sample_count(void) { return audio_frame_count; }
int threedoh_audio_overflowed(void) { return audio_overflow; }
uint32_t *threedoh_audio_sample_ptr(void) { return audio_frame; }
int threedoh_audio_sample_rate(void) { return 44100; }
