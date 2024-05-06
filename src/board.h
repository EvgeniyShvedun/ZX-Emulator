class Board : public IO {
    public:
        Board();
        ~Board();
        void setup(Hardware model);
        void frame();
        void reset();

        bool load_file(const char *path);
        bool save_file(const char *path);

        void run(CFG &cfg);

        void viewport_setup(int width, int height);
        void set_window_size(int width, int height);
        void set_video_filter(Filter filter);
        void set_full_screen(bool state);
        void set_vsync(bool state);

        void read(u16 port, u8 *byte, s32 clk=0);
        void write(u16 port, u8 byte, s32 clk=0);

        Z80 cpu;
        ULA ula;
        Sound sound;
        Tape tape;
        Keyboard keyboard;
        s32 frame_clk;
    private:
        SDL_Window *window = NULL;
        int viewport_width = -1;
        int viewport_height = -1;
        GLuint screen_texture = 0;
        GLuint pbo = 0;
        // Devices
        FDC fdc;
        Joystick joystick;
        Mouse mouse;
        bool paused = false;
};
