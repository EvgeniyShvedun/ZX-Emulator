#include "base.h"
#include "tape.h"

Tape::Tape(){
    p_data = NULL;
    data_size = 0;
    block_end = 0;
    last_clk = 0;
    time = 0;
    idx = 0;
    pFE = ~TAPE_EAR_IN;
    stop();
}

Tape::~Tape(){
    DELETE_ARRAY(p_data);
}

bool Tape::load_tap(const char *p_path){
    try {
        p_file = fopen(p_path, "rb");
        fseek(p_file, 0, SEEK_END);
        data_size = ftell(p_file);
        fseek(p_file, 0, SEEK_SET);
        DELETE_ARRAY(p_data);
        p_data = new unsigned char[data_size];
        fread(p_data, 1, data_size, p_file);
        fclose(p_file);
        return true;
    }catch(std::exception &e){
        if (p_file)
            fclose(p_file);
        return false;
    }
}

void Tape::play(){
    time = 0;
    last_clk = 0;
    bit = 0;
    pulse = 0;
    state = SILENCE;
}

void Tape::rewind(int time_shift){
}

void Tape::stop(){
    state = STOP;
}

void Tape::update(int clk){
    clk -= last_clk;
    last_clk += clk;
    time += clk;
    while (time > state_wait[state]){
        time -= state_wait[state];
        switch(state){
            case STOP:
                break;
            case SILENCE:
                state = TONE;
                break;
            case TONE:
                pFE ^= TAPE_EAR_IN;
                if (++pulse < TONE_PULSES)
                    break;
                pulse = 0;
                state = SYNC_1;
                break;
            case SYNC_1:
                pFE ^= TAPE_EAR_IN;
                state = SYNC_2;
                break;
            case SYNC_2:
                pFE ^= TAPE_EAR_IN;
                block_end = p_data[idx++];
                block_end |= p_data[idx++] << 8;
                block_end += idx;
                state = DATA;
                break;
            case DATA:
                if (bit >= 8){
                    bit &= 0x07;
                    if (++idx >= block_end){
                        if (idx >= data_size)
                            state = STOP;
                        else
                            state = SILENCE;
                        break;
                    }
                }
                state = p_data[idx] & (0x80 >> bit++) ? ONE : ZERO;
                break;
            case ONE:
            case ZERO:
                pFE ^= TAPE_EAR_IN;
                if (!pulse++)
                    break;
                pulse = 0;
                state = DATA;
                break;
        }
    }
}

void Tape::frame(int clk){
    update(clk);
    last_clk -= clk;
}

bool Tape::io_rd(unsigned short port, unsigned char *p_byte, int clk){
    if (!(port & 0x01)){
        update(clk);
        *p_byte &= pFE;
    }
    return false;
}

