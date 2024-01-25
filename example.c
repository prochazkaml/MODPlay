#include <math.h>
#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL/SDL.h>

#include "modplay.h"

// thx, https://gist.github.com/ictlyh/b9be0b020ae3d044dc75ef0caac01fbb

/* Program documentation. */
static const char doc[] = "MODPlay example program";

/* A description of the arguments we accept. */
static const char args_doc[] = "MODFILE";

/* The options we understand. */
static struct argp_option options[] = {
	{ "disable-ch-info", 'd', 0, 0,  "disable live channel info during playback\n(enabled by default)" },
	{ "start-order", 'o', "ORDER_ID", 0, "start playback from specified order\n(1 by default)" },
	{ 0 }
};

static bool disable_ch_info = false;
static int start_order = 1;
static char *filename;

/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state) {
	switch(key) {
		case 'd':
			disable_ch_info = true;
			break;

		case 'o':
			start_order = atoi(arg);
			break;

		case ARGP_KEY_ARG:
			if (state->arg_num >= 1)
				// Too many arguments.
				argp_usage (state);

			filename = arg;
			break;

		case ARGP_KEY_END:
			if (state->arg_num < 1)
				/* Not enough arguments. */
				argp_usage (state);

			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

#define SAMPLERATE 44100
#define BARGRAPHSTEPS 48

static double bargraphvals[CHANNELS];
static double bargraphtargets[CHANNELS];
static double bargrapholdages[CHANNELS];

static int channels;

void SDL_Callback(void *data, uint8_t *stream, int len) {
	short *buf = (short *) stream;

	ModPlayerStatus_t *mp = RenderMOD(buf, len / (sizeof(short) * 2));

	if(!disable_ch_info) {
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

		printf("\e[%dA", 2 + channels);
	}

	printf("\r\e[2KRow %02d, order %02d/%02d (pattern %02d) @ speed %d", mp->row, mp->order + 1, mp->orders, mp->ordertable[mp->order], mp->speed);

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
	argp_parse(&argp, argc, argv, 0, 0, NULL);

	FILE *f = fopen(filename, "rb");

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

	if(start_order > 1 && start_order <= mp->orders) {
		JumpMOD(start_order - 1);
	}

//	printf("Status type size: %d\n", sizeof(ModPlayerStatus_t));

	for(int i = 0; i < CHANNELS; i++) {
		bargraphvals[i] = 0.0;
		bargraphtargets[i] = 0.0;
		bargrapholdages[i] = INFINITY;
	}

	printf("Playing %s...\n\n", filename);

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
