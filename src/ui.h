#define UI_WindowFlags ImGuiWindowFlags_NoCollapse | \
    ImGuiWindowFlags_NoScrollbar | \
    ImGuiWindowFlags_NoResize 
//    ImGuiWindowFlags_NoMove
// | ImGuiWindowFlags_NoNavFocus 
//| ImGuiWindowFlags_NoSavedSettings 

namespace UI {
    enum UI_Mode { UI_None, UI_Exit, UI_KbdLayout, UI_OpenFile, UI_SaveFile, UI_Settings, UI_Debugger };
    void setup(Cfg &cfg, SDL_Window *window, SDL_GLContext context, const char *glsl_version);
    bool frame(Cfg &cfg, Board *board);
    bool event(SDL_Event &event);
    void open(UI_Mode mode);
    void hide();
    bool is_shown();
    bool is_modal();
    void set_alpha(float alpha);
    void set_gamepad_ctrl(bool state);
}
