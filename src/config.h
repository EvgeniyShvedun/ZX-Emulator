#define ZX_SCREEN_WIDTH             320L
#define ZX_SCREEN_HEIGHT            240L
#define SCREEN_WIDTH                (ZX_SCREEN_WIDTH*2L)
#define SCREEN_HEIGHT               (ZX_SCREEN_HEIGHT*2L)
#define ASPECT_RATIO                ((float)SCREEN_WIDTH/(float)SCREEN_HEIGHT)
#define CONFIG_H_MODIFIED           __TIMESTAMP__

enum Hardware { HW_Pentagon_128, HW_Sinclair_128, HW_Sinclair_48 };
enum AY_Mixer { ABC, ACB, Mono };
enum Filter {  Nearest, Linear };
enum ROM_Bank { ROM_Trdos, ROM_128, ROM_48 };

//#pragma pack(1)
struct Cfg {
    char format_id[32];
    struct Main {
        int model = HW_Pentagon_128;
        char rom_path[sizeof(ROM_Bank)][4096] = {
            "data/rom/trdos.rom",
            "data/rom/128.rom",
            "data/rom/48.rom"
        };
        bool full_speed = false;
    } main;
    struct Video {
        int screen_width = SCREEN_WIDTH;
        int screen_height = SCREEN_HEIGHT;
        int filter = Nearest;
        bool full_screen = false;
        bool vsync = true;
    } video;
    struct Audio {
        int dsp_rate = 44100;
        int lpf_rate = 20000;
        int ay_mixer_mode = ACB;
        float ay_side_level = 0.90f;
        float ay_center_level = 0.45f;
        float ay_penetr_level = 0.25f;
        float ay_volume = 0.80f;
        float speaker_volume = 0.50f;
        float tape_volume = 0.25f;
    } audio;
    struct UI {
        float alpha = 0.90f;
        bool gamepad = false;
    } ui;
};
//#pragma pack()

namespace Config {
    Cfg& load(const char *path);
    void save(const char *path);
    Cfg& get();
    Cfg& get_defaults();
}
