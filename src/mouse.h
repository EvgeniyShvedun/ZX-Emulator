class Mouse : public Device {
    public:
        void motion(char x, char y);
        void button(char button, bool status);
        void wheel(char pos);
        bool io_rd(unsigned short port, unsigned char *val, int clk);
    private:
        char wheel_button = 0;
        char x_coord = 0;
        char y_coord = 0;
};
