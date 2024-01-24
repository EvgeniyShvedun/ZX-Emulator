#include <iostream>
#include <cstddef>
//#define SDL_WINDOW_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdarg.h>

#include "base.h"
#include "config.h"
#include "keyboard.h"
#include "sound.h"
#include "wd1793.h"
#include "tape.h"
#include "mouse.h"
#include "joystick.h"

//#define FRAME_TIME
//#define FPS
//#define TIME_AVG
//#define FRAME_LIMIT 10000

using namespace std;

bool loop_continue = true;
bool window_active = true;

Config *p_cfg = NULL;
Board *p_board = NULL;
Keyboard *p_keyboard = NULL;
Sound *p_sound = NULL;
WD1793 *p_wd1793 = NULL;
Tape *p_tape = NULL;
KMouse *p_mouse = NULL;
KJoystick *p_joystick = NULL;
GLshort *p_ui_buffer = NULL;

int window_width = SCREEN_WIDTH;
int window_height = SCREEN_HEIGHT;
bool full_speed = false;
SDL_Window *p_win = NULL;
SDL_GLContext p_context = NULL;
SDL_AudioDeviceID audio_device_id = 0;
GLuint gProgramId = 0;
GLuint gVertexShaderId = 0;
GLuint gFragmentShaderId = 0;
GLuint gVAOId = 0;
GLuint gVBOId = 0;
GLuint gPBOId[2] = {0, 0};
GLuint textureId[2] = {0, 0};
GLfloat projection_matrix[16];

void info(){
    printf("ZX-Spectrum Emulator v1.1 by Evgeniy Shvedun 2006-2024.\n");
    printf("_______________________________________________________________________\n");
    printf("Facebook: https://www.facebook.com/profile.php?id=61555407730196\n");
    printf("Github: https://github.com/EvgeniyShvedun/ZX-Emulator\n\n");
}

const GLchar *gVertexSrc[] = {
	"#version 330\n"\
    "layout (location=0) in vec2 in_Position2D;\n"\
    "layout (location=1) in vec2 in_TextureCoord;\n"\
    "out vec2 ScreenCoord;\n"\
    "uniform mat4 projectionMatrix;\n"\
    "void main() {\n"\
        "gl_Position = projectionMatrix * vec4(in_Position2D.x, in_Position2D.y, 0, 1);\n"\
        "ScreenCoord = in_TextureCoord;\n"\
    "}"
};

const GLchar *gFragmentSrc[] = {
    "#version 330\n"\
    "in vec2 ScreenCoord;\n"\
    "uniform sampler2D ScreenTexture;\n"\
    "void main() {\n"\
        "gl_FragColor = texture(ScreenTexture, ScreenCoord);\n"\
    "}"
};

void free_all(){
    loop_continue = false;
    DELETE(p_cfg);
    DELETE(p_board);
    DELETE(p_keyboard);
    DELETE(p_sound);
    DELETE(p_wd1793);
    DELETE(p_tape);
    DELETE(p_joystick);
    DELETE(p_mouse);
    DELETE_ARRAY(p_ui_buffer);

    // GL
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (gVertexShaderId)
        glDeleteShader(gVertexShaderId);
    gVertexShaderId = 0;
    if (gFragmentShaderId)
        glDeleteShader(gFragmentShaderId);
    gFragmentShaderId = 0;
    if (gProgramId)
        glDeleteProgram(gProgramId);
    gProgramId = 0;
    if (textureId[0])
        glDeleteTextures(1, &textureId[0]);
    textureId[0] = 0;
    if (textureId[1])
        glDeleteTextures(1, &textureId[1]);
    textureId[1] = 0;
    if (gVAOId)
        glDeleteVertexArrays(1, &gVAOId);
    gVAOId = 0;
    if (gVBOId)
        glDeleteBuffers(1, &gVBOId);
    gVBOId = 0;
	if (gPBOId[0])
		glDeleteBuffers(1, &gPBOId[0]);
	gPBOId[0] = 0;
	if (gPBOId[1])
		glDeleteBuffers(1, &gPBOId[1]);
	gPBOId[1] = 0;

    SDL_GL_DeleteContext(p_context);
    p_context = NULL;
    SDL_DestroyWindow(p_win);
    p_win = NULL;
    if (audio_device_id)
        SDL_PauseAudioDevice(audio_device_id, 0);
    audio_device_id = 0;
    SDL_CloseAudio();
    SDL_Quit();
}

int fatal_error(const char *p_msg){
    cout << p_msg << ".\n";
    free_all();
    return -1;
}

int main(int argc, char **argv){
    info();
    p_cfg = new Config(CONFIG_FILE);
    int scale = p_cfg->get("scale", 2, 1, 5);
    window_width = SCREEN_WIDTH * scale;
    window_height = SCREEN_HEIGHT * scale;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK) != 0)
        return fatal_error("SDL: Can't initialize");
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	/*
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG
    );*/
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    //SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 8);

    p_win = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!p_win)
        return fatal_error("SDL: Create window");
    p_context = SDL_GL_CreateContext(p_win);
    if (!p_context)
        return fatal_error("SDL: Create GL context");
    if (glewInit() != GLEW_OK)
        return fatal_error("GLEW: Initialization");
    SDL_GL_SetSwapInterval(0);

    SDL_Surface* icon = IMG_Load("data/logo.png");
    SDL_SetWindowIcon(p_win, icon);

    /*
	printf("Version: 	%s\n", glGetString(GL_VERSION));
	printf("Vendor: 	%s\n", glGetString(GL_VENDOR));
	printf("Renderer: 	%s\n", glGetString(GL_RENDERER));
	printf("Shading: 	%s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    */

    glViewport(0, 0, (GLsizei)window_width, (GLsizei)window_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, window_height, 0, 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Vertex shader.
    GLint gSuccess = GL_FALSE;
    gVertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(gVertexShaderId, 1, gVertexSrc, NULL);
    glCompileShader(gVertexShaderId);
    glGetShaderiv(gVertexShaderId, GL_COMPILE_STATUS, &gSuccess);
    if (gSuccess != GL_TRUE)
        return fatal_error("GL: Compile vertex shader");
    // Fragment shader.
    gFragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(gFragmentShaderId, 1, gFragmentSrc, NULL);
    glCompileShader(gFragmentShaderId);
    glGetShaderiv(gFragmentShaderId, GL_COMPILE_STATUS, &gSuccess);
    if (gSuccess != GL_TRUE){
		//char buffer[512];
        //glGetShaderInfoLog(gFragmentShaderId, 512, NULL, buffer);
		//printf("Compilation error: %s\n", buffer);
        return fatal_error("GL: Compile fragment shader");
	}
    // Link.
    gProgramId = glCreateProgram();
    glAttachShader(gProgramId, gVertexShaderId);
    glAttachShader(gProgramId, gFragmentShaderId);
    glLinkProgram(gProgramId);
    glGetProgramiv(gProgramId, GL_LINK_STATUS, &gSuccess);
    if (gSuccess != GL_TRUE)
        return fatal_error("GL: Link shaders");
    glDeleteShader(gVertexShaderId);
    gVertexShaderId = 0;
    glDeleteShader(gFragmentShaderId);
    gFragmentShaderId = 0;

    GLfloat vertex_data[4*4] = {
        (GLfloat)window_width,  0.0f,                   1.0f,                   0.0f,
        0.0f,                   0.0f,                   0.0f,                   0.0f,
        0.0f,                   (GLfloat)window_height, 0.0f,                   1.0f,
        (GLfloat)window_width,  (GLfloat)window_height, 1.0f,                   1.0f
    };

    glGenVertexArrays(1, &gVAOId);
    glBindVertexArray(gVAOId);
    glGenBuffers(1, &gVBOId);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOId);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, NULL);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (void*)(2*sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glEnable(GL_TEXTURE_2D);
    glGenTextures(2, textureId);
    glBindTexture(GL_TEXTURE_2D, textureId[0]);
    if (p_cfg->get_case_index("scale_filter", 0, regex(R"((nearest)|(linear))", regex_constants::icase)) == 0){
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }else{
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 0);

    glBindTexture(GL_TEXTURE_2D, textureId[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, window_width, window_height, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 0);

	glGenBuffers(2, gPBOId);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gPBOId[0]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(GLushort), 0, GL_STREAM_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gPBOId[1]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, window_width * window_height * sizeof(GLushort), 0, GL_STREAM_DRAW);

    glUseProgram(gProgramId);
    GLint projectionLocation = glGetUniformLocation(gProgramId, "projectionMatrix");
    glGetFloatv(GL_PROJECTION_MATRIX, projection_matrix);
    glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, projection_matrix);

    glClearColor(0.f, 0.f, 0.f, 0.0f);
	glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClear(GL_COLOR_BUFFER_BIT);

    p_board = new Board(HW::SPECTRUM_128);
    p_keyboard = new Keyboard();

    
	p_sound = new Sound(
        p_cfg->get("sample_rate", AUDIO_RATE, 22050, 192100),
        (Sound::MODE)p_cfg->get_case_index("ay_mixer_mode", Sound::MODE::ACB, regex(R"((mono)|(abc)|(acb))", regex_constants::icase)),
        p_cfg->get("ay_main_volume", 0.8, 0.0, 1.0),
        p_cfg->get("speaker_volume", 0.5, 0.0, 1.0),
        p_cfg->get("tape_volume", 0.3, 0.0, 1.0)
    );
    p_sound->setup_lpf(p_cfg->get("lpf_cut_rate", 22050, 11025, 44100));

    p_wd1793 = new WD1793(p_board);
    p_tape = new Tape();
    p_mouse = new KMouse();
    p_joystick = new KJoystick();

    p_joystick->config(p_cfg->get("joy_left", 15, -1000, 1000), JOY_LEFT);
    p_joystick->config(p_cfg->get("joy_right", 13, -1000, 1000), JOY_RIGHT);
    p_joystick->config(p_cfg->get("joy_down", 14, -1000, 1000), JOY_DOWN);
    p_joystick->config(p_cfg->get("joy_up", 12, -1000, 1000), JOY_UP);
    p_joystick->config(p_cfg->get("joy_a", 3, -1000, 1000), JOY_A);
    p_joystick->config(p_cfg->get("joy_b", 1, -1000, 1000), JOY_B);


    if (p_cfg->exist("disk_a") && !p_wd1793->open_trd(0, p_cfg->get("disk_a").c_str()))
        cerr << "Filed to load disk B: '" << p_cfg->get("disk_b").c_str() << "'\n";
    if (p_cfg->exist("disk_b") && !p_wd1793->open_trd(1, p_cfg->get("disk_b").c_str()))
        cerr << "Filed to load disk B: '" << p_cfg->get("disk_b").c_str() << "'\n";
    if (p_cfg->exist("disk_c") && !p_wd1793->open_trd(2, p_cfg->get("disk_c").c_str()))
        cerr << "Filed to load disk C: '" << p_cfg->get("disk_c").c_str() << "'\n";
    if (p_cfg->exist("disk_d") && !p_wd1793->open_trd(3, p_cfg->get("disk_d").c_str()))
        cerr << "Filed to load disk D: '" << p_cfg->get("disk_d").c_str() << "'\n";

    p_board->load_rom(ROM_TRDOS, p_cfg->get("rom_trdos", "data/rom/trdos.rom").c_str());
    p_board->load_rom(ROM_128, p_cfg->get("rom_128", "data/rom/128.rom").c_str());
    p_board->load_rom(ROM_48, p_cfg->get("rom_48", "data/rom/48.rom").c_str());

    p_board->add_device(p_tape);
    p_board->add_device(p_sound);
    p_board->add_device(p_wd1793);
    p_board->add_device(p_keyboard);
    p_board->add_device(p_joystick);
    p_board->add_device(p_mouse);

    p_board->set_rom(ROM_128);

    // Load tape for config
    if (p_cfg->exist("open_tap") && !p_tape->load_tap(p_cfg->get("open_tap").c_str()))
        cerr << "Loading tape file error: " << p_cfg->get("open_tap").c_str() << "'\n";

    for (int i = 0; i < argc; i++){
        int len = strlen(argv[i]);
        if (len > 4){
            if ((!strcmp(argv[i] + len - 4, ".z80") or !strcmp(argv[i] + len - 4, ".Z80"))){
                p_board->load_z80(argv[i]);
            }else{
                if (!strcmp(argv[i] + len - 4, ".tap") or !strcmp(argv[i] + len - 4, ".TAP")){
                    p_tape->load_tap(argv[i]);
                }
                if (!strcmp(argv[i] + len - 4, ".trd") or !strcmp(argv[i] + len - 4, ".TRD")){
                    p_wd1793->open_trd(0, argv[i]);
                }
            }
        }
    }


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
    SDL_PauseAudioDevice(audio_device_id, 0);

    SDL_Event event;
#ifdef FRAME_TIME
    Uint64 time_start;
    Uint64 time_all = 0;
	int frame_cnt = 1;
#endif

    while (loop_continue){
#ifdef FRAME_TIME
    	time_start = SDL_GetPerformanceCounter();
#endif
        while(SDL_PollEvent(&event)){
            switch(event.type){
                case SDL_WINDOWEVENT:
                    switch (event.window.event){
                        case SDL_WINDOWEVENT_SHOWN:
                            window_active = true;
                            SDL_PauseAudioDevice(audio_device_id, 0);
                            break;
                        case SDL_WINDOWEVENT_HIDDEN:
                            window_active = false;
                            SDL_PauseAudioDevice(audio_device_id, 1);
                            break;
                    }
                    break;
                case SDL_QUIT:
                    loop_continue = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.repeat)
                        break;
                    switch(event.key.keysym.sym){
                        case SDLK_F1:
                            p_tape->play();
                            break;
                        case SDLK_F2:
                            p_tape->stop();
                            break;
                        case SDLK_F3:
                            p_tape->rewind(-15);
                            break;
                        case SDLK_F4:
                            p_tape->rewind(15);
                            break;
                        case SDLK_F11:
                            break;
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
                        case SDLK_F12:
                            full_speed ^= true;
                            break;
                    }
                    if (event.key.keysym.mod & (KMOD_NUM | KMOD_CAPS))
                        SDL_SetModState(KMOD_NONE);
                case SDL_KEYUP:
                    switch(event.key.keysym.sym){
                        case SDLK_ESCAPE:
                            loop_continue = false;
                            break;
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
                case SDL_JOYBUTTONDOWN:
                    p_joystick->button(event.jbutton.button, true);
                    break;
                case SDL_JOYBUTTONUP:
                    p_joystick->button(event.jbutton.button, false);
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
        }
        if (!window_active){
            SDL_Delay(1);
            continue;
        }

        glClear(GL_COLOR_BUFFER_BIT);

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gPBOId[0]);
        glBindTexture(GL_TEXTURE_2D, textureId[0]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
        p_board->frame();

        glBufferData(GL_PIXEL_UNPACK_BUFFER, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(GLushort), p_board->get_frame_buffer(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gPBOId[1]);
        glBindTexture(GL_TEXTURE_2D, textureId[1]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, window_width, window_height, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);


        glBufferData(GL_PIXEL_UNPACK_BUFFER, window_width * window_height * sizeof(GLushort), p_ui_buffer, GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        SDL_GL_SwapWindow(p_win);

#ifdef FRAME_TIME
		time_all += SDL_GetPerformanceCounter() - time_start;
#ifdef FRAME_LIMIT
		if (++frame_cnt > FRAME_LIMIT)
			break;
#endif
#endif
#ifndef FRAME_TIME
        if (!full_speed){
            unsigned int size = p_board->frame_clk * (p_sound->sample_rate / (float)Z80_FREQ) * 4;
            while (SDL_GetQueuedAudioSize(audio_device_id) > size)
                SDL_Delay(1);
            SDL_QueueAudio(audio_device_id, p_sound->p_sound, size);
        }
#endif
    }
#ifdef FRAME_TIME
	printf("Frame time avg: %ld, frames: %d\n", time_all / frame_cnt / 100, frame_cnt);
#endif
    free_all();
    return 0;
}
