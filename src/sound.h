/* Platform-neutral audio interface.  One backend implementation is linked at a time. */
#ifndef THREEDOH_SOUND_H
#define THREEDOH_SOUND_H

#ifdef __cplusplus
extern "C" {
#endif

int  soundInit(void);
void soundBeginFrame(void);
void soundFillBuffer(unsigned int dspLoop);
void soundRun(void);
void soundClose(void);

#ifdef __cplusplus
}
#endif

#endif
