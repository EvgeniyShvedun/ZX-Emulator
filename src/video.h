namespace Video {
    void setup();
    u16* update();
    void frame();
    void viewport_setup(int width, int height);
    void set_filter(Filter filter);
    void free();
}
