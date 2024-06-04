// CPU cycles per PAL line
// ZX Spectrum 16K, 48K, and ZX Spectrum+  - 224
// 128K, +2, +2A, +3                       - 228
// Colours simulated as sRGB assume non-bright as 84.615% voltage (0.55 V) and bright as 100% (0.65 V).
/*  See the detailed info here: https://worldofspectrum.org/faq/reference/48kreference.htm
    The timing is expressed as a T-states of the CPU and, when recalculated to pixels, the top of the border is 64 lines, with 48 lines being visible as the actual border.
    The bottom side of the border has 56 lines, with some being outside of the visible area. On the left and the right sides there are 48 pixels, some of them are outside of the visible area. */
/*  The device uses 14 MHz crystal as the master clock, and it gets divided by 2 to get 7 MHz pixel clock.
    The active raster is 256 pixels, and if we assume that standard 15.625 kHz horizontal scan rate is used, it adds up to 448 total pixel clocks per line.
    Due to standard horizontal timing, there will be 364 pixels of video at most, leaving up to 108 pixels for border area.
    And due to standard video timing, there should be 312.5 lines per video field, or 625 per frame, of which up to 576 contain video.
    But we can safely assume it outputs progressive video at 312 total lines per frame with 288 active video. Not all are visible due to overscan. This leaves 96 lines for border area in total.
    Using this as a reference, I found a ZX Spectrum reverse-engineering website which confirms this, and actually has the values.
    Borders are 48 pixels on left and 48 pixels on right for a total of 96 pixels. Due to overscan, not all of them are visible on a screen. This leaves 96 pixels for horizontal blanking and synchronization area.
    The vertical border is said to be 48 lines before active raster and 56 lines after active raster. */

// PAL 364x312.5
// All vetical frame - 312.5l
// |------------|
// |-----48l----|- - 64l
// |            |
// |            |
// |-----56l----|  - 56l
// |------------|
// L/R border - 48px;


#define HIGH_BRIGHTNESS     1.0f
#define LOW_BRIGHTNESS      0.84615f
#define LINE_CLK            224
#define START_CLK           LINE_CLK*60+45
#define BORDER_TOP_HEIGHT   20
#define BORDER_SIDE_WIDTH   32

class ULA : public Memory {
    enum Type { Border = 0x00, Paper = 0x01, Last = 0x02};
    struct Table {
        Type type;
        s32 clk;
        s32 len;
        int color;
        int pixel;
    };
    public:
        ULA();
        inline void write_byte(u16 ptr, u8 byte, s32 clk){
            update(clk);
            page_wr[ptr >> 0x0E][ptr] = byte;
        }
        void frame_setup(u16 *buffer) { frame_buffer = buffer; };
        void update(s32 clk);

        void read(u16 port, u8 *byte, s32 clk);
        void write(u16 port, u8 byte, s32 clk);
        void frame(s32 clk);
        void reset();

        u8 get_border_color() { return border_color; };
    private:
        inline unsigned short RGBA4444(float r, float g, float b, float a){
            return ((((unsigned short)(0x0F*r)) << 12) |
                (((unsigned short)(0x0F*g)) << 8) |
                (((unsigned short)(0x0F*b)) << 4) |
                ((unsigned short)(0x0F*a)));
        }
        inline unsigned short RGBA5551(float r, float g, float b, float a){
            return ((((unsigned short)(0x1F*r)) << 11) |
                (((unsigned short)(0x1F*g)) << 6) |
                (((unsigned short)(0x1F*b)) << 1) |
                ((unsigned short)(a ? 1 : 0)));
        }
        inline unsigned short RGB565(float r, float g, float b){
            return ((((unsigned short)(0x1F*r)) << 11) |
                (((unsigned short)(0x3F*g)) << 5) |
                ((unsigned short)(0x1F*b)));
        }
        s32 update_clk = 0;
        int idx = 0;
        Table table[ZX_SCREEN_HEIGHT*4];
        u8 border_color;
        u8 *display_page = NULL;
        u8 flash_mask = 0x7F;
        u16 palette[0x10];
        u16 pixel_table[0x10000*8];
        u16 *frame_buffer = NULL;
        int frame_count = 0;
};
