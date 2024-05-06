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

#define BUFFER_TIME_SEC 4
SDL_AudioSpec audio_spec;
SDL_AudioDeviceID audio_device_id = 0;

Sound::Sound(){
    reset();
}

void Sound::setup(int sample_rate, int lpf_rate, int frame_clk){
    factor = sample_rate / (float)Z80_FREQ;
    increment = AY_RATE / (float)sample_rate;
    set_lpf(sample_rate, lpf_rate);
    if (audio_device_id)
        SDL_CloseAudioDevice(audio_device_id);
    DELETE_ARRAY(buffer);
    buffer = new short[sample_rate*4];
    memset(buffer, 0, sample_rate*sizeof(short)*4);
    frame_samples = frame_clk * (sample_rate / (float)Z80_FREQ);
    SDL_zero(audio_spec);
    audio_spec.freq = sample_rate;
    audio_spec.format = AUDIO_S16;
    audio_spec.channels = 2;
    audio_spec.samples = frame_samples * 2;
    audio_spec.callback = NULL;
    audio_device_id = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    if (!audio_device_id)
        throw std::runtime_error("SDL: Open audio device");
    SDL_QueueAudio(audio_device_id, buffer, audio_spec.samples*2*4);
    SDL_PauseAudioDevice(audio_device_id, 0);
}

void Sound::set_lpf(int sample_rate, int lpf_rate){
    float RC = 1.0 / (sample_rate * 2 * M_PI);
    float dt = 1.0 / sample_rate;
    lpf_alpha = dt / (RC + dt);
}

void Sound::set_mixer(AY_Mixer mode, float side, float center, float penetr){
    mixer_mode = mode;
    switch(mode){
        case ABC:
            mixer[mode][Left][A] = side;
            mixer[mode][Right][C] = side;
            mixer[mode][Left][B] = center;
            mixer[mode][Right][B] = center;
            mixer[mode][Left][C] = penetr;
            mixer[mode][Right][A] = penetr;
            break;
        case ACB:
            mixer[mode][Left][A] = side;
            mixer[mode][Right][B] = side;
            mixer[mode][Left][C] = center;
            mixer[mode][Right][C] = center;
            mixer[mode][Left][B] = penetr;
            mixer[mode][Right][A] = penetr;
            break;
        case Mono:
            mixer[mode][Left][A] = 1.0;
            mixer[mode][Left][B] = 1.0;
            mixer[mode][Left][C] = 1.0;
            mixer[mode][Right][A] = 1.0;
            mixer[mode][Right][B] = 1.0;
            mixer[mode][Right][C] = 1.0;
            break;
    }
}

Sound::~Sound(){
    DELETE_ARRAY(buffer);
    if (audio_device_id)
        SDL_CloseAudioDevice(audio_device_id);
}

void Sound::update(int clk){
    if (!buffer)
        return;
    float square = (
        (port_wFE & SPEAKER ? speaker_volume : 0.0) +
        (port_wFE & TAPE_OUT ? tape_volume : 0.0f) +
        (port_rFE & TAPE_IN ? tape_volume : 0.0f)) / 3;
    for (int now = clk * factor; frame_pos < now; frame_pos++){
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
            /*    The input to the shift registersister is bit0 XOR bit3 (bit0 is the output).
                This was verified on AY-3-8910 and YM2149 chips.
            noise_seed >>= 1;
            noise = noise_seed & 0x01 ? 0x00 : 0xFF; */
            /*    The Random Number Generator of the 8910 is a 17-bit shift registersister.
                Hacker KAY & Sergey Bulba
            noise_seed = ((noise_seed << 1) + 1) ^ (((noise_seed >> 16) ^ (noise_seed >> 13)) & 0x01);
            noise = (noise_seed >> 16) & 0x01 ? 0x00 : 0xFF; */
            if ((noise_seed & 0x01) ^ ((noise_seed & 0x02) >> 1))
                noise ^= -1;
            if (noise_seed & 0x01)
                noise_seed ^= 0x24000;
            noise_seed >>= 1;
        }
        if (envelope_counter += increment >= envelope_limit){
            envelope_counter -= envelope_limit;
            if (++envelope_pos >= 0x20)
                envelope_pos = (registers[ENV_SHAPE] < 4 || registers[ENV_SHAPE] & 0x01) ? 0x10 : 0x00;
            envelope = envelope_shape[registers[ENV_SHAPE]*0x20 + envelope_pos];
        }
        float ay_left = 0.0f, ay_right = 0.0f;
        if ((tone_a | (registers[MIX] & 0x01)) && (noise | (registers[MIX] & 0x08))){
            float vol = registers[A_VOL] & 0x10 ? ay_volume_table[envelope] : ay_volume_table[registers[A_VOL] & 0x0F];
            ay_left += vol * mixer[mixer_mode][Left][A];
            ay_right += vol * mixer[mixer_mode][Right][A];
        }
        if ((tone_b | (registers[MIX] & 0x02)) && (noise | (registers[MIX] & 0x10))){
            float vol = registers[B_VOL] & 0x10 ? ay_volume_table[envelope] : ay_volume_table[registers[B_VOL] & 0x0F];
            ay_left += vol * mixer[mixer_mode][Left][B];
            ay_right += vol * mixer[mixer_mode][Right][B];
        }
        if ((tone_c | (registers[MIX] & 0x04)) && (noise | (registers[MIX] & 0x20))){
            float vol = registers[C_VOL] & 0x10 ? ay_volume_table[envelope] : ay_volume_table[registers[C_VOL] & 0x0F];
            ay_left += vol * mixer[mixer_mode][Left][C];
            ay_right += vol * mixer[mixer_mode][Right][C];
        }
        left += lpf_alpha * (((ay_left / 3.0f * ay_volume + square) / 2.0f) - left);
        right += lpf_alpha * (((ay_right / 3.0f * ay_volume + square) / 2.0f) - right);
        buffer[frame_pos * 2] = 0x8000 * left;
        buffer[frame_pos * 2 + 1] = 0x8000 * right;
    }
}

void Sound::write(u16 port, u8 byte, s32 clk){
    if (!(port & 0x01)){
        if ((port_wFE ^ byte) & (SPEAKER | TAPE_OUT)){
            update(clk);
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
                        envelope_pos = 0x00;
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
            port_rFE = *byte;
        }
    }
    if ((port & 0xC003) == 0xC001)
        *byte &= registers[port_wFFFD];
}

void Sound::reset(){
    noise_seed = 12345;
    for (int i = 0; i < 0x10; i++){
        write(0xFFFD, i++, 0);
        write(0xBFFD, 0xFF, 0);
    }
    port_rFE = port_wFE = 0xFF;
    left = right = 0.0f;
    frame_pos = 0;
}

void Sound::frame(int frame_clk){
    update(frame_clk);
    frame_pos = 0;
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
