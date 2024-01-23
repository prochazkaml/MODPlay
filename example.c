#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#include "modplay.h"

#define SAMPLERATE 44100
#define BARGRAPHSTEPS 48

double bargraphvals[CHANNELS];
double bargraphtargets[CHANNELS];
double bargrapholdages[CHANNELS];

int channels;

void SDL_Callback(void *data, uint8_t *stream, int len) {
	short *buf = (short *) stream;

	ModPlayerStatus_t *mp = RenderMOD(buf, len / (sizeof(short) * 2));

	printf("\n\nID    | FREQ | VL | SM | BARGRAPH");

	for(int i = 0; i < channels; i++) {
		printf("\n\r\e[2KCH %02d |% 5d |% 3d |% 3d | ",
			i + 1,
			mp->ch[i].samplegen.period ? (mp->paularate / mp->ch[i].samplegen.period) : 0,
			mp->ch[i].samplegen.volume,
			mp->ch[i].sample + 1);

		bargraphtargets[i] = mp->ch[i].samplegen.volume;

		if(mp->ch[i].samplegen.age < bargrapholdages[i])
			bargraphtargets[i] *= 1.5;

		bargrapholdages[i] = mp->ch[i].samplegen.age;

		if(mp->ch[i].period == 0) bargraphtargets[i] = 0.0;

		if(bargraphvals[i] < bargraphtargets[i]) bargraphvals[i] = bargraphtargets[i];

		for(int x = 0; x < BARGRAPHSTEPS; x++) {
			if(bargraphvals[i] > x * 96 / BARGRAPHSTEPS)
				putchar('=');
			else
				break;
		}

		if(bargraphvals[i] > bargraphtargets[i]) {
			bargraphvals[i] -= 2;

			if(bargraphvals[i] < bargraphtargets[i]) bargraphvals[i] = bargraphtargets[i];
		}
	}

	printf("\e[%dA\r\e[2KRow %02d, order %02d/%02d (pattern %02d) @ speed %d", 2 + channels, mp->row, mp->order + 1, mp->orders, mp->ordertable[mp->order], mp->speed);

	fflush(stdout);
}

SDL_AudioSpec sdl_audio = {
	.freq = SAMPLERATE,
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

	ModPlayerStatus_t *mp = InitMOD(tune, SAMPLERATE);

	if(!mp) {
		printf("Invalid file!\n");
		exit(1);
	}

	channels = mp->channels;

	printf("Status type size: %d\n", sizeof(ModPlayerStatus_t));

	for(int i = 0; i < CHANNELS; i++) {
		bargraphvals[i] = 0.0;
		bargraphtargets[i] = 0.0;
		bargrapholdages[i] = INFINITY;
	}

	printf("Playing %s...\n\n", argv[1]);

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	SDL_OpenAudio(&sdl_audio, 0);

	SDL_PauseAudio(0);
	
	SDL_Event event;

	while(1) {
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
				case SDL_QUIT:
					printf("\r\e[2KQuitting.\r\e[%dB\n\n", 2 + channels);
					SDL_Quit();
					exit(0);
					break;
			}
		}

		SDL_Delay(100);
	}
}
