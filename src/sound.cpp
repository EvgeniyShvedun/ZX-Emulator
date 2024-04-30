#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include "types.h"
#include "utils.h"
#include "config.h"
#include "device.h"
#include "memory.h"
#include "ula.h"
#include "z80.h"
#include "sound.h"
//#include <math.h>

#define BUFFER_TIME_SEC 4

void Sound::init(int sample_rate, int frame_clk){
    this->sample_rate = sample_rate;
    cpu_factor = sample_rate / (float)Z80_FREQ;
    increment = AY_RATE / (float)sample_rate;
    reset();
    if (audio_device_id)
        SDL_CloseAudioDevice(audio_device_id);
    DELETE_ARRAY(buffer);
    buffer = new short[sample_rate * BUFFER_TIME_SEC];
    memset(buffer, 0, sample_rate * sizeof(short) * BUFFER_TIME_SEC);
    SDL_zero(audio_spec);
    frame_samples = frame_clk * (sample_rate / (float)Z80_FREQ);
    audio_spec.freq = sample_rate;
    audio_spec.format = AUDIO_S16;
    audio_spec.channels = 2;
    audio_spec.samples = frame_samples * 2;
    audio_spec.callback = NULL;
    audio_device_id = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    if (!audio_device_id)
        throw std::runtime_error("SDL: Open audio device");
    SDL_QueueAudio(audio_device_id, buffer, audio_spec.samples * 2 * 4);
    SDL_PauseAudioDevice(audio_device_id, 0);
}

void Sound::set_mixer_levels(float side, float center, float opposite){
    switch(mixer_mode){
        case AY_ABC:
            if (side >= 0.0){
                mixer[mixer_mode][LEFT][A] = side;
                mixer[mixer_mode][RIGHT][C] = side;
            }
            if (center >= 0.0){
                mixer[mixer_mode][LEFT][B] = center;
                mixer[mixer_mode][RIGHT][B] = center;
            }
            if (opposite >= 0.0){
                mixer[mixer_mode][LEFT][C] = opposite;
                mixer[mixer_mode][RIGHT][A] = opposite;
            }
            break;
        case AY_ACB:
            if (side >= 0.0){
                mixer[mixer_mode][LEFT][A] = side;
                mixer[mixer_mode][RIGHT][B] = side;
            }
            if (center >= 0.0){
                mixer[mixer_mode][LEFT][C] = center;
                mixer[mixer_mode][RIGHT][C] = center;
            }
            if (opposite >= 0.0){
                mixer[mixer_mode][LEFT][B] = opposite;
                mixer[mixer_mode][RIGHT][A] = opposite;
            }
            break;
        case AY_MONO:
            mixer[mixer_mode][LEFT][A] = 1.0;
            mixer[mixer_mode][LEFT][B] = 1.0;
            mixer[mixer_mode][LEFT][C] = 1.0;
            mixer[mixer_mode][RIGHT][A] = 1.0;
            mixer[mixer_mode][RIGHT][B] = 1.0;
            mixer[mixer_mode][RIGHT][C] = 1.0;
            break;
    }
}

Sound::~Sound(){
    DELETE_ARRAY(buffer);
    if (audio_device_id)
        SDL_CloseAudioDevice(audio_device_id);
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
    //if (!buffer)
    //    return;
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
            left += vol * mixer[mixer_mode][LEFT][A];
            right += vol * mixer[mixer_mode][RIGHT][A];
        }
        if ((tone_b | (registers[MIX] & 0x02)) && (noise | (registers[MIX] & 0x10))){
            float vol = registers[B_VOL] & 0x10 ? ay_volume_table[envelope] : ay_volume_table[registers[B_VOL] & 0x0F];
            left += vol * mixer[mixer_mode][LEFT][B];
            right += vol * mixer[mixer_mode][RIGHT][B];
        }
        if ((tone_c | (registers[MIX] & 0x04)) && (noise | (registers[MIX] & 0x20))){
            float vol = registers[C_VOL] & 0x10 ? ay_volume_table[envelope] : ay_volume_table[registers[C_VOL] & 0x0F];
            left += vol * mixer[mixer_mode][LEFT][C];
            right += vol * mixer[mixer_mode][RIGHT][C];
        }
        left = (left / 3.0f * ay_volume + speaker_amp + tape_in_amp + tape_out_amp) / 4.0f;
        right = (right / 3.0f * ay_volume + speaker_amp + tape_in_amp + tape_out_amp) / 4.0f;
        left_out += lpf_alpha * (left - left_out);
        right_out += lpf_alpha * (right - right_out);
        buffer[frame_idx * 2] = 0x8000 * left_out;
        buffer[frame_idx * 2 + 1] = 0x8000 * right_out;
    }
}

void Sound::write(u16 port, u8 byte, s32 clk){
    if (!(port & 0x01)){
        if ((port_wFE ^ byte) & (SPEAKER | TAPE_OUT)){
            update(clk);
            if ((port_wFE ^ byte) & SPEAKER)
                speaker_amp = speaker_amp ? 0.0f : speaker_volume;
            if ((port_wFE ^ byte) & TAPE_OUT)
                tape_out_amp = tape_out_amp ? 0.0f : tape_volume;
            port_wFE = byte;
        }
    }else{
        if ((port & 0xC002) == 0xC000){
            port_wFFFD = byte & 0x0F;
        }else{
            if ((port & 0xC002) == 0x8000){
                update(clk);
                registers[port_wFFFD] = byte;
                switch(port_wFFFD){
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
            }
        }
    }
}

void Sound::read(u16 port, u8 *byte, s32 clk){
    if (!(port & 0x01)){
        if ((port_rFE ^ *byte) & TAPE_IN){
            update(clk);
            tape_in_amp = tape_in_amp ? 0.0f : tape_volume;
            port_rFE = *byte;
        }
    }
    if ((port & 0xC003) == 0xC001)
        *byte &= registers[port_wFFFD];
}

void Sound::reset(){
    tone_a = tone_b = tone_c = noise = envelope = 0;
    tone_a_counter = tone_b_counter = tone_c_counter = noise_counter = envelope_counter = 0;
    tone_a_limit = tone_b_limit = tone_c_limit = noise_limit = envelope_limit = 0xFFF;
    noise_seed = 12345;
    envelope_idx = 0;
    for (port_wFFFD = 0; port_wFFFD < 0x10; port_wFFFD++)
        registers[port_wFFFD] = 0x00;
    port_wFFFD = port_rFE = port_wFE = 0x00;
    left_out = right_out = 0.0f;
    speaker_amp = tape_in_amp = tape_out_amp = 0.0f;
    frame_idx = 0;
}

void Sound::frame(int frame_clk){
    update(frame_clk);
    frame_idx = 0;
}

void Sound::pause(bool state){
    if (audio_device_id)
        SDL_PauseAudioDevice(audio_device_id, state ? 1 : 0);
}

void Sound::queue(){
    if (SDL_GetAudioDeviceStatus(audio_device_id) != SDL_AUDIO_PLAYING)
        return;
    while (SDL_GetQueuedAudioSize(audio_device_id) > (audio_spec.samples - frame_samples) * 4)
        SDL_Delay(1);
    SDL_QueueAudio(audio_device_id, buffer, frame_samples * 4);
}
