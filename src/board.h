#define STATUS_PIC_X                    SCREEN_WIDTH - 18
#define STATUS_PIC_Y                    SCREEN_HEIGHT - 18
#define STATUS_PIC_WIDTH                16
#define STATUS_PIC_HEIGHT               16


#define HW_SPECTRUM_128                 0
#define HW_SPECTRUM_48                  1
#define HW_PENTAGON_128                 3


class Board : public IO {
    public:
        Board(HW hw);
        void set_hw(HW hw){ this->hw = hw; frame_clk = hw_frame_clk[hw]; };
        void frame();
        void reset();
        void load_rom(int id, const char *p_path){ ula.load_rom(id, p_path); };
        void load_z80(const char *p_path){ set_hw(snapshot.load_z80(p_path, &cpu, &ula, this)); };
        void set_frame_buffer(void *ptr) { ula.set_frame_buffer((GLushort*)ptr); };
        bool trdos_active() { return ula.trdos_active(); };
        void set_rom(int rom_id){ ula.set_rom(rom_id); };
        int frame_clk;
    private:
        //void draw_device_status();
        ULA ula;
        Z80 cpu;
        Snapshot snapshot;
        HW hw;
        int hw_frame_clk[3] = { 70908, 69888,  71680 };
};
