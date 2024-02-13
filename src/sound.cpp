#include <math.h>
#include "base.h"

Sound::Sound(int sample_rate, MODE mode, float ay_volume, float speaker_volume, float tape_volume) : sample_rate(sample_rate), mode(mode) {
	p_sound = new unsigned short[sample_rate * 2]();
    cpu_factor = sample_rate / (float)Z80_FREQ;
    increment = AY_RATE / (float)sample_rate;
    setup_lpf(22050);
    set_stereo_levels(MONO);
    set_stereo_levels(ABC);
    set_stereo_levels(ACB);
    set_ay_volume(ay_volume);
    set_speaker_volume(speaker_volume);
    set_tape_volume(tape_volume);
	reset();
}

void Sound::set_stereo_levels(MODE mixer_mode, float directed, float center, float oposite){
    switch(mixer_mode){
        case ACB:
            mixer[ACB][LEFT][A] = directed;
            mixer[ACB][LEFT][B] = center;
            mixer[ACB][LEFT][C] = oposite;
            mixer[ACB][RIGHT][A] = oposite;
            mixer[ACB][RIGHT][B] = center;
            mixer[ACB][RIGHT][C] = directed;
            break;
        case ABC:
	        mixer[ABC][LEFT][A] = directed;
	        mixer[ABC][LEFT][B] = center;
	        mixer[ABC][LEFT][C] = oposite;
    	    mixer[ABC][RIGHT][A] = oposite;
	        mixer[ABC][RIGHT][B] = center;
	        mixer[ABC][RIGHT][C] = directed;
            break;
        case MONO:
	        mixer[MONO][LEFT][A] = 1.0;
	        mixer[MONO][LEFT][B] = 1.0;
	        mixer[MONO][LEFT][C] = 1.0;
	        mixer[MONO][RIGHT][A] = 1.0;
	        mixer[MONO][RIGHT][B] = 1.0;
	        mixer[MONO][RIGHT][C] = 1.0;
            break;
    }
}

Sound::~Sound(){
	DELETE_ARRAY(p_sound);
}

void Sound::set_ay_volume(float volume){
	for (int i = 0; i < 0x10; i++)
		volume_table[i] = 0xFFFF * volume_level[i] * volume;
}

void Sound::set_speaker_volume(float volume){
	speaker_volume = 0xFFFF * volume;
}

void Sound::set_tape_volume(float volume){
	tape_volume = 0xFFFF * volume;
}

void Sound::setup_lpf(int cut_rate){
    float RC = 1.0 / (cut_rate * 2 * M_PI);
    float dt = 1.0 / sample_rate;
    float alpha = dt / (RC + dt);
    lpf = alpha * (1U << FRACT_BITS);
}

void Sound::update(int clk){
	for (int now = clk * cpu_factor; frame_idx < now; frame_idx++){
		tone_a_cnt += increment;
		if (tone_a_cnt >= tone_a_max){
			tone_a_cnt -= tone_a_max;
			tone_a ^= -1;
		}
		tone_b_cnt += increment;
		if (tone_b_cnt >= tone_b_max){
			tone_b_cnt -= tone_b_max;
			tone_b ^= -1;
		}
		tone_c_cnt += increment;
		if (tone_c_cnt >= tone_c_max){
			tone_c_cnt -= tone_c_max;
			tone_c ^= -1;
		}
		noise_cnt += increment;
		if (noise_cnt >= noise_max){
			noise_cnt -= noise_max;
			/* The input to the shift registersister is bit0 XOR bit3 (bit0 is the output).
 			This was verified on AY-3-8910 and YM2149 chips.*/
			/*
            noise_seed >>= 1;
			noise = noise_seed & 0x01 ? 0x00 : 0xFF;
            */
			/* The Random Number Generator of the 8910 is a 17-bit shift registersister.
			 Hacker KAY & Sergey Bulba*/
			/*
            noise_seed = ((noise_seed << 1) + 1) ^ (((noise_seed >> 16) ^ (noise_seed >> 13)) & 0x01);
			noise = (noise_seed >> 16) & 0x01 ? 0x00 : 0xFF;
            */
			if ((noise_seed & 0x01) ^ ((noise_seed & 0x02) >> 1))
				noise ^= -1;
			if (noise_seed & 0x01)
				noise_seed ^= 0x24000;
			noise_seed >>= 1;
		}
		envelope_cnt += increment;
		if (envelope_cnt >= envelope_max){
			envelope_cnt -= envelope_max;
			if (envelope_idx < 0x1F)
            	envelope_idx++;
            else{
				if (registers[ENV_SHAPE] < 4 || registers[ENV_SHAPE] & 0x01)
                	envelope_idx = 0x10;
            	else
                	envelope_idx = 0x00;
            }
			envelope = envelope_shape[registers[ENV_SHAPE] * 0x20 + envelope_idx];
		}

        unsigned int volume, left = 0, right = 0;
		if ((tone_a | (registers[MIX] & 0x01)) && (noise | (registers[MIX] & 0x08))){
        	volume = registers[A_VOL] & 0x10 ? volume_table[envelope] : volume_table[registers[A_VOL] & 0x0F];
        	left  += volume * mixer[mode][LEFT][A];
        	right += volume * mixer[mode][RIGHT][A];
		}
		if ((tone_b | (registers[MIX] & 0x02)) && (noise | (registers[MIX] & 0x10))){
        	volume = registers[B_VOL] & 0x10 ? volume_table[envelope] : volume_table[registers[B_VOL] & 0x0F];
        	left += volume * mixer[mode][LEFT][B];
        	right += volume * mixer[mode][RIGHT][B];
		}
		if ((tone_c | (registers[MIX] & 0x04)) && (noise | (registers[MIX] & 0x20))){
        	volume = registers[C_VOL] & 0x10 ? volume_table[envelope] : volume_table[registers[C_VOL] & 0x0F];
        	left += volume * mixer[mode][LEFT][C];
        	right += volume * mixer[mode][RIGHT][C];
		}
        left = (left + speaker_out + tape_in + tape_out) >> 4;
        right = (right + speaker_out + tape_in + tape_out) >> 4;
        output_left += lpf * (left - (output_left >> FRACT_BITS));
        output_right += lpf * (right - (output_right >> FRACT_BITS));
        p_sound[frame_idx * 2] = output_left >> FRACT_BITS;
        p_sound[frame_idx * 2 + 1] = output_right >> FRACT_BITS;
	}
}

bool Sound::io_wr(unsigned short port, unsigned char byte, int clk){
	if (!(port & 0x01)){
        unsigned char bits = pwFE ^ byte;
        if (bits & (SPEAKER | TAPE_OUT)){
            update(clk);
            if (bits & SPEAKER)
                speaker_out ^= speaker_volume;
            if (bits & TAPE_OUT)
           	    tape_out ^= tape_volume;
            pwFE = byte;
		}
    }else{
        if ((port & 0xC002) == 0xC000){
            pwFFFD = byte & 0x0F;
            return true;
        }else{
            if ((port & 0xC002) == 0x8000){
                update(clk);
                registers[pwFFFD] = byte;
				switch(pwFFFD){
					case A_PCH_H:
                        registers[A_PCH_H] &= 0x0F;
					case A_PCH_L:
						tone_a_max = registers[A_PCH_H] << 0x08 | registers[A_PCH_L];
						break;
					case B_PCH_H:
                        registers[B_PCH_H] &= 0x0F;
                    case B_PCH_L:
						tone_b_max = registers[B_PCH_H] << 0x08 | registers[B_PCH_L];
						break;
					case C_PCH_H:
                        registers[C_PCH_H] &= 0x0F;
					case C_PCH_L:
						tone_c_max = registers[C_PCH_H] << 0x08 | registers[C_PCH_L];
						break;
					case N_PCH:
						noise_max = registers[N_PCH] & 0x1F;// * 2;
						break;
                    case MIX: // bit (0 - 7/5?)
						break;
					case A_VOL:
						registers[A_VOL] &= 0x1F;
						break;
					case B_VOL:
						registers[B_VOL] &= 0x1F;
						break;
					case C_VOL:
						registers[C_VOL] &= 0x1F;
						break;
					case ENV_L:
					case ENV_H:
						envelope_max = registers[ENV_H] << 0x8 | registers[ENV_L];
						break;
					case ENV_SHAPE:
						registers[ENV_SHAPE] &= 0x0F;
						envelope = envelope_shape[registers[ENV_SHAPE] * 0x20];
						envelope_idx = 0x00;
						break;
					case PORT_A:
                        break;
					case PORT_B:
						break;
				}
                return true;
			}
		}
	}
    return false;
}

bool Sound::io_rd(unsigned short port, unsigned char *p_byte, int clk){
    if (!(port & 0x01)){
        if ((prFE ^ *p_byte) & TAPE_IN){
            update(clk);
            tape_in ^= tape_volume;
            prFE = *p_byte;
        }
    }
    if ((port & 0xC003) == 0xC001){
		*p_byte &= registers[pwFFFD];
        return true;
    }
    return false;
}

void Sound::reset(){
	tone_a = tone_b = tone_c = noise = envelope = 0;
    tone_a_cnt = tone_b_cnt = tone_c_cnt = noise_cnt = envelope_cnt = 0;
    tone_a_max = tone_b_max = tone_c_max = noise_max = envelope_max = 0xFFF;
	noise_seed = 12345;
    envelope_idx = 0;
    speaker_out = tape_in = tape_out = 0;
	output_left = output_right = 0;
    for (pwFFFD = 0; pwFFFD < 0x10; pwFFFD++)
        registers[pwFFFD] = 0x0;
    pwFFFD = prFE = pwFE = 0;
	frame_idx = 0;
}
void Sound::frame(int frame_clk){
	update(frame_clk);
	frame_idx = 0;
}
