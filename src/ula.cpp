#include <cstddef>
#include <stdio.h>
#include "base.h"

ULA::ULA() {
    for (int i = 0x00; i < 0x08; i++)
        palette[i] = RGBA32_4444(((i & 0x02 ? COLOR_DARK : 0x00) << 24) | ((i & 0x04 ? COLOR_DARK : 0x00) << 16) | ((i & 0x01 ? COLOR_DARK : 0x00) << 8) | 0xFF);
    for (int i = 0x08; i < 0x10; i++)
        palette[i] = RGBA32_4444(((i & 0x02 ? COLOR_BRIGHT : 0x00) << 24) | ((i & 0x04 ? COLOR_BRIGHT : 0x00) << 16) | ((i & 0x01 ? COLOR_BRIGHT : 0x00) << 8) | 0xFF);
    for (int i = 0; i < 0x10000; i++){
        unsigned short color_one = palette[(i >> (8 + 3)) & 0x0F];
        unsigned short color_two = palette[(((i & 0x700) | ((i >> 3) & 0x800)) >> 8) & 0x0F];
        for (int b = 0; b < 8; b++)
            pixel_table[i * 8 + b] = !(i & 0x8000) && i & (0x80 >> b) ? color_two : color_one;
    }
    make_table();
    pFE = 0x00;
    reset();
}

void ULA::make_table(){
    int clk = SCREEN_START_CLK;
    int line = 0;
    idx = 0;
    for (int i = 0; i < (SCREEN_HEIGHT - 192) / 2; i++){
        display[idx].region = Region::BORDER;
        display[idx].clk = clk;
        display[idx].end = clk + SCREEN_WIDTH / 2;
        display[idx++].p_frame = &frame_buffer[line * SCREEN_WIDTH];
        clk += SCREEN_LINE_CLK; 
        line++;
    }       
    for (int i = 0; i < 192; i++){
        display[idx].region = Region::BORDER;
        display[idx].clk = clk;
        display[idx++].p_frame = &frame_buffer[line * SCREEN_WIDTH];
        display[idx].region = Region::PAPER;
        display[idx-1].end = display[idx].clk = clk + (SCREEN_WIDTH - 256) / 2 / 2;
        display[idx].p_frame = &frame_buffer[line * SCREEN_WIDTH + (SCREEN_WIDTH - 256) / 2];
		display[idx].pixel = ((i & 0x07) * 256) + (((i >> 3) & 7) * 32) + (((i >> 6) & 3) * (256 * 8));
        display[idx++].attr = 0x1800 + (((i >> 3) & 0x07) * 32) + (((i >> 6) & 3) * (32 * 8));
        display[idx].region = Region::BORDER;
        display[idx-1].end = display[idx].clk = clk + ((SCREEN_WIDTH - 256) / 2 + 256) / 2;
        display[idx].end = clk + SCREEN_WIDTH / 2;
        display[idx++].p_frame = &frame_buffer[line * SCREEN_WIDTH + (SCREEN_WIDTH - 256) / 2 + 256];
        clk += SCREEN_LINE_CLK;
        line++;
    }
    for (int i = 0; i < (SCREEN_HEIGHT - 192) / 2; i++){
        display[idx].region = Region::BORDER;
        display[idx].clk = clk;
        display[idx].end = clk + SCREEN_WIDTH / 2;
        display[idx++].p_frame = &frame_buffer[line * SCREEN_WIDTH];
        clk += SCREEN_LINE_CLK;
        line++;
    }
    display[idx].region = Region::BORDER;
    display[idx].clk = 0xFFFFFF;
    display[idx].end = 0xFFFFFF;
    display[idx].p_frame = NULL;
}

void ULA::update(int clk){
    int offset;
    unsigned short *p_frame;
    unsigned char *p_attr, *p_pixel;
    while (update_clk < clk){
        switch(display[idx].region){
            case Region::BORDER:
                p_frame = &display[idx].p_frame[(update_clk - display[idx].clk) * 2];
                if (clk >= display[idx].end){
                    for (; update_clk < display[idx].end; update_clk++){
                        *p_frame++ = palette[pFE & 0x07];
                        *p_frame++ = palette[pFE & 0x07];
                    }
                    update_clk = display[++idx].clk;
                }else{
                    for (; update_clk < clk; update_clk++){
                        *p_frame++ = palette[pFE & 0x07];
                        *p_frame++ = palette[pFE & 0x07];
                    }
                }
                break;
            case Region::PAPER:
                //clk &= ~0x03;
                offset = (update_clk - display[idx].clk) / 4;
                p_attr = &p_screen_page[display[idx].attr + offset];
                p_pixel = &p_screen_page[display[idx].pixel + offset];
                p_frame = &display[idx].p_frame[offset * 8];
                /*
                offset = (update_clk - display[idx].clk);
                p_attr = &p_screen_page[display[idx].attr + offset / 4];
                p_pixel = &p_screen_page[display[idx].pixel + offset / 4];
                p_frame = &display[idx].p_frame[offset * 2];
                */
                if (clk >= display[idx].end){
                    for (; update_clk < display[idx].end; update_clk += 4){
                        double *p_src = (double*)&pixel_table[(((*p_attr++ & flash_mask) << 8) | *p_pixel++) * 8];
                        ((double*)p_frame)[0] = p_src[0];
                        ((double*)p_frame)[1] = p_src[1];
                        p_frame += 8;
                    }
                    /*
                    for (offset = (offset * 2) % 8; update_clk < display[idx].end; update_clk++){
                        *p_frame++ = *p_pixel & (0x80 >> offset) ? palette[(*p_attr & 0x07) | ((*p_attr >> 3) & 0x08)] : palette[(*p_attr >> 3) & 0x0F];
                        if (++offset >= 8){
                            p_attr++;
                            p_pixel++;
                            offset = 0;
                        }
                        *p_frame++ = *p_pixel & (0x80 >> offset) ? palette[(*p_attr & 0x07) | ((*p_attr >> 3) & 0x08)] : palette[(*p_attr >> 3) & 0x0F];
                        if (++offset >= 8){
                            p_attr++;
                            p_pixel++;
                            offset = 0;
                        }
                    }
                    */
                    update_clk = display[++idx].clk;
                }else{
                    /*
                    for (offset = (offset * 2) % 8; update_clk < clk; update_clk++){
                        *p_frame++ = *p_pixel & (0x80 >> offset) ? palette[(*p_attr & 0x07) | ((*p_attr >> 3) & 0x08)] : palette[(*p_attr >> 3) & 0x0F];
                        if (++offset >= 8){
                            p_attr++;
                            p_pixel++;
                            offset = 0;
                        }
                        *p_frame++ = *p_pixel & (0x80 >> offset) ? palette[(*p_attr & 0x07) | ((*p_attr >> 3) & 0x08)] : palette[(*p_attr >> 3) & 0x0F];
                        if (++offset >= 8){
                            p_attr++;
                            p_pixel++;
                            offset = 0;
                        }
                    }
                    */
                    for (; update_clk < clk; update_clk += 4){
                        double *p_src = (double*)&pixel_table[(((*p_attr++ & flash_mask) << 8) | *p_pixel++) * 8];
                        ((double*)p_frame)[0] = p_src[0];
                        ((double*)p_frame)[1] = p_src[1];
                        p_frame += 8;
                    }
                }
                break;
        }
    }
}

void ULA::frame(int frame_clk){
    update(frame_clk);
    idx = 0;
    update_clk = SCREEN_START_CLK;
    if (!(++frame_cnt % 16))
        flash_mask ^= 0x80;
}


void ULA::reset(){
    Memory::reset();
    p_screen_page = p_ram[5];
    flash_mask = 0x7F;
    frame_cnt = 0;
    update_clk = SCREEN_START_CLK;
    idx = 0;
}

bool ULA::io_wr(unsigned short port, unsigned char byte, int clk){
    if (!(port & 0x01)){
        if ((pFE ^ byte) & 0x07){
            update(clk);
            pFE = byte;
        }
    }else{
        if (!(port & 0x8002)){ // 7FFD decoded if A2 and A15 is zero.
            if (p7FFD & P7FFD_LOCK)
                return true;
            if ((p7FFD ^ byte) & P7FFD_SCREEN7){
                update(clk);
                p_screen_page = byte & P7FFD_SCREEN7 ? p_ram[7] : p_ram[5];
            }
        }
    }
    return Memory::io_wr(port, byte, clk);
}

bool ULA::io_rd(unsigned short port, unsigned char *p_byte, int clk){
    if (port & 0x01){
        update(clk);
        if (display[idx].region == Region::PAPER and clk >= display[idx].clk and clk < display[idx].end)
            *p_byte &= p_screen_page[display[idx].attr + (update_clk - display[idx].clk) / 4];
    }
    return false;
}
