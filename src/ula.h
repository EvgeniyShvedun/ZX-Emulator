#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       240
#define DARK_INTENSITY      0.85
#define SCANLINE_CLK        224
#define VISIBLELINE_CLK    SCREEN_WIDTH/2
#define SCREEN_START_CLK   ((SCANLINE_CLK * 34) + 20/2)

class ULA : public Memory {
    public: 
        ULA();
        inline void write_byte(u16 ptr, u8 byte, s32 clk){
            update(clk);
            page_wr[ptr >> 0x0E][ptr] = byte;
        }

        void read(u16 port, u8 *byte, s32 clk);
        void write(u16 port, u8 byte, s32 clk);
        void update(s32 clk);
        void frame(s32 clk);
        void reset();
        u8 read_wFE(){ return port_wFE; };

        u16 *buffer = NULL;
    private:
        u16 pixel_table[0x10000*8];
        u16 palette[0x10];
        u8 *screen_page = NULL;
        u8 flash_mask = 0x7F;
        u8 port_wFE = 0xFF;
        s32 update_clk = 0;
        int frame_count = 0;
        int idx = 0;
};
