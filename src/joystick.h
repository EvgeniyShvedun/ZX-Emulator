#define JOY_RIGHT           0b00000001
#define JOY_LEFT            0b00000010
#define JOY_DOWN            0b00000100
#define JOY_UP              0b00001000
#define JOY_A               0b00010000
#define JOY_B               0b00100000


class KJoystick : public Device {
    public:
        void gamepad_event(int pad_btn, bool state);
        void set_state(unsigned char btn_mask, bool state);
        void map(char btn_mask, int pad_btn);
        bool io_rd(unsigned short addr, unsigned char *p_val, int clk);
    private:
        unsigned char p1F = 0x00;
        int button_map[6];
};
