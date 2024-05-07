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

Joystick::Joystick(){
    if (SDL_IsGameController(0) == SDL_TRUE){
       controller = SDL_GameControllerOpen(0);
       SDL_GameControllerEventState(SDL_ENABLE);
    }
}

Joystick::~Joystick(){
    if (controller){
        SDL_GameControllerClose(controller);
        controller = NULL;
    }
}

void Joystick::button(char mask, bool state){
    if (state)
        port_r1F |= mask;
    else
        port_r1F &= ~mask;
}

void Joystick::read(u16 port, u8 *byte, s32 clk){
    if (port & 0x01)
        *byte &= port_r1F;
}

void Joystick::event(SDL_Event &event){
    if (event.type != SDL_CONTROLLERBUTTONDOWN && event.type != SDL_CONTROLLERBUTTONUP)
        return;
    switch (event.cbutton.button){
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
             button(JB_Left, event.cbutton.state);
             break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
             button(JB_Right, event.cbutton.state);
             break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
             button(JB_Down, event.cbutton.state);
             break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
             button(JB_Up, event.cbutton.state);
             break;
        case SDL_CONTROLLER_BUTTON_A:
             button(JB_A, event.cbutton.state);
             break;
        case SDL_CONTROLLER_BUTTON_B:
             button(JB_B, event.cbutton.state);
             break;
    }
}
