#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "utils.h"
#include "config.h"
#include "device.h"
#include "joystick.h"

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
        port_1F |= mask;
    else
        port_1F &= ~mask;
}

void Joystick::map(char mask, int code){
    for (int i = 0; i <= 5; i++){
        if (mask & (0x01 << i)){
            gamepad_map[i] = code;
            break;
        }
    }
}

void Joystick::read(u16 port, u8 *byte, s32 clk){
    if (port & 0x01)
        *byte &= port_1F;
}
