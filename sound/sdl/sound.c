#include <SDL/SDL.h>
#include "config.h"

static Uint8 *audio_buffer;
static Uint32 audio_len=0;
static Uint8 *audio_pos;
SDL_AudioSpec wanted, empty;
int audio_pointer=0;
int audio_read=0;

int buffer_size=4096*32;
//int buffer_size=512;

void fill_audio(void *udata, Uint8 *stream, int len)
{
	if (( audio_len == 0 )){
		return;
	}

	/* Mix as much data as possible */
	len = ( len > audio_len ? audio_len : len );

    /*protect against underruns*/
	if((audio_read+len>=audio_pointer)&&(audio_read<audio_pointer))
	{
		SDL_PauseAudio(1);
		audio_pointer=0;
		audio_read=0;
		return;
	}
	
	memcpy(stream,audio_buffer+audio_read,len);

	audio_read+=len;

	if(audio_read>=buffer_size)
	{
		audio_read=0;
	}
}


int soundInit()
{
	if((SDL_InitSubSystem( SDL_INIT_AUDIO )) < 0 )
	{
		printf("ERROR: can't init sound\n");
		return 0;
	}
	printf("INFO: sound init success\n");

	/* Set the audio format */
	wanted.freq = 44100;
	wanted.format = AUDIO_S16SYS;
	wanted.channels = 2;    /* 1 = mono, 2 = stereo */
	wanted.samples = 2048;  /* Good low-latency value for callback */
	wanted.callback = fill_audio;
	wanted.userdata = NULL;

	/* Open the audio device, forcing the desired format */
	SDL_OpenAudio(&wanted, &empty);

	audio_buffer=(Uint8 *)malloc(buffer_size);
	memset(audio_buffer,0,buffer_size);

	return 1;
}




void soundFillBuffer(unsigned int dspLoop)
{
		unsigned int Rloop=dspLoop & 0x0000FFFF;
		unsigned int Lloop=(dspLoop & 0xFFFF0000)>>16;

		if(audio_pointer<=buffer_size)
		{
			memcpy(audio_buffer+audio_pointer,&dspLoop,4);
			audio_pointer+=4;
		}
		else
		{
			audio_pointer=0;
		}

		if(audio_pointer>buffer_size*1/5)
			SDL_PauseAudio(0);

		if(audio_pointer>=audio_read)
			audio_len=(audio_pointer-audio_read);
		else
			audio_len=((buffer_size-audio_read));
}


void soundClose()
{
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

