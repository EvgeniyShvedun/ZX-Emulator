#include <cstddef>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "utils.h"
#include "config.h"
#include "device.h"
#include "memory.h"
#include "ula.h"

enum Screen { Border, Paper };

static struct { 
    Screen type;
    s32 clk;
    int pos;
    int end;
    int pixel;
    int attrs;
} table[SCREEN_HEIGHT*4];

inline u16 RGB565(u16 r, u16 g, u16 b){
    return (((r << 11) | (g << 5)) | b);
}
inline u16 RGB5551(u16 r, u16 g, u16 b){
    return (((r << 11) | (g << 6)) | b << 1) | 0x01;
}
inline u16 RGB444(u16 r, u16 g, u16 b){
    return ((r << (4+4+4)) | (g << (4+4)) | (b << 4) | 0x0F);
}

ULA::ULA(){
    for (idx = 0x00; idx < 0x10; idx++){
        u16 intensity = idx >= 0x08 ? 0x0F : 0x0F * DARK_INTENSITY;
        palette[idx] = RGB444(
            ((idx & 0x02) ? intensity : 0x00),
            ((idx & 0x04) ? intensity : 0x00),
            (idx & 0x01) ? intensity : 0x00);
    }
    for (idx = 0; idx < 0x10000; idx++){
        u16 paper = palette[idx >> (8+3) & 0x0F];
        u16  ink = palette[(((idx & 0x700) | ((idx >> 3) & 0x800)) >> 8) & 0x0F];
        for (int bit = 0; bit < 8; bit++)
            pixel_table[idx * 8 + bit] = (((idx >> 8) & 0x80) >> bit) ^ (idx & (0x80 >> bit)) ? ink : paper;
    }
    idx = 0;
    for (int line = 0; line < (SCREEN_HEIGHT-192)/2; line++){
        table[idx].type = Border;
        table[idx].clk = SCANLINE_CLK * line + SCREEN_START_CLK;
        table[idx].pos = SCREEN_WIDTH * line;
        table[idx].end = table[idx].clk + VISIBLELINE_CLK;
        idx++;
    }       
    for (int line = 0; line < 192; line++){
        table[idx].type = Border;
        table[idx].clk = SCANLINE_CLK * ((SCREEN_HEIGHT-192)/2 + line) + SCREEN_START_CLK;
        table[idx].pos = SCREEN_WIDTH * ((SCREEN_HEIGHT-192)/2 + line);
        table[idx].end = table[idx].clk + (SCREEN_WIDTH - 256) / 2 / 2;
        idx++;
        table[idx].type = Paper;
        table[idx].clk = table[idx-1].end;
        table[idx].pos = table[idx-1].pos + (SCREEN_WIDTH - 256) / 2;
        table[idx].end = table[idx-1].end + 128;
        table[idx].pixel = ((line & 0x07) * 256) + (((line >> 3) & 7) * 32) + (((line >> 6) & 3) * (256 * 8));
        table[idx].attrs = 0x1800 + (((line >> 3) & 0x07) * 32) + (((line >> 6) & 3) * (32 * 8));
        idx++;
        table[idx].type = Border;
        table[idx].clk = table[idx-1].end;
        table[idx].pos = table[idx-1].pos + 256;
        table[idx].end = table[idx-2].clk + VISIBLELINE_CLK;
        idx++;
    }
    for (int line = (SCREEN_HEIGHT - 192) / 2 + 192; line < SCREEN_HEIGHT; line++){
        table[idx].type = Border;
        table[idx].clk = SCANLINE_CLK * line + SCREEN_START_CLK;
        table[idx].end = table[idx].clk + VISIBLELINE_CLK;
        table[idx].pos = SCREEN_WIDTH * line;
        idx++;
    }
    table[idx].type = Border;
    table[idx].clk = 0xFFFFFF;
    reset();
}

void ULA::update(s32 clk){
    int offset, limit;
    u16 *ptr;
    while (update_clk < clk){
        limit = (clk < table[idx].end) ? clk : table[idx].end;
        switch (table[idx].type){
            case Border:
                ptr = &buffer[table[idx].pos + (update_clk - table[idx].clk) * 2];
                for (; update_clk < limit; update_clk++){
                    *ptr++ = palette[port_wFE & 0x07];
                    *ptr++ = palette[port_wFE & 0x07];
                }
                break;
            case Paper:
                offset = (update_clk - table[idx].clk) / 4;
                double* ptr = (double*)&buffer[table[idx].pos];
                for (; update_clk < limit; update_clk += 4){
                    ptr[offset*2] = ((double*)&pixel_table[(((screen_page[table[idx].attrs + offset] & flash_mask) << 8) | screen_page[table[idx].pixel + offset]) << 3])[0];
                    ptr[offset*2+1] = ((double*)&pixel_table[(((screen_page[table[idx].attrs + offset] & flash_mask) << 8) | screen_page[table[idx].pixel + offset]) << 3])[1]; 
                    offset++;
                }
                break;
        }
        if (update_clk >= table[idx].end)
            update_clk = table[++idx].clk;
    }
}

void ULA::frame(s32 frame_clk){
    update(frame_clk);
    update_clk = table[0].clk;
    if (!(frame_count++ % 16))
        flash_mask ^= 0x80;
    idx = 0;
}

void ULA::reset(){
    Memory::reset();
    //printf("ROM byte: %02x\n", read_byte_ex(1));
    screen_page = ram[5];
    update_clk = table[0].clk;
    port_wFE = 0xFF;
    idx = 0;
}

void ULA::write(u16 port, u8 byte, s32 clk){
    Memory::write(port, byte, clk);
    if (!(port & 0x01)){
        if ((port_wFE ^ byte) & 0x07){
            update(clk);
            port_wFE = byte;
        }
    }else{
        if (!(port & 0x8002)){ // 7FFD decoded if A2 and A15 is zero.
            //if (port_7FFD & PORT_LOCKED)
            //    return;
            if ((port_7FFD ^ byte) & SCREEN_PAGE7){
                update(clk);
                screen_page = byte & SCREEN_PAGE7 ? ram[7] : ram[5];
            }
        }
    }
}

void ULA::read(u16 port, u8 *byte, s32 clk){
    if (port == 0xFF){
        update(clk);
        if (table[idx].type == Paper && clk >= table[idx].clk && clk < table[idx].end)
            *byte &= screen_page[table[idx].attrs + (clk - table[idx].clk) / 4];
    }
}
