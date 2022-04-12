#include "modplay.h"
#include "tables.h"
#include <string.h>

ModPlayerStatus_t mp;

ModPlayerStatus_t *ProcessMOD() {
	int i;

	if(mp.tick == 0) {
		mp.skiporderrequest = -1;

		for(i = 0; i < 4; i++) {
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

				if(eff_tmp != 0x3 && eff_tmp != 5) {
					mp.paula[i].age = mp.paula[i].currentptr = 0;
					mp.paula[i].period = mp.note[i] = note_tmp;
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

				case 0x5:
					mp.slidenote[i] = mp.note[i];
					break;

				case 0xC:
					mp.paula[i].volume = (effval_tmp > 0x40) ? 0x40 : effval_tmp;
					break;

				case 0x9:
					mp.paula[i].currentptr = effval_tmp << 24;
					mp.paula[i].age = 0;
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
						mp.arp = (mp.note[i] * arpeggio_table[effval_tmp >> 4]) >> 16;
						break;

					case 2:
						mp.arp = (mp.note[i] * arpeggio_table[effval_tmp & 0xF]) >> 16;
						break;
				}

				mp.paula[i].period = mp.arp;
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
						if(mp.tick && !(mp.tick % (effval_tmp & 0xF))) mp.paula[i].age = mp.paula[i].currentptr = 0;
						break;

					case 0xC:
						if(mp.tick >= (effval_tmp & 0xF)) mp.paula[i].volume = 0;
						break;
				}

				break;
		}

		if(mp.paula[i].period < period_tables[0][3*12-1]) {
			mp.paula[i].period = period_tables[0][3*12-1];
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

				if(!mp.paula[ch].muted) {
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
					mp.paula[ch].currentptr += (mp.paularate << 16) / ((uint32_t) mp.paula[ch].period);

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

	mp.speed = 6; mp.audiospeed = mp.samplerate / 50; mp.audiotick = 0;

	for(int i = 0; i < 4; i++) {
		mp.paula[i].age = INT32_MAX;
	}

	return &mp;
}
