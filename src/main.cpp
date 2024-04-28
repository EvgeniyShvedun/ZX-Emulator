#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include "types.h"
#include "utils.h"
#include "config.h"
#include "device.h"
#include "memory.h"
#include "ula.h"
#include "z80.h"
#include "snapshot.h"
#include "floppy.h"
#include "keyboard.h"
#include "joystick.h"
#include "tape.h"
#include "sound.h"
#include "mouse.h"
#include "board.h"
#include "ui.h"

#define TITLE "2024 Sinclair Research v1.1"
#define CONFIG "zx.dat"
const char* glsl_version = "#version 130";
SDL_Window *window = NULL;
SDL_GLContext gl_context = NULL;
Board *board = NULL;

void release_all(){
    DELETE(board);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int fatal_error(const char *msg){
    printf("ERROR: %s\n", msg);
    release_all();
    return -1;
}

int main(int argc, char **argv){
    CFG &cfg = Config::load(CONFIG);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0)
        return fatal_error("SDL: init");
    if (SDL_NumJoysticks()){
        SDL_JoystickOpen(0);
        SDL_JoystickEventState(SDL_ENABLE);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_WindowFlags flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!(window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        cfg.video.full_screen ? SCREEN_WIDTH*2 : SCREEN_WIDTH*cfg.video.screen_scale,
        cfg.video.full_screen ? SCREEN_HEIGHT*2 : SCREEN_HEIGHT*cfg.video.screen_scale,
        cfg.video.full_screen ? (SDL_WindowFlags)(flags | SDL_WINDOW_FULLSCREEN) : flags)))
        return fatal_error("SDL_CreateWindow");
    if (!(gl_context = SDL_GL_CreateContext(window)))
        return fatal_error("SDL_GL_CreateContext");
    SDL_GL_MakeCurrent(window, gl_context);
    glewExperimental = true;
    if (glewInit() != GLEW_OK)
        return fatal_error("glewInit");
    SDL_SetWindowIcon(window, IMG_Load("data/icon.png"));
    SDL_SetWindowMinimumSize(window, SCREEN_WIDTH*2, SCREEN_HEIGHT*2);
    SDL_ShowCursor(SDL_DISABLE);

    board = new Board(cfg);
    for (int i = 1; i < argc; i++)
        board->load_file(argv[i]);
    UI::setup(window, gl_context, glsl_version);

    static bool loop = true;
    static bool supsend = false;

    while (loop){
        SDL_Event event;
        while (SDL_PollEvent(&event)){
            if (event.type == SDL_WINDOWEVENT){
                switch (event.window.event){
                    case SDL_WINDOWEVENT_CLOSE:
                        loop = false;
                        break;
                    case SDL_WINDOWEVENT_HIDDEN:
                        supsend = true;
                        break;
                    case SDL_WINDOWEVENT_SHOWN:
                        supsend = false;
                        break;
                    case SDL_WINDOWEVENT_EXPOSED:
                        int width, height;
                        SDL_GetWindowSize(window, &width, &height);
                        board->set_viewport_size(width, height);
                        break;
                }
                break;
            }
            if (UI::event(event))
                board->keyboard.clear();
            else
                board->event(event);
        }
        if (supsend){
            SDL_Delay(100);
            continue;
        }
        board->frame();
        if (UI::frame(cfg, board))
            break;
        SDL_GL_SwapWindow(window);
    }
    Config::save(CONFIG);
    release_all();
    return 0;
}
