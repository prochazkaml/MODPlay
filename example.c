#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#include "modplay.h"

#define SAMPLERATE 44100
#define BARGRAPHSTEPS 48

double bargraphvals[4] = { 0.0, 0.0, 0.0, 0.0 };
double bargraphtargets[4] = { 0.0, 0.0, 0.0, 0.0 };
double bargrapholdages[4] = { INFINITY, INFINITY, INFINITY, INFINITY };

void SDL_Callback(void *data, uint8_t *stream, int len) {
	short *buf = (short *) stream;

	ModPlayerStatus_t *mp = RenderMOD(buf, len / (sizeof(short) * 2));

	printf("\n\nID   | FREQ | VL | SM | BARGRAPH");

	for(int i = 0; i < 4; i++) {
		printf("\n\r\e[2KCH %d |% 5d |% 3d |% 3d | ",
			i + 1,
			mp->paula[i].period,
			mp->paula[i].volume,
			mp->sample[i] + 1);

		bargraphtargets[i] = mp->paula[i].volume;

		if(mp->paula[i].age < bargrapholdages[i])
			bargraphtargets[i] *= 1.5;

		bargrapholdages[i] = mp->paula[i].age;

		if(mp->paula[i].period == 0) bargraphtargets[i] = 0.0;

		if(bargraphvals[i] < bargraphtargets[i]) bargraphvals[i] = bargraphtargets[i];

		for(int x = 0; x < BARGRAPHSTEPS; x++) {
			if(bargraphvals[i] > x * 96 / BARGRAPHSTEPS)
				putchar('=');
			else
				break;
		}

		if(bargraphvals[i] > bargraphtargets[i]) {
			bargraphvals[i] -= 1.5;

			if(bargraphvals[i] < bargraphtargets[i]) bargraphvals[i] = bargraphtargets[i];
		}
	}

	printf("\e[6A\r\e[2KRow %02d, order %02d/%02d (pattern %02d) @ speed %d", mp->row, mp->order, mp->orders - 1, mp->ordertable[mp->order], mp->speed);

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

	if(!InitMOD(tune, SAMPLERATE)) {
		printf("Invalid file!\n");
		exit(1);
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
					printf("\r\e[2KQuitting.\n\n\n\n\n\n\n");
					SDL_Quit();
					exit(0);
					break;
			}
		}

		SDL_Delay(100);
	}
}
