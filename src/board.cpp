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

Board::Board(){
    CFG &cfg = Config::get();
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &screen_texture);
    glBindTexture(GL_TEXTURE_2D, screen_texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA, SCREEN_WIDTH, SCREEN_HEIGHT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glGenBuffers(1, &pbo);

    set_video_filter((Filter)cfg.video.filter);
    set_vsync(cfg.video.vsync);

    sound.set_mixer((AY_Mixer)cfg.audio.ay_mixer, cfg.audio.ay_side, cfg.audio.ay_center, cfg.audio.ay_penetr);
    sound.set_ay_volume(cfg.audio.ay_volume);
    sound.set_speaker_volume(cfg.audio.speaker_volume);
    sound.set_tape_volume(cfg.audio.tape_volume);

    joystick.map(JOY_LEFT, cfg.gamepad.left);
    joystick.map(JOY_RIGHT, cfg.gamepad.right);
    joystick.map(JOY_DOWN, cfg.gamepad.down);
    joystick.map(JOY_UP, cfg.gamepad.up);
    joystick.map(JOY_A, cfg.gamepad.a);
    joystick.map(JOY_B, cfg.gamepad.b);

    ula.load_rom(ROM_Trdos, cfg.main.rom_path[ROM_Trdos]);
    ula.load_rom(ROM_128, cfg.main.rom_path[ROM_128]);
    ula.load_rom(ROM_48, cfg.main.rom_path[ROM_48]);
    setup((Hardware)cfg.main.model);
    reset();
}

Board::~Board(){
    if (screen_texture)
        glDeleteTextures(1, &screen_texture);
    screen_texture = 0;
    if (pbo)
        glDeleteBuffers(1, &pbo);
    pbo = 0;
}

void Board::setup(Hardware model){
    static struct {
        ROM_Bank rom;
        s32 clk;
    } profile[sizeof(Hardware)] = {
        { ROM_128, 70908 }, 
        { ROM_128, 69888 },
        { ROM_48, 71680 }
    };
    frame_clk = profile[model].clk;
    ula.set_main_rom(profile[model].rom);
    CFG &cfg = Config::get();
    sound.setup(cfg.audio.dsp_rate, cfg.audio.lpf_rate, frame_clk);
}
void Board::read(u16 port, u8 *byte, s32 clk){
    *byte = 0xFF;
    if (ula.is_trdos_active()){ ////
        fdc.read(port, byte, clk);
        return;
    }
    tape.read(port, byte, clk);
    keyboard.read(port, byte, clk);
    joystick.read(port, byte, clk);
    mouse.read(port, byte, clk);
    sound.read(port, byte, clk);
    ula.read(port, byte, clk);
}

void Board::write(u16 port, u8 byte, s32 clk){
    if (ula.is_trdos_active())
        fdc.write(port, byte, clk);
    sound.write(port, byte, clk);
    tape.write(port, byte, clk);
    ula.write(port, byte, clk);
}

void Board::viewport_setup(int width, int height){
    viewport_width = width;
    viewport_height = height;
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void Board::set_window_size(int width, int height){
    viewport_setup(width, height);
    SDL_SetWindowSize(window, width, height);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void Board::set_video_filter(Filter filter){
    glBindTexture(GL_TEXTURE_2D, screen_texture);
    switch (filter){
        case Nearest:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case Linear:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Board::set_full_screen(bool state){
    CFG &cfg = Config::get();
    if (state){
        Board::set_window_size(SCREEN_WIDTH*2, SCREEN_HEIGHT*2);
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
    }else{
        SDL_ShowCursor(SDL_DISABLE);
        SDL_SetWindowFullscreen(window, 0);
        Board::set_window_size(SCREEN_WIDTH*cfg.video.screen_scale, SCREEN_HEIGHT*cfg.video.screen_scale);
    }
}

void Board::set_vsync(bool state){
    SDL_GL_SetSwapInterval(state ? 1 : 0);
}
 
void Board::reset(){
    cpu.reset();
    fdc.reset();
    ula.reset();
    sound.reset();
}

void Board::run(CFG &cfg){
    window = SDL_GL_GetCurrentWindow();
    SDL_SetWindowMinimumSize(window, SCREEN_WIDTH*2, SCREEN_HEIGHT*2);
#ifdef TIME
    int frame_count = 0;
    set_vsync(false);
    cfg.main.full_speed = true;
    Uint32 time_start = SDL_GetTicks();
#endif
    while (true){
        SDL_Event event;
        while (SDL_PollEvent(&event)){
            switch (event.type){
                case SDL_WINDOWEVENT:
                    switch (event.window.event){
                        case SDL_WINDOWEVENT_CLOSE:
                            return;
                        case SDL_WINDOWEVENT_RESIZED:
                            viewport_width = event.window.data1;
                            viewport_height = event.window.data2;
                            break;
                        case SDL_WINDOWEVENT_EXPOSED:
                            viewport_setup(viewport_width, viewport_height);
                            break;
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.repeat)
                        break;
                    switch (event.key.keysym.sym){
                        case SDLK_F8:
                            continue;
                        case SDLK_F5:
                            if (tape.is_play())
                                tape.stop();
                            else
                                tape.play();
                            continue;
                        case SDLK_F6:
                            tape.rewind_begin();
                            continue;
                        case SDLK_F7:
                            sound.pause(paused ^= true);
                            continue;
                        case SDLK_F9:
                            set_vsync(!(cfg.main.full_speed ^= true));
                            continue;
                        case SDLK_F11:
                            ula.set_main_rom(cfg.main.model != HW_Sinclair_48 ? ROM_128 : ROM_48);
                            reset();
                            continue;
                        case SDLK_F12:
                            ula.set_main_rom(ROM_Trdos);
                            reset();
                            break;
                        case SDLK_RETURN:
                            if (event.key.keysym.mod & KMOD_CTRL){
                                set_full_screen(cfg.video.full_screen ^= true);
                                continue;
                            }
                            break;
                    }
                    break;
            }
            if (UI::event(event)){
                keyboard.clear();
                continue;
            }
            switch (event.type){
                case SDL_KEYDOWN:
                    if (event.key.keysym.mod & (KMOD_NUM | KMOD_CAPS))
                        SDL_SetModState(KMOD_NONE);
                case SDL_KEYUP:
                    if (event.key.repeat)
                        continue;
                    keyboard.event(event);
                    break;
                default:
                    joystick.event(event);
                    mouse.event(event);
                    break;
            }
        }
        glBindTexture(GL_TEXTURE_2D, screen_texture);
        glBegin(GL_QUADS);  
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, viewport_height);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(viewport_width, viewport_height);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(viewport_width, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
        glEnd();
        if (!paused){
            // Update the PBO and setup async byte-transfer to the screen texture.
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(GLushort), NULL, GL_DYNAMIC_DRAW);
            ula.buffer = (u16*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            cpu.frame(&ula, this, frame_clk);
            cpu.interrupt(&ula);
            cpu.clk -= frame_clk;
            fdc.frame(frame_clk);
            tape.frame(frame_clk);
            sound.frame(frame_clk);
            ula.frame(frame_clk);
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            // Start update texture.
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            if (!cfg.main.full_speed)
                sound.queue();
        }else
            SDL_Delay(100);
        if (UI::frame(cfg, this))
            break;
        SDL_GL_SwapWindow(window);
#ifdef TIME
        if (++frame_count > FRAME_LIMIT)
            break;
#endif
    }
#ifdef TIME
    printf("Frames: %d, Time: %d\n", frame_count, (SDL_GetTicks() - time_start));
    cfg.video.vsync = true;
#endif
}

bool Board::load_file(const char *path){
    int len = strlen(path);
    if (len < 4)
        return false;
    if (!strcmp(path+len-4, ".z80") || !strcmp(path+len-4, ".Z80"))
        setup(Snapshot::load_z80(path, &cpu, &ula, this));
    if (!strcmp(path+len-4, ".trd") || !strcmp(path+len-4, ".TRD"))
        fdc.load_trd(0, path);
    if (!strcmp(path+len-4, ".scl") || !strcmp(path+len-4, ".SCL"))
        fdc.load_scl(0, path);
    if (!strcmp(path+len-4, ".tap") || !strcmp(path+len-4, ".TAP"))
        tape.load_tap(path);
    return true;
}

bool Board::save_file(const char *path){
    int len = strlen(path);
    if (len < 4)
        return false;
    if (!strcmp(path+len-4, ".z80") || !strcmp(path+len-4, ".Z80"))
        Snapshot::save_z80(path, &cpu, &ula, this);
    if (!strcmp(path+len-4, ".trd") || !strcmp(path+len-4, ".TRD"))
        fdc.save_trd(0, path);
    return true;
}
