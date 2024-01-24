#define COLOR_DARK              200
#define COLOR_BRIGHT            255

#define SCREEN_LINE_CLK         224
#define SCREEN_START_CLK        SCREEN_LINE_CLK * 56 + ((SCREEN_WIDTH - 256) / 2) / 2 + 26

//#define SLOW_MEM_TEST

class ULA : public Memory {
    public: 
        enum Region { BORDER, PAPER };
        struct Display { 
	        Region region;
	        int clk;
            int end;
            unsigned short *p_frame;
	        int pixel;
	        int attr;
        };
        ULA();
#ifdef SLOW_MEM_TEST
        inline unsigned char read_byte_ex(unsigned short ptr, int clk){
            unsigned char *p_base = p_page_ex[ptr >> 0x0E];
            if (&p_base[ptr & 0x3FFF] == p_screen_page and display[idx].region == Region::PAPER and clk > display[idx].clk)
                clk += clk - display[idx].clk % 6;
            return p_base[ptr];
        };
        inline unsigned char read_byte(unsigned short ptr, int clk){
            unsigned char *p_base = p_page_rd[ptr >> 0x0E];
            if (&p_base[ptr & 0x3FFF] == p_screen_page and display[idx].region == Region::PAPER and clk > display[idx].clk)
                clk += clk - display[idx].clk % 6;
            return p_base[ptr];
        };
#else
#endif
        inline void write_byte(unsigned short ptr, unsigned char byte, int clk){
            unsigned char *p_base = p_page_wr[ptr >> 0x0E];
            //if (&p_base[ptr & 0x3FFF] == p_screen_page and display[idx].region == Region::PAPER and clk > display[idx].clk){
                update(clk);
#ifdef SLOW_MEM_TEST
                clk += clk - display[idx].clk % 4;
#endif
            //}
            //if (&p_base[ptr & ~0x3FFF] == p_screen_page)
            p_base[ptr] = byte;
        };
        void update(int clk);
        void frame(int frame_clk);
        void reset();
        bool io_wr(unsigned short port, unsigned char byte, int clk);
        bool io_rd(unsigned short port, unsigned char *p_byte, int clk);
        unsigned short* get_frame_buffer(){ return frame_buffer; };
    private:
        void make_table();
        unsigned short frame_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
        unsigned short pixel_table[0x10000 * 8];
        Display display[SCREEN_HEIGHT * 4];
        unsigned short palette[0x10];
        unsigned char *p_screen_page;
        unsigned char pFE;
        int idx;
        int update_clk;
        char flash_mask;
        int frame_cnt;
};
