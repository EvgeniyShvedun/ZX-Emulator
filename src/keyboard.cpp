#include "base.h"
#include "keyboard.h"

bool Keyboard::io_rd(unsigned short port, unsigned char *p_byte, int clk){
    if (!(port & 0x01)){
        for (int i = 0; i < 8; i++)
            if (!(port & (0x8000 >> i)))
                *p_byte &= kbd_state[i];
    }
    return false;
}

void Keyboard::set_btn_state(unsigned short port, unsigned char btn_mask, bool pressed){
    for (int i = 0; i < 8; i++){
        if (!(port & (0x8000 >> i))){
            if (pressed)
                kbd_state[i] &= ~btn_mask;
            else
                kbd_state[i] |= btn_mask;
        }
    }
}
