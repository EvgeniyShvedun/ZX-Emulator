#include <cstddef>
#include <stdio.h>
#include "base.h"

inline GLushort RGB565(GLushort r, GLushort g, GLushort b){
    return (((r << 11) | (g << 5)) | b);
}
inline GLushort RGB5551(GLushort r, GLushort g, GLushort b){
    return (((r << 11) | (g << 6)) | b << 1) | 0x01;
}
inline GLushort RGB444(GLushort r, GLushort g, GLushort b){
    return ((r << (4+4+4)) | (g << (4+4)) | (b << 4) | 0x0F);
}

ULA::ULA(){
    for (idx = 0x00; idx < 0x10; idx++){
        GLushort intensity = 0x0F;
        if (idx < 0x08)
            intensity *= LOW_BRIGHTNESS;
        palette[idx] = RGB444(
                ((idx & 0x02) ? intensity : 0x00),
                ((idx & 0x04) ? intensity : 0x00),
                (idx & 0x01) ? intensity : 0x00);
    }
    for (idx = 0; idx < 0x10000; idx++){
        GLshort paper = palette[idx >> (8+3) & 0x0F];
        GLshort ink = palette[(((idx & 0x700) | ((idx >> 3) & 0x800)) >> 8) & 0x0F];
        for (int bit = 0; bit < 8; bit++)
            pixel_table[idx * 8 + bit] = (((idx >> 8) & 0x80) >> bit) ^ (idx & (0x80 >> bit)) ? ink : paper;
    }
    idx = 0;
    for (int line = 0; line < (SCREEN_HEIGHT-192)/2; line++){
        display[idx].type = Type::Border;
        display[idx].clk = SCANLINE_CLK * line + SCREEN_START_CLK;
        display[idx].pos = SCREEN_WIDTH * line;
        display[idx].end = display[idx].clk + SCREENLINE_CLK;
        idx++;
    }       
    for (int line = 0; line < 192; line++){
        display[idx].type = Type::Border;
        display[idx].clk = SCANLINE_CLK * ((SCREEN_HEIGHT-192)/2 + line) + SCREEN_START_CLK;
        display[idx].pos = SCREEN_WIDTH * ((SCREEN_HEIGHT-192)/2 + line);
        display[idx].end = display[idx].clk + (SCREEN_WIDTH - 256) / 2 / 2;
        idx++;
        display[idx].type = Type::Paper;
        display[idx].clk = display[idx-1].end;
        display[idx].pos = display[idx-1].pos + (SCREEN_WIDTH - 256) / 2;
        display[idx].end = display[idx-1].end + 128;
        display[idx].pixel = ((line & 0x07) * 256) + (((line >> 3) & 7) * 32) + (((line >> 6) & 3) * (256 * 8));
        display[idx].attrs = 0x1800 + (((line >> 3) & 0x07) * 32) + (((line >> 6) & 3) * (32 * 8));
        idx++;
        display[idx].type = Type::Border;
        display[idx].clk = display[idx-1].end;
        display[idx].pos = display[idx-1].pos + 256;
        display[idx].end = display[idx-2].clk + SCREENLINE_CLK;
        idx++;
    }
    for (int line = (SCREEN_HEIGHT - 192) / 2 + 192; line < SCREEN_HEIGHT; line++){
        display[idx].type = Type::Border;
        display[idx].clk = SCANLINE_CLK * line + SCREEN_START_CLK;
        display[idx].end = display[idx].clk + SCREENLINE_CLK;
        display[idx].pos = SCREEN_WIDTH * line;
        idx++;
    }
    display[idx].type = Type::Border;
    display[idx].clk = 0xFFFFFF;
    reset();
}

void ULA::update(int clk){
    int offset, limit;
    GLushort *p_screen;
    while (update_clk < clk){
        limit = (clk < display[idx].end) ? clk : display[idx].end;
        switch (display[idx].type){
            case Type::Border:
                p_screen = &p_buffer[display[idx].pos + (update_clk - display[idx].clk) * 2];
                for (; update_clk < limit; update_clk++){
                    *p_screen++ = palette[pFE & 0x07];
                    *p_screen++ = palette[pFE & 0x07];
                }
                break;
            case Type::Paper:
                offset = (update_clk - display[idx].clk) / 4;
                double* p_screen = (double*)&p_buffer[display[idx].pos];
                for (; update_clk < limit; update_clk += 4){
                    p_screen[offset*2] = ((double*)&pixel_table[(((p_page[display[idx].attrs + offset] & flash_mask) << 8) | p_page[display[idx].pixel + offset]) << 3])[0];
                    p_screen[offset*2+1] = ((double*)&pixel_table[(((p_page[display[idx].attrs + offset] & flash_mask) << 8) | p_page[display[idx].pixel + offset]) << 3])[1]; 
                    offset++;
                }
                break;
        }
        if (update_clk >= display[idx].end)
            update_clk = display[++idx].clk;
    }
}

void ULA::frame(int frame_clk){
    update(frame_clk);
    update_clk = display[0].clk;
    if (!(frame_count++ % 16))
        flash_mask ^= 0x80;
    idx = 0;
}

void ULA::reset(){
    Memory::reset();
    p_page = p_ram[5];
    update_clk = display[0].clk;
    pFE = 0;
    idx = 0;
}

bool ULA::io_wr(unsigned short addr, unsigned char byte, int clk){
    if (!(addr & 0x01)){
        if ((pFE ^ byte) & 0x07){
            update(clk);
            pFE = byte;
        }
    }else{
        if (!(addr & 0x8002)){ // 7FFD decoded if A2 and A15 is zero.
            if (p7FFD & P7FFD_LOCK)
                return true;
            if ((p7FFD ^ byte) & P7FFD_SCR7){
                update(clk);
                p_page = byte & P7FFD_SCR7 ? p_ram[7] : p_ram[5];
            }
        }
    }
    return Memory::io_wr(addr, byte, clk);
}

bool ULA::io_rd(unsigned short addr, unsigned char *p_byte, int clk){
    if (addr & 0x01){
        update(clk);
        if (display[idx].type == Type::Paper and clk >= display[idx].clk and clk < display[idx].end)
            *p_byte &= p_page[display[idx].attrs + (clk - display[idx].clk) / 4];
    }
    return false;
}
