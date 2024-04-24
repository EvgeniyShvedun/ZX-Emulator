
class Board : public IO {
    public:
        Board(CFG *cfg);
        ~Board();
        void set_hw(HW_Model hw);
        void event(SDL_Event &event);
        void load_rom(int id, const char *p_path){ ula.load_rom(id, p_path); };
        void load(const char *path);

        void viewport_resize(int width, int height);
        void video_filter(VF_Filter filter);
        void full_screen(bool on);
        void vsync(bool on);

        void frame();
        void reset();

        unsigned char read(unsigned short port, int clk = 0);
        void write(unsigned short port, unsigned char byte, int clk = 0);

        int frame_clk;
        Z80 cpu;
        ULA ula;
        Sound sound;
        Keyboard keyboard;
    private:
        GLuint screen_texture = 0;
        GLuint pbo = 0;
        int viewport_width = -1;
        int viewport_height = -1;
        Tape tape;
        Joystick joystick;
        Mouse mouse;
        FDC fdc;
        int hw;
};
