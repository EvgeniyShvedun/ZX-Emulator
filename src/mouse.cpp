#include "base.h"

void Mouse::motion(char x, char y){
    x_coord += x;
    y_coord += y;
}

void Mouse::button(char button, bool status){
    if (status)
        wheel_button |= button & 0x03;
    else wheel_button &= ~(button & 0x03);
}

void Mouse::wheel(char pos){
    wheel_button &= 0x0F;
    wheel_button |= (pos & 0xF) << 4;
}

bool Mouse::io_rd(unsigned short port, unsigned char *val, int clk){
    // A0, A5, A7, A8, A1
    if (!(port & 0b100000)){
        if ((port & 0b10111111111) == 0b00011011111){ // 0xFADF
            *val &= wheel_button;
            return true;
        }
        if ((port & 0b10111111111) == 0b00111011111){ // 0xFBDF
            *val &= x_coord;
            return true;
        }
        if ((port & 0b10111111111) == 0b10111011111){ // 0xFFDF
            *val &= y_coord;
            return true;
        }
    }
    return false;
}
