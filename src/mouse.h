class KMouse : public Device {
    public:
        KMouse();
        bool io_rd(unsigned short port, unsigned char *p_val, int clk);
        void wheel(char pos);
        void button_press(char button);
        void button_release(char button);
        void motion(char x, char y);
    private:
        char wheel_and_button;
        char x_coord;
        char y_coord;
};
