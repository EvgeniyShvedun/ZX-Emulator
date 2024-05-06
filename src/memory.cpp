#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "types.h"
#include "utils.h"
#include "config.h"
#include "device.h"
#include "memory.h"

Memory::Memory(){
    for (unsigned int i = 0; i < sizeof(ROM_Bank); i++){
        rom[i] = new u8[PAGE_SIZE]();
        trap[i] = new u8[PAGE_SIZE]();
    }
    for (int i = 0; i < RAM_PAGES; i++)
        ram[i] = new u8[PAGE_SIZE]();
    page_wr_null = new u8[PAGE_SIZE]();
    reset();
}

Memory::~Memory(){
    for (unsigned int i = 0; i < sizeof(ROM_Bank); i++){
        DELETE_ARRAY(rom[i]);
        DELETE_ARRAY(trap[i]);
    }
    for (int i = 0; i < RAM_PAGES; i++)
        DELETE_ARRAY(ram[i]);
    DELETE_ARRAY(page_wr_null);
}

void Memory::set_main_rom(ROM_Bank bank){
    main_rom = bank;
}

void Memory::reset(){
    port_7FFD = 0x00;
    page_wr[0] = page_wr_null;
    page_rd[1] = page_wr[1] = page_ex[1] = ram[5] - PAGE_SIZE*1;                        // RAM5 0x4000 - 0x7FFF
    page_rd[2] = page_wr[2] = page_ex[2] = ram[2] - PAGE_SIZE*2;                        // RAM2 0x8000 - 0xBFFF
    page_rd[3] = page_wr[3] = page_ex[3] = ram[port_7FFD & PAGE_MASK] - PAGE_SIZE*3;    // USER 0xC000 - 0xFFFF
    if (main_rom != ROM_Trdos){
        page_rd[0] = rom[main_rom];
        page_ex[0] = trap[main_rom];
    }else{
        page_rd[0] = page_ex[0] = rom[main_rom];
        page_ex[1] = trap[main_rom] - PAGE_SIZE*1;
        page_ex[2] = trap[main_rom] - PAGE_SIZE*2;
        page_ex[3] = trap[main_rom] - PAGE_SIZE*3;
    }
}

void Memory::write(u16 port, u8 byte, s32 clk){
    if (!(port & 0x8002)){ // 7FFD decoded if A2 and A15 is zero.
        //if (port_7FFD & PORT_LOCKED)
        //    return;
        if (byte & BANK0_ROM48){
            page_rd[0] = rom[ROM_48];
            page_ex[0] = trap[ROM_48];
        }else{
            page_rd[0] = rom[ROM_128];
            page_ex[0] = trap[ROM_128];
        }
        page_wr[0] = page_wr_null;
        page_rd[1] = page_wr[1] = page_ex[1] = ram[5] - PAGE_SIZE*1;                    // RAM5 0x4000 - 0x7FFF
        page_rd[2] = page_wr[2] = page_ex[2] = ram[2] - PAGE_SIZE*2;                    // RAM2 0x8000 - 0xBFFF
        page_rd[3] = page_wr[3] = page_ex[3] = ram[byte & PAGE_MASK] - PAGE_SIZE*3;     // USER 0xC000 - 0xFFFF
        port_7FFD = byte;
    }
}

bool Memory::trap_trdos(u16 pc){
    if (page_rd[0] == rom[ROM_48] && pc >> 8 == 0x3D){/////
        page_rd[0] = page_ex[0] = rom[ROM_Trdos];
        page_ex[1] = trap[ROM_Trdos] - PAGE_SIZE*1;
        page_ex[2] = trap[ROM_Trdos] - PAGE_SIZE*2;
        page_ex[3] = trap[ROM_Trdos] - PAGE_SIZE*3;
        return true;
    }else
        if (page_rd[0] == rom[ROM_Trdos] && pc >= 0x4000){
            page_rd[0] = rom[ROM_48];
            page_ex[0] = trap[ROM_48];
            page_ex[1] = ram[5] - PAGE_SIZE*1;
            page_ex[2] = ram[2] - PAGE_SIZE*2;
            page_ex[3] = ram[port_7FFD & PAGE_MASK] - PAGE_SIZE*3;
            return true;
        }
    return false;
}

void Memory::load_rom(ROM_Bank bank, const char *path){
    FILE *fp = fopen(path, "r");
    if (!fp)
		throw("Load ROM file");
    fread(rom[bank], 1, PAGE_SIZE, fp);
    fclose(fp);
    if (bank != ROM_Trdos){
        memcpy(trap[bank], rom[bank], PAGE_SIZE);
        if (bank == ROM_48)
            memset(trap[bank] + 0x3D00, TRAP_TRDOS_BYTE, 0x100);
    }else
        memset(trap[bank], TRAP_TRDOS_BYTE, PAGE_SIZE);
}
