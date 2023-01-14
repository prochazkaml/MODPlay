#include "modplay.h"
#include "tables.h"
#include <string.h>

// Comment out to turn off sample interpolation - will sound crunchy, but will run faster
#define USE_LINEAR_INTERPOLATION

ModPlayerStatus_t mp;

void _RecalculateWaveform(Oscillator *oscillator) {
	int result;

	// The following generators _might_ have been inspired by micromod's code:
	// https://github.com/martincameron/micromod/blob/master/micromod-c/micromod.c

	switch(oscillator->waveform) {
		case 0:
			// Sine
			result = sine_table[oscillator->phase & 0x1F];
			if((oscillator->phase & 0x20) > 0) result *= (-1);
			break;

		case 1:
			// Sawtooth
			result = 255 - (((oscillator->phase + 0x20) & 0x3F) << 3);
			break;

		case 2:
			// Square
			result = 255 - ((oscillator->phase & 0x20) << 4);
			break;

		case 3:
			// Random
			result = (mp.random >> 20) - 255;
			mp.random = (mp.random * 65 + 17) & 0x1FFFFFFF;
			break;
	}

	oscillator->val = result * oscillator->depth;
}

ModPlayerStatus_t *ProcessMOD() {
	int i;

	if(mp.tick == 0) {
		mp.skiporderrequest = -1;

		for(i = 0; i < 4; i++) {
			mp.vibrato[i].val = mp.tremolo[i].val = 0;

			uint8_t *cell = mp.patterndata + mp.ordertable[mp.order] * (64 * 16) + mp.row * 16 + i * 4;

			int note_tmp = cell[0];
			int sample_tmp = cell[1];
			int eff_tmp = cell[2];
			int effval_tmp = cell[3];

			if(mp.eff[i] == 0 && mp.effval[i] != 0) {
				mp.paula[i].period = mp.note[i];
			}

			if(sample_tmp) {
				mp.sample[i] = sample_tmp - 1;
				
				mp.paula[i].length = mp.samples[sample_tmp - 1].actuallength;
				mp.paula[i].looplength = mp.samples[sample_tmp - 1].looplength;
				mp.paula[i].volume = mp.samples[sample_tmp - 1].volume;
				mp.paula[i].sample = mp.samples[sample_tmp - 1].data;
			}

			if(note_tmp) {
				char finetune;

				if(eff_tmp == 0xE && (effval_tmp & 0xF0) == 0x50)
					finetune = effval_tmp & 0xF;
				else
					finetune = mp.samples[mp.sample[i]].finetune;
					
				note_tmp = period_tables[finetune][note_tmp - 1];

				mp.note[i] = note_tmp;

				if(eff_tmp != 0x3 && eff_tmp != 0x5 && (eff_tmp != 0xE || (effval_tmp & 0xF0) != 0xD0)) {
					mp.paula[i].age = mp.paula[i].currentptr = 0;
					mp.paula[i].period = mp.note[i];

					if(mp.vibrato[i].waveform < 4) mp.vibrato[i].phase = 0;
					if(mp.tremolo[i].waveform < 4) mp.tremolo[i].phase = 0;
				}
			}

			if(eff_tmp || effval_tmp) switch(eff_tmp) {
				case 0x3:
					if(effval_tmp) mp.slideamount[i] = effval_tmp;

				case 0x5:
					mp.slidenote[i] = mp.note[i];
					break;

				case 0x4:
					if(effval_tmp & 0xF0) mp.vibrato[i].speed = effval_tmp >> 4;
					if(effval_tmp & 0x0F) mp.vibrato[i].depth = effval_tmp & 0x0F;

					// break intentionally left out here
	
				case 0x6:
					_RecalculateWaveform(&mp.vibrato[i]);
					break;

				case 0x7:
					if(effval_tmp & 0xF0) mp.tremolo[i].speed = effval_tmp >> 4;
					if(effval_tmp & 0x0F) mp.tremolo[i].depth = effval_tmp & 0x0F;
					_RecalculateWaveform(&mp.tremolo[i]);
					break;

				case 0xC:
					mp.paula[i].volume = (effval_tmp > 0x40) ? 0x40 : effval_tmp;
					break;

				case 0x9:
					if(effval_tmp) {
						mp.paula[i].currentptr = effval_tmp << 24;
						mp.sampleoffset[i] = effval_tmp;
					} else {
						mp.paula[i].currentptr = mp.sampleoffset[i] << 24;
					}

					mp.paula[i].age = 0;
					break;

				case 0xB:
					if(effval_tmp >= mp.orders) effval_tmp = 0;

					mp.skiporderrequest = effval_tmp;
					break;

				case 0xD:
					if(mp.skiporderrequest < 0) {
						if(mp.order + 1 < mp.orders)
							mp.skiporderrequest = mp.order + 1;
						else
							mp.skiporderrequest = 0;
					}

					if(effval_tmp > 0x3F) effval_tmp = 0;

					mp.skiporderdestrow = (effval_tmp >> 4) * 10 + (effval_tmp & 0xF); // What were the ProTracker guys smoking?!
					break;

				case 0xE:
					switch(effval_tmp >> 4) {
						case 0x1:
							mp.paula[i].period -= effval_tmp & 0xF;
							break;

						case 0x2:
							mp.paula[i].period += effval_tmp & 0xF;
							break;
						
						case 0x4:
							mp.vibrato[i].waveform = effval_tmp & 0x7;
							break;

						case 0x6:
							if(effval_tmp & 0xF) {
								if(!mp.patloopcycle)
									mp.patloopcycle = (effval_tmp & 0xF) + 1;

								if(mp.patloopcycle > 1) {
									mp.skiporderrequest = mp.order;
									mp.skiporderdestrow = mp.patlooprow;
								}

								mp.patloopcycle--;
							} else {
								mp.patlooprow = mp.row;
							}

						case 0x7:
							mp.tremolo[i].waveform = effval_tmp & 0x7;
							break;

						case 0xA:
							mp.paula[i].volume += effval_tmp & 0xF;
							if(mp.paula[i].volume > 0x40) mp.paula[i].volume = 0x40;
							break;

						case 0xB:
							mp.paula[i].volume -= effval_tmp & 0xF;
							if(mp.paula[i].volume < 0x00) mp.paula[i].volume = 0x00;
							break;

						case 0xE:
							mp.maxtick *= ((effval_tmp & 0xF) + 1);
							break;
					}
					break;

				case 0xF:
					if(effval_tmp) {
						if(effval_tmp < 0x20) {
							mp.maxtick = (mp.maxtick / mp.speed) * effval_tmp;
							mp.speed = effval_tmp;
						} else {
							mp.audiospeed = mp.samplerate * 125 / effval_tmp / 50;
						}
					}

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
						mp.arp = (mp.note[i] * arpeggio_table[effval_tmp >> 4]) >> 16;
						break;

					case 2:
						mp.arp = (mp.note[i] * arpeggio_table[effval_tmp & 0xF]) >> 16;
						break;
				}

				mp.paula[i].period = mp.arp;
				break;

			case 0x1:
				if(mp.tick) mp.paula[i].period -= effval_tmp;
				break;

			case 0x2:
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

			case 0x4:
				if(mp.tick) {
					mp.vibrato[i].phase += mp.vibrato[i].speed;
					_RecalculateWaveform(&mp.vibrato[i]);
				}
				break;

			case 0x6:
				if(mp.tick) {
					mp.vibrato[i].phase += mp.vibrato[i].speed;
					_RecalculateWaveform(&mp.vibrato[i]);
				}
				// break intentionally left out here

			case 0xA:
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

			case 0x7:
				if(mp.tick) {
					mp.tremolo[i].phase += mp.tremolo[i].speed;
					_RecalculateWaveform(&mp.tremolo[i]);
				}
				break;

			case 0xE:
				switch(effval_tmp >> 4) {
					case 0x9:
						if(mp.tick && !(mp.tick % (effval_tmp & 0xF))) mp.paula[i].age = mp.paula[i].currentptr = 0;
						break;

					case 0xC:
						if(mp.tick >= (effval_tmp & 0xF)) mp.paula[i].volume = 0;
						break;

					case 0xD:
						if(mp.tick == (effval_tmp & 0xF)) {
							mp.paula[i].age = mp.paula[i].currentptr = 0;
							mp.paula[i].period = mp.note[i];
						}
						break;
				}

				break;
		}

		if(mp.paula[i].period < period_tables[0][3*12-1] && mp.paula[i].period != 0) {
			mp.paula[i].period = period_tables[0][3*12-1];
		}
	}

	mp.tick++;
	if(mp.tick >= mp.maxtick) {
		mp.tick = 0;
		mp.maxtick = mp.speed;

		if(mp.skiporderrequest >= 0) {
			mp.row = mp.skiporderdestrow;
			mp.order = mp.skiporderrequest;

			mp.skiporderdestrow = 0;
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

	return &mp;
}

ModPlayerStatus_t *RenderMOD(short *buf, int len) {
	memset(buf, 0, len * 4);

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
				if(!mp.paula[ch].muted) {
					int vol = mp.paula[ch].volume + (mp.tremolo[ch].val >> 6);

					if(vol < 0) vol = 0;
					if(vol > 64) vol = 64;

#ifdef USE_LINEAR_INTERPOLATION
					uint32_t nextptr = mp.paula[ch].currentptr + 0x10000;
					
					if((nextptr >> 17) >= mp.paula[ch].length &&
						mp.paula[ch].looplength != 0)

						nextptr -= mp.paula[ch].looplength << 17;

					int sample1 = mp.paula[ch].sample[mp.paula[ch].currentptr >> 16] * vol;
					int sample2 = mp.paula[ch].sample[nextptr >> 16] * vol;

					short sample = (sample1 * (0x10000 - (nextptr & 0xFFFF)) +
						  sample2 * (nextptr & 0xFFFF)) / 0x10000;
#else
					short sample = mp.paula[ch].sample[mp.paula[ch].currentptr >> 16] * vol;
#endif

					// Distribute the rendered sample across both output channels

					if((ch & 3) == 1 || (ch & 3) == 2) {
						buf[s * 2] += sample / 3;
						buf[s * 2 + 1] += sample;
					} else {
						buf[s * 2] += sample;
						buf[s * 2 + 1] += sample / 3;
					}
				}

				// Advance to the next required sample

				if(mp.paula[ch].period)
					mp.paula[ch].currentptr += (mp.paularate << 16) / (((uint32_t) mp.paula[ch].period) + (mp.vibrato[ch].val >> 7));

				// Stop this channel if we have reached the end or loop it, if desired

				if((mp.paula[ch].currentptr >> 17) >= mp.paula[ch].length) {
					if(mp.paula[ch].looplength == 0) {
						mp.paula[ch].period = 0;
					} else {
						mp.paula[ch].currentptr -= mp.paula[ch].looplength << 17;
					}
				} else {
					if(mp.paula[ch].age < INT32_MAX)
					mp.paula[ch].age++;
				}
			}
		}
	}

	return &mp;
}

ModPlayerStatus_t *InitMOD(uint8_t *mod, int samplerate) {
	if(memcmp(mod + 1080, "M.K.", 4)) {
		return NULL;
	}

	memset(&mp, 0, sizeof(mp));

	mp.samplerate = samplerate;
	mp.paularate = 3546895 / samplerate;

	mp.orders = mod[950];
	mp.ordertable = mod + 952;

	mp.maxpattern = 0;

	for(int i = 0; i < 128; i++) {
		if(mp.ordertable[i] >= mp.maxpattern) mp.maxpattern = mp.ordertable[i];
	}
	mp.maxpattern++;

	int8_t *samplemem = mod + 1084 + 1024 * mp.maxpattern;
	mp.patterndata = mod + 1084;

	for(int i = 0; i < 31; i++) {
		uint8_t *sample = mod + 20 + i * 30;

		mp.samples[i].length = (sample[22] << 8) | sample[23];
		mp.samples[i].finetune = sample[24];
		mp.samples[i].volume = sample[25];
		mp.samples[i].looppoint = (sample[26] << 8) | sample[27];
		mp.samples[i].actuallength = (sample[28] << 8) | sample[29];

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

	for(int pat = 0; pat < mp.maxpattern; pat++) {
		for(int row = 0; row < 64; row++) {
			for(int col = 0; col < 4; col++) {
				uint8_t *cell = mp.patterndata + pat * (64 * 16) + row * 16 + col * 4;

				int period = ((cell[0] & 0x0F) << 8) | cell[1];
				int sample = (cell[0] & 0xF0) | ((cell[2] & 0xF0) >> 4);
				int eff = cell[2] & 0x0F;
				int effval = cell[3];

				if(period == 0) {
					cell[0] = 0;
				} else {
					int note = 0;

					for(int i = 1; i < 36; i++) {
						if(period_tables[0][i] <= period) {
							int low = period - period_tables[0][i];
							int high = period_tables[0][i - 1] - period;

							if(low < high)
								note = i;
							else
								note = i - 1;

							break;
						}
					}

					cell[0] = note + 1;
				}

				cell[1] = sample;
				cell[2] = eff;
				cell[3] = effval;
			}
		}
	}

	mp.maxtick = mp.speed = 6; mp.audiospeed = mp.samplerate / 50;

	for(int i = 0; i < 4; i++) {
		mp.paula[i].age = INT32_MAX;
	}

	return &mp;
}

ModPlayerStatus_t *JumpMOD(int order) {
	int neworder = mp.order;

	ModPlayerStatus_t old_mp = mp;

	memset(&mp, 0, sizeof(mp));

	mp.orders = old_mp.orders;
	mp.maxpattern = old_mp.maxpattern;
	mp.samplerate = old_mp.samplerate;
	mp.paularate = old_mp.paularate;

	mp.patterndata = old_mp.patterndata;
	mp.ordertable = old_mp.ordertable;

	memcpy(mp.samples, old_mp.samples, sizeof(mp.samples));

	mp.maxtick = mp.speed = 6; mp.audiospeed = mp.samplerate / 50;

	for(int i = 0; i < 4; i++) {
		mp.paula[i].age = INT32_MAX;
	}

	switch(order) {
		case -2:
			if(neworder > 0) neworder--;
			break;

		case -1:
			if(neworder < mp.orders - 1) neworder++;
			break;

		default:
			if(order < 0) order = 0;
			if(order >= mp.orders) order = mp.orders - 1;

			neworder = order;
			break;
	}

	int oldorder = 0;

	while(mp.order < neworder) {
		ProcessMOD();

		if(oldorder > mp.order)
			break;
		else
			oldorder = mp.order;
	}

	return &mp;
}
