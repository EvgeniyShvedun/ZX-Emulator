#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_image.h>
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

#define CONFIG_PATH         "zx.dat"
#define SOFTWARE            "ZX-Spectrum emulator v1.1.1"
#define COPYRIGHT           "Evgeniy Shvedun 2006-2024"

SDL_Window *window = NULL;
const char* glsl_version = "#version 130";
SDL_GLContext gl_context = NULL;
Board *board = NULL;

int fatal_error(const char *msg = SDL_GetError());

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
    printf("%s, %s.\n", SOFTWARE, COPYRIGHT);
    Cfg &cfg = Config::load(CONFIG_PATH);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
        return fatal_error();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    if (!(window = SDL_CreateWindow(SOFTWARE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, cfg.video.screen_width, cfg.video.screen_height,
        (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (cfg.video.full_screen ? SDL_WINDOW_FULLSCREEN : 0)))))
        return fatal_error();
    SDL_SetWindowMinimumSize(window, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!(gl_context = SDL_GL_CreateContext(window)))
        return fatal_error();
    SDL_GL_MakeCurrent(window, gl_context);
    //glewExperimental = true;
    if (glewInit() != GLEW_OK)
        return fatal_error("GLEW initialization");
    SDL_SetWindowIcon(window, IMG_Load("data/icon.png"));
    board = new Board(cfg);
    for (int i = 1; i < argc; i++)
        board->load_file(argv[i]);
    UI::setup(cfg, window, gl_context, glsl_version);
    board->run(cfg);
    Config::save(CONFIG_PATH);
    release_all();
    return 0;
}
