#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "build/ImGuiFileDialog/ImGuiFileDialog.h"
#include "build/ImGuiFileDialog/ImGuiFileDialogConfig.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include "base.h"
#include "config.h"
#include <math.h>

#include "disasm.h"

using namespace std;

#define TITLE "Sinclair Research 2024"
#define START_MSG "ZX-Spectrum Emulator v1.1 by Evgeniy Shvedun 2006-2024.\n"
#define EXIT_MSG "---\n"\
                 "Facebook: https://www.facebook.com/profile.php?id=61555407730196\n"\
                 "Github: https://github.com/EvgeniyShvedun/ZX-Emulator\n"

//#define FRAME_TIME
#define FRAME_LIMIT 10000

#define UI_STYLE_EDIT       -2
#define UI_EXIT             -1
#define UI_NONE             0
#define UI_KBLAYOUT         1
#define UI_OPEN_SNAPSHOT    2
#define UI_SETTINGS         5
#define UI_DEBUGGER         6


int window_width = SCREEN_WIDTH;
int window_height = SCREEN_HEIGHT;

Config *p_cfg = NULL;
Board *p_board = NULL;
Keyboard *p_keyboard = NULL;
Sound *p_sound = NULL;
WD1793 *p_wd1793 = NULL;
Tape *p_tape = NULL;
KMouse *p_mouse = NULL;
KJoystick *p_joystick = NULL;

SDL_AudioDeviceID audio_device_id = 0;

SDL_Window *window = NULL;
SDL_GLContext gl_context = NULL;
GLuint pbo = 0;
GLuint screen_texture = 0;

void release_all(){
    if (audio_device_id)
        SDL_PauseAudioDevice(audio_device_id, 0);
    audio_device_id = 0;

    DELETE(p_cfg);
    DELETE(p_board);
    DELETE(p_keyboard);
    DELETE(p_sound);
    DELETE(p_wd1793);
    DELETE(p_tape);
    DELETE(p_joystick);
    DELETE(p_mouse);

    if (screen_texture)
        glDeleteTextures(1, &screen_texture);
    screen_texture = 0;
    if (pbo)
        glDeleteBuffers(1, &pbo);
    pbo = 0;
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int fatal_error(const char *msg){
    printf("ERROR: %s\n", msg);
    release_all();
    return -1;
}

void load_file(const char *ptr){
    int len = strlen(ptr);
    if (len > 4){
        if ((!strcmp(ptr + len - 4, ".z80") or !strcmp(ptr + len - 4, ".Z80"))){
            p_board->load_z80(ptr);
        }else{
            if (!strcmp(ptr + len - 4, ".tap") or !strcmp(ptr + len - 4, ".TAP")){
                p_tape->load_tap(ptr);
            }
            if (!strcmp(ptr + len - 4, ".trd") or !strcmp(ptr + len - 4, ".TRD")){
                p_wd1793->load_trd(0, ptr);
            }
        }
    }
}

GLushort load_texture(const char *p_path){
    GLuint tex;
    SDL_Surface *p_image = IMG_Load(p_path);
    SDL_Surface *p_surface = SDL_ConvertSurfaceFormat(p_image, SDL_PIXELFORMAT_RGBA4444, 0);
    SDL_FreeSurface(p_image);
    glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, // target
		0, // level, 0 = base, no minimap,
		GL_RGBA4, // internalformat
		p_surface->w, // width
		p_surface->h, // height
		0, // border, always 0 in OpenGL ES
		GL_RGBA, // format
		GL_UNSIGNED_SHORT_4_4_4_4,
		p_surface->pixels);
	SDL_FreeSurface(p_surface);
    return  tex;
}

int main(int argc, char **argv){
    bool loop = true;
    bool active = true;
    bool full_screen = false;
    SDL_Event event;

    printf(START_MSG);
    p_cfg = new Config(CONFIG_FILE);
    int scale = p_cfg->get("scale", 2, 1, 5);
    window_width = SCREEN_WIDTH * scale;
    window_height = SCREEN_HEIGHT * scale;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0)
        return fatal_error("SDL init");
    if (SDL_NumJoysticks()){
        SDL_JoystickOpen(0);
        SDL_JoystickEventState(SDL_ENABLE);
        //if ( SDL_GameControllerAddMappingsFromFile("data/gamepad_mappings.db")== -1)
        //    printf("SDL ERROR: %s\n", SDL_GetError());
    }

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    full_screen = p_cfg->get_case_index("full_screen", 0, regex(R"((no)|(yes))", regex_constants::icase)) == 0 ? false : true;
    if (full_screen)
        window_flags = (SDL_WindowFlags) (window_flags | (p_cfg->get_case_index("full_screen_mode", 0, regex(R"((native)|(desktop))", regex_constants::icase)) == 0 ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP));
    if (!(window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, window_flags)))
        return fatal_error("Create window");
    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        return fatal_error("Create OpenGL context");
    SDL_GL_MakeCurrent(window, gl_context);
    glewExperimental = true;
    if (glewInit() != GLEW_OK)
        return fatal_error("glew init");
    #ifndef FRAME_TIME
    if (p_cfg->get_case_index("vsync", 0, regex(R"((yes)|(no))", regex_constants::icase)) == 0)
        SDL_GL_SetSwapInterval(1); // Enable vsync
    else
        SDL_GL_SetSwapInterval(0);
    #else
        SDL_GL_SetSwapInterval(0);
    #endif

    SDL_SetWindowIcon(window, IMG_Load("data/icon.png"));
    SDL_SetWindowMinimumSize(window, SCREEN_WIDTH, SCREEN_HEIGHT);

    // GL state setup
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &screen_texture);
    glBindTexture(GL_TEXTURE_2D, screen_texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA4, SCREEN_WIDTH, SCREEN_HEIGHT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
    if (p_cfg->get_case_index("scale_filter", 0, regex(R"((nearest)|(linear))", regex_constants::icase)) == 0){
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }else{
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glGenBuffers(1, &pbo);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = NULL;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.WindowPadding                     = ImVec2(10, 10);
    style.WindowRounding                    = 4.5;
    style.FrameRounding                     = 2.0;
    style.GrabRounding                      = 7.0;
    //style.Colors[ImGuiCol_WindowBg]         = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);
    //style.Colors[ImGuiCol_FrameBg]          = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
    //style.Colors[ImGuiCol_FrameBgActive]    = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_ChildBg].w        = 0.37f;
    style.Colors[ImGuiCol_Border]           = ImVec4(0.00f, 0.00f, 0.00f, 1.0f);
    //style.FrameRounding                   = 3.0f;
    //style.ItemSpacing                     = ImVec2(12, 8);
    //style.ItemInnerSpacing                = ImVec2(8, 6);
    //style.FramePadding                    = ImVec2(3, 3);
    //style.IndentSpacing                   = 25.0f;

    p_board = new Board(HW::SPECTRUM_128);
    p_keyboard = new Keyboard();
	p_sound = new Sound(
        p_cfg->get("audio_rate", AUDIO_RATE, 22050, 192100),
        (Sound::MODE)p_cfg->get_case_index("ay_stereo_mode", Sound::MODE::ACB, regex(R"((mono)|(abc)|(acb))", regex_constants::icase)),
        p_cfg->get("ay_volume", 0.8, 0.0, 1.0),
        p_cfg->get("speaker_volume", 0.5, 0.0, 1.0),
        p_cfg->get("tape_volume", 0.3, 0.0, 1.0)
    );
    p_sound->setup_lpf(p_cfg->get("lpf_rate", 22050, 11025, 44100));

    p_wd1793 = new WD1793(p_board);
    p_joystick = new KJoystick(p_board);
    p_tape = new Tape();
    p_mouse = new KMouse();

    p_joystick->map(JOY_LEFT, p_cfg->get("joy_left", 15, 0, 1000));
    p_joystick->map(JOY_RIGHT, p_cfg->get("joy_right", 13, 0, 1000));
    p_joystick->map(JOY_DOWN, p_cfg->get("joy_down", 14, 0, 1000));
    p_joystick->map(JOY_UP, p_cfg->get("joy_up", 12, 0, 1000));
    p_joystick->map(JOY_A, p_cfg->get("joy_a", 3, 0, 1000));
    p_joystick->map(JOY_B, p_cfg->get("joy_b", 1, 0, 1000));

    try {
        if (p_cfg->exist("disk_a"))
             p_wd1793->load_trd(0, p_cfg->get("disk_a").c_str());
        if (p_cfg->exist("disk_b"))
             p_wd1793->load_trd(1, p_cfg->get("disk_b").c_str());
        if (p_cfg->exist("disk_c"))
             p_wd1793->load_trd(2, p_cfg->get("disk_c").c_str());
        if (p_cfg->exist("disk_d"))
            p_wd1793->load_trd(3, p_cfg->get("disk_d").c_str());
    }catch(runtime_error & ex){
        cerr << ex.what() << endl;
    }

    p_board->load_rom(ROM_TRDOS, p_cfg->get("rom_trdos", "data/rom/trdos.rom").c_str());
    p_board->load_rom(ROM_128, p_cfg->get("rom_128", "data/rom/128.rom").c_str());
    p_board->load_rom(ROM_48, p_cfg->get("rom_48", "data/rom/48.rom").c_str());

    p_board->add_device(p_tape);
    p_board->add_device(p_sound);
    p_board->add_device(p_mouse);
    p_board->add_device(p_keyboard);
    p_board->add_device(p_wd1793);
    p_board->add_device(p_joystick);

    p_board->set_rom(ROM_128);

    // Load tape for config
    if (p_cfg->exist("open_tap") && !p_tape->load_tap(p_cfg->get("open_tap").c_str()))
        cerr << "Loading tape file error: " << p_cfg->get("open_tap").c_str() << "'\n";

    for (int i = 0; i < argc; i++)
        load_file(argv[i]);

    // Audio
    SDL_AudioSpec wanted, have;
    SDL_zero(wanted);
    SDL_zero(have);
    wanted.freq = p_sound->sample_rate;
    wanted.format = AUDIO_S16;
    wanted.channels = 2;
    wanted.samples = p_board->frame_clk * (p_sound->sample_rate / (float)Z80_FREQ);
    wanted.callback = NULL;
    audio_device_id = SDL_OpenAudioDevice(NULL, 0, &wanted, &have, 0);
    if (!audio_device_id)
        return fatal_error("SDL: Open audio device");
    if (wanted.freq != have.freq || wanted.channels != have.channels || wanted.samples != have.samples)
        return fatal_error("SDL Audio buffer init");
    SDL_PauseAudioDevice(audio_device_id, 0);

#ifdef FRAME_TIME
    Uint32 time_start = SDL_GetTicks();
	int frame_cnt = 0;
#endif
    bool full_speed = false;
    int ui = UI_NONE;
    ImVec2 btn_size = {80, 20};
    ImVec2 btn_small = {60, 20};
    IGFD::FileDialogConfig config = {.path = ".", .countSelectionMax = 1, .flags = ImGuiFileDialogFlags_Modal};
    GLuint kbd_layout = load_texture("data/keyboard_layout.png");
    bool style_editor = false;

    while (loop){
        while (SDL_PollEvent(&event)){
            if (event.type == SDL_QUIT)
                ui = UI_EXIT;
            switch(event.type){
                case SDL_WINDOWEVENT:
                    switch(event.window.event){
                        case SDL_WINDOWEVENT_SHOWN:
                            active = true;
                            break;
                        case SDL_WINDOWEVENT_HIDDEN:
                            active = false;
                            break;
                        case SDL_WINDOWEVENT_EXPOSED:
                            SDL_GetWindowSize(window, &window_width, &window_height);
                            glViewport(0, 0, (GLsizei)window_width, (GLsizei)window_height);
                            glMatrixMode(GL_PROJECTION);
                            glLoadIdentity();
                            glOrtho(0.0, window_width, window_height, 0.0, -1.0, 1.0);
                            glMatrixMode(GL_MODELVIEW);
                            glLoadIdentity();
                            break;
                        case SDL_WINDOWEVENT_CLOSE:
                            loop = false;
                            break;
                    }
                    break;
                case SDL_KEYDOWN:
                     if (event.key.keysym.sym == SDLK_ESCAPE){
                        if (ui == UI_NONE)
                            ui = UI_EXIT;
                        else
                            ui = UI_NONE;
                        break;
                    }
                    if (event.key.keysym.sym == SDLK_F8){
                        style_editor ^= true;
                    }
                    if (ui)
                        break;
                    if (event.key.repeat)
                        break;
                    switch(event.key.keysym.sym){
                        case SDLK_RETURN:
                            if (event.key.keysym.mod & KMOD_CTRL){
                                if (full_screen)
                                    SDL_SetWindowFullscreen(window, 0);
                                else{
                                    if (p_cfg->get_case_index("full_screen_mode", 0, regex(R"((native)|(desktop))", regex_constants::icase)) == 0)
                                        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
                                    else
                                        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                                }
                                full_screen ^= true;
                            }
                            break;
                        case SDLK_F1:
                            ui = UI_KBLAYOUT;
                            break;
                        case SDLK_F2:
                            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".z80;.sna;.tap;.trd {.z80,.sna,.tap,.trd}", config);
                            ui = UI_OPEN_SNAPSHOT;
                            break;
                        /*
                        case SDLK_F3:
                            ui = UI_SETTINGS;
                            break;
                        case SDLK_F4:
                            ui = UI_DEBUGGER;
                            break;
                        */
                        case SDLK_F5:
                            p_board->set_rom(ROM_48);
                            p_board->reset();
                            break;
                        case SDLK_F6:
                            p_board->set_rom(ROM_128);
                            p_board->reset();
                            break;
                        case SDLK_F7:
                            p_board->set_rom(ROM_TRDOS);
                            p_board->reset();
                            break;
                        case SDLK_F8:
                            break;
                        case SDLK_F9:
                            p_tape->play();
                            break;
                        case SDLK_F10:
                            p_tape->stop();
                            break;
                        case SDLK_F11:
                            p_tape->rewind(-15000);
                            break;
                        case SDLK_F12:
                            full_speed ^= true;
                            if (full_speed)
                                SDL_GL_SetSwapInterval(0);
                            else
                                if (p_cfg->get_case_index("vsync", 0, regex(R"((yes)|(no))", regex_constants::icase)) == 0)
                                    SDL_GL_SetSwapInterval(1);
                            break;
                        default:
                            if (event.key.keysym.mod & (KMOD_NUM | KMOD_CAPS))
                                SDL_SetModState(KMOD_NONE);
                            break;
                    }
                case SDL_KEYUP:
                    if (ui)
                        break;
                    switch(event.key.keysym.sym){
                        case SDLK_UP: // SHIFT + 7
                            p_keyboard->set_btn_state(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
                            p_keyboard->set_btn_state(0xEFFE, 0x08, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_DOWN: // SHIFT + 6
                            p_keyboard->set_btn_state(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
                            p_keyboard->set_btn_state(0xEFFE, 0x10, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_LEFT: // SHIFT + 5
                            p_keyboard->set_btn_state(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
                            p_keyboard->set_btn_state(0xF7FE, 0x10, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_RIGHT: // SHIFT + 8
                            p_keyboard->set_btn_state(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
                            p_keyboard->set_btn_state(0xEFFE, 0x04, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_DELETE:
                        case SDLK_BACKSPACE:
                            p_keyboard->set_btn_state(0xFEFE, 0x01, event.type == SDL_KEYDOWN); // SHIFT + 0
                            p_keyboard->set_btn_state(0xEFFE, 0x01, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_KP_PLUS: // CTRL + k for "+"
                            p_keyboard->set_btn_state(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
                            p_keyboard->set_btn_state(0xBFFE, 0x04, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_MINUS:
                        case SDLK_KP_MINUS:
                            p_keyboard->set_btn_state(0x7FFE, 0x02, event.type == SDL_KEYDOWN); // CTRL + j for "-"
                            p_keyboard->set_btn_state(0xBFFE, 0x08, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_KP_MULTIPLY: // CTRL + b for "*"
                            p_keyboard->set_btn_state(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
                            p_keyboard->set_btn_state(0x7FFE, 0x10, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_KP_DIVIDE: // CTRL + v for "/"
                            p_keyboard->set_btn_state(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
                            p_keyboard->set_btn_state(0xFEFE, 0x10, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_EQUALS:
                        case SDLK_KP_EQUALS:
                            p_keyboard->set_btn_state(0x7FFE, 0x02, event.type == SDL_KEYDOWN); // CTRL + l for "="
                            p_keyboard->set_btn_state(0xBFFE, 0x02, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_COMMA:
                        case SDLK_KP_COMMA:
                            p_keyboard->set_btn_state(0x7FFE, 0x02, event.type == SDL_KEYDOWN); // CTRL + n for ","
                            p_keyboard->set_btn_state(0x7FFE, 0x08, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_PERIOD:
                        case SDLK_KP_PERIOD:
                            p_keyboard->set_btn_state(0x7FFE, 0x02, event.type == SDL_KEYDOWN); // CTRL + m for "."
                            p_keyboard->set_btn_state(0x7FFE, 0x04, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_SPACE:
                            p_keyboard->set_btn_state(0x7FFE, 0x01, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_LCTRL:
                        case SDLK_RCTRL:
                            p_keyboard->set_btn_state(0x7FFE, 0x02, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_m:
                            p_keyboard->set_btn_state(0x7FFE, 0x04, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_n:
                            p_keyboard->set_btn_state(0x7FFE, 0x08, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_b:
                            p_keyboard->set_btn_state(0x7FFE, 0x10, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:
                            p_keyboard->set_btn_state(0xBFFE, 0x01, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_l:
                            p_keyboard->set_btn_state(0xBFFE, 0x02, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_k:
                            p_keyboard->set_btn_state(0xBFFE, 0x04, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_j:
                            p_keyboard->set_btn_state(0xBFFE, 0x08, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_h:
                            p_keyboard->set_btn_state(0xBFFE, 0x10, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_p:
                            p_keyboard->set_btn_state(0xDFFE, 0x01, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_o:
                            p_keyboard->set_btn_state(0xDFFE, 0x02, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_i:
                            p_keyboard->set_btn_state(0xDFFE, 0x04, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_u:
                            p_keyboard->set_btn_state(0xDFFE, 0x08, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_y:
                            p_keyboard->set_btn_state(0xDFFE, 0x10, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_0:
                            p_keyboard->set_btn_state(0xEFFE, 0x01, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_9:
                            p_keyboard->set_btn_state(0xEFFE, 0x02, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_8:
                            p_keyboard->set_btn_state(0xEFFE, 0x04, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_7:
                            p_keyboard->set_btn_state(0xEFFE, 0x08, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_6:
                            p_keyboard->set_btn_state(0xEFFE, 0x10, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_1:
                            p_keyboard->set_btn_state(0xF7FE, 0x01, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_2:
                            p_keyboard->set_btn_state(0xF7FE, 0x02, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_3:
                            p_keyboard->set_btn_state(0xF7FE, 0x04, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_4:
                            p_keyboard->set_btn_state(0xF7FE, 0x08, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_5:
                            p_keyboard->set_btn_state(0xF7FE, 0x10, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_q:
                            p_keyboard->set_btn_state(0xFBFE, 0x01, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_w:
                            p_keyboard->set_btn_state(0xFBFE, 0x02, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_e:
                            p_keyboard->set_btn_state(0xFBFE, 0x04, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_r:
                            p_keyboard->set_btn_state(0xFBFE, 0x08, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_t:
                            p_keyboard->set_btn_state(0xFBFE, 0x10, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_a:
                            p_keyboard->set_btn_state(0xFDFE, 0x01, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_s:
                            p_keyboard->set_btn_state(0xFDFE, 0x02, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_d:
                            p_keyboard->set_btn_state(0xFDFE, 0x04, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_f:
                            p_keyboard->set_btn_state(0xFDFE, 0x08, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_g:
                            p_keyboard->set_btn_state(0xFDFE, 0x10, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_LSHIFT:
                        case SDLK_RSHIFT:
                            p_keyboard->set_btn_state(0xFEFE, 0x01, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_z:
                            p_keyboard->set_btn_state(0xFEFE, 0x02, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_x:
                            p_keyboard->set_btn_state(0xFEFE, 0x04, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_c:
                            p_keyboard->set_btn_state(0xFEFE, 0x08, event.type == SDL_KEYDOWN);
                            break;
                        case SDLK_v:
                            p_keyboard->set_btn_state(0xFEFE, 0x10, event.type == SDL_KEYDOWN);
                            break;
                    }
                    break;
                case SDL_JOYHATMOTION:
                    switch (event.jhat.value){
                        case SDL_HAT_LEFT:
                            p_joystick->set_state(JOY_LEFT, true);
                            p_joystick->set_state(JOY_RIGHT | JOY_UP | JOY_DOWN, false);
                            break;
                        case SDL_HAT_LEFTUP:
                            p_joystick->set_state(JOY_LEFT | JOY_UP, true);
                            p_joystick->set_state(JOY_RIGHT | JOY_DOWN, false);
                            break;
                        case SDL_HAT_LEFTDOWN:
                            p_joystick->set_state(JOY_LEFT | JOY_DOWN, true);
                            p_joystick->set_state(JOY_RIGHT | JOY_UP, false);
                            break;
                        case SDL_HAT_RIGHT:
                            p_joystick->set_state(JOY_RIGHT, true);
                            p_joystick->set_state(JOY_LEFT | JOY_UP | JOY_DOWN, false);
                            break;
                        case SDL_HAT_RIGHTUP:
                            p_joystick->set_state(JOY_RIGHT | JOY_UP, true);
                            p_joystick->set_state(JOY_LEFT | JOY_DOWN, false);
                            break;
                        case SDL_HAT_RIGHTDOWN:
                            p_joystick->set_state(JOY_RIGHT | JOY_DOWN, true);
                            p_joystick->set_state(JOY_LEFT | JOY_UP, false);
                            break;
                        case SDL_HAT_UP:
                            p_joystick->set_state(JOY_UP, true);
                            p_joystick->set_state(JOY_LEFT | JOY_RIGHT | JOY_DOWN, false);
                            break;
                        case SDL_HAT_DOWN:
                            p_joystick->set_state(JOY_DOWN, true);
                            p_joystick->set_state(JOY_LEFT | JOY_RIGHT | JOY_UP, false);
                            break;
                        case SDL_HAT_CENTERED:
                            p_joystick->set_state(JOY_LEFT | JOY_RIGHT | JOY_UP | JOY_DOWN, false);
                            break;
                    }
                    break;
                case SDL_JOYBUTTONDOWN:
                    p_joystick->gamepad_event(event.jbutton.button, true);
                    break;
                case SDL_JOYBUTTONUP:
                    p_joystick->gamepad_event(event.jbutton.button, false);
                    break;
                case SDL_MOUSEMOTION:
                    p_mouse->motion(event.motion.xrel, event.motion.yrel);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    p_mouse->button_press((event.button.button & SDL_BUTTON_LEFT ? 0x01 : 0x00) | (event.button.button & SDL_BUTTON_RIGHT ? 0x02 : 0x00));
                    break;
                case SDL_MOUSEBUTTONUP:
                    p_mouse->button_release((event.button.button & SDL_BUTTON_LEFT ? 0x01 : 0x00) | (event.button.button & SDL_BUTTON_RIGHT ? 0x02 : 0x00));
                    break;
                case SDL_MOUSEWHEEL:
                    p_mouse->wheel(event.wheel.y);
                    break;
            }
            ImGui_ImplSDL2_ProcessEvent(&event);
        }
        if (!active){
            SDL_Delay(100);
            continue;
        }
        // PBO data is transferred, rendering a full-screen textured quad.
        glBindTexture(GL_TEXTURE_2D, screen_texture);
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f,                  window_height);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(window_width,          window_height);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(window_width,          0.0f);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f,                  0.0f);
        glEnd();
        // Update the PBO and setup async byte-transfer to the screen texture.
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(GLushort), NULL, GL_DYNAMIC_DRAW);
        p_board->set_frame_buffer(glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
        if (ui != UI_DEBUGGER)
            p_board->frame();
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        // Start update texture.
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        if (ui){
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            //if (style_editor)
            //    ImGui::ShowStyleEditor();
            switch(ui){
                case UI_EXIT:
                    static const char *confirm_txt = "Do you exit?";
                    ImGui::SetNextWindowPos(ImVec2(window_width*0.5, window_height*0.5), ImGuiCond_Always, ImVec2(0.5, 0.5));
                    if (ImGui::Begin("Exit", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration)){
                        ImGui::SetWindowFocus();
                        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(confirm_txt).x) * 0.5);
                        ImGui::Text(confirm_txt);
                        ImGui::Spacing();
                        if (ImGui::Button("Yes", btn_size))
                            loop = false;
                        ImGui::SetItemDefaultFocus();
                        ImGui::SameLine();
                        if (ImGui::Button("No", btn_size))
                            ui = UI_NONE;
                        ImGui::End();
                    }
                    break;
                case UI_KBLAYOUT:
                    ImGui::SetNextWindowSize(ImVec2(window_width*0.95, window_height * 0.5), ImGuiCond_Always);
                    ImGui::SetNextWindowPos(ImVec2(window_width*0.5, window_height*0.5), ImGuiCond_Always, ImVec2(0.5, 0.5));
                    if (ImGui::Begin("#kbd", NULL, ImGuiWindowFlags_NoDecoration)){
                        ImGui::Image((ImTextureID)kbd_layout, ImGui::GetWindowSize());
                        ImGui::End();
                    }
                    break;
                case UI_OPEN_SNAPSHOT:
                    ImGui::SetNextWindowPos(ImVec2(window_width*0.15, window_height*0.25), ImGuiCond_Always);
                    ImGui::SetNextWindowSize(ImVec2(window_width*0.7, window_height*0.5), ImGuiCond_Always);
                    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoDecoration)){
                        if (ImGuiFileDialog::Instance()->IsOk()){
                            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                            std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
                            load_file(filePathName.c_str());
                        }
                        ImGuiFileDialog::Instance()->Close();
                        ui = UI_NONE;
                    }
                    break;
                /*
                case UI_SETTINGS:
                    static int model_id = 1;
                    static int scale = 3;
                    static int video_filter = 0;
                    static bool v_sync = false;
                    static char disk_a[256]="floppy/disk_a.trd";
                    static char disk_b[256]="floppy/disk_b.trd";
                    static char disk_c[256]="floppy/disk_c.trd";
                    static char disk_d[256]="floppy/disk_d.trd";
                    //ImGui::SetNextWindowPos(ImVec2(window_width*0.15, window_height*0.25), ImGuiCond_Always);
                    //ImGui::SetNextWindowSize(ImVec2(window_width*0.7, window_height*0.5), ImGuiCond_Always);
                    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Always);
                    ImGui::SetNextWindowPos(ImVec2(window_width*0.5, window_height*0.5), ImGuiCond_Always, ImVec2(0.5, 0.5));
                    //if (ImGui::Begin("Main", NULL, ImGuiWindowFlags_NoDecoration)){// | ImGuiWindowFlags_NavFlattened)){
                    if (ImGui::Begin("Main", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration)){
                        ImGui::BeginChild("##frame", ImVec2(400, 300));
                        if (ImGui::BeginTabBar("tabs")){
                            if (ImGui::BeginTabItem("General")){
                                ImGui::Combo("##model", &model_id, "Pentagon-128\0Sinclair-128\0Sinclair-48\0\0");
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Video")){
                                ImGui::SliderInt("##scale", &scale, 0, 5);
                                ImGui::RadioButton("Nearest", &video_filter, 0);
                                ImGui::RadioButton("Linear", &video_filter, 1);
                                ImGui::Checkbox("v_sync", &v_sync);
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Audio")){
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Disk")){
                                ImGui::SetKeyboardFocusHere();
                                ImGui::InputText("##disk_a", disk_a, 256, ImGuiInputTextFlags_ReadOnly);
                                ImGui::SameLine();
                                if (ImGui::Button("A:", btn_small)){
                                }
                                ImGui::InputText("##disk_b", disk_b, 256, ImGuiInputTextFlags_ReadOnly);
                                ImGui::SameLine();
                                if (ImGui::Button("B:", btn_small)){
                                }
                                ImGui::InputText("##disk_c", disk_c, 256, ImGuiInputTextFlags_ReadOnly);
                                ImGui::SameLine();
                                if (ImGui::Button("C:", btn_small)){
                                }
                                ImGui::InputText("##disk_d", disk_d, 256, ImGuiInputTextFlags_ReadOnly);
                                ImGui::SameLine();
                                if (ImGui::Button("D:", btn_small)){
                                }
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Input")){
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }
                        ImGui::EndChild();
                        ImGui::Spacing();
                        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (80 * 2 + ImGui::GetStyle().ItemSpacing.x * 2));
                        if (ImGui::Button("OK", ImVec2(80, 20)))
                            ui = UI_NONE;
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel", ImVec2(80, 20)))
                            ui = UI_NONE;
                        ImGui::End();
                }
                    break;
                case UI_DEBUGGER:
                    unsigned short pointer = p_board->cpu.pc, next;
                    char line[128];
                    char reg_value[8];
                    const char *flags[] = {"S", "Z", "5", "H", "3", "P", "N", "C"};
                    #define DISASM_LINES 24
                    //static char* desasm_lines[DISASM_LINES];
                    ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Always);
                    ImGui::SetNextWindowPos(ImVec2(window_width*0.5, window_height*0.5), ImGuiCond_Always, ImVec2(0.5, 0.5));
                    if (ImGui::Begin("Debugger")){
                        ImGui::BeginTable("##disasm_columns", 2);
                        ImGui::TableSetupColumn("Disassembler", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Registers", ImGuiTableColumnFlags_WidthFixed);
                        //ImGui::TableSetupColumn("Disassembler", ImGuiTableColumnFlags_WidthStretch);
                        //ImGui::TableSetupColumn("Registers", ImGuiTableColumnFlags_WidthFixed, 0.120f);
                        ImGui::TableNextRow();
                        ImGui::TableHeadersRow();
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        for (int i = 0; i < DISASM_LINES; i++){
                            next = Disasm::line(line, pointer, &p_board->cpu, &p_board->ula, true);
                            ImGui::Selectable(line, pointer == p_board->cpu.pc, ImGuiSelectableFlags_Disabled);
                            pointer = next;
                        }
                        ImGui::TableNextColumn();
                        for (int i = 0; i < 8; i++){
                            if (i)
                                ImGui::SameLine(0.0f, 0.0f);
                            if (p_board->cpu.af & (0x80 >> i))
                                ImGui::Text(flags[i]);
                            else
                                ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1.0), flags[i]);
                        }
                        sprintf(reg_value, "%04x", p_board->cpu.af);
                        ImGui::Text("AF");
                        ImGui::SameLine();
                        ImGui::InputText("##af", reg_value, sizeof(reg_value)-1, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll);
                        ImGui::Text("BC=");
                        ImGui::SameLine();
                        sprintf(reg_value, "%04x", p_board->cpu.bc);
                        ImGui::InputText("##bc", reg_value, sizeof(reg_value)-1, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll);
                        ImGui::Text("HL=");
                        ImGui::SameLine();
                        sprintf(reg_value, "%04x", p_board->cpu.hl);
                        ImGui::InputText("##hl", reg_value, sizeof(reg_value)-1, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll);
                        ImGui::Text("DE=");
                        ImGui::SameLine();
                        sprintf(reg_value, "%04x", p_board->cpu.de);
                        ImGui::InputText("##de", reg_value, sizeof(reg_value)-1, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll);
                        ImGui::Text("PC=");
                        ImGui::SameLine();
                        sprintf(reg_value, "%04x", p_board->cpu.pc);
                        ImGui::InputText("##pc", reg_value, sizeof(reg_value)-1, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll);
                        ImGui::Text("CLK=");
                        ImGui::SameLine();
                        sprintf(reg_value, "%d", p_board->cpu.clk);
                        ImGui::InputText("##clk", reg_value, sizeof(reg_value)-1, ImGuiInputTextFlags_ReadOnly);
                        ImGui::EndTable();
                        ImGui::End();
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_F7))
                        p_board->cpu.step_into(&p_board->ula, p_board, p_board->frame_clk);
                    if (ImGui::IsKeyPressed(ImGuiKey_F8))
                        p_board->cpu.step_into(&p_board->ula, p_board, p_board->frame_clk);
                    break;
                    */
            }
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
#ifdef FRAME_TIME
		if (++frame_cnt > FRAME_LIMIT)
            break;
#else
        if (!full_speed){
            unsigned int size = p_board->frame_clk * (p_sound->sample_rate / (float)Z80_FREQ) * 4;
            while (SDL_GetQueuedAudioSize(audio_device_id) > size)
                SDL_Delay(1);
            SDL_QueueAudio(audio_device_id, p_sound->p_sound, size);
        }
#endif
        SDL_GL_SwapWindow(window);
    }
#ifdef FRAME_TIME
	printf("Time: %d for frames: %d\n", (SDL_GetTicks() - time_start), frame_cnt);
#endif
    printf(EXIT_MSG);
    release_all();
    return 0;
}
