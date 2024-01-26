#include <math.h>
#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL/SDL.h>
#include <sys/time.h>

#include "modplay.h"

// thx, https://gist.github.com/ictlyh/b9be0b020ae3d044dc75ef0caac01fbb

/* Program documentation. */
#ifdef TEST
static const char doc[] = "MODPlay example program ** COMPILED IN TEST MODE, MAY BE SLOWER **";
#else
static const char doc[] = "MODPlay example program";
#endif

/* A description of the arguments we accept. */
static const char args_doc[] = "MODFILE";

/* The options we understand. */
static struct argp_option options[] = {
	{ "render", 'r', "filename.wav", 0, "render to a WAV file (disables playback)" },
	{ "sample-rate", 'a', "HZ", 0, "sets the playback/render sample rate\n(in Hz, 44100 by default)" },
	{ "buffer-size", 'b', "SAMPLES", 0, "forces a specific audio buffer size" },
	{ "disable-ch-info", 'd', 0, 0,  "disable live channel info during playback\n(enabled by default)" },
	{ "start-order", 'o', "ORDER_ID", 0, "start playback from specified order\n(1 by default)" },
	{ "speed", 's', "SPEED", 0, "force a given speed (the tune may override this setting)" },
	{ "tempo", 't', "TEMPO", 0, "force a given tempo (the tune may override this setting)" },
	{ 0 }
};

static bool disable_ch_info = false;
static char *render_filename = NULL;
static int start_order = 1;
static int start_speed = 0;
static int start_tempo = 0;
static int sample_rate = 44100;
static int buffer_size = 0;
static char *filename;

/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state) {
	switch(key) {
		case 'r':
			render_filename = arg;
			break;
		
		case 'a':
			sample_rate = atoi(arg);
			break;

		case 'b':
			buffer_size = atoi(arg);
			break;

		case 'd':
			disable_ch_info = true;
			break;

		case 'o':
			start_order = atoi(arg);
			break;

		case 's':
			start_speed = atoi(arg);
			break;

		case 't':
			start_tempo = atoi(arg);
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

static int channels;

#define BARGRAPHSTEPS 48

static double bargraphvals[CHANNELS];
static double bargraphtargets[CHANNELS];
static double bargrapholdages[CHANNELS];

static double callback_rate; // 2 for 44100 Hz, 4 for 22050 Hz, etc.

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
				bargraphvals[i] -= callback_rate;

				if(bargraphvals[i] < bargraphtargets[i]) bargraphvals[i] = bargraphtargets[i];
			}
		}

		printf("\e[%dA", 2 + channels);
	}

	printf("\r\e[2KRow %02d, order %02d/%02d (pattern %02d) @ speed %d", mp->row, mp->order + 1, mp->orders, mp->ordertable[mp->order], mp->speed);

	fflush(stdout);
}

SDL_AudioSpec sdl_audio = {
	.freq = 0,
	.format = AUDIO_S16,
	.channels = 2,
	.samples = 1024,
	.callback = SDL_Callback
};

void play(ModPlayerStatus_t *mp) {
	for(int i = 0; i < CHANNELS; i++) {
		bargraphvals[i] = 0.0;
		bargraphtargets[i] = 0.0;
		bargrapholdages[i] = INFINITY;
	}

	sdl_audio.freq = sample_rate;

	if(buffer_size) {
		sdl_audio.samples = buffer_size;
	} else {
		while(sample_rate / sdl_audio.samples > 60 && sdl_audio.samples < 4096) {
			sdl_audio.samples *= 2;
		}

		while(sample_rate / sdl_audio.samples < 30 && sdl_audio.samples > 64) {
			sdl_audio.samples /= 2;
		}
	}

	callback_rate = 44100.f * 2.f / ((double) sample_rate) * ((double) sdl_audio.samples) / 1024.f;

	printf("Playing %s (at %d Hz, %d samples per frame)...\n\n", filename, sdl_audio.freq, sdl_audio.samples);

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

void render(ModPlayerStatus_t *mp) {
	// Yes, this will only work on a little endian machine. Too bad.

	printf("Rendering %s...\n", filename);

	int oldorder = -1, samples = 0;

	uint8_t outbuf[512] = "RIFFabcdWAVE"
		"fmt " // Format header
		"\x10\x00\x00\x00" // Format header size (16 bytes)
		"\x01\x00" // Type = PCM
		"\x02\x00" // 2 channels
		"efgh" // Sample rate (will fill this in later)
		"ijkl" // Byte rate (will fill this in later)
		"\x04\x00" // 4 bytes per sample
		"\x10\x00" // 16 bits per sample
		"data" // Data header
		"mnop"; // Data size (will fill this in later)

	struct timeval stop, start;

	gettimeofday(&start, NULL);

	FILE *f = fopen(render_filename, "wb");

	if(f == NULL) {
		printf("Error opening file for writing!\n");
		exit(1);
	}

	fwrite(outbuf, 1, 44, f);

	while(oldorder <= mp->order) {
		if(oldorder != mp->order) {
			printf("-> order %d/%d\n", mp->order + 1, mp->orders);
			oldorder = mp->order;
		}

		RenderMOD((short *) outbuf, 128); // TODO - use audiospeed for this
		fwrite(outbuf, 1, sizeof(outbuf), f);
		samples += 128;
	}

	// Fill in the sample rate & byte rate

	fseek(f, 24, SEEK_SET);

	uint32_t tmp = sample_rate;
	fwrite(&tmp, 1, 4, f);

	tmp *= 4;
	fwrite(&tmp, 1, 4, f);

	// Fill in the data size

	tmp = samples * 4;

	fseek(f, 40, SEEK_SET);
	fwrite(&tmp, 1, 4, f);

	// Fill in the file size

	tmp += 36;

	fseek(f, 4, SEEK_SET);
	fwrite(&tmp, 1, 4, f);

	fclose(f);

	gettimeofday(&stop, NULL);

	double rendered = ((double) samples) / ((double) sample_rate);
	double elapsed = (double) (stop.tv_usec - start.tv_usec) / 1000000.0 + (double) (stop.tv_sec - start.tv_sec);

	printf("Rendered %.3fs of audio in %.3fs (%.3fx as fast as real time).\n", rendered, elapsed, rendered / elapsed);
}

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

	ModPlayerStatus_t *mp = InitMOD(tune, sample_rate);

	if(!mp) {
		printf("Invalid file!\n");
		exit(1);
	}

	channels = mp->channels;

	if(start_order > 1 && start_order <= mp->orders) {
		JumpMOD(start_order - 1);
	}

	if(start_speed > 0) {
		mp->speed = start_speed;
	}

	if(start_tempo > 0) {
		mp->audiospeed = mp->samplerate * 125 / start_tempo / 50;
	}

//	printf("Status type size: %d\n", sizeof(ModPlayerStatus_t));

	if(render_filename == NULL) {
		play(mp);
	} else {
		render(mp);
	}
}
