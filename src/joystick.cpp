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

void Joystick::event(SDL_Event &event){
    switch (event.type){
        case SDL_JOYHATMOTION:
            switch (event.jhat.value){
                case SDL_HAT_LEFT:
                    button(JOY_LEFT, true);
                    button(JOY_RIGHT | JOY_UP | JOY_DOWN, false);
                    break;
                case SDL_HAT_LEFTUP:
                    button(JOY_LEFT | JOY_UP, true);
                    button(JOY_RIGHT | JOY_DOWN, false);
                    break;
                case SDL_HAT_LEFTDOWN:
                    button(JOY_LEFT | JOY_DOWN, true);
                    button(JOY_RIGHT | JOY_UP, false);
                    break;
                case SDL_HAT_RIGHT:
                    button(JOY_RIGHT, true);
                    button(JOY_LEFT | JOY_UP | JOY_DOWN, false);
                    break;
                case SDL_HAT_RIGHTUP:
                    button(JOY_RIGHT | JOY_UP, true);
                    button(JOY_LEFT | JOY_DOWN, false);
                    break;
                case SDL_HAT_RIGHTDOWN:
                    button(JOY_RIGHT | JOY_DOWN, true);
                    button(JOY_LEFT | JOY_UP, false);
                    break;
                case SDL_HAT_UP:
                    button(JOY_UP, true);
                    button(JOY_LEFT | JOY_RIGHT | JOY_DOWN, false);
                    break;
                case SDL_HAT_DOWN:
                    button(JOY_DOWN, true);
                    button(JOY_LEFT | JOY_RIGHT | JOY_UP, false);
                    break;
                case SDL_HAT_CENTERED:
                    button(JOY_LEFT | JOY_RIGHT | JOY_UP | JOY_DOWN, false);
                    break;
            }
            break;
        case SDL_JOYBUTTONDOWN:
            gamepad(event.jbutton.button, true);
            break;
        case SDL_JOYBUTTONUP:
            gamepad(event.jbutton.button, false);
            break;
    }
}
