
#define UI_WindowFlags ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings

enum UI_Mode { UI_None, UI_Exit, UI_KbdLayout, UI_OpenFile, UI_SaveFile, UI_Settings };
namespace UI {
    void setup(SDL_Window *window, SDL_GLContext context, const char *glsl_version);
    bool event(SDL_Event &event);
    bool frame(CFG *cfg, Board *board);
}
