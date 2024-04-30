#define UI_WindowFlags ImGuiWindowFlags_NoCollapse \
    | ImGuiWindowFlags_NoScrollbar \
    | ImGuiWindowFlags_NoResize \
    | ImGuiWindowFlags_NoMove \
    | ImGuiWindowFlags_NoSavedSettings \
    | ImGuiWindowFlags_NoDecoration 
//    | ImGuiWindowFlags_NoNavFocus 

namespace UI {
    enum UI_Mode { UI_Hidden, UI_Exit, UI_KbdLayout, UI_OpenFile, UI_SaveFile, UI_Settings, UI_Debugger };
    void setup(SDL_Window *window, SDL_GLContext context, const char *glsl_version);
    bool frame(CFG &cfg, Board *board);
    bool event(SDL_Event &event);
}
