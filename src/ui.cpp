#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_image.h>
#include <sys/stat.h>
#include <unistd.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "ext/ImGuiFileDialog/ImGuiFileDialog.h"
#include "ext/ImGuiFileDialog/ImGuiFileDialogConfig.h"
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

#define LABEL_WIDTH                       140
#define KBD_IMAGE_PATH                    "data/kbd_layout.png"
//#define DEBUGGER
//#define STYLE_EDITOR

using namespace ImGui;

namespace UI {
    ImVec2 btn_size = { 100.0f, 21.0f };
    ImVec2 btn_small = { 50.0f, 21.0f };
    IGFD::FileDialogConfig file_config = { .path = ".", .countSelectionMax = 1, .flags = ImGuiFileDialogFlags_Modal };
    GLuint kbd_texture = 0;
    UI_Mode mode = UI_None;

    GLuint load_texture(const char *path){
        GLuint texture;
        SDL_Surface *image = IMG_Load(path);
        SDL_Surface *surface = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_RGBA4444, 0);
        SDL_FreeSurface(image);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, surface->pixels);
        SDL_FreeSurface(surface);
        return texture;
    }

    void setup(Cfg &cfg, SDL_Window *window, SDL_GLContext context, const char *glsl_version){
        IMGUI_CHECKVERSION();
        CreateContext();
        ImGuiIO &io = GetIO();

        io.FontGlobalScale = 1.2f;
        io.IniFilename = NULL;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        StyleColorsDark();
        ImGui_ImplSDL2_InitForOpenGL(window, context);
        ImGui_ImplOpenGL3_Init(glsl_version);
        ImGuiStyle &style = GetStyle();
        style.WindowBorderSize = 0.0f;
        style.WindowPadding = ImVec2(9, 9);
        style.ItemSpacing = ImVec2(6, 4);
        //style.FramePadding = ImVec2(4, 2);
        style.WindowRounding = 3.0;
        style.FrameRounding = 2.0;
        style.GrabRounding = 2.0;
        style.TabRounding = 2.0;
        style.GrabMinSize = 10.0;
        style.WindowTitleAlign = ImVec2(0.5, 0.5);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        set_alpha(cfg.ui.alpha);
        set_gamepad_ctrl(cfg.ui.gamepad);
        kbd_texture = load_texture(KBD_IMAGE_PATH);
    }
    bool event(SDL_Event &event){
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_KEYDOWN){
            if (!is_shown()){
                switch (event.key.keysym.sym){
                    case SDLK_ESCAPE:
                        open(UI_Exit);
                        break;
                    case SDLK_F1:
                        open(UI_KbdLayout);
                        break;
                    case SDLK_F2:
                        open(UI_OpenFile);
                        break;
                    case SDLK_F3:
                        open(UI_SaveFile);
                        break;
                    case SDLK_F4:
                        open(UI_Settings);
                        break;
                    case SDLK_F9:
#ifdef DEBUGGER
                        open(UI_Debugger);
#endif
                        break;

                }
            } else
                if (event.key.keysym.sym == SDLK_ESCAPE && !IsPopupOpen(NULL, ImGuiPopupFlags_AnyPopup))
                    hide();
        }
        return is_shown();
    }

    bool frame(Cfg &cfg, Board *board){
        bool status = false;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        NewFrame();
        ImGuiIO &io = GetIO();
        ImGuiStyle &style = GetStyle();
        switch (mode){
            case UI_None:
                break;
            case UI_Exit:
                static const char *exit_agree = "Do you exit ?";
                SetNextWindowSize(ImVec2(270.0f, 0.0f), ImGuiCond_Always);
                SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                if (Begin("Exit", NULL, UI_WindowFlags | ImGuiWindowFlags_AlwaysAutoResize)){// | ImGuiWindowFlags_NoDecoration)){
                    Dummy(ImVec2(0.0f, 10.0f));
                    SetCursorPosX((GetWindowWidth() - CalcTextSize(exit_agree).x)*0.5f);
                    TextUnformatted(exit_agree);
                    Dummy(ImVec2(0.0f, 10.0f));
                    SetCursorPosX((GetWindowWidth() - style.ItemSpacing.x - btn_size.x*2)*0.5f);
                    if (Button("Yes", btn_size))
                        status = true;
                    SameLine();
                    if (Button("No", btn_size))
                        hide();
                    End();
                }
                break;
            case UI_KbdLayout:
                SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                SetNextWindowSize(ImVec2(io.DisplaySize.x*0.8f, io.DisplaySize.y*0.5f), ImGuiCond_Always);
                if (Begin("Keyboard", NULL, UI_WindowFlags | ImGuiWindowFlags_NoDecoration)){
                    Image((ImTextureID)kbd_texture, GetContentRegionAvail());
                    End();
                }
                break;
            case UI_OpenFile:
                SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                SetNextWindowSize(ImVec2(io.DisplaySize.x*(0.85f-(1.0f-SCREEN_WIDTH/io.DisplaySize.x)*0.5f), io.DisplaySize.y*(0.75f-(1.0f-SCREEN_HEIGHT/io.DisplaySize.y)*0.5f)), ImGuiCond_Always);
                ImGuiFileDialog::Instance()->OpenDialog("##file_dlg", "Open file", ".z80;.tap;.trd;.scl {(([.]z80|Z80|trd|TRD|scl|SCL|tap|TAP))}", file_config);
                if (ImGuiFileDialog::Instance()->Display("##file_dlg", UI_WindowFlags)){
                    if (ImGuiFileDialog::Instance()->IsOk())
                        board->load_file(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
                    ImGuiFileDialog::Instance()->Close();
                    hide();
                }
                break;
            case UI_SaveFile:
                SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                SetNextWindowSize(ImVec2(io.DisplaySize.x*(0.85f-(1.0f-SCREEN_WIDTH/io.DisplaySize.x)*0.5f), io.DisplaySize.y*(0.75f-(1.0f-SCREEN_HEIGHT/io.DisplaySize.y)*0.5f)), ImGuiCond_Always);
                ImGuiFileDialog::Instance()->OpenDialog("##file_dlg", "Save file", ".z80;.trd; {(([.]z80|Z80|trd|TRD))}", file_config);
                if (ImGuiFileDialog::Instance()->Display("##file_dlg", UI_WindowFlags)){
                    if (ImGuiFileDialog::Instance()->IsOk())
                        board->save_file(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
                    ImGuiFileDialog::Instance()->Close();
                    hide();
                }
                break;
            case UI_Settings:
                static int load_rom = -1;
                static const char *label[sizeof(ROM_Bank)] = { "Rom TR-Dos", "Rom 128", "Rom 48" };
                SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, (io.DisplaySize.y-400.0f)*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
                SetNextWindowSize(ImVec2(io.DisplaySize.x*(0.60f-(1.0f-SCREEN_WIDTH/io.DisplaySize.x)*0.5f), 0), ImGuiCond_Always);
                if (Begin("Settings", NULL, UI_WindowFlags)){
                    ImVec2 glyph = CalcTextSize("A");
                    if (BeginTabBar("tabs")){
                        if (BeginTabItem("Main")){
                            SeparatorText("Hardware");
                            Text("Model");
                            SameLine(LABEL_WIDTH);
                            SetNextItemWidth(-FLT_MIN);
                            if (Combo("##model", &cfg.main.model, "Pentagon - 128k\0Sinclair - 128k\0Sinclair - 48k\0\0")){
                                board->setup((Hardware)cfg.main.model);
                                board->reset();
                            }
                            SetCursorPosX(LABEL_WIDTH);
                            if (Checkbox("Full speed", &cfg.main.full_speed))
                                board->set_vsync(cfg.video.vsync & !cfg.main.full_speed);
                            SeparatorText("BIOS");
                            for (int i = 0; i < (int)sizeof(ROM_Bank) - 1; i++){
                                TextUnformatted(label[i]);
                                SameLine(LABEL_WIDTH);
                                SetNextItemWidth(-FLT_MIN - btn_small.x - style.ItemSpacing.x);
                                float path_width = GetContentRegionAvail().x - btn_small.x - style.ItemSpacing.x - style.FramePadding.x;
                                int path_len = strlen(cfg.main.rom_path[i]);
                                PushID(i);
                                InputText("##path", cfg.main.rom_path[i] + (path_len*glyph.x > path_width ? (int)(path_len - path_width/glyph.x) : 0), PATH_MAX);
                                SameLine();
                                PushID(0x10 + i);
                                if (Button("Load", btn_small))
                                    load_rom = i;
                                PopID(); PopID();
                            }
                            if (load_rom >= 0){
                                SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                                SetNextWindowSize(ImVec2(io.DisplaySize.x*(0.85f-(1.0f-SCREEN_WIDTH/io.DisplaySize.x)*0.5f), io.DisplaySize.y*(0.75f-(1.0f-SCREEN_HEIGHT/io.DisplaySize.y)*0.5f)), ImGuiCond_Always);
                                ImGuiFileDialog::Instance()->OpenDialog("load_rom", "Load ROM", ".rom {(([.]rom|ROM))}", file_config);
                                if (ImGuiFileDialog::Instance()->Display("load_rom", UI_WindowFlags)){
                                    if (ImGuiFileDialog::Instance()->IsOk()){
                                        strcpy(cfg.main.rom_path[load_rom], ImGuiFileDialog::Instance()->GetFilePathName().c_str());
                                        board->ula.load_rom((ROM_Bank)load_rom, cfg.main.rom_path[load_rom]);
                                        board->reset();
                                    }
                                    load_rom = -1;
                                    ImGuiFileDialog::Instance()->Close();
                                }
                            }
                            Spacing();
                            SetCursorPosX(GetWindowWidth() - btn_size.x - style.WindowPadding.x);
                            if (Button("Defaults", btn_size)){
                                memcpy(&cfg.main, &Config::get_defaults().main, sizeof(Cfg::main));
                                board->setup((Hardware)cfg.main.model);
                                board->reset();
                            }
                            EndTabItem();
                        }
                        if (BeginTabItem("Video")){
                            SeparatorText("Resolution");
                            Text("Scale");
                            SameLine(LABEL_WIDTH);
                            SetNextItemWidth(-FLT_MIN);

                            int screen_size = (float)cfg.video.screen_width / cfg.video.screen_height == ASPECT_RATIO ? std::min(((float)cfg.video.screen_width - SCREEN_WIDTH) / ZX_SCREEN_WIDTH, 3.0f) : 4;
                            if (!cfg.video.full_screen){
                                if (Combo("##screen_size", &screen_size, "640x480\0""960x720\0""1280x960\0""1600x1200\0\0"))
                                    if (screen_size >= 0 && screen_size <= 3)
                                        board->set_window_size(SCREEN_WIDTH+ZX_SCREEN_WIDTH*screen_size, SCREEN_HEIGHT+ZX_SCREEN_HEIGHT*screen_size);
                            }else{
                                BeginDisabled();
                                Combo("##screen_size", &screen_size, "640x480\0""960x720\0""1280x960\0""1600x1200\0\0");
                                EndDisabled();
                            }

                            SetCursorPosX(LABEL_WIDTH);
                            if (Checkbox("Full screen", &cfg.video.full_screen))
                                board->set_full_screen(cfg.video.full_screen);
                            SeparatorText("Display");
                            Text("Type");
                            SameLine(LABEL_WIDTH);
                            if (RadioButton("LCD", &cfg.video.filter, Nearest))
                                board->set_texture_filter(Nearest);
                            SameLine();
                            if (RadioButton("CRT", &cfg.video.filter, Linear))
                                board->set_texture_filter(Linear);
                            SetCursorPosX(LABEL_WIDTH);
                            if (!cfg.main.full_speed){
                                if (Checkbox("V-Sync", &cfg.video.vsync))
                                    board->set_vsync(cfg.video.vsync);
                            }else{
                                BeginDisabled();
                                Checkbox("V-Sync", &cfg.video.vsync);
                                EndDisabled();
                            }
                            Spacing();
                            SetCursorPosX(GetWindowWidth()-btn_size.x-style.WindowPadding.x);
                            if (Button("Defaults", btn_size)){
                                memcpy(&cfg.video, &Config::get_defaults().video, sizeof(Cfg::video));
                                board->set_window_size(SCREEN_WIDTH, SCREEN_HEIGHT);
                                board->set_full_screen(cfg.video.full_screen);
                                board->set_texture_filter((Filter)cfg.video.filter);
                                board->set_vsync(cfg.video.vsync);
                            }
                            EndTabItem();
                        }
                        if (BeginTabItem("Audio")){
                            SeparatorText("Frequency");
                            Text("Audio rate");
                            SameLine(LABEL_WIDTH);
                            PushItemWidth(-FLT_MIN);
                            if (InputInt("##dsp", &cfg.audio.dsp_rate, 1000, 10000, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue)){
                                cfg.audio.dsp_rate = std::min(std::max(cfg.audio.dsp_rate, 11025), 192000);
                                board->sound.setup(cfg.audio.dsp_rate, cfg.audio.lpf_rate, board->frame_clk);
                            }
                            Text("Low-pass cut");
                            SameLine(LABEL_WIDTH);
                            if (InputInt("##lpf", &cfg.audio.lpf_rate, 1000, 1000, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue)){
                                cfg.audio.lpf_rate = std::min(std::max(cfg.audio.lpf_rate, 5000), 25000);
                                board->sound.set_lpf(cfg.audio.lpf_rate);
                            }
                            SeparatorText("Volume");
                            Text("AY");
                            SameLine(LABEL_WIDTH);
                            if (SliderFloat("##ay_volume", &cfg.audio.ay_volume, 0.0, 1.0, "%.2f"))
                                board->sound.set_ay_volume(cfg.audio.ay_volume, (AY_Mixer)cfg.audio.ay_mixer_mode, cfg.audio.ay_side_level, cfg.audio.ay_center_level, cfg.audio.ay_penetr_level);
                            Text("Speaker");
                            SameLine(LABEL_WIDTH);
                            if (SliderFloat("##speaker_volume", &cfg.audio.speaker_volume, 0.0, 1.0, "%.2f"))
                                board->sound.set_speaker_volume(cfg.audio.speaker_volume);
                            Text("Tape");
                            SameLine(LABEL_WIDTH);
                            if (SliderFloat("##tape_volume", &cfg.audio.tape_volume, 0.0, 1.0, "%.2f"))
                                board->sound.set_tape_volume(cfg.audio.tape_volume);
                            SeparatorText("AY mixer");
                            Text("Mode");
                            SameLine(LABEL_WIDTH);
                            if (Combo("##ay_mixer_mode", &cfg.audio.ay_mixer_mode, "ABC\0ACB\0Mono\0\0"))
                                board->sound.set_ay_volume(cfg.audio.ay_volume, (AY_Mixer)cfg.audio.ay_mixer_mode, cfg.audio.ay_side_level, cfg.audio.ay_center_level, cfg.audio.ay_penetr_level);
                            Text("Left/Right");
                            SameLine(LABEL_WIDTH);
                            if (SliderFloat("##ay_side_level", &cfg.audio.ay_side_level, 0.0, 1.0, "%.2f"))
                                board->sound.set_ay_volume(cfg.audio.ay_volume, (AY_Mixer)cfg.audio.ay_mixer_mode, cfg.audio.ay_side_level, cfg.audio.ay_center_level, cfg.audio.ay_penetr_level);
                            Text("Center");
                            SameLine(LABEL_WIDTH);
                            if (SliderFloat("##ay_center_level", &cfg.audio.ay_center_level, 0.0, 1.0, "%.2f"))
                                board->sound.set_ay_volume(cfg.audio.ay_volume, (AY_Mixer)cfg.audio.ay_mixer_mode, cfg.audio.ay_side_level, cfg.audio.ay_center_level, cfg.audio.ay_penetr_level);
                            Text("Penetration");
                            SameLine(LABEL_WIDTH);
                            if (SliderFloat("##ay_penetr_level", &cfg.audio.ay_penetr_level, 0.0, 1.0, "%.2f"))
                                board->sound.set_ay_volume(cfg.audio.ay_volume, (AY_Mixer)cfg.audio.ay_mixer_mode, cfg.audio.ay_side_level, cfg.audio.ay_center_level, cfg.audio.ay_penetr_level);
                            PopItemWidth();
                            Spacing();
                            SetCursorPosX(GetWindowWidth()-btn_size.x-style.WindowPadding.x);
                            if (Button("Defaults", btn_size)){
                                memcpy(&cfg.audio, &Config::get_defaults().audio, sizeof(Cfg::audio));
                                board->sound.setup(cfg.audio.dsp_rate, cfg.audio.lpf_rate, board->frame_clk);
                                board->sound.set_ay_volume(cfg.audio.ay_volume, (AY_Mixer)cfg.audio.ay_mixer_mode, cfg.audio.ay_side_level, cfg.audio.ay_center_level, cfg.audio.ay_penetr_level);
                                board->sound.set_speaker_volume(cfg.audio.speaker_volume);
                                board->sound.set_tape_volume(cfg.audio.tape_volume);
                            }
                            EndTabItem();
                        }
                        if (BeginTabItem("UI")){
                            SeparatorText("Appearance");
                            Text("Alpha");
                            SameLine(LABEL_WIDTH);
                            SetNextItemWidth(-FLT_MIN);
                            if (SliderFloat("##alpha", &cfg.ui.alpha, 0.7f, 1.0f, "%.2f"))
                                set_alpha(cfg.ui.alpha);
                            SeparatorText("Controls");
                            Text("Gamepad");
                            SameLine(LABEL_WIDTH);
                            if (Checkbox("##gamepad", &cfg.ui.gamepad))
                                set_gamepad_ctrl(cfg.ui.gamepad);
                            Spacing();
                            SetCursorPosX(GetWindowWidth()-btn_size.x-style.WindowPadding.x);
                            if (Button("Defaults", btn_size)){
                                memcpy(&cfg.ui, &Config::get_defaults().ui, sizeof(Cfg::ui));
                                set_alpha(cfg.ui.alpha);
                                set_gamepad_ctrl(cfg.ui.gamepad);
                            }
                            EndTabItem();
                        }
                        EndTabBar();
                    }
                    End();
                }
                break;
            case UI_Debugger:
#ifdef DEBUGGER
                Debugger::display(board);
#endif
                break;
        }
#ifdef STYLE_EDITOR
        ShowStyleEditor();
#endif
        //ShowDemoWindow();
        Render();
        ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());
        return status;
    }
    void open(UI_Mode mode){
        ImGuiIO &io = GetIO();
        io.ClearInputKeys();
        io.ClearEventsQueue();
        UI::mode = mode;
    }
    void hide(){
        mode = UI_None;
    }
    bool is_shown(){
        return mode != UI_None;
    }
    bool is_modal(){
        return mode == UI_SaveFile || mode == UI_Debugger;
    }

    void set_alpha(float alpha){
        ImGuiStyle &style = GetStyle();
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.01f, 0.01f, 0.01f, alpha);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.01f, alpha * 0.25f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.01, 0.01f, 0.01f, alpha * 0.99);
        style.Colors[ImGuiCol_FrameBg].w = alpha;
    }
    void set_gamepad_ctrl(bool state){
        ImGuiIO &io = GetIO();
        if (state)
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        else
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    }
}
