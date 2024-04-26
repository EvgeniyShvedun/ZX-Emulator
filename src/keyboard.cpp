#include "base.h"

void Keyboard::button(unsigned short port, char mask, bool state){
    for (int i = 0; i < 8; i++){
        if (!(port & (0x8000 >> i))){
            if (state)
                kbd[i] &= ~mask;
            else
                kbd[i] |= mask;
        }
    }
}

bool Keyboard::io_rd(unsigned short port, unsigned char *byte, int clk){
    if (!(port & 0x01)){
        for (int i = 0; i < 8; i++)
            if (!(port & (0x8000 >> i)))
                *byte &= kbd[i];
        //return true;
    }
    return false;
}
