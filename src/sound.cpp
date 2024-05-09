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

Sound::Sound(){
    reset();
}

void Sound::setup(int sample_rate, int lpf_rate, int frame_clk){
    reset();
    factor = sample_rate / (float)Z80_FREQ;
    increment = AY_RATE / (float)sample_rate;
    set_lpf(sample_rate, lpf_rate);
    if (device_id)
        SDL_CloseAudioDevice(device_id);
    samples = frame_clk * (sample_rate / (float)Z80_FREQ);
    samples &= ~0x03;
    SDL_zero(audio_spec);
    audio_spec.freq = sample_rate;
    audio_spec.format = AUDIO_S16;
    audio_spec.channels = 2;
    audio_spec.samples = samples * 2;
    audio_spec.callback = NULL;
    device_id = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    if (!device_id)
        throw std::runtime_error("Open audio device");
    if (!audio_frame)
        audio_frame = new short[sample_rate*2*3]; // Audio frame for 3 sec.
    memset(audio_frame, 0x00, samples*4);
    SDL_QueueAudio(device_id, audio_frame, samples*4);
    SDL_PauseAudioDevice(device_id, 0);
}

void Sound::set_lpf(int sample_rate, int lpf_rate){
    float RC = 1.0 / (sample_rate * 2 * M_PI);
    float dt = 1.0 / sample_rate;
    lpf_alpha = dt / (RC + dt);
}

void Sound::set_mixer(AY_Mixer mode, float side, float center, float penetr){
    mix_mode = mode;
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
    DeleteArray(audio_frame);
    if (device_id){
        SDL_CloseAudioDevice(device_id);
        device_id = 0;
    }
}

void Sound::update(int clk){
    if (!audio_frame)
        return;

    float square = (
        (wFE & Speaker ? 0.0f : speaker_volume) + 
        (wFE & TapeOut ? 0.0f : tape_volume) +
        (rFE & TapeIn ? 0.0f : tape_volume)) / 3;

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
            /*  The input to the shift registersister is bit0 XOR bit3 (bit0 is the output).
                This was verified on AY-3-8910 and YM2149 chips.
                noise_seed >>= 1;
                noise = noise_seed & 0x01 ? 0x00 : 0xFF; */
            /*  The Random Number Generator of the 8910 is a 17-bit shift registersister.
                Hacker KAY & Sergey Bulba
                noise_seed = ((noise_seed << 1) + 1) ^ (((noise_seed >> 16) ^ (noise_seed >> 13)) & 0x01);
                noise = (noise_seed >> 16) & 0x01 ? 0x00 : 0xFF; */
            if ((noise_seed & 0x01) ^ ((noise_seed & 0x02) >> 1))
                noise ^= -1;
            if (noise_seed & 0x01)
                noise_seed ^= 0x24000;
            noise_seed >>= 1;
        }
        envelope_counter += increment;
        if (envelope_counter >= envelope_limit){
            envelope_counter -= envelope_limit;
            if (++envelope_pos >= 0x20)
                envelope_pos = (registers[EnvShape] < 4 || registers[EnvShape] & 0x01) ? 0x10 : 0x00;
            envelope = envelope_shape[registers[EnvShape]*0x20 + envelope_pos];
        }
        float ay_left = 0.0f, ay_right = 0.0f;
        if ((tone_a | (registers[Mixer] & 0x01)) && (noise | (registers[Mixer] & 0x08))){
            ay_left += registers[VolA] & 0x10 ? volume_table[envelope] : volume_table[registers[VolA] & 0x0F] * mixer[mix_mode][Left][A];
            ay_right += registers[VolA] & 0x10 ? volume_table[envelope] : volume_table[registers[VolA] & 0x0F] * mixer[mix_mode][Right][A];
        }
        if ((tone_b | (registers[Mixer] & 0x02)) && (noise | (registers[Mixer] & 0x10))){
            ay_left += registers[VolB] & 0x10 ? volume_table[envelope] : volume_table[registers[VolB] & 0x0F] * mixer[mix_mode][Left][B];
            ay_right += registers[VolB] & 0x10 ? volume_table[envelope] : volume_table[registers[VolB] & 0x0F] * mixer[mix_mode][Right][B];
        }
        if ((tone_c | (registers[Mixer] & 0x04)) && (noise | (registers[Mixer] & 0x20))){
            ay_left += registers[VolC] & 0x10 ? volume_table[envelope] : volume_table[registers[VolC] & 0x0F] * mixer[mix_mode][Left][C];
            ay_right += registers[VolC] & 0x10 ? volume_table[envelope] : volume_table[registers[VolC] & 0x0F] * mixer[mix_mode][Right][C];
        }
        left += lpf_alpha * (((ay_left / 3.0f * ay_volume + square) / 2.0f) - left);
        right += lpf_alpha * (((ay_right / 3.0f * ay_volume + square) / 2.0f) - right);
        audio_frame[frame_pos * 2] = 0x8000 * left;
        audio_frame[frame_pos * 2 + 1] = 0x8000 * right;
    }
}

void Sound::write(u16 port, u8 byte, s32 clk){
    if (!(port & 0x01)){
        if ((wFE ^ byte) & (Speaker | TapeOut)){
            update(clk);
            wFE = byte;
        }
    }else{
        if ((port & 0xC002) == 0xC000){
            wFFFD = byte & 0x0F;
        }else{
            if ((port & 0xC002) == 0x8000){
                update(clk);
                registers[wFFFD] = byte;
                switch(wFFFD){
                    case ToneAHigh:
                        registers[ToneAHigh] &= 0x0F;
                    case ToneALow:
                        tone_a_limit = registers[ToneAHigh] << 0x08 | registers[ToneALow];
                        break;
                    case ToneBHigh:
                        registers[ToneBHigh] &= 0x0F;
                    case ToneBLow:
                        tone_b_limit = registers[ToneBHigh] << 0x08 | registers[ToneBLow];
                        break;
                    case ToneCHigh:
                        registers[ToneCHigh] &= 0x0F;
                    case ToneCLow:
                        tone_c_limit = registers[ToneCHigh] << 0x08 | registers[ToneCLow];
                        break;
                    case Noise:
                        noise_limit = registers[Noise] & 0x1F;// * 2;
                        break;
                    case Mixer: // bit (0 - 7/5?)
                        break;
                    case VolA:
                        registers[VolA] &= 0x1F;
                        break;
                    case VolB:
                        registers[VolB] &= 0x1F;
                        break;
                    case VolC:
                        registers[VolC] &= 0x1F;
                        break;
                    case EnvHigh:
                    case EnvLow:
                        envelope_limit = registers[EnvHigh] << 0x8 | registers[EnvLow];
                        break;
                    case EnvShape:
                        registers[EnvShape] &= 0x0F;
                        envelope = envelope_shape[registers[EnvShape] * 0x20];
                        envelope_pos = 0x00;
                        break;
                    case PortA:
                        break;
                    case PortB:
                        break;
                }
            }
        }
    }
}

void Sound::read(u16 port, u8 *byte, s32 clk){
    if (!(port & 0x01)){
        if ((rFE ^ *byte) & TapeIn){
            update(clk);
            rFE = *byte;
        }
    }
    if ((port & 0xC003) == 0xC001)
        *byte &= registers[wFFFD];
}

void Sound::reset(){
    noise_seed = 12345;
    for (int i = 0; i < 0x10; i++){
        write(0xFFFD, i++, 0);
        write(0xBFFD, 0x00, 0);
    }
    rFE = wFE = 0xFF;
    left = right = 0.0f;
    frame_pos = 0;
}

void Sound::frame(int frame_clk){
    update(frame_clk);
    frame_pos = 0;
}

void Sound::pause(bool state){
    if (device_id)
        SDL_PauseAudioDevice(device_id, state ? 1 : 0);
}

void Sound::queue(){
    if (SDL_GetAudioDeviceStatus(device_id) == SDL_AUDIO_PLAYING){
        while (SDL_GetQueuedAudioSize(device_id) > (audio_spec.samples - samples) * 4)
            SDL_Delay(1);
        SDL_QueueAudio(device_id, audio_frame, samples * 4);
    }
}
