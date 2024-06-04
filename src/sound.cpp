#include <cstddef>
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
#include <math.h>

Sound::Sound(){
    reset();
}

void Sound::setup(int rate, int cutoff_rate, int frame_clk){
    reset();
    sample_rate = rate;
    ay_fract = (1 << FRACT_BITS) * AY_RATE / sample_rate;
    set_lpf(cutoff_rate);
    if (device_id)
        SDL_CloseAudioDevice(device_id);
    frame_samples = frame_clk * (sample_rate / Z80_FREQ);
    SDL_zero(audio_spec);
    audio_spec.freq = sample_rate;
    audio_spec.format = AUDIO_S16;
    audio_spec.channels = 2;
    audio_spec.samples = frame_samples*2;//pow(2, ceil(log(71680 * 4 * (sample_rate / Z80_FREQ))/log(2)));
    audio_spec.callback = NULL;
    device_id = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    if (!device_id)
        throw std::runtime_error("Open audio device");
    if (!buffer)
        buffer = new s16[frame_samples*2];
    memset(buffer, 0x00, audio_spec.samples/2*4);
    SDL_QueueAudio(device_id, buffer, audio_spec.samples/2*4);
    SDL_PauseAudioDevice(device_id, 0);
}

void Sound::set_lpf(int cutoff_rate){
    float RC = 1.0 / (cutoff_rate * 2 * M_PI);
    float dt = 1.0 / sample_rate;
    float alpha =  dt / (RC + dt);
    lpf_fract =(1U << FRACT_BITS) * alpha;
}

void Sound::set_ay_volume(float volume, AY_Mixer channel_mode, float side_level, float center_level, float penetr_level){
    for (int i = 0; i < 0x10; i++){
        u16 amp = MAX_AMP / pow(sqrt(2), (15-i)) * volume;
        switch(channel_mode){
            case ABC:
                mixer[Left][A][i] = amp * side_level;
                mixer[Right][C][i] = amp * side_level;
                mixer[Left][B][i] = amp * center_level;
                mixer[Right][B][i] = amp * center_level;
                mixer[Left][C][i] = amp * penetr_level;
                mixer[Right][A][i] = amp * penetr_level;
                break;
            case ACB:
                mixer[Left][A][i] = amp * side_level;
                mixer[Right][B][i] = amp * side_level;
                mixer[Left][C][i] = amp * center_level;
                mixer[Right][C][i] = amp * center_level;
                mixer[Left][B][i] = amp * penetr_level;
                mixer[Right][A][i] = amp * penetr_level;
                break;
            case Mono:
                mixer[Left][A][i] = amp;
                mixer[Left][B][i] = amp;
                mixer[Left][C][i] = amp;
                mixer[Right][A][i] = amp;
                mixer[Right][B][i] = amp;
                mixer[Right][C][i] = amp;
                break;
        }
    }
}

Sound::~Sound(){
    DELETE_ARRAY(buffer);
    if (device_id){
        SDL_CloseAudioDevice(device_id);
        device_id = 0;
    }
}

void Sound::update(int clk){
    if (!buffer)
        return;
    u16 square = (
        ((rFE & TapeIn) ? MAX_AMP * tape_volume : 0) +
        ((wFE & TapeOut) ? MAX_AMP * tape_volume : 0) +
        ((wFE & Speaker) ? MAX_AMP * speaker_volume : 0));

    for (int now = clk * (sample_rate / Z80_FREQ); pos < now; pos++){
        u16 mix_left, mix_right;
        tone_a_counter += ay_fract;
        if (tone_a_counter >= tone_a_limit){
            tone_a_counter -= tone_a_limit;
            tone_a ^= true;
        }
        tone_b_counter += ay_fract;
        if (tone_b_counter >= tone_b_limit){
            tone_b_counter -= tone_b_limit;
            tone_b ^= true;
        }
        tone_c_counter += ay_fract;
        if (tone_c_counter >= tone_c_limit){
            tone_c_counter -= tone_c_limit;
            tone_c ^= true;

        }
        noise_counter += ay_fract;
        if (noise_counter >= noise_limit){
            noise_counter -= noise_limit;
            noise_seed = (noise_seed >> 1) ^ ((noise_seed & 1) ? 0x14000 : 0);
            noise = noise_seed & 0x01;
        }
        envelope_counter += ay_fract;
        if (envelope_counter >= envelope_limit){
            envelope_counter -= envelope_limit;
            if (++envelope_pos >= 0x20)
                envelope_pos = (registers[EnvShape] < 4 || registers[EnvShape] & 0x01) ? 0x10 : 0x00;
            envelope = envelope_shape[registers[EnvShape]*0x20 + envelope_pos];
        }
        mix_left = mix_right = square;
        if ((tone_a || (registers[Mixer] & 0x01)) && (noise || (registers[Mixer] & 0x08))){
            mix_left += mixer[Left][A][registers[VolA] & 0x10 ? envelope : registers[VolA]];
            mix_right += mixer[Right][A][registers[VolA] & 0x10 ? envelope : registers[VolA]];
        }
        if ((tone_b || (registers[Mixer] & 0x02)) && (noise || (registers[Mixer] & 0x10))){
            mix_left += mixer[Left][B][registers[VolB] & 0x10 ? envelope : registers[VolB]];
            mix_right += mixer[Right][B][registers[VolB] & 0x10 ? envelope : registers[VolB]];
        }
        if ((tone_c || (registers[Mixer] & 0x04)) && (noise || (registers[Mixer] & 0x20))){
            mix_left += mixer[Left][C][registers[VolC] & 0x10 ? envelope : registers[VolC]];
            mix_right += mixer[Right][C][registers[VolC] & 0x10 ? envelope : registers[VolC]];
        }
        left += lpf_fract * (mix_left - (left >> FRACT_BITS));
        right += lpf_fract * (mix_right - (right >> FRACT_BITS));
        buffer[pos * 2] = left >> FRACT_BITS;
        buffer[pos * 2 + 1] = right >> FRACT_BITS;
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
                        tone_a_limit = (registers[ToneAHigh] << 0x08 | registers[ToneALow]) << FRACT_BITS;
                        break;
                    case ToneBHigh:
                        registers[ToneBHigh] &= 0x0F;
                    case ToneBLow:
                        tone_b_limit = (registers[ToneBHigh] << 0x08 | registers[ToneBLow]) << FRACT_BITS;
                        break;
                    case ToneCHigh:
                        registers[ToneCHigh] &= 0x0F;
                    case ToneCLow:
                        tone_c_limit = (registers[ToneCHigh] << 0x08 | registers[ToneCLow]) << FRACT_BITS;
                        break;
                    case Noise:
                        noise_limit = (registers[Noise] & 0x1F) << FRACT_BITS;
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
                        envelope_limit = ((registers[EnvHigh] << 0x8 | registers[EnvLow])*2) << FRACT_BITS;
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
    tone_a = tone_b = tone_c = noise = 0;
    tone_a_counter = tone_b_counter = tone_c_counter = noise_counter = envelope_counter = 0;
    noise_seed = 12345;
    for (wFFFD= 0; wFFFD < 0x10; wFFFD++){
        write(0xFFFD, wFFFD, 0);
        write(0xBFFD, 0x00, 0);
    }
    rFE = 0x00;
    wFE = 0x00;
    wFFFD = 0x0F;
    left = right = 0;
    pos = 0;
}

void Sound::frame(int frame_clk){
    update(frame_clk);
    pos = 0;
}

void Sound::queue(){
    //u32 frame_samples = frame_clk * (sample_rate / Z80_FREQ);
    if (SDL_GetAudioDeviceStatus(device_id) == SDL_AUDIO_PLAYING){
        while (SDL_GetQueuedAudioSize(device_id) > (audio_spec.samples - frame_samples) * 4)
            SDL_Delay(1);
        SDL_QueueAudio(device_id, buffer, frame_samples * 4);
    }
}
