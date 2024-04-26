#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "ext/ImGuiFileDialog/ImGuiFileDialog.h"
#include "ext/ImGuiFileDialog/ImGuiFileDialogConfig.h"
#include "base.h"
#include "ui.h"

using namespace ImGui;

#define WIDTH            250
#define HEIGHT           400
#define LEFT             100
#define FILE_WIDTH       600
#define FILE_HEIGHT      350

namespace UI {
    ImGuiIO *io = NULL;
    ImGuiStyle *style = NULL;
    ImVec2 btn_size = { 80.0f, 20.0f };
    GLuint kbd_layout_texture = 0;
    GLuint gamepad_texture = 0;
    GLuint x_texture = 0;
    UI_Mode ui_mode = UI_None;
    const char *kbd_path = "data/kbd_layout.png";
    const char *gamepad_path = "data/gamepad.png";
    const char *x_path = "data/x.png";
    IGFD::FileDialogConfig file_config = { .path = ".", .countSelectionMax = 1, .flags = ImGuiFileDialogFlags_Modal };

    GLuint load_texture(const char *path){
        GLuint texture;
        try{
            SDL_Surface *image = IMG_Load(path);
            SDL_Surface *surface = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_RGBA4444, 0);
            SDL_FreeSurface(image);
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, surface->pixels);
            SDL_FreeSurface(surface);
            return texture;
        }catch (std::exception &e){
            std::cerr << "Can't load image: " << path << "\n";
            return 0;
        }
    }

    void setup(SDL_Window *window, SDL_GLContext context, const char *glsl_version){
        IMGUI_CHECKVERSION();
        CreateContext();
        io = &GetIO();
        io->IniFilename = NULL;
        io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        // ImGui style
        StyleColorsDark();
        ImGui_ImplSDL2_InitForOpenGL(window, context);
        ImGui_ImplOpenGL3_Init(glsl_version);
        style = &GetStyle();
        style->Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style->WindowPadding                     = ImVec2(10, 10);
        style->WindowRounding                    = 3.0;
        style->FrameRounding                     = 2.0;
        style->GrabRounding                      = 2.0;
        style->TabRounding                       = 2.0;
        style->GrabMinSize                       = 10.0;
        //style->SeparatorTextPadding              = ImVec2(10.0, 8.0);
        style->SeparatorTextBorderSize           = 2.0;
        style->Colors[ImGuiCol_ChildBg].w        = 0.37f;
        style->Colors[ImGuiCol_Border]           = ImVec4(0.00f, 0.00f, 0.00f, 1.0f);
        style->ItemSpacing                       = ImVec2(8.0, 4.0);
        style->WindowTitleAlign                  = ImVec2(0.5, 0.5);

        kbd_layout_texture = load_texture(kbd_path);
        gamepad_texture = load_texture(gamepad_path);
        x_texture = load_texture(x_path);
    }

    bool event(SDL_Event &event){
        if (ui_mode != UI_None){
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE){
                if (IsPopupOpen(NULL, ImGuiPopupFlags_AnyPopup)){
                    ImGuiFileDialog::Instance()->Close();
                    if (ui_mode == UI_OpenFile || ui_mode == UI_SaveFile)
                        ui_mode = UI_None;
                }else
                    ui_mode = UI_None;
            }
            return true;
        }else{
            if (event.type == SDL_KEYDOWN){
                switch (event.key.keysym.sym){
                    case SDLK_ESCAPE:
                        ui_mode = UI_Exit;
                        break;
                    case SDLK_F1:
                        ui_mode = UI_KbdLayout;
                        break;
                    case SDLK_F2:
                        ui_mode = UI_OpenFile;
                        break;
                    case SDLK_F3:
                        ui_mode = UI_SaveFile;
                        break;
                    case SDLK_F4:
                        ui_mode = UI_Settings;
                        break;
                }
            }
        }
        return false;
    }

    bool frame(CFG *cfg, Board *board){
        static bool status = false;
        if (ui_mode != UI_None){
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            NewFrame();
            switch (ui_mode){
                case UI_None:
                    break;
                case UI_Exit:
                    SetNextWindowPos(ImVec2(io->DisplaySize.x*0.5f, io->DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
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
                            ui_mode = UI_None;
                        End();
                    }
                    break;
                case UI_KbdLayout:
                    SetNextWindowPos(ImVec2(io->DisplaySize.x*0.5f, io->DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    SetNextWindowSize(ImVec2(io->DisplaySize.x * 0.8f, io->DisplaySize.y * 0.5f), ImGuiCond_Always);
                    if (Begin("Keyboard layout", NULL, UI_WindowFlags)){
                        Image((ImTextureID)kbd_layout_texture, GetContentRegionAvail());
                        End();
                    }
                    break;
                case UI_OpenFile:
                    SetNextWindowSize(ImVec2(FILE_WIDTH, FILE_HEIGHT), ImGuiCond_Always);
                    SetNextWindowPos(ImVec2(io->DisplaySize.x*0.5f, io->DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    ImGuiFileDialog::Instance()->OpenDialog("##file_open", "Open file", ".z80;.tap;.trd;.scl {(([.]z80|Z80|trd|TRD|scl|SCL|tap|TAP))}", file_config);
                    if (ImGuiFileDialog::Instance()->Display("##file_open", UI_WindowFlags)){
                        if (ImGuiFileDialog::Instance()->IsOk())
                            board->load_file(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
                        ImGuiFileDialog::Instance()->Close();
                        ui_mode = UI_None;
                    }
                    break;
                case UI_SaveFile:
                    SetNextWindowSize(ImVec2(FILE_WIDTH, FILE_HEIGHT), ImGuiCond_Always);
                    SetNextWindowPos(ImVec2(io->DisplaySize.x*0.5f, io->DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    ImGuiFileDialog::Instance()->OpenDialog("##file_save", "Save file", ".z80;.trd; {(([.]z80|Z80|trd|TRD))}", file_config);
                    if (ImGuiFileDialog::Instance()->Display("##file_save", UI_WindowFlags)){
                        if (ImGuiFileDialog::Instance()->IsOk())
                            board->save_file(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
                        ImGuiFileDialog::Instance()->Close();
                        ui_mode = UI_None;
                    }
                    break;
                case UI_Settings:
                    static struct {
                        const char *label;
                        const char *name;
                    } rom[sizeof(ROM_BANK)] = {
                        { .label = "##rom_trdos", .name = "Rom TR-Dos" },
                        { .label = "##rom_128",   .name = "Rom 128" },
                        { .label = "##rom_48",    .name = "Rom 48" }
                    };
                    float glyph_width = CalcTextSize("A").x;
                    SetNextWindowPos(ImVec2(io->DisplaySize.x*0.5f, io->DisplaySize.y/2-HEIGHT/2), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
                    SetNextWindowSize(ImVec2(WIDTH, 0), ImGuiCond_Always);
                    if (Begin("Settings", NULL, UI_WindowFlags | ImGuiWindowFlags_AlwaysAutoResize)){
                        if (BeginTabBar("tabs")){
                            if (BeginTabItem("General")){
                                float width = GetContentRegionAvail().x;
                                SeparatorText("Hardware");
                                AlignTextToFramePadding();
                                Text("Model");
                                SameLine(LEFT);
                                SetNextItemWidth(width-(LEFT-style->ItemSpacing.x));
                                if (Combo("##model", &cfg->main.hw_model, "Pentagon - 128k\0Sinclair - 128k\0Sinclair - 48k\0\0")){
                                    board->set_hw((HW_Model)cfg->main.hw_model);
                                    board->reset();
                                }
                                SetCursorPosX(LEFT);
                                Checkbox("Full speed", &cfg->main.full_speed);
                                SeparatorText("BIOS");
                                for (int i = 0; i < (int)sizeof(ROM_BANK) - 1; i++){
                                    AlignTextToFramePadding();
                                    Text(rom[i].name);
                                    SameLine(LEFT);
                                    SetNextItemWidth(width-(LEFT+btn_size.x/2));
                                    if (strlen(cfg->main.rom_path[i])*glyph_width > width-(LEFT+btn_size.x/2+style->FramePadding.x*2))
                                        InputText(rom[i].label, cfg->main.rom_path[i]+strlen(cfg->main.rom_path[i])-(size_t)((width-(LEFT+btn_size.x/2+style->FramePadding.x*2))/glyph_width), PATH_MAX, ImGuiInputTextFlags_ReadOnly);
                                    else 
                                        InputText(rom[i].label, cfg->main.rom_path[i], PATH_MAX, ImGuiInputTextFlags_ReadOnly);
                                    SameLine();
                                    PushID(i);
                                    if (Button("Load", ImVec2(btn_size.x / 2, btn_size.y))){
                                        ImGuiFileDialog::Instance()->OpenDialog("##load_rom", "Load ROM", ".rom {(([.]rom|ROM))}", file_config);
                                        SetNextWindowPos(ImVec2(io->DisplaySize.x*0.5f, io->DisplaySize.y*0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                                        SetNextWindowSize(ImVec2(FILE_WIDTH, FILE_HEIGHT), ImGuiCond_Always);
                                    }
                                    PopID();
                                    if (ImGuiFileDialog::Instance()->Display("##load_rom", UI_WindowFlags)){
                                        if (ImGuiFileDialog::Instance()->IsOk()){
                                            strcpy(cfg->main.rom_path[i], ImGuiFileDialog::Instance()->GetFilePathName().c_str());
                                            board->ula.load_rom((ROM_BANK)i, cfg->main.rom_path[i]);
                                            board->reset();
                                        }
                                        ImGuiFileDialog::Instance()->Close();
                                    }
                                }
                                SetCursorPos(ImVec2(GetWindowWidth()-btn_size.x-style->WindowPadding.x, GetCursorPosY()+style->ItemSpacing.y*3));
                                if (Button("Reset", btn_size)){
                                    memcpy(&cfg->main, &Config::get_defaults()->main, sizeof(CFG::main));
                                    board->set_hw((HW_Model)cfg->main.hw_model);
                                    board->reset();
                                }
                                EndTabItem();
                            }
                            if (BeginTabItem("Video")){
                                float width = GetContentRegionAvail().x;
                                SeparatorText("Resolution");
                                AlignTextToFramePadding();
                                Text("Scale");
                                SameLine(LEFT);
                                SetNextItemWidth(width-(LEFT-style->ItemSpacing.x));
                                if (InputInt("##scale", &cfg->video.scale, 1, 1, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank | (cfg->video.full_screen ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None))){
                                    cfg->video.scale = std::min(std::max(cfg->video.scale, 2), 5);
                                    board->viewport_resize(SCREEN_WIDTH*cfg->video.scale, SCREEN_HEIGHT*cfg->video.scale);
                                }
                                SetCursorPosX(LEFT);
                                if (Checkbox("Full screen", &cfg->video.full_screen)){
                                    if (cfg->video.full_screen)
                                        board->viewport_resize(SCREEN_WIDTH*2, SCREEN_HEIGHT*2);
                                    else
                                        board->viewport_resize(SCREEN_WIDTH*cfg->video.scale, SCREEN_HEIGHT*cfg->video.scale);
                                    board->full_screen(cfg->video.full_screen);
                                }
                                board->full_screen(cfg->video.full_screen);
                                SeparatorText("Display");
                                AlignTextToFramePadding();
                                Text("Type");
                                SameLine(LEFT);
                                if (RadioButton("LCD", &cfg->video.filter, VF_NEAREST))
                                    board->video_filter(VF_NEAREST);
                                SameLine();
                                if (RadioButton("CRT", &cfg->video.filter, VF_LINEAR))
                                    board->video_filter(VF_LINEAR);
                                SetCursorPosX(LEFT);
                                if (Checkbox("V-Sync", &cfg->video.vsync))
                                    board->vsync(cfg->video.vsync);
                                SetCursorPos(ImVec2(GetWindowWidth()-btn_size.x-style->WindowPadding.x, GetCursorPosY()+style->ItemSpacing.y*3));
                                if (Button("Reset", btn_size)){
                                    memcpy(&cfg->video, &Config::get_defaults()->video, sizeof(CFG::video));
                                    if (cfg->video.full_screen)
                                        board->viewport_resize(SCREEN_WIDTH*2, SCREEN_HEIGHT*2);
                                    else
                                        board->viewport_resize(SCREEN_WIDTH*cfg->video.scale, SCREEN_HEIGHT*cfg->video.scale);
                                    board->full_screen(cfg->video.full_screen);
                                    board->video_filter(VF_NEAREST);
                                    board->vsync(cfg->video.vsync);
                                }
                                EndTabItem();
                            }
                            if (BeginTabItem("Audio")){
                                float width = GetContentRegionAvail().x;
                                SeparatorText("Frequency");
                                AlignTextToFramePadding();
                                Text("Audio rate");
                                SameLine(LEFT);
                                SetNextItemWidth(width-(LEFT-style->ItemSpacing.x));
                                if (InputInt("##dsp", &cfg->audio.sample_rate, 1000, 10000, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue)){
                                    cfg->audio.sample_rate = std::min(std::max(cfg->audio.sample_rate, 11025), 119200);
                                    board->sound.init(cfg->audio.sample_rate, board->frame_clk);
                                }
                                AlignTextToFramePadding();
                                Text("Low-pass cut");
                                SameLine(LEFT);
                                SetNextItemWidth(width-(LEFT-style->ItemSpacing.x));
                                if (InputInt("##lpf", &cfg->audio.lpf_rate, 1000, 1000, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue)){
                                    cfg->audio.lpf_rate = std::min(std::max(cfg->audio.lpf_rate, 5000), 22000);
                                    board->sound.setup_lpf(cfg->audio.lpf_rate);
                                }
                                SeparatorText("Volume");
                                AlignTextToFramePadding();
                                Text("AY");
                                SameLine(LEFT);
                                SetNextItemWidth(width-(LEFT-style->ItemSpacing.x));
                                if (SliderFloat("##ay", &cfg->audio.ay_volume, 0.0, 1.0, "%.2f"))
                                    board->sound.set_ay_volume(cfg->audio.ay_volume);
                                AlignTextToFramePadding();
                                Text("Speaker");
                                SameLine(LEFT);
                                SetNextItemWidth(width-(LEFT-style->ItemSpacing.x));
                                if (SliderFloat("##speaker", &cfg->audio.speaker_volume, 0.0, 1.0, "%.2f"))
                                    board->sound.set_speaker_volume(cfg->audio.speaker_volume);
                                AlignTextToFramePadding();
                                Text("Tape");
                                SameLine(LEFT);
                                SetNextItemWidth(width-(LEFT-style->ItemSpacing.x));
                                if (SliderFloat("##tape", &cfg->audio.tape_volume, 0.0, 1.0, "%.2f"))
                                    board->sound.set_tape_volume(cfg->audio.tape_volume);
                                SeparatorText("AY mixer");
                                AlignTextToFramePadding();
                                Text("Mode");
                                SameLine(LEFT);
                                SetNextItemWidth(width-(LEFT-style->ItemSpacing.x));
                                if (Combo("##mode", &cfg->audio.ay_mixer_mode, "abc\0acb\0mono\0\0")){
                                    board->sound.set_mixer_mode(cfg->audio.ay_mixer_mode);
                                    board->sound.set_mixer_levels(cfg->audio.ay_side_level, cfg->audio.ay_center_level, cfg->audio.ay_interpenetration_level);
                                }
                                AlignTextToFramePadding();
                                Text("Left/Right");
                                SameLine(LEFT);
                                SetNextItemWidth(width-(LEFT-style->ItemSpacing.x));
                                if (SliderFloat("##ay_side", &cfg->audio.ay_side_level, 0.0, 1.0, "%.2f"))
                                    board->sound.set_mixer_levels(cfg->audio.ay_side_level, -1.0, -1.0);
                                AlignTextToFramePadding();
                                Text("Center");
                                SameLine(LEFT);
                                SetNextItemWidth(width-(LEFT-style->ItemSpacing.x));
                                if (SliderFloat("##ay_center", &cfg->audio.ay_center_level, 0.0, 1.0, "%.2f"))
                                    board->sound.set_mixer_levels(-1.0, cfg->audio.ay_center_level, -1.0);
                                AlignTextToFramePadding();
                                Text("Penetration");
                                SameLine(LEFT);
                                SetNextItemWidth(width-(LEFT-style->ItemSpacing.x));
                                if (SliderFloat("##ay_penetration", &cfg->audio.ay_interpenetration_level, 0.0, 1.0, "%.2f"))
                                    board->sound.set_mixer_levels(-1.0, -1.0, cfg->audio.ay_interpenetration_level);
                                SetCursorPos(ImVec2(GetWindowWidth()-btn_size.x-style->WindowPadding.x, GetCursorPosY()+style->ItemSpacing.y*3));
                                if (Button("Reset", btn_size)){
                                    memcpy(&cfg->audio, &Config::get_defaults()->audio, sizeof(CFG::audio));
                                    board->sound.init(cfg->audio.sample_rate, board->frame_clk);
                                    board->sound.setup_lpf(cfg->audio.lpf_rate);
                                    board->sound.set_ay_volume(cfg->audio.ay_volume);
                                    board->sound.set_speaker_volume(cfg->audio.speaker_volume);
                                    board->sound.set_tape_volume(cfg->audio.tape_volume);
                                    board->sound.set_mixer_mode(cfg->audio.ay_mixer_mode);
                                    board->sound.set_mixer_levels(cfg->audio.ay_side_level, cfg->audio.ay_center_level, cfg->audio.ay_interpenetration_level);
                                }
                                EndTabItem();
                            }
                            /*
                            if (BeginTabItem("Gamepad")){
                                static struct { const char *name; ImVec2 pos; } gamepad[] = {
                                    { "Left", ImVec2(28.0f, 46.0f) },
                                    { "Right", ImVec2(55.0f, 45.0f) },
                                    { "Down", ImVec2(40.0f, 60.0f) },
                                    { "Up", ImVec2(40.0f, 33.0) },
                                    { "A", ImVec2(159.0, 46.0) },
                                    { "B", ImVec2(175.0, 30.0) }
                                };
                                static int gamepad_button_idx = -1;
                                float width = GetContentRegionAvail().x;
                                float hover_size = 24.0f;
                                float gamepad_aspect = 616.0f / 425.0f;
                                ImVec2 gamepad_pos = { width * 0.2f, GetCursorPosY()};
                                SetCursorPos(gamepad_pos);
                                Image((ImTextureID)gamepad_texture, ImVec2(width * 0.8f, width * 0.8f / gamepad_aspect));
                                //SetCursorPosX((width - CalcTextSize(gamepad[button_idx].name).x) / 2);
                                //Text(gamepad[button_idx].name);
                                //if (gamepad_config != JOY_NONE)
                                //SetCursorPosY(gamepad_pos.y + gamepad_size.y - 20.0f);
                                //SetCursorPosX((GetContentRegionAvail().x - CalcTextSize(gamepad_button_name[gamepad_button_idx]).x) / 2);
                                if (gamepad_button_idx == -1){
                                    SetCursorPos(ImVec2((width - btn_size.x * 2) / 2, GetCursorPosY() + 10.0f));
                                    Button("Configure", ImVec2(btn_size.x * 2, btn_size.y));
                                    //    gamepad_button_idx++;
                                }else{
                                    if (Button("Next", ImVec2(btn_size.x * 2, btn_size.y)))
                                        if (++gamepad_button_idx >= (int)sizeof(gamepad))
                                            gamepad_button_idx = 0;
                                    SetCursorPos(ImVec2(gamepad_pos.x + gamepad[gamepad_button_idx].pos.x, gamepad_pos.y + gamepad[gamepad_button_idx].pos.y));
                                    Image((ImTextureID)x_texture, ImVec2(hover_size, hover_size));
                                }
                                EndTabItem();
                            }*/
                            EndTabBar();
                        }
                        End();
                    }
                    break;
            }
            Render();
            ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());
        }
        return status;
    }
}
