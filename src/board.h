class Board : public IO {
    public:
        Board(Cfg &cfg);
        ~Board();
        void setup(Hardware model);
        void frame();
        void reset();

        bool load_file(const char *path);
        bool save_file(const char *path);

        void run(Cfg &cfg);

        void viewport_setup(int width, int height);
        void set_window_size(int width, int height);
        void set_texture_filter(Filter filter);
        void set_full_screen(bool state);
        void set_vsync(bool state);

        void read(u16 port, u8 *byte, s32 clk=0);
        void write(u16 port, u8 byte, s32 clk=0);
        Z80_State& cpu_state() { return cpu; };

        ULA ula;
        Sound sound;
        Tape tape;
        Keyboard keyboard;
        s32 frame_clk;
    private:
        Z80 cpu;
        Cfg &cfg;
        SDL_Window *window = NULL;
        GLuint screen_texture = 0;
        GLuint pbo = 0;
        int viewport_width = SCREEN_WIDTH;
        int viewport_height = SCREEN_HEIGHT;
        // Devices
        FDC fdc;
        Joystick joystick;
        Mouse mouse;
};
