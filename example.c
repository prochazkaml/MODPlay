#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#include "modplay.h"

void SDL_Callback(void *data, uint8_t *stream, int len) {
	short *buf = (short *) stream;
	memset(buf, 0, len);

	ModPlayerStatus_t *mp = RenderMOD(buf, len / (sizeof(short) * 2));

	printf("\rRow %02d, order %02d/%02d (pattern %02d) @ speed %d", mp->row, mp->order, mp->orders - 1, mp->ordertable[mp->order], mp->speed);
	fflush(stdout);
}

SDL_AudioSpec sdl_audio = {
	.freq = 44100,
	.format = AUDIO_S16,
	.channels = 2,
	.samples = 1024,
	.callback = SDL_Callback
};

int main(int argc, char *argv[]) {
	if(argc != 2) {
		printf("Usage: %s modfile\n", argv[0]);
		exit(1);
	}

	FILE *f = fopen(argv[1], "rb");

	if(f == NULL) {
		printf("Error loading file!\n");
		exit(1);
	}

	fseek(f, 0, SEEK_END);
	int tune_len = ftell(f);
	fseek(f, 0, SEEK_SET);

	uint8_t *tune = malloc(tune_len);
	fread(tune, 1, tune_len, f);
	fclose(f);

	printf("Playing %s...\n", argv[1]);

	InitMOD(tune, 44100);

	SDL_Init(SDL_INIT_AUDIO);
	SDL_OpenAudio(&sdl_audio, 0);

	SDL_PauseAudio(0);
	
	atexit(SDL_Quit);

	while(1) SDL_Delay(1000);
}
