#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "utils.h"
#include "config.h"
#include "device.h"
#include "mouse.h"

void Mouse::motion(char x, char y){
    x_coord += x;
    y_coord += y;
}

void Mouse::button(char button, bool status){
    if (status)
        wheel_button |= button & 0x03;
    else
        wheel_button &= ~(button & 0x03);
}

void Mouse::wheel(char pos){
    wheel_button &= 0x0F;
    wheel_button |= (pos & 0xF) << 4;
}

void  Mouse::read(u16 port, u8 *byte, s32 clk){
    // A0, A5, A7, A8, A1
    if (!(port & 0b100000)){
        if ((port & 0b10111111111) == 0b00011011111) // 0xFADF
            *byte &= wheel_button;
        if ((port & 0b10111111111) == 0b00111011111) // 0xFBDF
            *byte &= x_coord;
        if ((port & 0b10111111111) == 0b10111011111) // 0xFFDF
            *byte &= y_coord;
    }
}
