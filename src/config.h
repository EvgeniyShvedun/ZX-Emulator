
enum Hardware { HW_Pentagon_128, HW_Sinclair_128, HW_Sinclair_48 };
enum AY_Mixer { ABC, ACB, Mono };
enum Filter {  Nearest, Linear };
enum ROM_Bank { ROM_Trdos, ROM_128, ROM_48 };

#define CONFIG_H_MODIFIED __TIMESTAMP__

struct Cfg {
    char format_id[32];
    struct Main {
        int model = HW_Pentagon_128;
        char rom_path[sizeof(ROM_Bank)][PATH_MAX] = {
            "data/rom/trdos.rom",
            "data/rom/128.rom",
            "data/rom/48.rom"
        };
        bool full_speed = false;
    } main;
    struct Video {
        int screen_scale = 2;
        int filter = Nearest;
        bool full_screen = false;
        bool vsync = true;
    } video;
    struct Audio {
        int dsp_rate = 44100;
        int lpf_rate = 11050;
        int ay_mixer = ABC;
        float ay_side = 1.0;
        float ay_center = 0.7;
        float ay_penetr = 0.3;
        float ay_volume = 1.0f;
        float speaker_volume = 0.70f;
        float tape_volume = 0.35f;
    } audio;
    struct UI {
        float alpha = 0.95f;
        bool gamepad = false;
    } ui;
};

namespace Config {
    Cfg& load(const char *path);
    void save(const char *path);
    Cfg& get();
    Cfg& get_defaults();
}
