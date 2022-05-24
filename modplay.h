#include <stdint.h>

typedef struct {
	const int8_t *sample;
	uint32_t age;
	uint32_t currentptr;
	uint16_t length;
	uint16_t looplength;
	int16_t period;
	int8_t volume;
	int8_t muted;
} PaulaChannel_t;

typedef struct {
	const int8_t *data;
	uint16_t length;
	uint16_t actuallength;
	uint16_t looppoint;
	uint16_t looplength;
	uint8_t volume;
	int8_t finetune;
} Sample;

typedef struct {
	int val;
	uint8_t waveform;
	uint8_t phase;
	uint8_t speed;
	uint8_t depth;
} Oscillator;

typedef struct {
	int orders, maxpattern, order, row, tick, maxtick, speed, arp,
		skiporderrequest, skiporderdestrow,
		patlooprow, patloopcycle,
		samplerate, paularate, audiospeed, audiotick, random;

	int note[4], sample[4], eff[4], effval[4];
	int slideamount[4], slidenote[4], sampleoffset[4];
	
	Oscillator vibrato[4], tremolo[4];

	uint8_t *patterndata, *ordertable;
	Sample samples[31];

	PaulaChannel_t paula[4];
} ModPlayerStatus_t;

ModPlayerStatus_t *InitMOD(uint8_t *mod, int samplerate);
ModPlayerStatus_t *RenderMOD(short *buf, int len);
ModPlayerStatus_t *ProcessMOD();
ModPlayerStatus_t *JumpMOD(int order); // -2 = decrement, -1 = increment, 0+ = absolute
