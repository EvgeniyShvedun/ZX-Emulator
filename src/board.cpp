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

    set_video_filter((VideoFilter)cfg.video.filter);
    set_vsync(cfg.video.vsync);

    sound.set_ay_volume(cfg.audio.ay_volume);
    sound.set_speaker_volume(cfg.audio.speaker_volume);
    sound.set_tape_volume(cfg.audio.tape_volume);
    sound.set_mixer_mode(cfg.audio.ay_mixer);
    sound.set_mixer_levels(cfg.audio.ay_side, cfg.audio.ay_center, cfg.audio.ay_penetr);

    joystick.map(JOY_LEFT, cfg.gamepad.left);
    joystick.map(JOY_RIGHT, cfg.gamepad.right);
    joystick.map(JOY_DOWN, cfg.gamepad.down);
    joystick.map(JOY_UP, cfg.gamepad.up);
    joystick.map(JOY_A, cfg.gamepad.a);
    joystick.map(JOY_B, cfg.gamepad.b);

    ula.load_rom(ROM_Trdos, cfg.main.rom_path[ROM_Trdos]);
    ula.load_rom(ROM_128, cfg.main.rom_path[ROM_128]);
    ula.load_rom(ROM_48, cfg.main.rom_path[ROM_48]);
    //ula.set_main_rom(ROM_48);
    //ula.reset();
    setup((Hardware)cfg.main.model);
    ula.reset();
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
    sound.init(Config::get().audio.dsp_rate, frame_clk);
    sound.setup_lpf(Config::get().audio.lpf_rate);
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

void Board::frame(){
    // PBO data is transferred, rendering a full-screen textured quad.
    glBindTexture(GL_TEXTURE_2D, screen_texture);
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, viewport_height);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(viewport_width, viewport_height);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(viewport_width, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
    glEnd();

    if (frame_hold){
        SDL_Delay(100);
        return;
    }
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

    
    if (!Config::get().main.full_speed)
        sound.queue();
}

void Board::set_viewport_size(int width, int height){
    viewport_width = width;
    viewport_height = height;
    SDL_SetWindowSize(SDL_GL_GetCurrentWindow(), width, height);
    SDL_SetWindowPosition(SDL_GL_GetCurrentWindow(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void Board::set_video_filter(VideoFilter filter){
    glBindTexture(GL_TEXTURE_2D, screen_texture);
    switch (filter){
        case VF_Nearest:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case VF_Linear:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Board::set_full_screen(bool state){
    SDL_SetWindowFullscreen(SDL_GL_GetCurrentWindow(), state ? SDL_WINDOW_FULLSCREEN : 0);
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

void Board::event(SDL_Event &event){
    // Control keys
    CFG &cfg = Config::get();
    if (event.type == SDL_KEYDOWN && !event.key.repeat){
        switch (event.key.keysym.sym){
            case SDLK_F5:
                if (tape.is_play())
                    tape.stop();
                else
                    tape.play();
                return;
            case SDLK_F6:
                tape.rewind_begin();
                return;
            case SDLK_F7:
                frame_hold ^= true;
                return;
            case SDLK_F9:
                set_vsync(!(cfg.main.full_speed ^= true));
                return;
           case SDLK_F11:
                ula.set_main_rom(cfg.main.model != HW_Sinclair_48 ? ROM_128 : ROM_48);
                reset();
                return;
           case SDLK_F12:
                ula.set_main_rom(ROM_Trdos);
                reset();
                return;
           case SDLK_RETURN:
                if (event.key.keysym.mod & KMOD_CTRL){
                    set_full_screen(cfg.video.full_screen ^= true);
                    return;
                }
                break;
        }
    }
    // GUI
    if (UI::event(event)){
        keyboard.clear(); //////////////////////////
        return;
    }

    // Joystick, mouse, keyboard
    switch (event.type){
        case SDL_JOYHATMOTION:
            switch (event.jhat.value){
                case SDL_HAT_LEFT:
                    joystick.button(JOY_LEFT, true);
                    joystick.button(JOY_RIGHT | JOY_UP | JOY_DOWN, false);
                    break;
                case SDL_HAT_LEFTUP:
                    joystick.button(JOY_LEFT | JOY_UP, true);
                    joystick.button(JOY_RIGHT | JOY_DOWN, false);
                    break;
                case SDL_HAT_LEFTDOWN:
                    joystick.button(JOY_LEFT | JOY_DOWN, true);
                    joystick.button(JOY_RIGHT | JOY_UP, false);
                    break;
                case SDL_HAT_RIGHT:
                    joystick.button(JOY_RIGHT, true);
                    joystick.button(JOY_LEFT | JOY_UP | JOY_DOWN, false);
                    break;
                case SDL_HAT_RIGHTUP:
                    joystick.button(JOY_RIGHT | JOY_UP, true);
                    joystick.button(JOY_LEFT | JOY_DOWN, false);
                    break;
                case SDL_HAT_RIGHTDOWN:
                    joystick.button(JOY_RIGHT | JOY_DOWN, true);
                    joystick.button(JOY_LEFT | JOY_UP, false);
                    break;
                case SDL_HAT_UP:
                    joystick.button(JOY_UP, true);
                    joystick.button(JOY_LEFT | JOY_RIGHT | JOY_DOWN, false);
                    break;
                case SDL_HAT_DOWN:
                    joystick.button(JOY_DOWN, true);
                    joystick.button(JOY_LEFT | JOY_RIGHT | JOY_UP, false);
                    break;
                case SDL_HAT_CENTERED:
                    joystick.button(JOY_LEFT | JOY_RIGHT | JOY_UP | JOY_DOWN, false);
                    break;
            }
            break;
        case SDL_JOYBUTTONDOWN:
            joystick.gamepad(event.jbutton.button, true);
            break;
        case SDL_JOYBUTTONUP:
            joystick.gamepad(event.jbutton.button, false);
            break;
        case SDL_MOUSEMOTION:
            mouse.motion(event.motion.xrel, event.motion.yrel);
            break;
        case SDL_MOUSEBUTTONDOWN:
            mouse.button((event.button.button & SDL_BUTTON_LEFT ? 0x01 : 0x00) | (event.button.button & SDL_BUTTON_RIGHT ? 0x02 : 0x00), true);
            break;
        case SDL_MOUSEBUTTONUP:
            mouse.button((event.button.button & SDL_BUTTON_LEFT ? 0x01 : 0x00) | (event.button.button & SDL_BUTTON_RIGHT ? 0x02 : 0x00), false);
            break;
        case SDL_MOUSEWHEEL:
            mouse.wheel(event.wheel.y);
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.mod & (KMOD_NUM | KMOD_CAPS))
                SDL_SetModState(KMOD_NONE);
        case SDL_KEYUP:
            if (event.key.repeat)
                break;
            switch (event.key.keysym.sym){
                case SDLK_UP:
                    keyboard.button(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
                    keyboard.button(0xEFFE, 0x08, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_DOWN:
                    keyboard.button(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
                    keyboard.button(0xEFFE, 0x10, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_LEFT:
                    keyboard.button(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
                    keyboard.button(0xF7FE, 0x10, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_RIGHT:
                    keyboard.button(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
                    keyboard.button(0xEFFE, 0x04, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_BACKSPACE:
                    keyboard.button(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
                    keyboard.button(0xEFFE, 0x01, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_KP_PLUS:
                    keyboard.button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
                    keyboard.button(0xBFFE, 0x04, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_MINUS:
                case SDLK_KP_MINUS:
                    keyboard.button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
                    keyboard.button(0xBFFE, 0x08, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_KP_MULTIPLY:
                    keyboard.button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
                    keyboard.button(0x7FFE, 0x10, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_KP_DIVIDE:
                    keyboard.button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
                    keyboard.button(0xFEFE, 0x10, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_EQUALS:
                case SDLK_KP_EQUALS:
                    keyboard.button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
                    keyboard.button(0xBFFE, 0x02, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_COMMA:
                case SDLK_KP_COMMA:
                    keyboard.button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
                    keyboard.button(0x7FFE, 0x08, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_PERIOD:
                case SDLK_KP_PERIOD:
                    keyboard.button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
                    keyboard.button(0x7FFE, 0x04, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_SPACE:
                    keyboard.button(0x7FFE, 0x01, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_LCTRL:
                case SDLK_RCTRL:
                    keyboard.button(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_m:
                    keyboard.button(0x7FFE, 0x04, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_n:
                    keyboard.button(0x7FFE, 0x08, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_b:
                    keyboard.button(0x7FFE, 0x10, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    keyboard.button(0xBFFE, 0x01, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_l:
                    keyboard.button(0xBFFE, 0x02, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_k:
                    keyboard.button(0xBFFE, 0x04, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_j:
                    keyboard.button(0xBFFE, 0x08, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_h:
                    keyboard.button(0xBFFE, 0x10, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_p:
                    keyboard.button(0xDFFE, 0x01, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_o:
                    keyboard.button(0xDFFE, 0x02, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_i:
                    keyboard.button(0xDFFE, 0x04, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_u:
                    keyboard.button(0xDFFE, 0x08, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_y:
                    keyboard.button(0xDFFE, 0x10, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_0:
                    keyboard.button(0xEFFE, 0x01, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_9:
                    keyboard.button(0xEFFE, 0x02, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_8:
                    keyboard.button(0xEFFE, 0x04, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_7:
                    keyboard.button(0xEFFE, 0x08, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_6:
                    keyboard.button(0xEFFE, 0x10, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_1:
                    keyboard.button(0xF7FE, 0x01, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_2:
                    keyboard.button(0xF7FE, 0x02, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_3:
                    keyboard.button(0xF7FE, 0x04, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_4:
                    keyboard.button(0xF7FE, 0x08, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_5:
                    keyboard.button(0xF7FE, 0x10, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_q:
                    keyboard.button(0xFBFE, 0x01, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_w:
                    keyboard.button(0xFBFE, 0x02, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_e:
                    keyboard.button(0xFBFE, 0x04, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_r:
                    keyboard.button(0xFBFE, 0x08, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_t:
                    keyboard.button(0xFBFE, 0x10, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_a:
                    keyboard.button(0xFDFE, 0x01, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_s:
                    keyboard.button(0xFDFE, 0x02, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_d:
                    keyboard.button(0xFDFE, 0x04, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_f:
                    keyboard.button(0xFDFE, 0x08, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_g:
                    keyboard.button(0xFDFE, 0x10, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_LSHIFT:
                case SDLK_RSHIFT:
                    keyboard.button(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_z:
                    keyboard.button(0xFEFE, 0x02, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_x:
                    keyboard.button(0xFEFE, 0x04, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_c:
                    keyboard.button(0xFEFE, 0x08, event.type == SDL_KEYDOWN);
                    break;
                case SDLK_v:
                    keyboard.button(0xFEFE, 0x10, event.type == SDL_KEYDOWN);
                    break;
            }
        }
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
