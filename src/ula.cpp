#include <cstddef>
#include <stdio.h>
#include "base.h"

enum Type { Border, Paper };

static struct { 
    Type type;
    int clk;
    int pos;
    int end;
    int pixel;
    int attrs;
} table[SCREEN_HEIGHT*4];

inline GLshort RGB565(GLshort r, GLshort g, GLshort b){
    return (((r << 11) | (g << 5)) | b);
}
inline GLshort RGB5551(GLshort r, GLshort g, GLshort b){
    return (((r << 11) | (g << 6)) | b << 1) | 0x01;
}
inline GLshort RGB444(GLshort r, GLshort g, GLshort b){
    return ((r << (4+4+4)) | (g << (4+4)) | (b << 4) | 0x0F);
}

ULA::ULA(){
    for (idx = 0x00; idx < 0x10; idx++){
        GLshort intensity = 0x0F;
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
        table[idx].type = Type::Border;
        table[idx].clk = SCANLINE_CLK * line + SCREEN_START_CLK;
        table[idx].pos = SCREEN_WIDTH * line;
        table[idx].end = table[idx].clk + SCREENLINE_CLK;
        idx++;
    }       
    for (int line = 0; line < 192; line++){
        table[idx].type = Type::Border;
        table[idx].clk = SCANLINE_CLK * ((SCREEN_HEIGHT-192)/2 + line) + SCREEN_START_CLK;
        table[idx].pos = SCREEN_WIDTH * ((SCREEN_HEIGHT-192)/2 + line);
        table[idx].end = table[idx].clk + (SCREEN_WIDTH - 256) / 2 / 2;
        idx++;
        table[idx].type = Type::Paper;
        table[idx].clk = table[idx-1].end;
        table[idx].pos = table[idx-1].pos + (SCREEN_WIDTH - 256) / 2;
        table[idx].end = table[idx-1].end + 128;
        table[idx].pixel = ((line & 0x07) * 256) + (((line >> 3) & 7) * 32) + (((line >> 6) & 3) * (256 * 8));
        table[idx].attrs = 0x1800 + (((line >> 3) & 0x07) * 32) + (((line >> 6) & 3) * (32 * 8));
        idx++;
        table[idx].type = Type::Border;
        table[idx].clk = table[idx-1].end;
        table[idx].pos = table[idx-1].pos + 256;
        table[idx].end = table[idx-2].clk + SCREENLINE_CLK;
        idx++;
    }
    for (int line = (SCREEN_HEIGHT - 192) / 2 + 192; line < SCREEN_HEIGHT; line++){
        table[idx].type = Type::Border;
        table[idx].clk = SCANLINE_CLK * line + SCREEN_START_CLK;
        table[idx].end = table[idx].clk + SCREENLINE_CLK;
        table[idx].pos = SCREEN_WIDTH * line;
        idx++;
    }
    table[idx].type = Type::Border;
    table[idx].clk = 0xFFFFFF;
    reset();
}

void ULA::update(int clk){
    int offset, limit;
    GLshort *ptr;
    while (update_clk < clk){
        limit = (clk < table[idx].end) ? clk : table[idx].end;
        switch (table[idx].type){
            case Type::Border:
                ptr = &buffer[table[idx].pos + (update_clk - table[idx].clk) * 2];
                for (; update_clk < limit; update_clk++){
                    *ptr++ = palette[pFE & 0x07];
                    *ptr++ = palette[pFE & 0x07];
                }
                break;
            case Type::Paper:
                offset = (update_clk - table[idx].clk) / 4;
                double* ptr = (double*)&buffer[table[idx].pos];
                for (; update_clk < limit; update_clk += 4){
                    ptr[offset*2] = ((double*)&pixel_table[(((page[table[idx].attrs + offset] & flash_mask) << 8) | page[table[idx].pixel + offset]) << 3])[0];
                    ptr[offset*2+1] = ((double*)&pixel_table[(((page[table[idx].attrs + offset] & flash_mask) << 8) | page[table[idx].pixel + offset]) << 3])[1]; 
                    offset++;
                }
                break;
        }
        if (update_clk >= table[idx].end)
            update_clk = table[++idx].clk;
    }
}

void ULA::frame(int frame_clk){
    update(frame_clk);
    update_clk = table[0].clk;
    if (!(frame_count++ % 16))
        flash_mask ^= 0x80;
    idx = 0;
}

void ULA::reset(){
    Memory::reset();
    page = p_ram[5];
    update_clk = table[0].clk;
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
                page = byte & P7FFD_SCR7 ? p_ram[7] : p_ram[5];
            }
        }
    }
    return Memory::io_wr(addr, byte, clk);
}

bool ULA::io_rd(unsigned short port, unsigned char *byte, int clk){
    if (port & 0x01){
        update(clk);
        if (table[idx].type == Type::Paper and clk >= table[idx].clk and clk < table[idx].end)
            *byte &= page[table[idx].attrs + (clk - table[idx].clk) / 4];
        return true;
    }
    return false;
}
