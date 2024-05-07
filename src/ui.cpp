#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#include <GL/gl.h>
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
//#include "ui_debugger.h"
#include "ui.h"

#define LABEL_WIDTH                       140
#define KBD_IMAGE_PATH                    "data/kbd_layout.png"

using namespace ImGui;

namespace UI {
    ImVec2 btn_size = { 100.0f, 21.0f };
    ImVec2 btn_small = { 50.0f, 21.0f };
    int gamepad_code = 0;
    GLuint kbd_texture = 0;
    IGFD::FileDialogConfig file_config = { .path = ".", .countSelectionMax = 1, .flags = ImGuiFileDialogFlags_Modal };
    UI_Mode mode = UI_Hidden;

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

    void setup(CFG &cfg, SDL_Window *window, SDL_GLContext context, const char *glsl_version){
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
        set_gamepad_ctrl(cfg.ui.gamepad_ctrl);
        kbd_texture = load_texture(KBD_IMAGE_PATH);
    }
    bool event(SDL_Event &event){
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_KEYDOWN){
            if (!is_active()){
                switch (event.key.keysym.sym){
                    case SDLK_ESCAPE:
                        open(UI_Exit);
                        return true;
                    case SDLK_F1:
                        open(UI_KbdLayout);
                        return true;
                    case SDLK_F2:
                        open(UI_OpenFile);
                        return true;
                    case SDLK_F3:
                        open(UI_SaveFile);
                        return true;
                    case SDLK_F4:
                        open(UI_Settings);
                        return true;
                    case SDLK_F8:
                        open(UI_Debugger);
                        return true;
                }
            }else
                if (event.key.keysym.sym == SDLK_ESCAPE && !IsPopupOpen(NULL, ImGuiPopupFlags_AnyPopup)){
                    hide();
                    return true;
                }
        }else
            gamepad_code = event.type == SDL_JOYBUTTONDOWN ? event.jbutton.button : 0;
        return is_active();
    }

    bool frame(CFG &cfg, Board *board){
        bool status = false;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        NewFrame();
        if (is_active()){
            ImGuiIO &io = GetIO();
            ImGuiStyle &style = GetStyle();
            switch (mode){
                case UI_Hidden:
                    break;
                case UI_Exit:
                    static const char *exit_agree = "Do you exit ?";
                    SetNextWindowSize(ImVec2(270.0f, 0.0f), ImGuiCond_Always);
                    SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    if (Begin("Exit", NULL, UI_WindowFlags | ImGuiWindowFlags_AlwaysAutoResize)){// | ImGuiWindowFlags_NoDecoration)){
                        Dummy(ImVec2(0.0f, 10.0f));
                        SetCursorPosX((GetWindowWidth() - CalcTextSize(exit_agree).x)*0.5f);
                        Text(exit_agree);
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
                    SetNextWindowSize(ImVec2(io.DisplaySize.x*(0.85f-(1.0f-SCREEN_WIDTH*2/io.DisplaySize.x)*0.5f), io.DisplaySize.y*(0.75f-(1.0f-SCREEN_HEIGHT*2/io.DisplaySize.y)*0.5f)), ImGuiCond_Always);
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
                    SetNextWindowSize(ImVec2(io.DisplaySize.x*(0.85f-(1.0f-SCREEN_WIDTH*2/io.DisplaySize.x)*0.5f), io.DisplaySize.y*(0.75f-(1.0f-SCREEN_HEIGHT*2/io.DisplaySize.y)*0.5f)), ImGuiCond_Always);
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
                    SetNextWindowSize(ImVec2(io.DisplaySize.x*(0.60f-(1.0f-SCREEN_WIDTH*2/io.DisplaySize.x)*0.5f), 0), ImGuiCond_Always);
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
                                    Text(label[i]);
                                    SameLine(LABEL_WIDTH);
                                    SetNextItemWidth(-FLT_MIN - btn_small.x - style.ItemSpacing.x);
                                    float path_width = GetContentRegionAvail().x - btn_small.x - style.ItemSpacing.x - style.FramePadding.x*2;
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
                                    SetNextWindowSize(ImVec2(io.DisplaySize.x*(0.85f-(1.0f-SCREEN_WIDTH*2/io.DisplaySize.x)*0.5f), io.DisplaySize.y*(0.75f-(1.0f-SCREEN_HEIGHT*2/io.DisplaySize.y)*0.5f)), ImGuiCond_Always);
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
                                    memcpy(&cfg.main, &Config::get_defaults().main, sizeof(CFG::main));
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
                                if (InputInt("##screen_scale", &cfg.video.screen_scale, 1, 1, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank | (cfg.video.full_screen ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None))){
                                    cfg.video.screen_scale = std::min(std::max(cfg.video.screen_scale, 2), 5);
                                    board->set_window_size(SCREEN_WIDTH*cfg.video.screen_scale, SCREEN_HEIGHT*cfg.video.screen_scale);
                                }
                                SetCursorPosX(LABEL_WIDTH);
                                if (Checkbox("Full screen", &cfg.video.full_screen))
                                    board->set_full_screen(cfg.video.full_screen);
                                SeparatorText("Display");
                                Text("Type");
                                SameLine(LABEL_WIDTH);
                                if (RadioButton("LCD", &cfg.video.filter, Nearest))
                                    board->set_video_filter(Nearest);
                                SameLine();
                                if (RadioButton("CRT", &cfg.video.filter, Linear))
                                    board->set_video_filter(Linear);
                                SetCursorPosX(LABEL_WIDTH);
                                if (!cfg.main.full_speed){
                                    if (Checkbox("V-Sync", &cfg.video.vsync))
                                        board->set_vsync(cfg.video.vsync);
                                }else{
                                    bool none = cfg.video.vsync;
                                    PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
                                    Checkbox("V-Sync", &none);
                                    PopStyleVar();
                                }
                                Spacing();
                                SetCursorPosX(GetWindowWidth()-btn_size.x-style.WindowPadding.x);
                                if (Button("Defaults", btn_size)){
                                    memcpy(&cfg.video, &Config::get_defaults().video, sizeof(CFG::video));
                                    board->set_window_size(cfg.video.full_screen ? SCREEN_WIDTH*2 : SCREEN_WIDTH*cfg.video.screen_scale, cfg.video.full_screen ? SCREEN_HEIGHT*2 : SCREEN_HEIGHT*2);
                                    board->set_full_screen(cfg.video.full_screen);
                                    board->set_video_filter((Filter)cfg.video.filter);
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
                                    cfg.audio.lpf_rate = std::min(std::max(cfg.audio.lpf_rate, 5000), 22000);
                                    board->sound.set_lpf(cfg.audio.dsp_rate, cfg.audio.lpf_rate);
                                }
                                SeparatorText("Volume");
                                Text("AY");
                                SameLine(LABEL_WIDTH);
                                if (SliderFloat("##ay", &cfg.audio.ay_volume, 0.0, 1.0, "%.2f"))
                                    board->sound.set_ay_volume(cfg.audio.ay_volume);
                                Text("Speaker");
                                SameLine(LABEL_WIDTH);
                                if (SliderFloat("##speaker", &cfg.audio.speaker_volume, 0.0, 1.0, "%.2f"))
                                    board->sound.set_speaker_volume(cfg.audio.speaker_volume);
                                Text("Tape");
                                SameLine(LABEL_WIDTH);
                                if (SliderFloat("##tape", &cfg.audio.tape_volume, 0.0, 1.0, "%.2f"))
                                    board->sound.set_tape_volume(cfg.audio.tape_volume);
                                SeparatorText("AY mixer");
                                Text("Mode");
                                SameLine(LABEL_WIDTH);
                                if (Combo("##mode", &cfg.audio.ay_mixer, "ABC\0ACB\0Mono\0\0"))
                                    board->sound.set_mixer((AY_Mixer)cfg.audio.ay_mixer, cfg.audio.ay_side, cfg.audio.ay_center, cfg.audio.ay_penetr);
                                Text("Left/Right");
                                SameLine(LABEL_WIDTH);
                                if (SliderFloat("##ay_side", &cfg.audio.ay_side, 0.0, 1.0, "%.2f"))
                                    board->sound.set_mixer((AY_Mixer)cfg.audio.ay_mixer, cfg.audio.ay_side, cfg.audio.ay_center, cfg.audio.ay_penetr);
                                Text("Center");
                                SameLine(LABEL_WIDTH);
                                if (SliderFloat("##ay_center", &cfg.audio.ay_center, 0.0, 1.0, "%.2f"))
                                    board->sound.set_mixer((AY_Mixer)cfg.audio.ay_mixer, cfg.audio.ay_side, cfg.audio.ay_center, cfg.audio.ay_penetr);
                                Text("Penetration");
                                SameLine(LABEL_WIDTH);
                                if (SliderFloat("##ay_penetration", &cfg.audio.ay_penetr, 0.0, 1.0, "%.2f"))
                                    board->sound.set_mixer((AY_Mixer)cfg.audio.ay_mixer, cfg.audio.ay_side, cfg.audio.ay_center, cfg.audio.ay_penetr);
                                PopItemWidth();
                                Spacing();
                                SetCursorPosX(GetWindowWidth()-btn_size.x-style.WindowPadding.x);
                                if (Button("Defaults", btn_size)){
                                    memcpy(&cfg.audio, &Config::get_defaults().audio, sizeof(CFG::audio));
                                    board->sound.setup(cfg.audio.dsp_rate, cfg.audio.lpf_rate, board->frame_clk);
                                    board->sound.set_mixer((AY_Mixer)cfg.audio.ay_mixer, cfg.audio.ay_side, cfg.audio.ay_center, cfg.audio.ay_penetr);
                                    board->sound.set_ay_volume(cfg.audio.ay_volume);
                                    board->sound.set_speaker_volume(cfg.audio.speaker_volume);
                                    board->sound.set_tape_volume(cfg.audio.tape_volume);
                                }
                                EndTabItem();
                            }
                            if (BeginTabItem("Gamepad")){
                                static struct {
                                    const char *name;
                                    int *code;
                                } gamepad[6] = {
                                    {"Left", &cfg.gamepad.left },
                                    {"Right", &cfg.gamepad.right },
                                    {"Down", &cfg.gamepad.down },
                                    {"Up", &cfg.gamepad.up },
                                    {"A", &cfg.gamepad.a },
                                    {"B", &cfg.gamepad.b }
                                };
                                static int gamepad_idx = 6;
                                static int gamepad_log_pos = 0;
                                static char gamepad_log[1024];
                                SeparatorText("Configure");
                                InputTextMultiline("##gamepad_log", gamepad_log, IM_ARRAYSIZE(gamepad_log), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight()*7 + style.FramePadding.y*2));
                                Spacing();
                                SetCursorPosX((GetWindowWidth()-btn_size.x)/2);
                                if (Button("Start", btn_size)){
                                    gamepad_idx = 0;
                                    gamepad_code = 0;
                                    gamepad_log_pos = sprintf(&gamepad_log[gamepad_log_pos], "Press %s button", gamepad[gamepad_idx].name);
                                }
                                if (gamepad_idx < 6 && gamepad_code){
                                    *gamepad[gamepad_idx].code = gamepad_code;
                                    gamepad_code = 0;
                                    if (++gamepad_idx < 6 )
                                       gamepad_log_pos += sprintf(&gamepad_log[gamepad_log_pos], " Ok\nPress %s button", gamepad[gamepad_idx].name);
                                    else
                                       gamepad_log_pos += sprintf(&gamepad_log[gamepad_log_pos], " Ok\nDone.");
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
                                Text("Use gamepad");
                                SameLine(LABEL_WIDTH);
                                if (Checkbox("##gamepad", &cfg.ui.gamepad_ctrl))
                                    set_gamepad_ctrl(cfg.ui.gamepad_ctrl);
                                Spacing();
                                SetCursorPosX(GetWindowWidth()-btn_size.x-style.WindowPadding.x);
                                if (Button("Defaults", btn_size)){
                                    memcpy(&cfg.ui, &Config::get_defaults().ui, sizeof(CFG::ui));
                                    set_alpha(cfg.ui.alpha);
                                    set_gamepad_ctrl(cfg.ui.gamepad_ctrl);
                                }
                                EndTabItem();
                            }
                            EndTabBar();
                        }
                        End();
                    }
                    break;
                    /*
                case UI_Debugger:
                    break;
                    SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    SetNextWindowSize(ImVec2(SCREEN_WIDTH*2, SCREEN_HEIGHT*2), ImGuiCond_Always);
                    if (Begin("##debugger", NULL, UI_WindowFlags)){
                        static struct Debugger debugger(board->cpu);
                    }
                    */
            }
        }
        //ShowStyleEditor();
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
        mode = UI_Hidden;
    }
    bool is_active(){
        return mode != UI_Hidden;
    }
    void set_alpha(float alpha){
        ImGuiStyle &style = GetStyle();
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.05f, alpha);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, alpha * 0.035f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0f, 0.0f, 0.0f, alpha);
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
