#include "base.h"

Board::Board(CFG *cfg){
    this->cfg = cfg;
    set_hw((HW_Model)cfg->main.hw_model);

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &screen_texture);
    glBindTexture(GL_TEXTURE_2D, screen_texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA4, SCREEN_WIDTH, SCREEN_HEIGHT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glGenBuffers(1, &pbo);
    video_filter((VF)cfg->video.filter);
    vsync(cfg->video.vsync);

    sound.set_ay_volume(cfg->audio.ay_volume);
    sound.set_speaker_volume(cfg->audio.speaker_volume);
    sound.set_tape_volume(cfg->audio.tape_volume);
    sound.set_mixer_mode(cfg->audio.ay_mixer_mode);
    sound.set_mixer_levels(cfg->audio.ay_side_level, cfg->audio.ay_center_level, cfg->audio.ay_interpenetration_level);
    sound.init(cfg->audio.sample_rate, frame_clk);
    sound.setup_lpf(cfg->audio.lpf_rate);

    joystick.map(JOY_LEFT, cfg->gamepad.left);
    joystick.map(JOY_RIGHT, cfg->gamepad.right);
    joystick.map(JOY_DOWN, cfg->gamepad.down);
    joystick.map(JOY_UP, cfg->gamepad.up);
    joystick.map(JOY_A, cfg->gamepad.button_a);
    joystick.map(JOY_B, cfg->gamepad.button_b);

    ula.load_rom(ROM_TRDOS, cfg->main.rom_path[ROM_TRDOS]);
    ula.load_rom(ROM_128, cfg->main.rom_path[ROM_128]);
    ula.load_rom(ROM_48, cfg->main.rom_path[ROM_48]);
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

void Board::set_hw(HW_Model model){
     switch(model){
         case HW_SINCLAIR_48:
             ula.set_rom(ROM_48);
             frame_clk = 71680;
             break;
         case HW_SINCLAIR_128:
             ula.set_rom(ROM_128);
             frame_clk = 69888;
             break;
         case HW_PENTAGON_128:
             ula.set_rom(ROM_128);
             frame_clk = 70908;
             break;
     }
}

/*
p_board->add_device(p_tape);
    p_board->add_device(p_sound);
    p_board->add_device(p_wd1793);
    p_board->add_device(p_keyboard);
    p_board->add_device(p_joystick);
    p_board->add_device(p_mouse);
    */

unsigned char Board::read(unsigned short port, int clk){
    unsigned char byte = 0xFF;
    /*
    if (tape.io_rd(port, &byte, clk))
        return byte;
    if (sound.io_rd(port, &byte, clk))
        return byte;
    if (ula.trdos_active())
        if (fdc.io_rd(port, &byte, clk))
            return byte;
    if (keyboard.io_rd(port, &byte, clk))
        return byte;
    if (joystick.io_rd(port, &byte, clk))
        return byte;
    if (mouse.io_rd(port, &byte, clk))
        return byte;
    if (tape.io_rd(port, &byte, clk))
        return byte;
    */
    if (ula.trdos_active())
        if (fdc.io_rd(port, &byte, clk))
            return byte;
    if (keyboard.io_rd(port, &byte, clk))
        return byte;
    if (joystick.io_rd(port, &byte, clk))
        return byte;
    if (mouse.io_rd(port, &byte, clk))
        return byte;
    if (sound.io_rd(port, &byte, clk))
        return byte;
    if (tape.io_rd(port, &byte, clk))
        return byte;
    ula.io_rd(port, &byte, clk);
    return byte;
}

void Board::write(unsigned short port, unsigned char byte, int clk){
    /*
    if (tape.io_wr(port, byte, clk))
        return;
    if (sound.io_wr(port, byte, clk))
        return;
    if (ula.trdos_active())
        if (fdc.io_wr(port, byte, clk))
            return;
    */
    if (ula.trdos_active())
        if (fdc.io_wr(port, byte, clk))
            return;
    if (sound.io_wr(port, byte, clk))
        return;
    if (tape.io_wr(port, byte, clk))
        return;
    ula.io_wr(port, byte, clk);
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
    // Update the PBO and setup async byte-transfer to the screen texture.
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(GLushort), NULL, GL_DYNAMIC_DRAW);
    ula.buffer = (GLshort*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);

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

    if (!cfg->main.full_speed)
        sound.queue();
}

void Board::viewport_resize(int width, int height){
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

void Board::video_filter(VF filter){
    glBindTexture(GL_TEXTURE_2D, screen_texture);
    switch (filter){
        case VF_NEAREST:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case VF_LINEAR:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Board::full_screen(bool on){
    SDL_SetWindowFullscreen(SDL_GL_GetCurrentWindow(), on ? SDL_WINDOW_FULLSCREEN : 0);
}

void Board::vsync(bool on){
    SDL_GL_SetSwapInterval(on ? 1 : 0);
}
 
void Board::reset(){
    cpu.reset();
    fdc.reset();
    ula.reset();
    sound.reset();
}

void Board::event(SDL_Event &event){
    // Functional keys
    if (event.type == SDL_KEYDOWN && !event.key.repeat){
        switch (event.key.keysym.sym){
            case SDLK_F5:
                if (tape.is_play())
                    tape.stop();
                else
                    tape.play();
                break;
            case SDLK_F6:
                tape.rewind_begin();
                break;
            case SDLK_F9:
                if (Config::get()->main.full_speed ^= true)
                    vsync(false);
                else
                    vsync(Config::get()->video.vsync);
                break;
            case SDLK_F11:
                ula.set_rom(Config::get()->main.hw_model != HW_SINCLAIR_48 ? ROM_128 : ROM_48);
                reset();
                break;
            case SDLK_F12:
                ula.set_rom(ROM_TRDOS);
                reset();
                break;
            case SDLK_RETURN:
                if (event.key.keysym.mod & KMOD_CTRL)
                    full_screen(Config::get()->video.full_screen ^= true);
                break;
        }
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

void Board::load_file(const char *path){
    int len = strlen(path);
    if (len < 4)
        return;
    if (!strcmp(path+len-4, ".z80") || !strcmp(path+len-4, ".Z80")){
        set_hw(Snapshot::load_z80(path, &cpu, &ula, this));
        sound.init(sound.sample_rate, frame_clk);
    }
    if (!strcmp(path+len-4, ".trd") || !strcmp(path+len-4, ".TRD"))
        fdc.load_trd(0, path);
    if (!strcmp(path+len-4, ".scl") || !strcmp(path+len-4, ".SCL"))
        fdc.load_scl(0, path);
    if (!strcmp(path+len-4, ".tap") || !strcmp(path+len-4, ".TAP"))
        tape.load_tap(path);
}

void Board::save_file(const char *path){
    int len = strlen(path);
    if (len < 4)
        return;
    if (!strcmp(path+len-4, ".z80") || !strcmp(path+len-4, ".Z80"))
        Snapshot::save_z80(path, &cpu, &ula, this);
    if (!strcmp(path+len-4, ".trd") || !strcmp(path+len-4, ".TRD"))
        fdc.save_trd(0, path);
}
