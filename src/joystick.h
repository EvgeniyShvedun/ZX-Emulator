#define JOY_RIGHT           0b00000001
#define JOY_LEFT            0b00000010
#define JOY_DOWN            0b00000100
#define JOY_UP              0b00001000
#define JOY_A               0b00010000
#define JOY_B               0b00100000


class KJoystick : public Device {
    public:
        KJoystick();
        bool io_rd(unsigned short port, unsigned char *p_val, int clk);
        void button(int phy_idx, bool state);
        void config(int phy_idx, char button);
    private:
        unsigned char p1F;
        int button_map[6];
};
