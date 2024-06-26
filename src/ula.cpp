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
//#include <emmintrin.h>

ULA::ULA(){
    for (int i = 0; i < 0x10000; i++){
        u8 paper_color = (i >> 11) & 0x0F;
        u8 ink_color = (((i & 0x700) | ((i >> 3) & 0x800)) >> 8) & 0x0F;
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
    table[BORDER_TOP_HEIGHT + 192*3 + (ZX_SCREEN_HEIGHT - BORDER_TOP_HEIGHT - 192)].len = 0;
    reset();
}

void ULA::update(s32 clk){
    while (update_clk < clk){
        int offset = update_clk - table[idx].clk;
        int limit = offset + MIN(clk, table[idx].clk + table[idx].len) - update_clk;
        if (table[idx].type == Border){
            while (offset < limit)
                frame_buffer[offset++] = border_color;
        }else{
            u8 *color = &display_page[table[idx].color + (offset >> 2)];
            u8 *pixel = &display_page[table[idx].pixel + (offset >> 2)];
            while (offset < limit){
                void *src = &pixel_table[(((*color++ & flash_mask) << 8) | *pixel++) << 3];
                do {
                    frame_buffer[offset] = ((u16*)src)[offset & 3];
                } while (++offset & 3);
            }
        }
        if (limit < table[idx].len)
            update_clk = clk;
        else{
            frame_buffer += table[idx].len;
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
    display_page = Memory::port_7FFD & ULA_PAGE5 ? Memory::ram[7] : Memory::ram[5];
    border_color = 0x0707;
    update_clk = table[0].clk;
    idx = 0;
}

void ULA::write(u16 port, u8 byte, s32 clk){
    Memory::write(port, byte, clk);
    if (!(port & 0x01)){
        if ((border_color & 0x07) ^ (byte & 0x07)){
            update(clk);
            border_color = (byte & 0x07) * 0x0101;
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
    if (port == 0xFF){
        update(clk);
        if (table[idx].type == Paper && clk >= table[idx].clk && clk < table[idx].clk + table[idx].len)
            *byte &= display_page[table[idx].color + (clk - table[idx].clk) / 4];
    }
}
