#include "modplay.h"
#include <string.h>

ModPlayerStatus_t mp;

void ProcessMOD() {
	int i;

	const int arplookup[16] = {
		65536, 61858, 58386, 55109,
		52016, 49096, 46341, 43740,
		41285, 38968, 36781, 34716,
		32768, 30929, 29193, 27554
	};

	if(mp.tick == 0) {
		mp.skiporderrequest = -1;

		for(i = 0; i < 4; i++) {
			uint8_t *cell = mp.patterndata + mp.ordertable[mp.order] * (64 * 16) + mp.row * 16 + i * 4;

			int note_tmp = ((cell[0] & 0x0F) << 8) | cell[1];
			int sample_tmp = (cell[0] & 0xF0) | ((cell[2] & 0xF0) >> 4);
			int eff_tmp = cell[2] & 0x0F;
			int effval_tmp = cell[3];

			if(sample_tmp) {
				mp.sample[i] = sample_tmp - 1;
				
				mp.paula[i].length = mp.samples[sample_tmp - 1].actuallength;
				mp.paula[i].looplength = mp.samples[sample_tmp - 1].looplength;
				mp.paula[i].volume = mp.samples[sample_tmp - 1].volume;
				mp.paula[i].sample = mp.samples[sample_tmp - 1].data;
			}

			if(note_tmp) {
				mp.note[i] = note_tmp;

				if(eff_tmp != 0x3 && eff_tmp != 5) {
					mp.paula[i].currentptr = 0;
					mp.paula[i].period = mp.note[i] = note_tmp;
					mp.paula[i].period -= mp.samples[mp.sample[i]].finetune;
				}
			}

			if(eff_tmp || effval_tmp) switch(eff_tmp) {
				case 0x0: case 0xA: case 0x4: case 0x6:
					break;

				case 0x1: case 0x2:
					if(effval_tmp) mp.slideamount[i] = effval_tmp;
					break;

				case 0x3:
					if(effval_tmp) mp.slideamount[i] = effval_tmp;
	//						if(note_tmp) slidenote[i] = note_tmp - samples[sample[i]].finetune;
	//						break;

				case 0x5:
					mp.slidenote[i] = mp.note[i] - mp.samples[mp.sample[i]].finetune;
					break;

				case 0xC:
					mp.paula[i].volume = (effval_tmp > 0x40) ? 0x40 : effval_tmp;
					break;

				case 0x9:
					mp.paula[i].currentptr = effval_tmp << 24;
					break;

				case 0xB:
					mp.skiporderrequest = effval_tmp;
					break;

				case 0xD:
					if(mp.order + 1 < mp.orders)
						mp.skiporderrequest = mp.order + 1;
					else
						mp.skiporderrequest = 0;

					break;

				case 0xE:
					switch(effval_tmp >> 4) {
						case 0x0: case 0x9: case 0xC:
							break;

						case 0x1:
							mp.paula[i].period -= effval_tmp & 0xF;
							break;

						case 0x2:
							mp.paula[i].period += effval_tmp & 0xF;
							break;
						
						case 0xA:
							mp.paula[i].volume += effval_tmp & 0xF;
							if(mp.paula[i].volume > 0x40) mp.paula[i].volume = 0x40;
							break;

						case 0xB:
							mp.paula[i].volume -= effval_tmp & 0xF;
							if(mp.paula[i].volume < 0x00) mp.paula[i].volume = 0x00;
							break;
					}
					break;

				case 0xF:
					if(effval_tmp < 0x20)
						mp.speed = effval_tmp;

					if(effval_tmp >= 0x20)
						mp.audiospeed = mp.samplerate * 125 / effval_tmp / 50;

					break;
			}

			mp.eff[i] = eff_tmp;
			mp.effval[i] = effval_tmp;
		}
	}

	for(i = 0; i < 4; i++) {
		int eff_tmp = mp.eff[i];
		int effval_tmp = mp.effval[i];

		if(eff_tmp || effval_tmp) switch(eff_tmp) {
			case 0x0:
				switch(mp.tick % 3) {
					case 0:
						mp.arp = mp.note[i];
						break;

					case 1:
						mp.arp = (mp.note[i] * arplookup[effval_tmp >> 4]) >> 16;
						break;

					case 2:
						mp.arp = (mp.note[i] * arplookup[effval_tmp & 0xF]) >> 16;
						break;
				}

				mp.paula[i].period = mp.arp - mp.samples[mp.sample[i]].finetune;
				break;

			case 0x1:
				if(!effval_tmp) effval_tmp = mp.slideamount[i];
				if(mp.tick) mp.paula[i].period -= effval_tmp;
				break;

			case 0x2:
				if(!effval_tmp) effval_tmp = mp.slideamount[i];
				if(mp.tick) mp.paula[i].period += effval_tmp;
				break;

			case 0x5:
				if(mp.tick) {
					if(effval_tmp > 0xF) {
						mp.paula[i].volume += (effval_tmp >> 4);
						if(mp.paula[i].volume > 0x40) mp.paula[i].volume = 0x40;
					} else {
						mp.paula[i].volume -= (effval_tmp & 0xF);
						if(mp.paula[i].volume < 0x00) mp.paula[i].volume = 0x00;
					}
				}
				
				effval_tmp = 0;
				// break intentionally left out here

			case 0x3:
				if(mp.tick) {
					if(!effval_tmp) effval_tmp = mp.slideamount[i];

					if(mp.slidenote[i] > mp.paula[i].period) {
						mp.paula[i].period += effval_tmp;

						if(mp.slidenote[i] < mp.paula[i].period)
							mp.paula[i].period = mp.slidenote[i];
					} else if(mp.slidenote[i] < mp.paula[i].period) {
						mp.paula[i].period -= effval_tmp;

						if(mp.slidenote[i] > mp.paula[i].period)
							mp.paula[i].period = mp.slidenote[i];
					} 
				}

				break;


			case 0xA: case 0x6:
				if(mp.tick) {
					if(effval_tmp > 0xF) {
						mp.paula[i].volume += (effval_tmp >> 4);
						if(mp.paula[i].volume > 0x40) mp.paula[i].volume = 0x40;
					} else {
						mp.paula[i].volume -= (effval_tmp & 0xF);
						if(mp.paula[i].volume < 0x00) mp.paula[i].volume = 0x00;
					}
				}

				break;

			case 0xE:
				switch(effval_tmp >> 4) {
					case 0x9:
						if(mp.tick && !(mp.tick % (effval_tmp & 0xF))) mp.paula[i].currentptr = 0;
						break;

					case 0xC:
						if(mp.tick >= (effval_tmp & 0xF)) mp.paula[i].volume = 0;
						break;
				}

				break;
		}
	}

	mp.tick++;
	if(mp.tick >= mp.speed) {
		mp.tick = 0;

		if(mp.skiporderrequest >= 0) {
			mp.row = 0;
			mp.order = mp.skiporderrequest;
			mp.skiporderrequest = -1;
		} else {
			mp.row++;
			if(mp.row >= 0x40) {
				mp.row = 0;
				mp.order++;

				if(mp.order >= mp.orders) mp.order = 0;
			}
		}
	}
}

ModPlayerStatus_t *RenderMOD(short *buf, int len) {
	for(int s = 0; s < len; s++) {
		// Process the tick, if necessary

		if(!mp.audiotick) {
			ProcessMOD();
			mp.audiotick = mp.audiospeed;
		}

		mp.audiotick--;

		// Render the audio

		for(int ch = 0; ch < 4; ch++) {
			if(mp.paula[ch].sample) {
				// Perform linear interpolation on the sample (otherwise it will sound like crap)

				uint32_t nextptr = mp.paula[ch].currentptr + 0x10000;
				
				if((nextptr >> 17) >= mp.paula[ch].length &&
					mp.paula[ch].looplength != 0)

					nextptr -= mp.paula[ch].looplength << 17;

				int sample1 = mp.paula[ch].sample[mp.paula[ch].currentptr >> 16] * mp.paula[ch].volume;
				int sample2 = mp.paula[ch].sample[nextptr >> 16] * mp.paula[ch].volume;

				short sample = (sample1 * (0x10000 - (nextptr & 0xFFFF)) +
					sample2 * (nextptr & 0xFFFF)) / 0x10000;

	//			short sample = mp.paula[ch].sample[mp.paula[ch].currentptr >> 16] * mp.paula[ch].volume;

				// Distribute the rendered sample across both output channels

				if((ch & 3) == 1 || (ch & 3) == 2) {
					buf[s * 2] += sample / 6;
					buf[s * 2 + 1] += sample / 2;
				} else {
					buf[s * 2] += sample / 2;
					buf[s * 2 + 1] += sample / 6;
				}

				// Advance to the next required sample

				if(mp.paula[ch].period)
					mp.paula[ch].currentptr += (mp.paularate << 16) / ((uint32_t) mp.paula[ch].period);

				// Stop this channel if we have reached the end or loop it, if desired

				if((mp.paula[ch].currentptr >> 17) >= mp.paula[ch].length) {
					if(mp.paula[ch].looplength == 0) {
						mp.paula[ch].period = 0;
					} else {
						mp.paula[ch].currentptr -= mp.paula[ch].looplength << 17;
					}
				}
			}
		}
	}

	return &mp;
}

void InitMOD(uint8_t *mod, int samplerate) {
	int i;

	memset(&mp, 0, sizeof(mp));

	mp.samplerate = samplerate;
	mp.paularate = 3546895 / samplerate;

	mp.orders = mod[950];
	mp.ordertable = mod + 952;

	mp.maxpattern = 0;

	for(i = 0; i < 128; i++) {
		if(mp.ordertable[i] >= mp.maxpattern) mp.maxpattern = mp.ordertable[i];
	}
	mp.maxpattern++;

	int8_t *samplemem = mod + 1084 + 1024 * mp.maxpattern;
	mp.patterndata = mod + 1084;

	for(i = 0; i < 31; i++) {
		uint8_t *sample = mod + 20 + i * 30;

		mp.samples[i].length = (sample[22] << 8) | sample[23];
		mp.samples[i].finetune = sample[24];
		mp.samples[i].volume = sample[25];
		mp.samples[i].looppoint = (sample[26] << 8) | sample[27];
		mp.samples[i].actuallength = (sample[28] << 8) | sample[29];

		if(mp.samples[i].finetune & 0x08) mp.samples[i].finetune |= 0xF0;

		mp.samples[i].data = samplemem;
		samplemem += mp.samples[i].length * 2;

		mp.samples[i].actuallength += mp.samples[i].looppoint;

		if(mp.samples[i].actuallength < 0x2) {
			mp.samples[i].actuallength = mp.samples[i].length;
			mp.samples[i].looppoint = 0xFFFF;
			mp.samples[i].looplength = 0;
		} else if(mp.samples[i].actuallength > mp.samples[i].length) {
			mp.samples[i].looppoint /= 2;
			mp.samples[i].actuallength -= mp.samples[i].looppoint;
			mp.samples[i].looplength = mp.samples[i].actuallength - mp.samples[i].looppoint;
		} else {
			mp.samples[i].looplength = mp.samples[i].actuallength - mp.samples[i].looppoint;
		}
	}

	mp.speed = 6; mp.audiospeed = mp.samplerate / 50; mp.audiotick = 0;
}
