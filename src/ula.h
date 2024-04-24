// Color brightness mode level.
#define LOW_BRIGHTNESS      0.85
#define SCANLINE_CLK        224
#define SCREENLINE_CLK      SCREEN_WIDTH/2
// Visible screen
#define SCREEN_START_CLK   ((SCANLINE_CLK * 34) + 20/2)



class ULA : public Memory {
    public: 
        ULA();
        inline void write_byte(unsigned short ptr, unsigned char byte, int clk=0){
            update(clk);
            p_page_wr[ptr >> 0x0E][ptr] = byte;
        }
        void update(int clk);
        void frame(int clk);
        void reset();

        bool io_wr(unsigned short port, unsigned char byte, int clk);
        bool io_rd(unsigned short port, unsigned char *byte, int clk);

        GLshort *buffer = NULL;
    private:
        GLshort pixel_table[0x10000*8];
        GLshort palette[0x10];
        unsigned char *page = NULL;
        unsigned char flash_mask = 0x7F;
        unsigned char pFE = 0x00;
        int update_clk = 0;
        int frame_count = 0;
        int idx = 0;
};
