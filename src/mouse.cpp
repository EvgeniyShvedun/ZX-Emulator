#include "base.h"
#include "mouse.h"

KMouse::KMouse(){
    x_coord = 0;
    y_coord = 0;
    wheel_and_button = 0;
}

bool KMouse::io_rd(unsigned short port, unsigned char *p_val, int clk){
    // Kemston mouse port decode using A0,A5,A7,A8,A1
    if (!(port & 0b100000)){
        if ((port & 0b10111111111) == 0b00011011111){ // 0xFADF
            *p_val &= wheel_and_button;
            return true;
        }
        if ((port & 0b10111111111) == 0b00111011111){ // 0xFBDF
            *p_val &= x_coord;
            return true;
        }
        if ((port & 0b10111111111) == 0b10111011111){ // 0xFFDF
            *p_val &= y_coord;
            return true;
        }
    }
    return false;
}

void KMouse::wheel(char pos){
    wheel_and_button &= 0x0F;
    wheel_and_button |= (pos & 0xF) << 4;
}

void KMouse::button_press(char button){
    wheel_and_button |= button & 0x03;
}

void KMouse::button_release(char button){
    wheel_and_button &= ~(button & 0x03);
}

void KMouse::motion(char x, char y){
    x_coord += x;
    y_coord += y;
}
