#include "base.h"

void KJoystick::gamepad_event(int pad_btn, bool state){
    for (int i = 0; i <= 5; i++){
        if (button_map[i] == pad_btn){
            set_state(0x01 << i, state);
            break;
        }
    }
}
void KJoystick::set_state(unsigned char btn_mask, bool state){
    if (state)
        p1F |= btn_mask;
    else
        p1F &= ~btn_mask;
}

void KJoystick::map(char btn_mask, int pad_btn){
    for (int i = 0; i <= 5; i++){
        if (btn_mask & (0x01 << i)){
            button_map[i] = pad_btn;
            break;
        }
    }
}

bool KJoystick::io_rd(unsigned short addr, unsigned char *p_val, int clk){
    if (!(addr & 0x20)){
        *p_val &= p1F;
        return true;
    }
    return false;
}
