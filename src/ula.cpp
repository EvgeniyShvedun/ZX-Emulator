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

enum RegionType { Border, Paper };

static struct { 
    RegionType type;
    s32 clk;
    s32 end;
    int pos;
    int rastr;
    int color;
} table[ZX_SCREEN_HEIGHT*4];


ULA::ULA(){
    for (int i = 0x00; i < 0x10; i++){
        u16 intensity = i & 0x08 ? 0x0F : 0x0F * LOW_INTENSITY;
        palette[i] = RGB4444((i & 0x02) ? intensity : 0x00, (i & 0x04) ? intensity : 0x00, (i & 0x01) ? intensity : 0x00, 0x0F);
    }
    for (int i = 0; i < 0x10000; i++){
        u16 paper_color = palette[(i >> 11) & 0x0F];
        u16 ink_color = palette[(((i & 0x700) | ((i >> 3) & 0x800)) >> 8) & 0x0F];
        for (int b = 0; b < 8; b++)
            pixel_table[i * 8 + b] = (((i >> 8) & 0x80) >> b) ^ (i & (0x80 >> b)) ? ink_color : paper_color;
    }
    for (int i = 0; i < BORDER_TOP; i++){
        table[i].type = Border;
        table[i].clk = START_CLK + SCANLINE_CLK*i;
        table[i].end = START_CLK + SCANLINE_CLK*i + ZX_SCREEN_WIDTH/2;
        table[i].pos = ZX_SCREEN_WIDTH*i;
    }       
    for (int i = 0; i < 192; i++){
        table[BORDER_TOP+i*3+0].type = Border;
        table[BORDER_TOP+i*3+0].clk = START_CLK + SCANLINE_CLK*(BORDER_TOP + i);
        table[BORDER_TOP+i*3+0].end = START_CLK + SCANLINE_CLK*(BORDER_TOP + i) + BORDER_LEFT/2;
        table[BORDER_TOP+i*3+0].pos = ZX_SCREEN_WIDTH*(BORDER_TOP + i);
        table[BORDER_TOP+i*3+1].type = Paper;
        table[BORDER_TOP+i*3+1].clk = START_CLK + SCANLINE_CLK*(BORDER_TOP + i) + BORDER_LEFT/2;
        table[BORDER_TOP+i*3+1].end = START_CLK + SCANLINE_CLK*(BORDER_TOP + i) + BORDER_LEFT/2 + 256/2;
        table[BORDER_TOP+i*3+1].pos = ZX_SCREEN_WIDTH*(BORDER_TOP + i) + BORDER_LEFT;
        table[BORDER_TOP+i*3+1].rastr = ((i & 0x07)*256) + (((i >> 3) & 7)*32) + (((i >> 6) & 3)*(256*8));
        table[BORDER_TOP+i*3+1].color = 0x1800 + (((i >> 3) & 0x07)*32) + (((i >> 6) & 3)*(32*8));
        table[BORDER_TOP+i*3+2].type = Border;
        table[BORDER_TOP+i*3+2].clk = START_CLK + SCANLINE_CLK*(BORDER_TOP + i) + BORDER_LEFT/2 + 256/2;
        table[BORDER_TOP+i*3+2].end = START_CLK + SCANLINE_CLK*(BORDER_TOP + i) + ZX_SCREEN_WIDTH/2;
        table[BORDER_TOP+i*3+2].pos = ZX_SCREEN_WIDTH*(BORDER_TOP + i) + BORDER_LEFT + 256;
    }
    for (int i = 0; i < (ZX_SCREEN_HEIGHT - BORDER_TOP - 192); i++){
        table[BORDER_TOP+192*3+i].type = Border;
        table[BORDER_TOP+192*3+i].clk = START_CLK + SCANLINE_CLK*(BORDER_TOP+192+i);
        table[BORDER_TOP+192*3+i].end = START_CLK + SCANLINE_CLK*(BORDER_TOP+192+i) + ZX_SCREEN_WIDTH/2;
        table[BORDER_TOP+192*3+i].pos = ZX_SCREEN_WIDTH*(BORDER_TOP+192+i);
    }
    table[BORDER_TOP + 192*3 + (ZX_SCREEN_HEIGHT - BORDER_TOP - 192)].clk = 0xFFFFFF;
    reset();
}

void ULA::update(s32 clk){
    while (update_clk < clk){
        int limit = (clk <= table[idx].end) ? clk : table[idx].end;
        switch (table[idx].type){
            case Border:
                for (u16 *ptr = &buffer[table[idx].pos + (update_clk - table[idx].clk)*2]; update_clk < limit; update_clk++){
                    *ptr++ = palette[port_wFE & 0x07];
                    *ptr++ = palette[port_wFE & 0x07];
                }
                break;
            case Paper:
                int offset = (update_clk - table[idx].clk)/4;
                for (double *ptr = (double*)&buffer[table[idx].pos]; update_clk < limit; update_clk += 4){
                    ptr[offset*2] = ((double*)&pixel_table[(((screen_page[table[idx].color + offset] & flash_mask) << 8) | screen_page[table[idx].rastr + offset]) << 3])[0];
                    ptr[offset*2+1] = ((double*)&pixel_table[(((screen_page[table[idx].color + offset] & flash_mask) << 8) | screen_page[table[idx].rastr + offset]) << 3])[1]; 
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
            *byte &= screen_page[table[idx].color + (clk - table[idx].clk) / 4];
    }
}
