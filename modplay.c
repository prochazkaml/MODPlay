#include "modplay.h"
#include "tables.h"
#include <string.h>

// Comment out to turn off sample interpolation - will sound crunchy, but will run faster
#define USE_LINEAR_INTERPOLATION

ModPlayerStatus_t mp;

void _RecalculateWaveform(Oscillator_t *oscillator) {
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

		for(i = 0; i < mp.channels; i++) {
			mp.ch[i].vibrato.val = mp.ch[i].tremolo.val = 0;

			const uint8_t *cell = mp.patterndata + 4 * (i + mp.channels * (mp.row + 64 * mp.ordertable[mp.order]));

			int note_tmp = ((cell[0] << 8) | cell[1]) & 0xFFF;
			int sample_tmp = (cell[0] & 0xF0) | (cell[2] >> 4);
			int eff_tmp = cell[2] & 0x0F;
			int effval_tmp = cell[3];

			if(mp.ch[i].eff == 0 && mp.ch[i].effval != 0) {
				mp.ch[i].samplegen.period = mp.ch[i].note;
			}

			if(sample_tmp) {
				mp.ch[i].sample = sample_tmp - 1;
				
				mp.ch[i].samplegen.length = mp.samples[sample_tmp - 1].actuallength;
				mp.ch[i].samplegen.looplength = mp.samples[sample_tmp - 1].looplength;
				mp.ch[i].samplegen.volume = mp.samples[sample_tmp - 1].volume;
				mp.ch[i].samplegen.sample = mp.samples[sample_tmp - 1].data;
			}

			if(note_tmp) {
				char finetune;

				if(eff_tmp == 0xE && (effval_tmp & 0xF0) == 0x50)
					finetune = effval_tmp & 0xF;
				else
					finetune = mp.samples[mp.ch[i].sample].finetune;

				note_tmp = note_tmp * finetune_table[finetune & 0xF] >> 16;

				mp.ch[i].note = note_tmp;

				if(eff_tmp != 0x3 && eff_tmp != 0x5 && (eff_tmp != 0xE || (effval_tmp & 0xF0) != 0xD0)) {
					mp.ch[i].samplegen.age = mp.ch[i].samplegen.currentptr = 0;
					mp.ch[i].samplegen.period = mp.ch[i].note;

					if(mp.ch[i].vibrato.waveform < 4) mp.ch[i].vibrato.phase = 0;
					if(mp.ch[i].tremolo.waveform < 4) mp.ch[i].tremolo.phase = 0;
				}
			}

			if(eff_tmp || effval_tmp) switch(eff_tmp) {
				case 0x3:
					if(effval_tmp) mp.ch[i].slideamount = effval_tmp;

				case 0x5:
					mp.ch[i].slidenote = mp.ch[i].note;
					break;

				case 0x4:
					if(effval_tmp & 0xF0) mp.ch[i].vibrato.speed = effval_tmp >> 4;
					if(effval_tmp & 0x0F) mp.ch[i].vibrato.depth = effval_tmp & 0x0F;

					// break intentionally left out here
	
				case 0x6:
					_RecalculateWaveform(&mp.ch[i].vibrato);
					break;

				case 0x7:
					if(effval_tmp & 0xF0) mp.ch[i].tremolo.speed = effval_tmp >> 4;
					if(effval_tmp & 0x0F) mp.ch[i].tremolo.depth = effval_tmp & 0x0F;
					_RecalculateWaveform(&mp.ch[i].tremolo);
					break;

				case 0xC:
					mp.ch[i].samplegen.volume = (effval_tmp > 0x40) ? 0x40 : effval_tmp;
					break;

				case 0x9:
					if(effval_tmp) {
						mp.ch[i].samplegen.currentptr = effval_tmp << 24;
						mp.ch[i].sampleoffset = effval_tmp;
					} else {
						mp.ch[i].samplegen.currentptr = mp.ch[i].sampleoffset << 24;
					}

					mp.ch[i].samplegen.age = 0;
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

					if(effval_tmp > 0x63) effval_tmp = 0;

					mp.skiporderdestrow = (effval_tmp >> 4) * 10 + (effval_tmp & 0xF); // What were the ProTracker guys smoking?!
					break;

				case 0xE:
					switch(effval_tmp >> 4) {
						case 0x1:
							mp.ch[i].samplegen.period -= effval_tmp & 0xF;
							break;

						case 0x2:
							mp.ch[i].samplegen.period += effval_tmp & 0xF;
							break;
						
						case 0x4:
							mp.ch[i].vibrato.waveform = effval_tmp & 0x7;
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
							mp.ch[i].tremolo.waveform = effval_tmp & 0x7;
							break;

						case 0xA:
							mp.ch[i].samplegen.volume += effval_tmp & 0xF;
							if(mp.ch[i].samplegen.volume > 0x40) mp.ch[i].samplegen.volume = 0x40;
							break;

						case 0xB:
							mp.ch[i].samplegen.volume -= effval_tmp & 0xF;
							if(mp.ch[i].samplegen.volume < 0x00) mp.ch[i].samplegen.volume = 0x00;
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

			mp.ch[i].eff = eff_tmp;
			mp.ch[i].effval = effval_tmp;
		}
	}

	for(i = 0; i < mp.channels; i++) {
		int eff_tmp = mp.ch[i].eff;
		int effval_tmp = mp.ch[i].effval;

		if(eff_tmp || effval_tmp) switch(eff_tmp) {
			case 0x0:
				switch(mp.tick % 3) {
					case 0:
						mp.arp = mp.ch[i].note;
						break;

					case 1:
						mp.arp = (mp.ch[i].note * arpeggio_table[effval_tmp >> 4]) >> 16;
						break;

					case 2:
						mp.arp = (mp.ch[i].note * arpeggio_table[effval_tmp & 0xF]) >> 16;
						break;
				}

				mp.ch[i].samplegen.period = mp.arp;
				break;

			case 0x1:
				if(mp.tick) mp.ch[i].samplegen.period -= effval_tmp;
				break;

			case 0x2:
				if(mp.tick) mp.ch[i].samplegen.period += effval_tmp;
				break;

			case 0x5:
				if(mp.tick) {
					if(effval_tmp > 0xF) {
						mp.ch[i].samplegen.volume += (effval_tmp >> 4);
						if(mp.ch[i].samplegen.volume > 0x40) mp.ch[i].samplegen.volume = 0x40;
					} else {
						mp.ch[i].samplegen.volume -= (effval_tmp & 0xF);
						if(mp.ch[i].samplegen.volume < 0x00) mp.ch[i].samplegen.volume = 0x00;
					}
				}
				
				effval_tmp = 0;
				// break intentionally left out here

			case 0x3:
				if(mp.tick) {
					if(!effval_tmp) effval_tmp = mp.ch[i].slideamount;

					if(mp.ch[i].slidenote > mp.ch[i].samplegen.period) {
						mp.ch[i].samplegen.period += effval_tmp;

						if(mp.ch[i].slidenote < mp.ch[i].samplegen.period)
							mp.ch[i].samplegen.period = mp.ch[i].slidenote;
					} else if(mp.ch[i].slidenote < mp.ch[i].samplegen.period) {
						mp.ch[i].samplegen.period -= effval_tmp;

						if(mp.ch[i].slidenote > mp.ch[i].samplegen.period)
							mp.ch[i].samplegen.period = mp.ch[i].slidenote;
					} 
				}

				break;

			case 0x4:
				if(mp.tick) {
					mp.ch[i].vibrato.phase += mp.ch[i].vibrato.speed;
					_RecalculateWaveform(&mp.ch[i].vibrato);
				}
				break;

			case 0x6:
				if(mp.tick) {
					mp.ch[i].vibrato.phase += mp.ch[i].vibrato.speed;
					_RecalculateWaveform(&mp.ch[i].vibrato);
				}
				// break intentionally left out here

			case 0xA:
				if(mp.tick) {
					if(effval_tmp > 0xF) {
						mp.ch[i].samplegen.volume += (effval_tmp >> 4);
						if(mp.ch[i].samplegen.volume > 0x40) mp.ch[i].samplegen.volume = 0x40;
					} else {
						mp.ch[i].samplegen.volume -= (effval_tmp & 0xF);
						if(mp.ch[i].samplegen.volume < 0x00) mp.ch[i].samplegen.volume = 0x00;
					}
				}

				break;

			case 0x7:
				if(mp.tick) {
					mp.ch[i].tremolo.phase += mp.ch[i].tremolo.speed;
					_RecalculateWaveform(&mp.ch[i].tremolo);
				}
				break;

			case 0xE:
				switch(effval_tmp >> 4) {
					case 0x9:
						if(mp.tick && !(mp.tick % (effval_tmp & 0xF))) mp.ch[i].samplegen.age = mp.ch[i].samplegen.currentptr = 0;
						break;

					case 0xC:
						if(mp.tick >= (effval_tmp & 0xF)) mp.ch[i].samplegen.volume = 0;
						break;

					case 0xD:
						if(mp.tick == (effval_tmp & 0xF)) {
							mp.ch[i].samplegen.age = mp.ch[i].samplegen.currentptr = 0;
							mp.ch[i].samplegen.period = mp.ch[i].note;
						}
						break;
				}

				break;
		}

		if(mp.ch[i].samplegen.period < 0 && mp.ch[i].samplegen.period != 0) {
			mp.ch[i].samplegen.period = 0;
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

	int majorchmul = 131072 / (mp.channels / 2);
	int minorchmul = 131072 / 3 / (mp.channels / 2);

	for(int s = 0; s < len; s++) {
		// Process the tick, if necessary

		if(!mp.audiotick) {
			ProcessMOD();
			mp.audiotick = mp.audiospeed;
		}

		mp.audiotick--;

		// Render the audio

		for(int ch = 0; ch < mp.channels; ch++) {
			if(mp.ch[ch].samplegen.sample) {
				if(!mp.ch[ch].samplegen.muted) {
					int vol = mp.ch[ch].samplegen.volume + (mp.ch[ch].tremolo.val >> 6);

					if(vol < 0) vol = 0;
					if(vol > 64) vol = 64;

#ifdef USE_LINEAR_INTERPOLATION
					uint32_t nextptr = mp.ch[ch].samplegen.currentptr + 0x10000;
					
					if((nextptr >> 17) >= mp.ch[ch].samplegen.length &&
						mp.ch[ch].samplegen.looplength != 0)

						nextptr -= mp.ch[ch].samplegen.looplength << 17;

					int sample1 = mp.ch[ch].samplegen.sample[mp.ch[ch].samplegen.currentptr >> 16] * vol;
					int sample2 = mp.ch[ch].samplegen.sample[nextptr >> 16] * vol;

					short sample = (sample1 * (0x10000 - (nextptr & 0xFFFF)) +
						  sample2 * (nextptr & 0xFFFF)) >> 16;
#else
					short sample = mp.ch[ch].samplegen.sample[mp.ch[ch].samplegen.currentptr >> 16] * vol;
#endif

					// Distribute the rendered sample across both output channels

					if((ch & 3) == 1 || (ch & 3) == 2) {
						buf[s * 2] += sample * minorchmul / 65536;
						buf[s * 2 + 1] += sample * majorchmul / 65536;
					} else {
						buf[s * 2] += sample * majorchmul / 65536;
						buf[s * 2 + 1] += sample * minorchmul / 65536;
					}
				}

				// Advance to the next required sample

				if(mp.ch[ch].samplegen.period)
					mp.ch[ch].samplegen.currentptr += (mp.paularate << 16) / (((uint32_t) mp.ch[ch].samplegen.period) + (mp.ch[ch].vibrato.val >> 7));

				// Stop this channel if we have reached the end or loop it, if desired

				if((mp.ch[ch].samplegen.currentptr >> 17) >= mp.ch[ch].samplegen.length) {
					if(mp.ch[ch].samplegen.looplength == 0) {
						mp.ch[ch].samplegen.period = 0;
					} else {
						mp.ch[ch].samplegen.currentptr -= mp.ch[ch].samplegen.looplength << 17;
					}
				} else {
					if(mp.ch[ch].samplegen.age < INT32_MAX)
					mp.ch[ch].samplegen.age++;
				}
			}
		}
	}

	return &mp;
}

ModPlayerStatus_t *InitMOD(const uint8_t *mod, int samplerate) {
	uint32_t signature = mod[1083] | (mod[1082] << 8) | (mod[1081] << 16) | (mod[1080] << 24);

	int channels = 0;

	// M.K. and M!K! = 4 channels

	if(signature == 0x4D2E4B2E || signature == 0x4D214B21) {
		channels = 4;
	}

	// FLTx

	if((signature & 0xFFFFFF00) == 0x464C5400) {
		channels = (signature & 0xFF) - '0';
	}

	// xCHN

	if((signature & 0x00FFFFFF) == 0x0043484E) {
		channels = (signature >> 24) - '0';
	}

	// xxCH

	if((signature & 0x0000FFFF) == 0x00004348) {
		channels = ((signature >> 24) & 0xF) * 10 + ((signature >> 16) & 0xF);
	}

	if(channels == 0 || channels >= CHANNELS) {
		return NULL;
	}

	memset(&mp, 0, sizeof(mp));

	mp.channels = channels;

	mp.samplerate = samplerate;
	mp.paularate = 3546895 / samplerate;

	mp.orders = mod[950];
	mp.ordertable = mod + 952;

	mp.maxpattern = 0;

	for(int i = 0; i < 128; i++) {
		if(mp.ordertable[i] >= mp.maxpattern) mp.maxpattern = mp.ordertable[i];
	}
	mp.maxpattern++;

	const int8_t *samplemem = mod + 1084 + 64 * 4 * mp.channels * mp.maxpattern;
	mp.patterndata = mod + 1084;

	for(int i = 0; i < 31; i++) {
		const uint8_t *sample = mod + 20 + i * 30;

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

	mp.maxtick = mp.speed = 6; mp.audiospeed = mp.samplerate / 50;

	for(int i = 0; i < mp.channels; i++) {
		mp.ch[i].samplegen.age = INT32_MAX;
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

	for(int i = 0; i < mp.channels; i++) {
		mp.ch[i].samplegen.age = INT32_MAX;
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
