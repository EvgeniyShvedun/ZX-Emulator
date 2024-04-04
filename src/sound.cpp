#include <math.h>
#include "base.h"

Sound::Sound(int sample_rate, MODE mode, float ay_volume, float speaker_volume, float tape_volume) : sample_rate(sample_rate), mode(mode) {
    buffer = new unsigned short[sample_rate * 2];
    cpu_factor = sample_rate / (float)Z80_FREQ;
    increment = AY_RATE / (float)sample_rate;
    set_stereo_levels(MONO);
    set_stereo_levels(ABC);
    set_stereo_levels(ACB);
    set_ay_volume(ay_volume);
    set_speaker_volume(speaker_volume);
    set_tape_volume(tape_volume);
	reset();
}

void Sound::set_stereo_levels(MODE mixer_mode, float side, float center, float oposite){
    switch(mixer_mode){
        case ACB:
            mixer[ACB][LEFT][A] = side;
            mixer[ACB][LEFT][B] = center;
            mixer[ACB][LEFT][C] = oposite;
            mixer[ACB][RIGHT][A] = oposite;
            mixer[ACB][RIGHT][B] = center;
            mixer[ACB][RIGHT][C] = side;
            break;
        case ABC:
	        mixer[ABC][LEFT][A] = side;
	        mixer[ABC][LEFT][B] = center;
	        mixer[ABC][LEFT][C] = oposite;
    	    mixer[ABC][RIGHT][A] = oposite;
	        mixer[ABC][RIGHT][B] = center;
	        mixer[ABC][RIGHT][C] = side;
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
	DELETE_ARRAY(buffer);
}

void Sound::set_ay_volume(float volume){
    ay_volume = volume;
}

void Sound::set_speaker_volume(float volume){
	speaker_volume = volume;
}

void Sound::set_tape_volume(float volume){
    tape_volume = volume;
}

void Sound::setup_lpf(int rate){
    float RC = 1.0 / (rate * 2 * M_PI);
    float dt = 1.0 / sample_rate;
    lpf_alpha = dt / (RC + dt);
}

void Sound::update(int clk){
	for (int now = clk * cpu_factor; frame_idx < now; frame_idx++){
		tone_a_counter += increment;
		if (tone_a_counter >= tone_a_limit){
			tone_a_counter -= tone_a_limit;
			tone_a ^= -1;
		}
		tone_b_counter += increment;
		if (tone_b_counter >= tone_b_limit){
			tone_b_counter -= tone_b_limit;
			tone_b ^= -1;
		}
		tone_c_counter += increment;
		if (tone_c_counter >= tone_c_limit){
			tone_c_counter -= tone_c_limit;
			tone_c ^= -1;
		}
		noise_counter += increment;
		if (noise_counter >= noise_limit){
			noise_counter -= noise_limit;
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
		envelope_counter += increment;
		if (envelope_counter >= envelope_limit){
			envelope_counter -= envelope_limit;
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

        float left = 0.0f, right = 0.0f;
		if ((tone_a | (registers[MIX] & 0x01)) && (noise | (registers[MIX] & 0x08))){
        	float vol = registers[A_VOL] & 0x10 ? ay_volume_table[envelope] : ay_volume_table[registers[A_VOL] & 0x0F];
        	left += vol * mixer[mode][LEFT][A];
        	right += vol * mixer[mode][RIGHT][A];
		}
		if ((tone_b | (registers[MIX] & 0x02)) && (noise | (registers[MIX] & 0x10))){
        	float vol = registers[B_VOL] & 0x10 ? ay_volume_table[envelope] : ay_volume_table[registers[B_VOL] & 0x0F];
        	left += vol * mixer[mode][LEFT][B];
        	right += vol * mixer[mode][RIGHT][B];
		}
		if ((tone_c | (registers[MIX] & 0x04)) && (noise | (registers[MIX] & 0x20))){
        	float vol = registers[C_VOL] & 0x10 ? ay_volume_table[envelope] : ay_volume_table[registers[C_VOL] & 0x0F];
        	left += vol * mixer[mode][LEFT][C];
        	right += vol * mixer[mode][RIGHT][C];
		}
        left_amp += lpf_alpha * ((left + speaker_amp + tape_in_amp + tape_out_amp) / 8.0f - left_amp);
        right_amp += lpf_alpha * ((right + speaker_amp + tape_in_amp + tape_out_amp) / 8.0f - right_amp);
        buffer[frame_idx * 2] = 0x8000 * left_amp;
        buffer[frame_idx * 2 + 1] = 0x8000 * right_amp;
	}
}

bool Sound::io_wr(unsigned short port, unsigned char byte, int clk){
	if (!(port & 0x01)){
        if ((pwFE ^ byte) & (SPEAKER | TAPE_OUT)){
            update(clk);
            if ((pwFE ^ byte) & SPEAKER)
                speaker_amp = speaker_amp ? 0.0f : speaker_volume;
            if ((pwFE ^ byte) & TAPE_OUT)
                tape_out_amp = tape_out_amp ? 0.0f : tape_volume;
            pwFE = byte;
            return true;
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
						tone_a_limit = registers[A_PCH_H] << 0x08 | registers[A_PCH_L];
						break;
					case B_PCH_H:
                        registers[B_PCH_H] &= 0x0F;
                    case B_PCH_L:
						tone_b_limit = registers[B_PCH_H] << 0x08 | registers[B_PCH_L];
						break;
					case C_PCH_H:
                        registers[C_PCH_H] &= 0x0F;
					case C_PCH_L:
						tone_c_limit = registers[C_PCH_H] << 0x08 | registers[C_PCH_L];
						break;
					case N_PCH:
						noise_limit = registers[N_PCH] & 0x1F;// * 2;
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
						envelope_limit = registers[ENV_H] << 0x8 | registers[ENV_L];
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
            tape_in_amp = tape_in_amp ? 0.0f : tape_volume;
            prFE = *p_byte;
            return true;
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
    tone_a_counter = tone_b_counter = tone_c_counter = noise_counter = envelope_counter = 0;
    tone_a_limit = tone_b_limit = tone_c_limit = noise_limit = envelope_limit = 0xFFF;
	noise_seed = 12345;
    envelope_idx = 0;
    for (pwFFFD = 0; pwFFFD < 0x10; pwFFFD++)
        registers[pwFFFD] = 0x00;
    pwFFFD = prFE = pwFE = 0x00;
	left_amp = right_amp = speaker_amp = tape_in_amp = tape_out_amp = 0.0f;
	frame_idx = 0;
}

void Sound::frame(int frame_clk){
	update(frame_clk);
	frame_idx = 0;
}
