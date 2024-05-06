#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "utils.h"
#include "config.h"
#include "device.h"
#include <SDL.h>
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

void Keyboard::event(SDL_Event &event){
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP)
        return;
    switch (event.key.keysym.sym){
        case SDLK_UP:
            button(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
            button(0xEFFE, 0x08, event.type == SDL_KEYDOWN);
            break;
        case SDLK_DOWN:
            button(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
            button(0xEFFE, 0x10, event.type == SDL_KEYDOWN);
            break;
        case SDLK_LEFT:
            button(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
            button(0xF7FE, 0x10, event.type == SDL_KEYDOWN);
            break;
        case SDLK_RIGHT:
            button(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
            button(0xEFFE, 0x04, event.type == SDL_KEYDOWN);
            break;
        case SDLK_BACKSPACE:
            button(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
            button(0xEFFE, 0x01, event.type == SDL_KEYDOWN);
            break;
        case SDLK_KP_PLUS:
            button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
            button(0xBFFE, 0x04, event.type == SDL_KEYDOWN);
            break;
        case SDLK_MINUS:
        case SDLK_KP_MINUS:
            button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
            button(0xBFFE, 0x08, event.type == SDL_KEYDOWN);
            break;
        case SDLK_KP_MULTIPLY:
            button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
            button(0x7FFE, 0x10, event.type == SDL_KEYDOWN);
            break;
        case SDLK_KP_DIVIDE:
            button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
            button(0xFEFE, 0x10, event.type == SDL_KEYDOWN);
            break;
        case SDLK_EQUALS:
        case SDLK_KP_EQUALS:
            button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
            button(0xBFFE, 0x02, event.type == SDL_KEYDOWN);
            break;
        case SDLK_COMMA:
        case SDLK_KP_COMMA:
            button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
            button(0x7FFE, 0x08, event.type == SDL_KEYDOWN);
            break;
        case SDLK_PERIOD:
        case SDLK_KP_PERIOD:
            button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
            button(0x7FFE, 0x04, event.type == SDL_KEYDOWN);
            break;
        case SDLK_SPACE:
            button(0x7FFE, 0x01, event.type == SDL_KEYDOWN);
            break;
        case SDLK_LCTRL:
        case SDLK_RCTRL:
            button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
            break;
        case SDLK_m:
            button(0x7FFE, 0x04, event.type == SDL_KEYDOWN);
            break;
        case SDLK_n:
            button(0x7FFE, 0x08, event.type == SDL_KEYDOWN);
            break;
        case SDLK_b:
            button(0x7FFE, 0x10, event.type == SDL_KEYDOWN);
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            button(0xBFFE, 0x01, event.type == SDL_KEYDOWN);
            break;
        case SDLK_l:
            button(0xBFFE, 0x02, event.type == SDL_KEYDOWN);
            break;
        case SDLK_k:
            button(0xBFFE, 0x04, event.type == SDL_KEYDOWN);
            break;
        case SDLK_j:
            button(0xBFFE, 0x08, event.type == SDL_KEYDOWN);
            break;
        case SDLK_h:
            button(0xBFFE, 0x10, event.type == SDL_KEYDOWN);
            break;
        case SDLK_p:
            button(0xDFFE, 0x01, event.type == SDL_KEYDOWN);
            break;
        case SDLK_o:
            button(0xDFFE, 0x02, event.type == SDL_KEYDOWN);
            break;
        case SDLK_i:
            button(0xDFFE, 0x04, event.type == SDL_KEYDOWN);
            break;
        case SDLK_u:
            button(0xDFFE, 0x08, event.type == SDL_KEYDOWN);
            break;
        case SDLK_y:
            button(0xDFFE, 0x10, event.type == SDL_KEYDOWN);
            break;
        case SDLK_0:
            button(0xEFFE, 0x01, event.type == SDL_KEYDOWN);
            break;
        case SDLK_9:
            button(0xEFFE, 0x02, event.type == SDL_KEYDOWN);
            break;
        case SDLK_8:
            button(0xEFFE, 0x04, event.type == SDL_KEYDOWN);
            break;
        case SDLK_7:
            button(0xEFFE, 0x08, event.type == SDL_KEYDOWN);
            break;
        case SDLK_6:
            button(0xEFFE, 0x10, event.type == SDL_KEYDOWN);
            break;
        case SDLK_1:
            button(0xF7FE, 0x01, event.type == SDL_KEYDOWN);
            break;
        case SDLK_2:
            button(0xF7FE, 0x02, event.type == SDL_KEYDOWN);
            break;
        case SDLK_3:
            button(0xF7FE, 0x04, event.type == SDL_KEYDOWN);
            break;
        case SDLK_4:
            button(0xF7FE, 0x08, event.type == SDL_KEYDOWN);
            break;
        case SDLK_5:
            button(0xF7FE, 0x10, event.type == SDL_KEYDOWN);
            break;
        case SDLK_q:
            button(0xFBFE, 0x01, event.type == SDL_KEYDOWN);
            break;
        case SDLK_w:
            button(0xFBFE, 0x02, event.type == SDL_KEYDOWN);
            break;
        case SDLK_e:
            button(0xFBFE, 0x04, event.type == SDL_KEYDOWN);
            break;
        case SDLK_r:
            button(0xFBFE, 0x08, event.type == SDL_KEYDOWN);
            break;
        case SDLK_t:
            button(0xFBFE, 0x10, event.type == SDL_KEYDOWN);
            break;
        case SDLK_a:
            button(0xFDFE, 0x01, event.type == SDL_KEYDOWN);
            break;
        case SDLK_s:
            button(0xFDFE, 0x02, event.type == SDL_KEYDOWN);
            break;
        case SDLK_d:
            button(0xFDFE, 0x04, event.type == SDL_KEYDOWN);
            break;
        case SDLK_f:
            button(0xFDFE, 0x08, event.type == SDL_KEYDOWN);
            break;
        case SDLK_g:
            button(0xFDFE, 0x10, event.type == SDL_KEYDOWN);
            break;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            button(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
            break;
        case SDLK_z:
            button(0xFEFE, 0x02, event.type == SDL_KEYDOWN);
            break;
        case SDLK_x:
            button(0xFEFE, 0x04, event.type == SDL_KEYDOWN);
            break;
        case SDLK_c:
            button(0xFEFE, 0x08, event.type == SDL_KEYDOWN);
            break;
        case SDLK_v:
            button(0xFEFE, 0x10, event.type == SDL_KEYDOWN);
            break;
    }
}

