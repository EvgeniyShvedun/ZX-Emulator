class Mouse : public Device {
    public:
        void motion(char x, char y);
        void button(char button, bool status);
        void wheel(char pos);
        void read(u16 port, u8 *byte, s32 clk);
        void event(SDL_Event &event);
    private:
        char wheel_button = 0;
        char x_coord = 0;
        char y_coord = 0;
};
