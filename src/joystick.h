enum JoyButton {
    JB_Right = 0b00000001,
    JB_Left  = 0b00000010,
    JB_Down  = 0b00000100,
    JB_Up    = 0b00001000,
    JB_A     = 0b00010000,
    JB_B     = 0b00100000
};

class Joystick : public Device {
    public:
        Joystick();
        ~Joystick();
        void button(char mask, bool state);
        void read(u16, u8 *byte, s32 clk);
        void event(SDL_Event &event);
    private:
        SDL_GameController *controller;        
        u8 port_r1F = 0xC0;
};
