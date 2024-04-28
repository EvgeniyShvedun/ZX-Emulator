#define JOY_NONE            0b00000000
#define JOY_RIGHT           0b00000001
#define JOY_LEFT            0b00000010
#define JOY_DOWN            0b00000100
#define JOY_UP              0b00001000
#define JOY_A               0b00010000
#define JOY_B               0b00100000

class Joystick : public Device {
    public:
        void map(char mask, int code);
        void gamepad(int code, bool state);
        void button(char mask, bool state);
        void read(u16, u8 *byte, s32 clk);
    private:
        u8 port_1F = 0xC0;
        int gamepad_map[6];
};
