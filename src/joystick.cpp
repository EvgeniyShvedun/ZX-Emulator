#include "base.h"

void Joystick::gamepad(int code, bool state){
    for (int i = 0; i <= 5; i++){
        if (gamepad_map[i] == code){
            button(0x01 << i, state);
            break;
        }
    }
}

void Joystick::button(char mask, bool state){
    if (state)
        p1F |= mask;
    else
        p1F &= ~mask;
}

void Joystick::map(char mask, int code){
    for (int i = 0; i <= 5; i++){
        if (mask & (0x01 << i)){
            gamepad_map[i] = code;
            break;
        }
    }
}

bool Joystick::io_rd(unsigned short port, unsigned char *val, int clk){
    if (!(port & 0x20)){
        *val &= p1F;
        return true;
    }
    return false;
}
