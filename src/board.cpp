#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <GL/glew.h>
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
#include "video.h"
#include "ui.h"

//#define TIME
#define FRAME_LIMIT 50000

Board::Board(Cfg &cfg) : cfg(cfg) {
    Video::setup();
    Video::set_filter((Filter)cfg.video.filter);

    sound.set_ay_volume(cfg.audio.ay_volume, (AY_Mixer)cfg.audio.ay_mixer_mode, cfg.audio.ay_side_level, cfg.audio.ay_center_level, cfg.audio.ay_penetr_level);
    sound.set_speaker_volume(cfg.audio.speaker_volume);
    sound.set_tape_volume(cfg.audio.tape_volume);

    ula.load_rom(ROM_Trdos, (const char*)&cfg.main.rom_path[ROM_Trdos]);
    ula.load_rom(ROM_128, (const char*)&cfg.main.rom_path[ROM_128]);
    ula.load_rom(ROM_48, (const char*)&cfg.main.rom_path[ROM_48]);
    setup((Hardware)cfg.main.model);
    reset();
}

Board::~Board(){
    Video::free();
}

void Board::setup(Hardware model){
    static struct {
        ROM_Bank rom;
        s32 clk;
    } profile[sizeof(Hardware)] = {
        { ROM_128, 71680 },
        { ROM_128, 70908 },
        { ROM_48, 69888 }
    };
    frame_clk = profile[model].clk;
    ula.set_main_rom(profile[model].rom);
    sound.setup(cfg.audio.dsp_rate, cfg.audio.lpf_rate, frame_clk);
}
void Board::read(u16 port, u8 *byte, s32 clk){
    *byte = 0xFF;
    if (ula.is_trdos_active())
        fdc.read(port, byte, clk);
    else{
        joystick.read(port, byte, clk);
        ula.read(port, byte, clk);
    }
    keyboard.read(port, byte, clk);
    tape.read(port, byte, clk);
    mouse.read(port, byte, clk);
    sound.read(port, byte, clk);
}

void Board::write(u16 port, u8 byte, s32 clk){
    if (ula.is_trdos_active())
        fdc.write(port, byte, clk);
    sound.write(port, byte, clk);
    tape.write(port, byte, clk);
    ula.write(port, byte, clk);
}

void Board::set_window_size(int width, int height){
    viewport_width = cfg.video.screen_width = width;
    viewport_height = cfg.video.screen_height = height;
    SDL_SetWindowSize(window, width, height);
    Video::viewport_setup(width, height);
}

void Board::set_texture_filter(Filter filter){
    Video::set_filter(filter);///////////////////////
}

void Board::set_full_screen(bool state){
    if (state){
        SDL_SetWindowSize(window, SCREEN_WIDTH, SCREEN_HEIGHT);
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
    }else{
        SDL_ShowCursor(SDL_DISABLE);
        SDL_SetWindowFullscreen(window, 0);
        viewport_width = cfg.video.screen_width;
        viewport_height = cfg.video.screen_height;
        SDL_SetWindowSize(window, cfg.video.screen_width, cfg.video.screen_height);
        Video::viewport_setup(cfg.video.screen_width, cfg.video.screen_height);
    }
}

void Board::set_vsync(bool state){
    SDL_GL_SetSwapInterval(state ? 1 : 0);
}

void Board::reset(){
    cpu.reset();
    ula.reset();
    fdc.reset();
    sound.reset();
    tape.reset();
    keyboard.reset();
    joystick.reset();
    mouse.reset();
}

void Board::run(Cfg &cfg){
    window = SDL_GL_GetCurrentWindow();
    viewport_width = cfg.video.screen_width;
    viewport_height = cfg.video.screen_height;
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
                            Video::viewport_setup(viewport_width, viewport_height);
                            break;
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.repeat)
                        break;
                    if (event.key.keysym.sym == SDLK_RETURN && event.key.keysym.mod & KMOD_CTRL){
                        set_full_screen(cfg.video.full_screen ^= true);
                        continue;
                    }
                    break;
            }
            if (UI::event(event)){
                keyboard.clear();
                continue;
            }
            switch (event.type){
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym){
                        case SDLK_F5:
                            if (tape.is_play())
                                tape.stop();
                            else
                                tape.play();
                            continue;
                        case SDLK_F6:
                            tape.rewind_begin();
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
                case SDL_KEYUP:
                    if (event.key.keysym.mod & (KMOD_NUM | KMOD_CAPS))
                        SDL_SetModState(KMOD_NONE);
                    keyboard.event(event);
                    break;
                default:
                    joystick.event(event);
                    mouse.event(event);
                    break;
            }
        }
        void *frame_buffer = Video::update();
        if (!UI::is_modal()){
            ula.frame_setup(frame_buffer);
            cpu.frame(&ula, this, frame_clk);
            cpu.interrupt(&ula);
            cpu.clk -= frame_clk;
            fdc.frame(frame_clk);
            tape.frame(frame_clk);
            sound.frame(frame_clk);
            ula.frame(frame_clk);
            /*
            // Update the PBO and setup async byte-transfer to the screen texture.
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, ZX_SCREEN_WIDTH * ZX_SCREEN_HEIGHT * sizeof(GLushort), NULL, GL_DYNAMIC_DRAW);
            ula.frame_setup((u16*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
            cpu.frame(&ula, this, frame_clk);
            cpu.interrupt(&ula);
            cpu.clk -= frame_clk;
            fdc.frame(frame_clk);
            tape.frame(frame_clk);
            sound.frame(frame_clk);
            ula.frame(frame_clk);
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            // Start update texture.
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ZX_SCREEN_WIDTH, ZX_SCREEN_HEIGHT, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            */
            if (!cfg.main.full_speed)
                sound.queue();
        }else
            SDL_Delay(100);
        Video::frame();

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
        setup(Snapshot::load_z80(path, cpu, &ula, this));
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
        Snapshot::save_z80(path, cpu, &ula, this);
    if (!strcmp(path+len-4, ".trd") || !strcmp(path+len-4, ".TRD"))
        fdc.save_trd(0, path);
    return true;
}
