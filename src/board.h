class Board : public IO {
    public:
        Board(CFG &cfg);
        ~Board();
        void setup(Hardware model);
        void frame();
        void reset();

        bool load_file(const char *path);
        bool save_file(const char *path);

        void set_viewport_size(int width, int height);
        void set_video_filter(VideoFilter filter);
        void set_full_speed(bool state);
        void set_full_screen(bool state);
        void set_vsync(bool state);
        void event(SDL_Event &event);

        void read(u16 port, u8 *byte, s32 clk=0);
        void write(u16 port, u8 byte, s32 clk=0);

        s32 frame_clk;
        Z80 cpu;
        ULA ula;
        Sound sound;
        Keyboard keyboard;
    private:
        bool frame_hold = false;
        int viewport_width = -1;
        int viewport_height = -1;
        GLuint screen_texture = 0;
        GLuint pbo = 0;
        // Devices
        FDC fdc;
        Tape tape;
        Joystick joystick;
        Mouse mouse;
};
