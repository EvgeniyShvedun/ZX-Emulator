#include "base.h"
#include "joystick.h"

KJoystick::KJoystick(){
    p1F = 0xC0;
}

void KJoystick::button(int phy_idx, bool state){
    for (int i = 0; i < 6; i++){
        if (button_map[i] == phy_idx){
            if (state)
                p1F |= (0x01 << i);
            else
                p1F &= ~(0x01 << i);
            break;
        }
    }
}

void KJoystick::config(int phy_idx, char button){
    for (int i = 0; i < 6; i++){
        if (button & (0x01 << i)){
            button_map[i] = phy_idx;
            break;
        }
    }
}

bool KJoystick::io_rd(unsigned short port, unsigned char *p_val, int clk){
    if ((port & 0xFF) == 0x1F){
        *p_val &= p1F;
        return true;
    }
    return false;
}
