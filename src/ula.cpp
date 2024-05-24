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
#include <emmintrin.h>

ULA::ULA(){
    for (int i = 0x00; i < 0x10; i++){
        float bright = i & 0x08 ? HIGH_BRIGHTNESS : LOW_BRIGHTNESS;
        color64[i] = palette[i] = RGBA4444(bright*((i >> 1) % 2), bright*((i >> 2) % 2), bright*(i % 2), 1.0f);
        color64[i] |= (color64[i] << 48) | (color64[i] << 32) | (color64[i] << 16);
    }
    for (int i = 0; i < 0x10000; i++){
        u16 paper_color = palette[(i >> 11) & 0x0F];
        u16 ink_color = palette[(((i & 0x700) | ((i >> 3) & 0x800)) >> 8) & 0x0F];
        for (int b = 0; b < 8; b++)
            pixel_table[i * 8 + b] = (((i >> 8) & 0x80) >> b) ^ (i & (0x80 >> b)) ? ink_color : paper_color;
    }
    for (int i = 0; i < BORDER_TOP_HEIGHT; i++){
        table[i].type = Border;
        table[i].clk = START_CLK + LINE_CLK*i;
        table[i].len = ZX_SCREEN_WIDTH/2;
    }
    for (int i = 0; i < 192; i++){
        table[BORDER_TOP_HEIGHT+i*3+0].type = Border;
        table[BORDER_TOP_HEIGHT+i*3+0].clk = START_CLK + LINE_CLK*(BORDER_TOP_HEIGHT + i);
        table[BORDER_TOP_HEIGHT+i*3+0].len = BORDER_SIDE_WIDTH/2;
        table[BORDER_TOP_HEIGHT+i*3+1].type = Paper;
        table[BORDER_TOP_HEIGHT+i*3+1].clk = START_CLK + LINE_CLK*(BORDER_TOP_HEIGHT + i) + BORDER_SIDE_WIDTH/2;
        table[BORDER_TOP_HEIGHT+i*3+1].len = 256/2;
        table[BORDER_TOP_HEIGHT+i*3+1].pixel = ((i & 0x07)*256) + (((i >> 3) & 7)*32) + (((i >> 6) & 3)*(256*8));
        table[BORDER_TOP_HEIGHT+i*3+1].color = 0x1800 + (((i >> 3) & 0x07)*32) + (((i >> 6) & 3)*(32*8));
        table[BORDER_TOP_HEIGHT+i*3+2].type = Border;
        table[BORDER_TOP_HEIGHT+i*3+2].clk = START_CLK + LINE_CLK*(BORDER_TOP_HEIGHT + i) + BORDER_SIDE_WIDTH/2 + 256/2;
        table[BORDER_TOP_HEIGHT+i*3+2].len = BORDER_SIDE_WIDTH/2;
    }
    for (int i = 0; i < (ZX_SCREEN_HEIGHT - BORDER_TOP_HEIGHT - 192); i++){
        table[BORDER_TOP_HEIGHT+192*3+i].type = Border;
        table[BORDER_TOP_HEIGHT+192*3+i].clk = START_CLK + LINE_CLK*(BORDER_TOP_HEIGHT+192+i);
        table[BORDER_TOP_HEIGHT+192*3+i].len = ZX_SCREEN_WIDTH/2;
    }
    table[BORDER_TOP_HEIGHT + 192*3 + (ZX_SCREEN_HEIGHT - BORDER_TOP_HEIGHT - 192)].clk = 0xFFFFFF;
    reset();
}

void ULA::update(s32 clk){
    while (update_clk < clk){
        int offset = update_clk - table[idx].clk;
        int clocks = MIN(clk, table[idx].clk + table[idx].len) - update_clk;
        update_clk += clocks;
        if (table[idx].type == Border){
            long long color = color64[border_color];
            for (int end = offset + (clocks & ~3); offset < end; offset += 4){
                *(long long*)(&frame_buffer[offset*2])= color;
                *(long long*)(&frame_buffer[offset*2+4]) = color;
            }
            for (clocks &= 3; clocks--; offset++)
                ((u32*)frame_buffer)[offset] = (u32)color;
        }else{
            u8 *color = &display_page[table[idx].color + offset/4];
            u8 *pixel = &display_page[table[idx].pixel + offset/4];
            if (offset % 4){
                u32* src = (u32*)(&pixel_table[(((*color++ & flash_mask) << 8) | *pixel++) << 3]);
                do {
                    ((u32*)frame_buffer)[offset] = src[offset % 4];
                } while (--clocks && ++offset % 4);
            }
            for (int end = offset + (clocks & ~3); offset < end; offset += 4){
                long long *src = &((long long*)pixel_table)[(((*color++ & flash_mask) << 8) | *pixel++)<<1];
                *(long long*)(&frame_buffer[offset*2]) = src[0];
                *(long long*)(&frame_buffer[offset*2+4]) = src[1];
            }
            for (clocks &= 3; clocks--; offset++)
                ((u32*)frame_buffer)[offset] = ((u32*)(&pixel_table[(((*color & flash_mask) << 8) | *pixel) << 3]))[offset % 4];
        }
        if (clk >= table[idx].clk + table[idx].len){
            frame_buffer += table[idx].len*2;
            update_clk = table[++idx].clk;
        }
    }
}

void ULA::frame(s32 frame_clk){
    update(frame_clk);
    if (!(frame_count++ % 16))
        flash_mask ^= 0x80;
    update_clk = table[0].clk;
    frame_buffer = NULL;
    idx = 0;
}

void ULA::reset(){
    Memory::reset();
    display_page = !(Memory::Memory::port_7FFD & ULA_PAGE5) ? Memory::ram[5] : Memory::ram[7];
    border_color = 7;
    update_clk = table[0].clk;
    idx = 0;
}

void ULA::write(u16 port, u8 byte, s32 clk){
    Memory::write(port, byte, clk);
    if (!(port & 0x01)){
        if (border_color ^ (byte & 0x07)){
            update(clk);
            border_color = byte & 0x07;
        }
    }else{
        if (!(port & 0x8002)){ // 7FFD decoded if A2 and A15 is zero.
            //if (port_7FFD & PORT_LOCKED)
            //    return;
            if ((Memory::port_7FFD ^ byte) & ULA_PAGE5){
                update(clk);
                display_page = !(byte & ULA_PAGE5) ? Memory::ram[5] : Memory::ram[7];
            }
        }
    }
}

void ULA::read(u16 port, u8 *byte, s32 clk){
    if (port == 0xFF){ /////////////////
        update(clk);
        if (table[idx].type == Paper && clk >= table[idx].clk && clk < table[idx].clk + table[idx].len)
            *byte &= display_page[table[idx].color + (clk - table[idx].clk) / 4];
    }
}
