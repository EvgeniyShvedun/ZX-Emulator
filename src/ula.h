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

#define LOW_INTENSITY       0.84615
#define BORDER_TOP          20
#define BORDER_LEFT         32
#define SCANLINE_CLK        224
#define LINE_CLK            ZX_SCREEN_WIDTH/2
#define START_CLK           SCANLINE_CLK*16+24

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
        u32 palette[0x10];
        u8 *screen_page = NULL;
        u8 flash_mask = 0x7F;
        u8 port_wFE = 0xFF;
        s32 update_clk = 0;
        int frame_count = 0;
        int idx = 0;
};
