#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "utils.h"
#include "config.h"
#include "device.h"
#include "keyboard.h"

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

void Keyboard::read(u16 port, u8 *byte, s32 clk){
    if (!(port & 0x01)){
        for (int i = 0; i < 8; i++)
            if (!(port & (0x8000 >> i)))
                *byte &= kbd[i];
    }
}
