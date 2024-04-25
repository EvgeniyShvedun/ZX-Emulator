#include "base.h"

Memory::Memory(){
    for (int i = 0; i < ROM_PAGES; i++){
        p_rom[i] = new unsigned char [PAGE_SIZE]();
        p_trap[i] = new unsigned char [PAGE_SIZE]();
    }
    for (int i = 0; i < RAM_PAGES; i++)
        p_ram[i] = new unsigned char [PAGE_SIZE]();
    p_page_wr_null = new unsigned char [PAGE_SIZE]();
    set_rom(ROM_128);
    reset();
}

Memory::~Memory(){
    for (int i = 0; i < ROM_PAGES; i++){
        DELETE_ARRAY(p_rom[i]);
        DELETE_ARRAY(p_trap[i]);
    }
    for (int i = 0; i < RAM_PAGES; i++)
        DELETE_ARRAY(p_ram[i]);
    DELETE_ARRAY(p_page_wr_null);
}

void Memory::reset(){
    p7FFD = 0x00;
    p_page_wr[0] = p_page_wr_null;
    p_page_rd[1] = p_page_wr[1] = p_page_ex[1] = p_ram[5] - PAGE_SIZE*1;                    // RAM5 0x4000 - 0x7FFF
    p_page_rd[2] = p_page_wr[2] = p_page_ex[2] = p_ram[2] - PAGE_SIZE*2;                    // RAM2 0x8000 - 0xBFFF
    p_page_rd[3] = p_page_wr[3] = p_page_ex[3] = p_ram[p7FFD & P7FFD_PAGE] - PAGE_SIZE*3;   // RAM  0xC000 - 0xFFFF
    if (rom_id != ROM_TRDOS){
        p_page_rd[0] = p_rom[rom_id];
        p_page_ex[0] = p_trap[rom_id];
    }else{
        p_page_rd[0] = p_page_ex[0] = p_rom[rom_id];
        p_page_ex[1] = p_trap[rom_id] - PAGE_SIZE*1;
        p_page_ex[2] = p_trap[rom_id] - PAGE_SIZE*2;
        p_page_ex[3] = p_trap[rom_id] - PAGE_SIZE*3;
    }
}

void Memory::set_rom(int id){
    rom_id = id;
}

bool Memory::io_wr(unsigned short port, unsigned char byte, int clk){
    if (!(port & 0x8002)){ // 7FFD decoded if A2 and A15 is zero.
        if (p7FFD & P7FFD_LOCK)
            return true;
        if (byte & P7FFD_ROM48){
            p_page_rd[0] = p_rom[ROM_48];
            p_page_ex[0] = p_trap[ROM_48];
        }else{
            p_page_rd[0] = p_rom[ROM_128];
            p_page_ex[0] = p_trap[ROM_128];
        }
        p_page_wr[0] = p_page_wr_null;
        p_page_rd[1] = p_page_wr[1] = p_page_ex[1] = p_ram[5] - PAGE_SIZE*1;                    // RAM5 0x4000 - 0x7FFF
        p_page_rd[2] = p_page_wr[2] = p_page_ex[2] = p_ram[2] - PAGE_SIZE*2;                    // RAM2 0x8000 - 0xBFFF
        p_page_rd[3] = p_page_wr[3] = p_page_ex[3] = p_ram[byte & P7FFD_PAGE] - PAGE_SIZE*3;    // RAM  0xC000 - 0xFFFF
        p7FFD = byte;
    }
    return false;
}

bool Memory::exec_trap(unsigned short pc){
    if (p_page_rd[0] == p_rom[ROM_48] && pc >> 8 == 0x3D){
        p_page_rd[0] = p_page_ex[0] = p_rom[ROM_TRDOS];
        p_page_ex[1] = p_trap[ROM_TRDOS] - PAGE_SIZE*1;
        p_page_ex[2] = p_trap[ROM_TRDOS] - PAGE_SIZE*2;
        p_page_ex[3] = p_trap[ROM_TRDOS] - PAGE_SIZE*3;
        return true;
    }else{
        if (p_page_rd[0] == p_rom[ROM_TRDOS] && pc >= 0x4000){
            p_page_rd[0] = p_rom[ROM_48];
            p_page_ex[0] = p_trap[ROM_48];
            p_page_ex[1] = p_ram[5] - PAGE_SIZE*1;
            p_page_ex[2] = p_ram[2] - PAGE_SIZE*2;
            p_page_ex[3] = p_ram[p7FFD & P7FFD_PAGE] - PAGE_SIZE*3;
            return true;
        }
    }
    return false;
}

void Memory::load_rom(ROM_BANK id, const char *path){
    FILE *fp = fopen(path, "rb");
    if (!fp)
        throw std::runtime_error(std::string("Open file: ") + std::string(path));
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size != PAGE_SIZE)
        throw std::runtime_error(std::string("ROM file: ") + std::string(path) + std::string(" has wring format"));
    fread(p_rom[id], 1, PAGE_SIZE, fp);
    if (id != ROM_TRDOS){
        memcpy(p_trap[id], p_rom[id], PAGE_SIZE);
        if (id == ROM_48)
            memset(p_trap[id] + 0x3D00, Z80_TRAP_OPCODE, 0x100);
    }else
        memset(p_trap[id], Z80_TRAP_OPCODE, PAGE_SIZE);
    fclose(fp);
}
