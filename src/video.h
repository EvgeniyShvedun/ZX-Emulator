namespace Video {
    void setup();
    void* update();
    void frame();
    void viewport_setup(int width, int height);
    void set_filter(Filter filter);
    void free();
}
