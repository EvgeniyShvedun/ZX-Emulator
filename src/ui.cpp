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
#include "ui.h"

#define WIDTH            350
#define HEIGHT           400
#define LEFT             100

#define KBD_IMAGE_PATH                    "data/kbd_layout.png"

using namespace ImGui;

namespace UI {
    ImVec2 btn_size = { 80.0f, 20.0f };
    ImVec2 btn_small = { 40.0f, 20.0f };
    int gamepad_code = 0;
    GLuint kbd_texture = 0;
    UI_Mode mode = UI_Hidden;
    IGFD::FileDialogConfig file_config = { .path = ".", .countSelectionMax = 1, .flags = ImGuiFileDialogFlags_Modal };

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

    void setup(SDL_Window *window, SDL_GLContext context, const char *glsl_version){
        IMGUI_CHECKVERSION();
        CreateContext();
        ImGuiIO &io = GetIO();
        io.IniFilename = NULL;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        StyleColorsDark();
        ImGui_ImplSDL2_InitForOpenGL(window, context);
        ImGui_ImplOpenGL3_Init(glsl_version);
        ImGuiStyle &style = GetStyle();
        style.WindowPadding = ImVec2(10, 10);
        style.WindowRounding = 3.0;
        style.FrameRounding = 2.0;
        style.GrabRounding = 2.0;
        style.TabRounding = 2.0;
        style.GrabMinSize = 10.0;
        //style.SeparatorTextPadding = ImVec2(10.0, 8.0);
        style.WindowTitleAlign = ImVec2(0.5, 0.5);
        //style.SeparatorTextBorderSize = 2.0;
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_ChildBg].w = 0.37f;
        //style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 1.0f);
        //style.ItemSpacing = ImVec2(8.0, 4.0);
        kbd_texture = load_texture(KBD_IMAGE_PATH);
    }

    bool event(SDL_Event &event){
        if (event.type == SDL_KEYDOWN){
            if (event.key.repeat)
                return false;
            if (mode == UI_Hidden){
                switch (event.key.keysym.sym){
                    case SDLK_ESCAPE:
                        mode = UI_Exit;
                        break;
                    case SDLK_F1:
                        mode = UI_KbdLayout;
                        break;
                    case SDLK_F2:
                        mode = UI_OpenFile;
                        break;
                    case SDLK_F3:
                        mode = UI_SaveFile;
                        break;
                    case SDLK_F4:
                        mode = UI_Settings;
                        break;
                }
            }else{
                if (event.key.keysym.sym == SDLK_ESCAPE && !IsPopupOpen(NULL, ImGuiPopupFlags_AnyPopup)){
                    mode = UI_Hidden;
                    return true;
                }
            }
        }else
            gamepad_code = event.type == SDL_JOYBUTTONDOWN ? event.jbutton.button : 0;
        ImGui_ImplSDL2_ProcessEvent(&event);
        return mode != UI_Hidden;
    }

    bool frame(CFG &cfg, Board *board){
        bool status = false;
        if (mode == UI_Hidden)
            return status;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        NewFrame();
        ImGuiIO &io = GetIO();
        ImGuiStyle &style = GetStyle();
        switch (mode){
            case UI_Hidden:
                break;
            case UI_Exit:
                SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                if (Begin("Exit", NULL, UI_WindowFlags | ImGuiWindowFlags_AlwaysAutoResize)){
                    SetCursorPosX((GetWindowSize().x - CalcTextSize("Do you exit ?").x) * 0.5);
                    Text("Do you exit ?");
                    Spacing();
                    Spacing();
                    if (Button("Yes", btn_size))
                        status = true;
                    SetItemDefaultFocus();
                    SameLine();
                    if (Button("No", btn_size))
                        mode = UI_Hidden;
                    End();
                }
                break;
            case UI_KbdLayout:
                SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.8f, io.DisplaySize.y * 0.5f), ImGuiCond_Always);
                if (Begin("Keyboard", NULL, UI_WindowFlags | ImGuiWindowFlags_NoDecoration)){
                    Image((ImTextureID)kbd_texture, GetContentRegionAvail());
                    End();
                }
                break;
            case UI_OpenFile:
                SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                SetNextWindowSize(ImVec2(SCREEN_WIDTH*2*0.87f, SCREEN_HEIGHT*2*0.65f), ImGuiCond_Always);
                //file_config.flags &= ~ImGuiFileDialogFlags_ConfirmOverwrite;
                ImGuiFileDialog::Instance()->OpenDialog("##file_open", "Open file", ".z80;.tap;.trd;.scl {(([.]z80|Z80|trd|TRD|scl|SCL|tap|TAP))}", file_config);
                if (ImGuiFileDialog::Instance()->Display("##file_open", UI_WindowFlags)){
                    if (ImGuiFileDialog::Instance()->IsOk())
                        board->load_file(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
                    ImGuiFileDialog::Instance()->Close();
                    mode = UI_Hidden;
                }
                break;
            case UI_SaveFile:
                SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                SetNextWindowSize(ImVec2(SCREEN_WIDTH*2*0.87f, SCREEN_HEIGHT*2*0.65f), ImGuiCond_Always);
                //file_config.flags |= ImGuiFileDialogFlags_ConfirmOverwrite;
                ImGuiFileDialog::Instance()->OpenDialog("##file_save", "Save file", ".z80;.trd; {(([.]z80|Z80|trd|TRD))}", file_config);
                if (ImGuiFileDialog::Instance()->Display("##file_save", UI_WindowFlags)){
                    if (ImGuiFileDialog::Instance()->IsOk())
                        board->save_file(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
                    ImGuiFileDialog::Instance()->Close();
                    mode = UI_Hidden;
                }
                break;
            case UI_Settings:
                SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y/2-HEIGHT/2), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
                SetNextWindowSize(ImVec2(WIDTH, 0), ImGuiCond_Always);
                if (Begin("Settings", NULL, UI_WindowFlags | ImGuiWindowFlags_AlwaysAutoResize)){
                    if (BeginTabBar("tabs")){
                        if (BeginTabItem("Main")){
                            float width = GetContentRegionAvail().x;
                            SeparatorText("Hardware");
                            AlignTextToFramePadding();
                            Text("Model");
                            SameLine(LEFT);
                            SetNextItemWidth(width-(LEFT-style.ItemSpacing.x));
                            if (Combo("##model", &cfg.main.model, "Pentagon - 128k\0Sinclair - 128k\0Sinclair - 48k\0\0")){
                                board->setup((Hardware)cfg.main.model);
                                board->reset();
                            }
                            SetCursorPosX(LEFT);
                            if (Checkbox("Full speed", &cfg.main.full_speed)){
                                if (cfg.main.full_speed)
                                    board->set_vsync(false);
                                else
                                    board->set_vsync(cfg.video.vsync);
                            }
                            SeparatorText("BIOS");
                            for (int i = 0; i < (int)sizeof(ROM_Bank) - 1; i++){
                                static struct {
                                    const char *label;
                                    const char *name;
                                } rom[sizeof(ROM_Bank)] = {
                                    { .label = "##rom_trdos", .name = "Rom TR-Dos" },
                                    { .label = "##rom_128",   .name = "Rom 128" },
                                    { .label = "##rom_48",    .name = "Rom 48" }
                                };
                                AlignTextToFramePadding();
                                Text(rom[i].name);
                                SameLine(LEFT);
                                SetNextItemWidth(width - LEFT - btn_small.x);
                                if (strlen(cfg.main.rom_path[i])*CalcTextSize("A").x > width - LEFT - btn_small.x - style.FramePadding.x*2)
                                    InputText(rom[i].label, cfg.main.rom_path[i] + strlen(cfg.main.rom_path[i]) -
                                            (size_t)((width - LEFT - btn_small.x - style.FramePadding.x*2) / CalcTextSize("A").x), PATH_MAX, ImGuiInputTextFlags_ReadOnly);
                                else 
                                    InputText(rom[i].label, cfg.main.rom_path[i], PATH_MAX, ImGuiInputTextFlags_ReadOnly);
                                SameLine();
                                PushID(i);
                                if (Button("Load", btn_small)){
                                    ImGuiFileDialog::Instance()->OpenDialog("##load_rom", "Load ROM", ".rom {(([.]rom|ROM))}", file_config);
                                    SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                                    SetNextWindowSize(ImVec2(SCREEN_WIDTH*2*0.87f, SCREEN_HEIGHT*2*0.65f), ImGuiCond_Always);
                                }
                                PopID();
                                if (ImGuiFileDialog::Instance()->Display("##load_rom", UI_WindowFlags)){
                                    if (ImGuiFileDialog::Instance()->IsOk()){
                                        strcpy(cfg.main.rom_path[i], ImGuiFileDialog::Instance()->GetFilePathName().c_str());
                                        board->ula.load_rom((ROM_Bank)i, cfg.main.rom_path[i]);
                                        board->reset();
                                    }
                                    ImGuiFileDialog::Instance()->Close();
                                }
                            }
                            Spacing();
                            SetCursorPosX(GetWindowWidth()-btn_size.x-style.WindowPadding.x);
                            if (Button("Defaults", btn_size)){
                                memcpy(&cfg.main, &Config::get_defaults().main, sizeof(CFG::main));
                                board->setup((Hardware)cfg.main.model);
                                board->reset();
                            }
                            EndTabItem();
                        }
                        if (BeginTabItem("Video")){
                            float width = GetContentRegionAvail().x;
                            float left = 75.0;
                            SeparatorText("Resolution");
                            AlignTextToFramePadding();
                            Text("Scale");
                            SameLine(left);
                            SetNextItemWidth(width-(left-style.ItemSpacing.x));
                            if (InputInt("##screen_scale", &cfg.video.screen_scale, 1, 1, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank | (cfg.video.full_screen ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None))){
                                cfg.video.screen_scale = std::min(std::max(cfg.video.screen_scale, 2), 5);
                                board->set_viewport_size(SCREEN_WIDTH*cfg.video.screen_scale, SCREEN_HEIGHT*cfg.video.screen_scale);
                            }
                            SetCursorPosX(left);
                            if (Checkbox("Full screen", &cfg.video.full_screen)){
                                if (cfg.video.full_screen)
                                    board->set_viewport_size(SCREEN_WIDTH*2, SCREEN_HEIGHT*2);
                                else
                                    board->set_viewport_size(SCREEN_WIDTH*cfg.video.screen_scale, SCREEN_HEIGHT*cfg.video.screen_scale);
                                board->set_full_screen(cfg.video.full_screen);
                            }
                            SeparatorText("Display");
                            AlignTextToFramePadding();
                            Text("Type");
                            SameLine(left);
                            if (RadioButton("LCD", &cfg.video.filter, VF_Nearest))
                                board->set_video_filter(VF_Nearest);
                            SameLine();
                            if (RadioButton("CRT", &cfg.video.filter, VF_Linear))
                                board->set_video_filter(VF_Linear);
                            SetCursorPosX(left);
                            if (!cfg.main.full_speed){
                                if (Checkbox("V-Sync", &cfg.video.vsync))
                                    board->set_vsync(cfg.video.vsync);
                            }else{
                                bool vsync = cfg.video.vsync;
                                PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
                                if (Checkbox("V-Sync", &cfg.video.vsync))
                                    cfg.video.vsync = vsync;
                                PopStyleVar();
                            }
                            Spacing();
                            SetCursorPosX(GetWindowWidth()-btn_size.x-style.WindowPadding.x);
                            if (Button("Defaults", btn_size)){
                                memcpy(&cfg.video, &Config::get_defaults().video, sizeof(CFG::video));
                                if (cfg.video.full_screen)
                                    board->set_viewport_size(SCREEN_WIDTH*2, SCREEN_HEIGHT*2);
                                else
                                    board->set_viewport_size(SCREEN_WIDTH*cfg.video.screen_scale, SCREEN_HEIGHT*cfg.video.screen_scale);
                                board->set_full_screen(cfg.video.full_screen);
                                board->set_video_filter((VideoFilter)cfg.video.filter);
                                board->set_vsync(cfg.video.vsync);
                            }
                            EndTabItem();
                        }
                        if (BeginTabItem("Audio")){
                            float width = GetContentRegionAvail().x;
                            SeparatorText("Frequency");
                            AlignTextToFramePadding();
                            Text("Audio rate");
                            SameLine(LEFT);
                            SetNextItemWidth(width-(LEFT-style.ItemSpacing.x));
                            if (InputInt("##dsp", &cfg.audio.dsp_rate, 1000, 10000, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue)){
                                cfg.audio.dsp_rate = std::min(std::max(cfg.audio.dsp_rate, 11025), 192000);
                                board->sound.init(cfg.audio.dsp_rate, board->frame_clk);
                            }
                            AlignTextToFramePadding();
                            Text("Low-pass cut");
                            SameLine(LEFT);
                            SetNextItemWidth(width-(LEFT-style.ItemSpacing.x));
                            if (InputInt("##lpf", &cfg.audio.lpf_rate, 1000, 1000, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue)){
                                cfg.audio.lpf_rate = std::min(std::max(cfg.audio.lpf_rate, 5000), 22000);
                                board->sound.setup_lpf(cfg.audio.lpf_rate);
                            }
                            SeparatorText("Volume");
                            AlignTextToFramePadding();
                            Text("AY");
                            SameLine(LEFT);
                            SetNextItemWidth(width-(LEFT-style.ItemSpacing.x));
                            if (SliderFloat("##ay", &cfg.audio.ay_volume, 0.0, 1.0, "%.2f"))
                                board->sound.set_ay_volume(cfg.audio.ay_volume);
                            AlignTextToFramePadding();
                            Text("Speaker");
                            SameLine(LEFT);
                            SetNextItemWidth(width-(LEFT-style.ItemSpacing.x));
                            if (SliderFloat("##speaker", &cfg.audio.speaker_volume, 0.0, 1.0, "%.2f"))
                                board->sound.set_speaker_volume(cfg.audio.speaker_volume);
                            AlignTextToFramePadding();
                            Text("Tape");
                            SameLine(LEFT);
                            SetNextItemWidth(width-(LEFT-style.ItemSpacing.x));
                            if (SliderFloat("##tape", &cfg.audio.tape_volume, 0.0, 1.0, "%.2f"))
                                board->sound.set_tape_volume(cfg.audio.tape_volume);
                            SeparatorText("AY mixer");
                            AlignTextToFramePadding();
                            Text("Mode");
                            SameLine(LEFT);
                            SetNextItemWidth(width-(LEFT-style.ItemSpacing.x));
                            if (Combo("##mode", &cfg.audio.ay_mixer, "abc\0acb\0mono\0\0")){
                                board->sound.set_mixer_mode(cfg.audio.ay_mixer);
                                board->sound.set_mixer_levels(cfg.audio.ay_side, cfg.audio.ay_center, cfg.audio.ay_penetr);
                            }
                            AlignTextToFramePadding();
                            Text("Left/Right");
                            SameLine(LEFT);
                            SetNextItemWidth(width-(LEFT-style.ItemSpacing.x));
                            if (SliderFloat("##ay_side", &cfg.audio.ay_side, 0.0, 1.0, "%.2f"))
                                board->sound.set_mixer_levels(cfg.audio.ay_side, -1.0, -1.0);
                            AlignTextToFramePadding();
                            Text("Center");
                            SameLine(LEFT);
                            SetNextItemWidth(width-(LEFT-style.ItemSpacing.x));
                            if (SliderFloat("##ay_center", &cfg.audio.ay_center, 0.0, 1.0, "%.2f"))
                                board->sound.set_mixer_levels(-1.0, cfg.audio.ay_center, -1.0);
                            AlignTextToFramePadding();
                            Text("Penetration");
                            SameLine(LEFT);
                            SetNextItemWidth(width-(LEFT-style.ItemSpacing.x));
                            if (SliderFloat("##ay_penetration", &cfg.audio.ay_penetr, 0.0, 1.0, "%.2f"))
                                board->sound.set_mixer_levels(-1.0, -1.0, cfg.audio.ay_penetr);
                            Spacing();
                            SetCursorPosX(GetWindowWidth()-btn_size.x-style.WindowPadding.x);
                            if (Button("Defaults", btn_size)){
                                memcpy(&cfg.audio, &Config::get_defaults().audio, sizeof(CFG::audio));
                                board->sound.init(cfg.audio.dsp_rate, board->frame_clk);
                                board->sound.setup_lpf(cfg.audio.lpf_rate);
                                board->sound.set_ay_volume(cfg.audio.ay_volume);
                                board->sound.set_speaker_volume(cfg.audio.speaker_volume);
                                board->sound.set_tape_volume(cfg.audio.tape_volume);
                                board->sound.set_mixer_mode(cfg.audio.ay_mixer);
                                board->sound.set_mixer_levels(cfg.audio.ay_side, cfg.audio.ay_center, cfg.audio.ay_penetr);
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
                        EndTabBar();
                    }
                    End();
                }
                break;
            case UI_Debugger:
                break;
        }
        Render();
        ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());
        return status;
    }
}
