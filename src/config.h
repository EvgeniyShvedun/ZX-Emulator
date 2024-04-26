
enum ROM_BANK {
    ROM_TRDOS, ROM_128, ROM_48
};

enum HW_Model {
    HW_PENTAGON_128, HW_SPECTRUM_128, HW_SPECTRUM_48
};

enum AY_MODE { AY_ABC, AY_ACB, AY_MONO};

enum VF { 
    VF_NEAREST, VF_LINEAR
};

struct CFG {
    struct {
        int hw_model = HW_PENTAGON_128;
        char rom_path[sizeof(ROM_BANK)][PATH_MAX] = {
            "data/rom/trdos.rom",
            "data/rom/128.rom",
            "data/rom/48.rom"
        };
        bool full_speed = false;
    } main;
    struct {
        int scale = 2;
        int filter = VF_NEAREST;
        bool full_screen = false;
        bool vsync = true;
    } video;
    struct {
        int sample_rate = 44100;
        int lpf_rate = 11050;
        int ay_mixer_mode = AY_ABC;
        float ay_volume = 1.0;
        float speaker_volume = 0.7;
        float tape_volume = 0.3;
        float ay_side_level = 1.0;
        float ay_center_level = 0.7;
        float ay_interpenetration_level = 0.3;
    } audio;
    struct {
        int left = 15;
        int right = 16;
        int down = 14;
        int up = 13;
        int button_a = 1;
        int button_b = 3;
    } gamepad;
};

namespace Config {
    CFG *load(const char *path = "zx.dat");
    void save(const char *path = "zx.dat");
    CFG *get();
    CFG* get_defaults();
}
