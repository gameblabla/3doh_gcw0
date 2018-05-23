#include <SDL/SDL.h>
#include "config.h"

#define SOUND_BUFFER_SIZE (1024 * 4 * 4)

static Uint8 *sound_buffer;
static Uint32 buf_read_pos = 0;
static Uint32 buf_write_pos = 0;
static Uint32 buffered_bytes = 0;
static int sound_running = 0;

static SDL_AudioSpec wanted, empty;

static SDL_mutex *sound_mutex;
static SDL_cond *sound_cv;

void fill_audio(void *udata, Uint8 *stream, int len)
{
	(void)udata;

	Uint8 *data = (Uint8 *)stream;
	Uint8 *buffer = (Uint8 *)sound_buffer;

	if (!sound_running)
		return;

	SDL_LockMutex(sound_mutex);

	if (buffered_bytes >= (Uint32)len) {
		if (buf_read_pos + len <= SOUND_BUFFER_SIZE ) {
			memcpy(data, buffer + buf_read_pos, len);
		} else {
			int tail = SOUND_BUFFER_SIZE - buf_read_pos;
			memcpy(data, buffer + buf_read_pos, tail);
			memcpy(data + tail, buffer, len - tail);
		}
		buf_read_pos = (buf_read_pos + len) % SOUND_BUFFER_SIZE;
		buffered_bytes -= len;
	}

	SDL_CondSignal(sound_cv);
	SDL_UnlockMutex(sound_mutex);
}

int soundInit()
{
	if ((SDL_InitSubSystem(SDL_INIT_AUDIO)) < 0 ) {
		printf("ERROR: can't init sound\n");
		return 0;
	}

	printf("INFO: sound init success\n");

	/* Set the audio format */
	wanted.freq = 44100;
	wanted.format = AUDIO_S16SYS;
	wanted.channels = 2;    /* 1 = mono, 2 = stereo */
	wanted.samples = 1024;  /* Good low-latency value for callback */
	wanted.callback = fill_audio;
	wanted.userdata = NULL;

	/* Open the audio device, forcing the desired format */
	SDL_OpenAudio(&wanted, &empty);

	sound_buffer = (Uint8 *)calloc(SOUND_BUFFER_SIZE, 1);
	memset(sound_buffer, 0, SOUND_BUFFER_SIZE);

	sound_mutex = SDL_CreateMutex();
	sound_cv = SDL_CreateCond();

	sound_running = 0;
	SDL_PauseAudio(0);

	return 1;
}

void soundFillBuffer(unsigned int dspLoop)
{
	Uint8 *buffer = (Uint8 *)sound_buffer;

	sound_running = 1;

	SDL_LockMutex(sound_mutex);

	while (buffered_bytes == SOUND_BUFFER_SIZE)
		SDL_CondWait(sound_cv, sound_mutex);

	*(Uint32 *)(buffer + buf_write_pos) = dspLoop;

	buf_write_pos = (buf_write_pos + 4) % SOUND_BUFFER_SIZE;
	buffered_bytes += 4;

	SDL_CondSignal(sound_cv);
	SDL_UnlockMutex(sound_mutex);
}

void soundClose()
{
	sound_running = 0;

	SDL_LockMutex(sound_mutex);
	buffered_bytes = SOUND_BUFFER_SIZE;
	SDL_CondSignal(sound_cv);
	SDL_UnlockMutex(sound_mutex);
	SDL_Delay(100);

	SDL_DestroyCond(sound_cv);
	SDL_DestroyMutex(sound_mutex);

	SDL_CloseAudio();

	if (sound_buffer)
		free(sound_buffer);

	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}
