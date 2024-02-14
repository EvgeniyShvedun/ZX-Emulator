// Color brightness mode level.
#define LOW_BRIGHTNESS      0.85
#define SCANLINE_CLK        224
#define SCREENLINE_CLK      SCREEN_WIDTH/2
// Visible screen
#define SCREEN_START_CLK   ((SCANLINE_CLK * 34) + 20/2)

enum Type { Border, Paper };

struct Display { 
    Type type;
    int clk;
    int pos;
    int end;
    int pixel;
    int attrs;
};

class ULA : public Memory {
    public: 
        ULA();
        void update(int clk);
        void frame(int clk);
        void reset();
        bool io_wr(unsigned short port, unsigned char byte, int clk);
        bool io_rd(unsigned short port, unsigned char *p_byte, int clk);
        inline void set_frame_buffer(GLushort *ptr){ p_buffer = ptr; };
        inline void write_byte(unsigned short ptr, unsigned char byte, int clk=0){
            update(clk);
            p_page_wr[ptr >> 0x0E][ptr] = byte;
        }
    private:
        Display display[SCREEN_HEIGHT*41];
        GLushort pixel_table[0x10000*8];
        GLushort *p_buffer = NULL;
        GLushort palette[0x10];
        unsigned char *p_page = NULL;
        int update_clk = 0;
        int frame_count = 0;
        unsigned char flash_mask = 0x7F;
        int idx = 0;
        unsigned char pFE = 0x00;
};
